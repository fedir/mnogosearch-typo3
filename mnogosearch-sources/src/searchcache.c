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
#include <fcntl.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_vars.h"
#include "udm_hash.h"
#include "udm_doc.h"
#include "udm_result.h"
#include "udm_word.h"
#include "udm_searchtool.h"
#include "udm_searchcache.h"
#include "udm_parsehtml.h"
#include "udm_log.h"
#include "udm_db_int.h"

/***************** Results cache functions **************/

/*
  Compose search cache file name 
  taking in account all parameters
  and query words themself       
*/

/*#define DEBUG_CACHE*/


static void
cache_file_name(char *dst,size_t len,UDM_ENV *Conf,UDM_RESULT *Res)
{
  char param[4*1024];
  const char *vardir=UdmVarListFindStr(&Conf->Vars,"VarDir",UDM_VAR_DIR);
/*  UDM_DB *db = Conf->db;*/
  
  udm_snprintf(param, sizeof(param)-1,
    "%s.%s.%s.%s.%s.%s.%s.%s.%s.%s.%s.%s.%s.%s:%s:%s:%s:%s:%s:%s:%s:%s:%s:%s-%s",
           UdmVarListFindStr(&Conf->Vars,"SearchMode","all"),
           UdmVarListFindStr(&Conf->Vars, "orig_m", ""),
           UdmVarListFindStr(&Conf->Vars, "fl", ""),
           UdmVarListFindStr(&Conf->Vars, "wm", ""),
           UdmVarListFindStr(&Conf->Vars, "o", ""),
           UdmVarListFindStr(&Conf->Vars, "t", ""),
           UdmVarListFindStr(&Conf->Vars, "cat", ""),
           UdmVarListFindStr(&Conf->Vars, "ul", ""),
           UdmVarListFindStr(&Conf->Vars, "u", ""),
           UdmVarListFindStr(&Conf->Vars, "wf", ""),
           UdmVarListFindStr(&Conf->Vars, "g", ""),
           UdmVarListFindStr(&Conf->Vars, "tmplt", ""),
           UdmVarListFindStr(&Conf->Vars, "GroupBySite", "no"),
           UdmVarListFindStr(&Conf->Vars, "site", "0"),
           UdmVarListFindStr(&Conf->Vars, "type", ""),
           UdmVarListFindStr(&Conf->Vars, "sp", ""),
           UdmVarListFindStr(&Conf->Vars, "sy", ""),
           UdmVarListFindStr(&Conf->Vars, "dt", ""),
           UdmVarListFindStr(&Conf->Vars, "dp", ""),
           UdmVarListFindStr(&Conf->Vars, "dx", ""),
           UdmVarListFindStr(&Conf->Vars, "dm", ""),
           UdmVarListFindStr(&Conf->Vars, "dy", ""),
           UdmVarListFindStr(&Conf->Vars, "db", ""),
           UdmVarListFindStr(&Conf->Vars, "de", ""),
           UdmVarListFindStr(&Conf->Vars, "s", "")
           );

#ifdef DEBUG_CACHE
  fprintf(stderr, "param: |%s|\n", param);
#endif
  
  udm_snprintf(dst, len, "%s%s%s%s%08X.%08X.%d",
               vardir, UDMSLASHSTR,
               "cache",UDMSLASHSTR,
               UdmStrHash32(param),
               UdmStrHash32(UdmVarListFindStr(&Conf->Vars,"q","")),
         0 /*(db->DBMode == UDM_DBMODE_CACHE) ? 0 : UdmVarListFindInt(&Conf->Vars, "np", 0)*/
         );
}


