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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <errno.h>

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_proto.h"
#include "udm_services.h"
#include "udm_agent.h"
#include "udm_db.h"
#include "udm_doc.h"
#include "udm_result.h"
#include "udm_sdp.h"
#include "udm_xmalloc.h"
#include "udm_searchtool.h"
#include "udm_vars.h"
#include "udm_word.h"
#include "udm_db_int.h"
#include "udm_log.h"
#include "udm_host.h"
#include "udm_http.h"
#include "udm_url.h"
#include "udm_mutex.h"
#include "udm_store.h"


/*
#define DEBUG_SDP
*/

/*
#define DEBUG_SEARCH
*/


int __UDMCALL UdmResAddDocInfoSearchd(UDM_AGENT *A,
                                      UDM_RESULT *TmpRes,
                                      UDM_DB *db,
                                      UDM_RESULT *Res,
                                      size_t dbnum)
{
  size_t i;
  for (i= 0; i < Res->num_rows; i++)
  {
    UDM_URLDATA *Data= &Res->URLData.Item[i + Res->first];
    size_t Doc_dbnum= UDM_COORD2DBNUM(Data->score);

    if (Doc_dbnum == dbnum)
    {
      size_t id, num= Data->url_id;
      UDM_RESULT *SrcRes= A->Conf->dbl.nitems == 1 ? Res : &TmpRes[dbnum];
      UDM_VARLIST *DstSections= &Res->Doc[i].Sections;
      id= UdmVarListFindInt(&SrcRes->Doc[num].Sections, "id", 0);
      Data->url_id= id;
      if (A->Conf->dbl.nitems > 1)
      {
        UdmVarListReplaceLst(DstSections, &SrcRes->Doc[num].Sections, NULL, "*");
      }
      else if (A->Conf->dbl.nitems == 1)
      {
        if (Res->first)
        {
          UdmVarListFree(DstSections);
          UdmVarListReplaceLst(DstSections, &SrcRes->Doc[num].Sections, NULL, "*");
        }
      }
      UdmVarListReplaceInt(DstSections, "id", id);
      UdmVarListReplaceInt(DstSections, "DBOrder", num + 1);
    }
  }
  return UDM_OK;
}


static int
UdmVarListLog2(UDM_AGENT *A,UDM_VARLIST *V,int l,const char *pre)
{
  size_t h;
  if (UdmNeedLog(l))
  {
    for(h=0; h < V->nvars; h++)
    {
      UDM_VAR *v=&V->Var[h];
      UdmLog(A,l,"%s.%s: %s",pre,v->name,v->val?v->val:"<NULL>");
    }
  }
  return UDM_OK;
}


static char *
UdmParseOpenSearchAddr(UDM_VARLIST *Vars, const char *src)
{
  char *res;
  UDM_DSTR d;
  UdmDSTRInit(&d, 1024);
  UdmDSTRParse(&d, src, Vars);
  res= UdmStrdup(d.data);
  UdmDSTRFree(&d);
  return res;
}


