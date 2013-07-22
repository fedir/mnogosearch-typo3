/* Copyright (C) 2000-2013 Lavtech.com corp. All rights reserved.

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
#include <ctype.h>
#include <sys/types.h>

#include "udm_common.h"
#include "udm_socket.h"
#include "udm_host.h"
#include "udm_utils.h"
#include "udm_xmalloc.h"
#include "udm_http.h"
#include "udm_conf.h"
#include "udm_contentencoding.h"
#include "udm_url.h"
#include "udm_vars.h"
#include "udm_hrefs.h"


udm_content_type_t
UdmContentTypeByName(const char *str)
{
  if (!strncasecmp(str, UDM_CSTR_WITH_LEN("text/plain")) ||
      !strncasecmp(str, UDM_CSTR_WITH_LEN("text/tab-separated-values")) ||
      !strncasecmp(str, UDM_CSTR_WITH_LEN("text/x-diff")) ||
      !strncasecmp(str, UDM_CSTR_WITH_LEN("text/x-patch")) ||
      !strncasecmp(str, UDM_CSTR_WITH_LEN("application/x-patch")) ||
      !strncasecmp(str, UDM_CSTR_WITH_LEN("text/css")))
    return UDM_CONTENT_TYPE_TEXT_PLAIN;

  if (!strncasecmp(str,"text/html", 9))
    return UDM_CONTENT_TYPE_TEXT_HTML;

  if (!strncasecmp(str, "text/xml", 8) ||
      !strncasecmp(str, "application/xml", 15) ||
      !strncasecmp(str, "application/rss+xml", 19) ||
      strstr(str, "+xml") ||
      strstr(str, "rss"))
    return UDM_CONTENT_TYPE_TEXT_XML;

  if (!strncasecmp(str, UDM_CSTR_WITH_LEN("message/rfc822")))
    return UDM_CONTENT_TYPE_MESSAGE_RFC822;

  if (!strncasecmp(str, "audio/mpeg", 10))
    return UDM_CONTENT_TYPE_AUDIO_MPEG;

  if (!strncasecmp(str, "mnogosearch/htdb", 16))
    return UDM_CONTENT_TYPE_HTDB;

  if (!strncasecmp(str, UDM_CSTR_WITH_LEN("application/vnd.openxmlformats-officedocument.wordprocessingml.document")))
    return UDM_CONTENT_TYPE_DOCX;

  /* Note: text/richtext and text/enriched is not rtf, it's something else */
  if (!strncasecmp(str, UDM_CSTR_WITH_LEN("text/rtf")) ||
      !strncasecmp(str, UDM_CSTR_WITH_LEN("application/rtf")) ||
      !strncasecmp(str, UDM_CSTR_WITH_LEN("application/x-rtf")))
    return UDM_CONTENT_TYPE_TEXT_RTF;

  return UDM_CONTENT_TYPE_UNKNOWN;
}


int UdmHTTPConnect(UDM_ENV * Conf, UDM_CONN *connp,
                   char *hostname, int port, int timeout)
{
  size_t len;
    
  if (!connp || !hostname || !port)
    return -1;

  connp->port= port;
  connp->timeout= timeout;

  len= strlen(hostname);
  connp->hostname= UdmXrealloc(connp->hostname, len+1);
  udm_snprintf(connp->hostname, len+1, "%s", hostname);

  if (UdmHostLookup(&Conf->Hosts,connp)) /* FIXME: not reentrant */
    return -1;

  if (socket_open(connp))
    return -1;

  if (socket_connect(connp))
    return -1;
                          
  return 0;
}


void UdmParseHTTPResponse(UDM_AGENT * Indexer,UDM_DOCUMENT * Doc){      
  char  *token, *lt, *headers;
  int     oldstatus;
  
  Doc->Buf.content=NULL;
  oldstatus = UdmVarListFindInt(&Doc->Sections, "Status", 0);
  UdmVarListReplaceInt(&Doc->Sections, "ResponseSize", (int)Doc->Buf.size);
  UdmVarListDel(&Doc->Sections, "Content-Length");
  UdmVarListDel(&Doc->Sections, "Last-Modified");
  
  /* Cut HTTP response header first        */
  for(token=Doc->Buf.buf;*token;token++)
  {
    if(!strncmp(token,"\r\n\r\n",4))
    {
      *token='\0';
      Doc->Buf.content=token+4;
      break;
    }
    else if(!strncmp(token,"\n\n",2))
    {
      *token='\0';
      Doc->Buf.content=token+2;
      break;
    }
  }
  
  /* Bad response, return */
  if(!Doc->Buf.content)
  {
    /* Put content pointer to the very end of the buffer */
    Doc->Buf.content= Doc->Buf.buf + Doc->Buf.size;
    return;
  }
  
  /* Copy headers not to break them */
  headers = (char*)UdmStrdup(Doc->Buf.buf);
  
  /* Now lets parse response header lines */
  token = udm_strtok_r(headers,"\r\n",&lt);
  
  if(!token)return;
  
  if(!strncmp(token,"HTTP/",5))
  {
    int  status = atoi(token + 8);
    UdmVarListReplaceStr(&Doc->Sections,"ResponseLine",token);
    UdmVarListReplaceInt(&Doc->Sections, "Status", (oldstatus > status) ? oldstatus : status );
  }
  else
  {
    return;
  }
  
  for (token= udm_strtok_r(NULL,"\r\n",&lt) ; token;
       token = udm_strtok_r(NULL,"\r\n",&lt))
  {
    char *val;
    
    if((val=strchr(token,':')))
    {
      *val++='\0';
      val = UdmTrim(val," \t:");
      
      if (!strcasecmp(token, "Content-Type") ||
          !strcasecmp(token, "Content-Encoding"))
      {
        char *v;
        for(v=val ; *v ; v++) 
          *v = tolower(*v);
      }
      if (!strcasecmp(token, "Set-Cookie"))
      {
        char fullname[256];
        char *part, *lpart;
        char *name= NULL;
        char *value= NULL;
        const char *domain= NULL;
        const char *path= NULL;
        for (part= udm_strtok_r(val, ";" , &lpart) ; part;
             part= udm_strtok_r(NULL, ";", &lpart))
        {
          char *arg;
          part= UdmTrim(part, " ");
          if ((arg= strchr(part, '=')))
          {
            *arg++= '\0';
            if (!name)
            {
              name= part;
              value= arg;
              continue;
            }
            if (!strcasecmp(part, "path"))
            {
              path= arg;
              continue;
            }
            if (!strcasecmp(part, "domain"))
            {
              domain= arg;
              continue;
            }
          }
        }
        if (name && value)
        {
          if (domain && domain[0] == '.')
          {
            domain++;
          }
          else
          {
            domain= Doc->CurURL.hostname ? Doc->CurURL.hostname : "localhost";
          }
          if (!path)
          {
            path= Doc->CurURL.path ? Doc->CurURL.path : "/";
          }
          udm_snprintf(fullname, sizeof(fullname), "Set-Cookie.%s@%s%s", name, domain, path);
          UdmVarListReplaceStr(&Doc->Sections, fullname, value);
        }
        continue;
      }
    }
    UdmVarListReplaceStr(&Doc->Sections,token,val?val:"<NULL>");
  }
  UDM_FREE(headers);
  
  UdmVarListInsInt(&Doc->Sections,"Content-Length",
                   Doc->Buf.buf-Doc->Buf.content+(int)Doc->Buf.size +
                   Doc->Buf.content_length);
}