int
UdmSearchCacheStore1(UDM_AGENT *A, UDM_RESULT *R)
{
  char  fname[1024];
  FILE  *f;
  
  UdmLog(A, UDM_LOG_DEBUG, "UdmSearchCacheStore: Start");

  cache_file_name(fname,sizeof(fname),A->Conf,R);
  strcat(fname,".xml");

  UdmLog(A, UDM_LOG_DEBUG, "write to %s", fname);

  if((f=fopen(fname,"w"))){
    size_t  maxlen=128*1024;
    char  *textbuf=(char*)UdmMalloc(maxlen);
    UdmResultToTextBuf(R,textbuf,maxlen);
    fprintf(f,"%s\n",textbuf);
    fclose(f);
  }

  UdmLog(A, UDM_LOG_DEBUG, "UdmSearchCacheCache: Done");
  return UDM_OK;
}


int
UdmSearchCacheFind1(UDM_AGENT *A, UDM_RESULT *R)
{
  char    fname[1024];
  int    fd, res = UDM_OK;
  ssize_t    nread;
  size_t    buflen=128*1024;
  char    *buf = (char*)UdmMalloc(buflen);
  
  UdmLog(A, UDM_LOG_DEBUG, "UdmSearchCacheFind: Start");

  cache_file_name(fname,sizeof(fname),A->Conf,R);
  strcat(fname,".xml");

  UdmLog(A, UDM_LOG_DEBUG, "read from %s", fname);

  fd=open(fname,O_RDONLY);
  if (!fd) {
    UdmLog(A, UDM_LOG_ERROR, "Can't open %s" __FILE__, fname);
    res = UDM_ERROR;
    goto free;
  }
  nread=read(fd,buf,buflen-1);
  close(fd);
  if(nread<=0) {
    UdmLog(A, UDM_LOG_ERROR, "Can't read from %s" __FILE__, fname);
    res = UDM_ERROR;
    goto free;
  }
  
  UdmLog(A, UDM_LOG_DEBUG, " %ld read", nread);

  buf[nread]='\0';
  UdmResultFromTextBuf(R,buf);
free:
  UDM_FREE(buf);

  UdmLog(A, UDM_LOG_DEBUG, "UdmSearchCacheFind: Done");
  return res;
}


int __UDMCALL
UdmSearchCacheStore(UDM_AGENT *query, UDM_RESULT *Res)
{
#if 1
  return 0;
#else
  int  fd;
  char  fname[1024];
  size_t  i;
  int  page_number = UdmVarListFindInt(&query->Conf->Vars,"np",0);
  int  page_size   = UdmVarListFindInt(&query->Conf->Vars,"ps",10);
  size_t  topcount;
  
  topcount= page_size * (page_number + 1) - 1;
  
  if (topcount >= Res->total_found)
    topcount= Res->total_found - 1;
  
  if (topcount < UDM_FAST_PRESORT_DOCS)
  {
/*UdmSortSearchWordsByWeight(Res->CoordList.Coords,Res->CoordList.ncoords);*/
    cache_file_name(fname,sizeof(fname),query->Conf,Res);
    
    UdmLog(query, UDM_LOG_DEBUG, "write to %s, found:%d", fname, Res->total_found);
    
    if((fd= open(fname,O_WRONLY|O_CREAT|O_TRUNC|UDM_BINARY,UDM_IWRITE)) >= 0)
    {
      write(fd, &Res->total_found, sizeof(Res->total_found));
      write(fd, &(Res->WWList),sizeof(UDM_WIDEWORDLIST)); 
      for (i= 0; i < Res->WWList.nwords; i++)
      {
        write(fd, &(Res->WWList.Word[i]), sizeof(UDM_WIDEWORD));
        write(fd, Res->WWList.Word[i].word, Res->WWList.Word[i].len);
        write(fd, Res->WWList.Word[i].uword, sizeof(int) * Res->WWList.Word[i].len);
      }
      
      write(fd, Res->CoordList.Coords, Res->CoordList.ncoords * sizeof(UDM_URL_CRD));
      write(fd, Res->CoordList.Data, Res->CoordList.ncoords * sizeof(UDM_URLDATA));
      topcount= 0;
      write(fd, &topcount, sizeof(topcount));
      close(fd);
    }
    else
    {
#ifdef DEBUG_CACHE
      fprintf(stderr,"%s\n",strerror(errno));
#endif
    }
  }
  return(0);
#endif
}


