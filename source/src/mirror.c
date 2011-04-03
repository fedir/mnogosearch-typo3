/* Copyright (C) 2000-2011 Lavtech.com corp. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/

#include "udm_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <fcntl.h>

#ifdef WIN32
#include <io.h>
#endif


#include "udm_common.h"
#include "udm_proto.h"
#include "udm_utils.h"
#include "udm_vars.h"
#include "udm_log.h"

__C_LINK int __UDMCALL UdmMirrorGET(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc, UDM_URL *url) {
     int       size = 0;
     int       fbody, fheader;
     char      *str, *estr;
     struct stat    sb;
     time_t         nowtime;
     size_t          str_len, estr_len;
     int       have_headers=0;
     int       mirror_period=UdmVarListFindInt(&Doc->Sections,"MirrorPeriod",-1);
     const char     *mirror_data=UdmVarListFindStr(&Doc->Sections,"MirrorRoot",NULL);
     const char     *mirror_hdrs=UdmVarListFindStr(&Doc->Sections,"MirrorHeadersRoot",NULL);

     Doc->Buf.size=0;
     nowtime = time(NULL);
     
     if(mirror_period <= 0)return(UDM_MIRROR_NOT_FOUND);
     
     /* MirrorRoot is not specified, nothing to do */
     if (!mirror_data)
     {
       UdmLog(Indexer, UDM_LOG_ERROR, "MirrorGet: MirrorRoot is not set");
       return UDM_MIRROR_NOT_FOUND;
     }
     
     str_len = 128 + strlen(mirror_data) + ((mirror_hdrs) ? strlen(mirror_hdrs) : 0) + strlen(UDM_NULL2EMPTY(url->schema)) +
       strlen(UDM_NULL2EMPTY(url->hostname)) + strlen(UDM_NULL2EMPTY(url->path)) + 
       (estr_len = (url->filename && url->filename[0]) ? 3 * strlen(url->filename) : 16);

     if ((str = (char*)UdmMalloc(str_len)) == NULL) return UDM_MIRROR_NOT_FOUND;
     if ((estr = (char*)UdmMalloc(estr_len)) == NULL) {
       UDM_FREE(str);
       return UDM_MIRROR_NOT_FOUND;
     }

     udm_snprintf(str, str_len, "%s", url->filename && strlen(url->filename) ? url->filename : "index.html");
     UdmEscapeURL(estr, str);

     udm_snprintf(str, str_len, "%s"UDMSLASHSTR"%s"UDMSLASHSTR"%s%s%s.body", mirror_data,
               UDM_NULL2EMPTY(url->schema), UDM_NULL2EMPTY(url->hostname), UDM_NULL2EMPTY(url->path), estr);

     if ((fbody = open(str, O_RDONLY|UDM_BINARY)) == -1){
          UdmLog(Indexer, UDM_LOG_EXTRA, "Mirror file %s not found", str);
          UDM_FREE(estr); UDM_FREE(str);
          return UDM_MIRROR_NOT_FOUND;
     }

     /* Check on file mtime > days ? return */
     if (fstat(fbody, &sb)) {
             UDM_FREE(estr); UDM_FREE(str);
          return UDM_MIRROR_NOT_FOUND;
     }
     if (nowtime > sb.st_mtime + mirror_period ) {
          close(fbody);
          UdmLog(Indexer, UDM_LOG_EXTRA, "%s is older then %d secs", str, mirror_period);
          UDM_FREE(estr); UDM_FREE(str);
          return UDM_MIRROR_EXPIRED;
     }

     if(mirror_hdrs){
          udm_snprintf(str, str_len, "%s"UDMSLASHSTR"%s"UDMSLASHSTR"%s%s%s.header", 
                mirror_hdrs, UDM_NULL2EMPTY(url->schema), UDM_NULL2EMPTY(url->hostname), UDM_NULL2EMPTY(url->path), estr);

          if ((fheader = open(str, O_RDONLY|UDM_BINARY))>=0) {
               size = read(fheader, Doc->Buf.buf, Doc->Buf.maxsize);
               close(fheader);
               strcpy(Doc->Buf.buf + size, "\r\n\r\n");
               have_headers=1;
          }
     }
     if(!have_headers){
          /* header file not found   */
          /* get body as file method */

          sprintf(Doc->Buf.buf,"HTTP/1.0 200 OK\r\n");
          sprintf(UDM_STREND(Doc->Buf.buf),"\r\n");
     }

     UDM_FREE(estr); UDM_FREE(str);
     Doc->Buf.content = UDM_STREND(Doc->Buf.buf);
     size = read(fbody, Doc->Buf.content, Doc->Buf.maxsize - (Doc->Buf.content-Doc->Buf.buf));
     close(fbody);
     if(size<0)return size;
     /* Calculate size and append trailing 0 */
     Doc->Buf.size=(Doc->Buf.content-Doc->Buf.buf)+size;
     Doc->Buf.content[Doc->Buf.size]='\0';
     return UDM_OK;
}