/*
  Create NODE.QUERY_STRING from ENV.QUERY_STRING,
  removing original 'ps' and 'np' variables.
*/
static void
UdmAddNodeQueryString(UDM_VARLIST *Vars)
{
  size_t dstlen;
  size_t ps= UdmVarListFindInt(Vars, "ps", 10);
  size_t np= UdmVarListFindInt(Vars, "np", 0);
  size_t offs= UdmVarListFindInt(Vars, "offs", 0);
  char *dst, *d;
  const char *query_string, *s;
  int group_by_site= UdmVarListFindBool(Vars, "GroupBySite", 0) &&
                     (UdmVarListFindInt(Vars, "site", 0) == 0);
  int group_by_site_factor= group_by_site ? 3 : 1;

  if (!(query_string= UdmVarListFindStr(Vars, "ENV.QUERY_STRING", NULL)))
    return;
  dstlen= strlen(query_string) + 20;
  dst= (char*) UdmMalloc(dstlen);

  for (d= dst, s= query_string; *s; )
  {
    const char *amp;
    for (amp= s; *amp && *amp != '&'; amp++);
    if (!strncasecmp(s, "ps=", 3))
    {
    }
    else if (!strncasecmp(s, "np=", 3))
    {
    }
    else if (!strncasecmp(s, "offs=", 5))
    {
    }
    else
    {
      size_t toklen= amp - s;
      if (d > dst)
        *d++= '&';
      memcpy(d, s, toklen);
      d+= toklen;
    }
    if (!*amp)
      break;
    s= amp + 1;
  }
  sprintf(d, "&ps=%d", (int) ((offs + ps * (np + 1)) * group_by_site_factor));
  UdmVarListReplaceStr(Vars, "NODE_QUERY_STRING", dst);
  UdmFree(dst);
}

                            
int __UDMCALL UdmFindWordsSearchd(UDM_AGENT *A,UDM_RESULT *Res, UDM_DB *db)
{
  UDM_DOCUMENT Inc;
  const char *host= UdmVarListFindStr(&db->Vars, "DBHost", "localhost");
  const char *dbaddr= UdmVarListFindStr(&db->Vars, "DBAddr", "");
  int port= UdmVarListFindInt(&db->Vars, "DBPort", 80);
  int lookup_error, rc;
  size_t length;
  udm_timer_t ticks;

  UdmDocInit(&Inc);
  Inc.Buf.maxsize= UDM_MAXDOCSIZE;
  if(!Inc.Buf.buf) Inc.Buf.buf= (char*)UdmMalloc(Inc.Buf.maxsize);
  Inc.Spider.read_timeout= UdmVarListFindInt(&A->Conf->Vars, "ReadTimeOut", UDM_READ_TIMEOUT);

  UDM_GETLOCK(A, UDM_LOCK_CONF);
  UdmAddNodeQueryString(&A->Conf->Vars);
  if (1)
  {
    char *dbaddr2;
    dbaddr2= UdmParseOpenSearchAddr(&A->Conf->Vars, dbaddr);
    UdmURLParse(&Inc.CurURL, dbaddr2);
    UdmLog(A, UDM_LOG_ERROR, "DBAddr: %s", dbaddr2);
    UdmFree(dbaddr2);
  }
  else
  {
    UdmURLParse(&Inc.CurURL, dbaddr);
  }
  UDM_RELEASELOCK(A, UDM_LOCK_CONF);

  if (!strcmp(Inc.CurURL.schema, "http"))
  {
    UdmVarListReplaceStr(&Inc.RequestHeaders, "Host", host);
    Inc.connp.hostname= (char*)UdmStrdup(host);
    Inc.connp.port= port;
    
    UDM_GETLOCK(A, UDM_LOCK_CONF);
    if ((lookup_error= UdmHostLookup(&A->Conf->Hosts, &Inc.connp)))
      sprintf(A->Conf->errstr, "Host lookup failed: '%s'", host);
    UDM_RELEASELOCK(A, UDM_LOCK_CONF);
    if(lookup_error)
      return UDM_ERROR;
  }
  
  ticks= UdmStartTimer();
  rc= UdmGetURL(A, &Inc);
  UdmLog(A, UDM_LOG_DEBUG, "Received searchd response: %.2f", UdmStopTimer(&ticks));
  
  if (UDM_OK != rc)
    return rc;
  
  UdmParseHTTPResponse(A, &Inc);
  if (!Inc.Buf.content)
    return UDM_ERROR;
  UdmVarListLog2(A, &Inc.Sections, UDM_LOG_DEBUG, "Response");
  
  UdmLog(A, UDM_LOG_DEBUG, "Start parsing results");
  ticks= UdmStartTimer();
  length= Inc.Buf.size - (Inc.Buf.content - Inc.Buf.buf);
  UdmResultFromXML(A, Res, Inc.Buf.content, length, A->Conf->lcs);
  UdmDocFree(&Inc);
  UdmLog(A, UDM_LOG_DEBUG, "Stop parsing results: %.2f", UdmStopTimer(&ticks));
  UdmLog(A, UDM_LOG_DEBUG, "searchd: %d rows, %d totalResults",
                           (int) Res->num_rows, (int) Res->total_found);
  return UDM_OK;
}


int __UDMCALL UdmSearchdCatAction(UDM_AGENT *A, UDM_CATEGORY *C, int cmd, void *db)
{
  return UDM_OK;
}


int __UDMCALL UdmSearchdURLAction(UDM_AGENT *A, UDM_DOCUMENT *D, int cmd, void *db)
{
  int rc= UDM_OK;
  if (cmd == UDM_URL_ACTION_GET_CACHED_COPY)
  {
    UDM_RESULT tmp;
    UdmResultInit(&tmp);
    /*
       UDM_LOCK_CONF mutex is locked here,
       UdmFindWordsSearchd expects unlocked mutex.
       Let's unlock it and then lock again.
    */
    UDM_RELEASELOCK(A, UDM_LOCK_CONF);
    rc= UdmFindWordsSearchd(A, &tmp, db);
    UDM_GETLOCK(A, UDM_LOCK_CONF);
    if (tmp.num_rows)
    {
      UDM_DOCUMENT *tmpDoc= &tmp.Doc[0];
      UDM_VAR *cc= UdmVarListFind(&tmpDoc->Sections, "CachedCopyBase64");
      if (cc && !D->Buf.content)
      {
        D->Buf.buf= UdmMalloc(UDM_MAXDOCSIZE);
        UdmCachedCopyUnpack(D, cc->val, cc->curlen);
      }
    }
    UdmResultFree(&tmp);
  }
  return rc;
}

int UdmCloneListSearchd(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc, UDM_RESULT *Res, UDM_DB *db)
{
  return UDM_OK;
}