int __UDMCALL
UdmSearchCacheFind(UDM_AGENT * Agent,UDM_RESULT *Res)
{
  return -1;
#if 0
  char fname[1024];
  int fd;
  UDM_URL_CRD *wrd = NULL;
  UDM_URLDATA *dat = NULL;
  int bytes;
  UDM_WIDEWORDLIST wwl;
  UDM_WIDEWORD ww;
  size_t i;
/*
  int page_size   = UdmVarListFindInt(&Agent->Conf->Vars,"ps",10);
  int page_number = UdmVarListFindInt(&Agent->Conf->Vars,"np",0);
*/
  size_t topcount;
  
#ifdef DEBUG_CACHE
  fprintf(stderr, "UdmSearchCacheFind: Start\n");
#endif
  
/*  Res->offset = 1;*/
  
  cache_file_name(fname,sizeof(fname),Agent->Conf,Res);
  if((fd=open(fname,O_RDONLY|UDM_BINARY))<0) {
#ifdef DEBUG_CACHE
    fprintf(stderr, " %s open error %s\n", fname, strerror(errno));
#endif
    return(-1);
  }
  
  if( (-1 == read(fd,&Res->total_found, sizeof(Res->total_found))) ){
    close(fd);
    return(-1);
  }
  
#ifdef DEBUG_CACHE
  fprintf(stderr, " found: %d\n", Res->total_found);
#endif
  if (-1==read(fd, &wwl, sizeof(UDM_WIDEWORDLIST))) {
    close(fd);
    return(-1);
  }
#ifdef DEBUG_CACHE
  fprintf(stderr, " nwords: %d\n", wwl.nwords);
#endif
  UdmWideWordListFree(&Res->WWList);
  for(i = 0; i < wwl.nwords; i++) {
    if (-1==read(fd, &ww, sizeof(UDM_WIDEWORD))) {
      close(fd);
      return(-1);
    }
    ww.word = (char*)UdmMalloc(ww.len + 1);
    bzero((void*)ww.word, ww.len+1);
    ww.uword = (int *)UdmMalloc(sizeof(int) * ww.len + 1);
    bzero((void*)ww.uword, sizeof(int) * ww.len + 1);
    if (-1==read(fd, ww.word, ww.len)) {
      close(fd);
      return(-1);
    }
    if (-1==read(fd, ww.uword, sizeof(int) * ww.len)) {
      close(fd);
      return(-1);
    }
    UdmWideWordListAdd(&Res->WWList, &ww);
    UDM_FREE(ww.word);
    UDM_FREE(ww.uword);
  }
  Res->WWList.nuniq = wwl.nuniq;
  
  wrd=(UDM_URL_CRD*)UdmMalloc(Res->total_found*sizeof(*wrd));
  dat = (UDM_URLDATA*)UdmMalloc(Res->total_found * sizeof(*dat));
  
  if(-1==lseek(fd,(off_t)0/*(page_number*page_size*sizeof(*wrd))*/,SEEK_CUR)){
    close(fd);
    return(-1);
  }
  if(-1==(bytes=read(fd, wrd, Res->total_found/* page_size*/ * sizeof(*wrd) ))){
    close(fd);
    return(-1);
  }
  Res->CoordList.ncoords=bytes/sizeof(*wrd);
  if(-1==(bytes=read(fd, dat, Res->total_found/* page_size*/ * sizeof(*dat) ))){
    close(fd);
    return(-1);
  }

  if( (-1 == read(fd, &topcount, sizeof(topcount))) ){
    close(fd);
    return(-1);
  }
  close(fd);
  UDM_FREE(Res->CoordList.Coords);
  Res->CoordList.Coords = wrd;
  Res->CoordList.Data = dat;
  Res->num_rows = Res->total_found = Res->CoordList.ncoords; /* ??? was commented, why ? */

#ifdef DEBUG_CACHE
  fprintf(stderr, "UdmSearchCacheFind: Done\n");
#endif

  return(0);
#endif
}