__C_LINK int __UDMCALL UdmMirrorPUT(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc, UDM_URL *url) {
     int       fd,size;
     char      *str, *estr;
     size_t         str_len, estr_len;
     const char     *mirror_data=UdmVarListFindStr(&Doc->Sections,"MirrorRoot",NULL);
     const char     *mirror_hdrs=UdmVarListFindStr(&Doc->Sections,"MirrorHeadersRoot",NULL);
     char            *token, savechar;
     
     if (!mirror_data)
     {
       UdmLog(Indexer, UDM_LOG_ERROR, "MirrorPUT: MirrorRoot is not set");
       return UDM_ERROR;
     }

     /* Cut HTTP response header first        */
     for(token = Doc->Buf.buf; *token; token++){
          if(!strncmp(token, "\r\n\r\n", 4)){
               *token = '\0';
               Doc->Buf.content = token + 4;
               savechar = '\r';
               break;
          }else
          if(!strncmp(token, "\n\n", 2)){
               *token = '\0';
               Doc->Buf.content = token + 2;
               savechar = '\n';
               break;
          }
     }

     str_len = 128 + strlen(mirror_data) + ((mirror_hdrs) ? strlen(mirror_hdrs) : 0) + strlen(UDM_NULL2EMPTY(url->schema)) +
       strlen(UDM_NULL2EMPTY(url->hostname)) + strlen(UDM_NULL2EMPTY(url->path)) + 
       (estr_len = (url->filename && url->filename[0]) ? 3 * strlen(url->filename) : 16);

     if ((str = (char*)UdmMalloc(str_len)) == NULL) return UDM_MIRROR_CANT_BUILD;
     if ((estr = (char*)UdmMalloc(estr_len)) == NULL) {
       UDM_FREE(str);
       return UDM_MIRROR_CANT_BUILD;
     }

     udm_snprintf(str, str_len, "%s", (url->filename && strlen(url->filename)) ? url->filename : "index.html");
     UdmEscapeURL(estr, str);

     /* Put Content if MirrorRoot is specified */
     if(mirror_data){
             udm_snprintf(str, str_len, "%s"UDMSLASHSTR"%s"UDMSLASHSTR"%s%s", mirror_data, 
                    UDM_NULL2EMPTY(url->schema), UDM_NULL2EMPTY(url->hostname), UDM_NULL2EMPTY(url->path));

          if(UdmBuild(str, 0755) != 0){
                        UdmLog(Indexer, UDM_LOG_ERROR, "Can't create dir %s", str);
                        *token = savechar;
               UDM_FREE(estr); UDM_FREE(str);
                        return UDM_MIRROR_CANT_BUILD;
                }

          strcat(str, UDMSLASHSTR);
          strcat(str, estr);
          strcat(str, ".body");

          if ((fd = open(str, O_CREAT|O_WRONLY|UDM_BINARY,UDM_IWRITE)) == -1){
               UdmLog(Indexer, UDM_LOG_EXTRA, "Can't open mirror file %s\n", str);
               *token = savechar;
               UDM_FREE(estr); UDM_FREE(str);
               return UDM_MIRROR_CANT_OPEN;
          }
          size = write(fd, Doc->Buf.content, Doc->Buf.size - (Doc->Buf.content-Doc->Buf.buf));
          close(fd);
     }

     /* Put Headers if MirrorHeadersRoot is specified */
     if(mirror_hdrs){
             udm_snprintf(str, str_len, "%s"UDMSLASHSTR"%s"UDMSLASHSTR"%s%s", mirror_hdrs, UDM_NULL2EMPTY(url->schema),
                    UDM_NULL2EMPTY(url->hostname), UDM_NULL2EMPTY(url->path));

          if(UdmBuild(str, 0755) != 0){
                        UdmLog(Indexer, UDM_LOG_ERROR, "Can't create dir %s", str);
                        *token = savechar;
               UDM_FREE(estr); UDM_FREE(str);
                        return UDM_MIRROR_CANT_BUILD;
                }

          strcat(str, UDMSLASHSTR);
          strcat(str, estr);
          strcat(str, ".header");

          if ((fd = open(str, O_CREAT|O_WRONLY|UDM_BINARY,UDM_IWRITE)) == -1){
               UdmLog(Indexer, UDM_LOG_EXTRA, "Can't open mirror file %s\n", str);
               *token = savechar;
               UDM_FREE(estr); UDM_FREE(str);
               return UDM_MIRROR_CANT_OPEN;
          }
          size = write(fd, Doc->Buf.buf, strlen(Doc->Buf.buf));
          close(fd);
     }
     UDM_FREE(estr); UDM_FREE(str);
     *token = savechar;
     return UDM_OK;
}
