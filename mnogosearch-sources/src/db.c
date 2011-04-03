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
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif


#include "udm_common.h"
#include "udm_utils.h"
#include "udm_db.h"
#include "udm_db_int.h"
#include "udm_sqldbms.h"
#include "udm_url.h"
#include "udm_sdp.h"
#include "udm_vars.h"
#include "udm_mutex.h"
#include "udm_searchtool.h"
#include "udm_result.h"
#include "udm_log.h"
#include "udm_agent.h"
#include "udm_proto.h"
#include "udm_host.h"
#include "udm_hash.h"
#include "udm_doc.h"
#include "udm_services.h"
#include "udm_xmalloc.h"
#include "udm_searchcache.h"
#include "udm_store.h"
#include "udm_match.h"
#include "udm_word.h"

#define UDM_THREADINFO(A,s,m)	if(A->Conf->ThreadInfo)A->Conf->ThreadInfo(A,s,m)


void *UdmDBInit(void *vdb)
{
  UDM_DB *db=vdb;
  size_t  nbytes=sizeof(UDM_DB);
  
  if(!db)
  {
    db=(UDM_DB*)UdmMalloc(nbytes);
    bzero((void*)db, nbytes);
    db->freeme=1;
  }
  else
  {
    bzero((void*)db, nbytes);
  }
  db->numtables=1;

#if HAVE_SQL
  UdmWordCacheInit(&db->WordCache);
#endif
 
  return db;
}


void UdmDBFree(void *vdb)
{
  UDM_DB  *db=vdb;

  UDM_FREE(db->DBName);
  UDM_FREE(db->where);
  UDM_FREE(db->from);
  
#if HAVE_SQL
  if (db->connected) UdmSQLClose(db);
#endif
  
  UdmVarListFree(&db->Vars);
  if(db->freeme)UDM_FREE(vdb);
  return;
}


/*
int UdmURLData(UDM_ENV *Conf, UDM_URLDATALIST *L, UDM_DB *db)
{
  int  res=UDM_OK;
  
  L->nitems=0;
  
#ifdef HAVE_SQL
  res=UdmURLDataSQL(Conf, L, db);
#endif
  return res;
}
*/


int
UdmDBIsActive(UDM_AGENT *A, size_t num_arg)
{
  size_t num= UdmVarListFindInt(&A->Conf->Vars, "DBLimit", 0);
  return num == 0 || num == num_arg + 1;
}


__C_LINK int __UDMCALL UdmClearDatabase(UDM_AGENT *A)
{
  int  res=UDM_ERROR;
  UDM_DB  *db;
  size_t i, dbto =  A->Conf->dbl.nitems;

  for (i = 0; i < dbto; i++)
  {
    if (!UdmDBIsActive(A, i))
      continue;
    db = &A->Conf->dbl.db[i];
#ifdef HAVE_SQL
    res = UdmClearDBSQL(A, db);
    UDM_FREE(db->where);          /* clear db->where for next parameters */
#endif
    if (res != UDM_OK) break;
  }
  if(res!=UDM_OK)
  {
    strcpy(A->Conf->errstr,db->errstr);
  }
  return res;
}


static int
UdmRegisterChild(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc)
{
  int result= UDM_OK;
  
  UDM_VAR    *Sec;
  const char  *parent=NULL;
  int    parent_id=0;
    
  UDM_LOCK_CHECK_OWNER(Indexer, UDM_LOCK_CONF);
  
  if((Sec=UdmVarListFind(&Doc->Sections,"Header.References")) && Sec->val)
  {
    /* 
      References contains all message IDs of my predecessors,
      space separated my direct parent is the last in the list.
    */
    if ((parent= strrchr(Sec->val,' ')))
    {
      /* parent now points to the space character, skip it */
      ++parent;
    }
    else
    {
      /* there is only one entry in references, so this is my parent */
      parent=Sec->val;
    }  
  }
    
  /* get parent from database */
  if (parent && strlen(parent) && strchr(parent,'@'))
  {
    UDM_DOCUMENT Msg;
      
    UdmDocInit(&Msg);
    UdmVarListReplaceStr(&Msg.Sections,"Header.Message-ID",parent);
    result= UdmURLActionNoLock(Indexer, &Msg, UDM_URL_ACTION_FINDBYMSG);
    parent_id = UdmVarListFindInt(&Msg.Sections,"ID",0);
    UdmVarListReplaceInt(&Doc->Sections,"Header.Parent-ID",parent_id);
    UdmDocFree(&Msg);
  }
  
  /* Now register me with my parent  */
  if(parent_id)
    result = UdmURLActionNoLock(Indexer, Doc, UDM_URL_ACTION_REGCHILD);
  return result;
}


static int DocUpdate(UDM_AGENT * Indexer, UDM_DOCUMENT *Doc)
{
  int        rc= UDM_OK;
  int        status=UdmVarListFindInt(&Doc->Sections,"Status",0);
  urlid_t    url_id = (urlid_t)UdmVarListFindInt(&Doc->Sections, "ID", 0);
  time_t     next_index_time= time(NULL) + Doc->Spider.period;
  char       dbuf[64];
  int        use_newsext; 
  int        action;

  UDM_LOCK_CHECK_OWNER(Indexer, UDM_LOCK_CONF);

  use_newsext= !strcasecmp(UdmVarListFindStr(&Indexer->Conf->Vars,"NewsExtensions","no"),"yes");

  if (Doc->method == UDM_METHOD_VISITLATER)
  {
    action= UDM_URL_ACTION_SUPDATE;
    goto update;
  }
  
  /* First of all check that URL must be delated */
  if(Doc->method==UDM_METHOD_DISALLOW)
  {
    UdmLog(Indexer,UDM_LOG_ERROR,"Deleting %s", UdmVarListFindStr(&Doc->Sections, "URL", ""));
    rc= UdmURLActionNoLock(Indexer, Doc, UDM_URL_ACTION_DELETE);
    return rc;
  }

  switch(status){
  
  case 0: /* No HTTP code */
    UdmHostErrorIncrement(&Indexer->Conf->Hosts, Doc->connp.hostname);
    UdmLog(Indexer,UDM_LOG_ERROR,"No HTTP response status");
    next_index_time=time(NULL)+Doc->Spider.net_error_delay_time;
    action= UDM_URL_ACTION_SUPDATE;
    break;
  
  case UDM_HTTP_STATUS_OK:                            /* 200 */
  case UDM_HTTP_STATUS_PARTIAL_OK:                    /* 206 */
    if(!UdmVarListFind(&Doc->Sections,"Content-Type"))
    {
      UdmLog(Indexer,UDM_LOG_ERROR,"No Content-type header");
      next_index_time=time(NULL)+Doc->Spider.net_error_delay_time;
      UdmVarListReplaceInt(&Doc->Sections,"Status",UDM_HTTP_STATUS_INTERNAL_SERVER_ERROR);
      UdmHostErrorIncrement(&Indexer->Conf->Hosts, Doc->connp.hostname);
      action= UDM_URL_ACTION_SUPDATE;
      break;
    }
    else
    {
      UdmHostErrorReset(&Indexer->Conf->Hosts, Doc->connp.hostname);

      /* Check clones */
      if(Doc->method == UDM_METHOD_GET && Doc->Spider.use_clones)
      {
        urlid_t    origin_id = 0;
        if (UDM_OK != (rc= UdmURLActionNoLock(Indexer, Doc, UDM_URL_ACTION_FINDORIG)))
          return rc;
        origin_id= (urlid_t)UdmVarListFindInt(&Doc->Sections,"Origin-ID",0);
  
        if(origin_id && origin_id != url_id)
        {
          if (UdmNeedLog(UDM_LOG_EXTRA))
            UdmLog(Indexer, UDM_LOG_EXTRA, "Duplicate Document %s with #%d", 
                   UdmVarListFindStr(&Doc->Sections, "URL", ""), origin_id);
          action= UDM_URL_ACTION_UPDCLONE;
          break;
        }
      }
  
      /* Check that document wasn't modified since last indexing */
      if ((UdmVarListFindInt(&Doc->Sections,"crc32", 0) != 0) 
          &&  (UdmVarListFindInt(&Doc->Sections,"crc32old",0)==UdmVarListFindInt(&Doc->Sections,"crc32",0)) 
          &&  (!(Indexer->flags&UDM_FLAG_REINDEX)))
      {
        action= UDM_URL_ACTION_SUPDATE;
        break;
      }
  
      /* For NEWS extension: get rec_id from my */
      /* parent out of db (if I have one...)    */
      if (use_newsext && (UDM_OK != (rc= UdmRegisterChild(Indexer, Doc))))
        return rc;

      action= UDM_URL_ACTION_LUPDATE;
      break;
    }

  case UDM_HTTP_STATUS_NOT_MODIFIED:                  /* 304 */
    action= UDM_URL_ACTION_SUPDATE;
    break;
  
  case UDM_HTTP_STATUS_MULTIPLE_CHOICES:              /* 300 */
  case UDM_HTTP_STATUS_MOVED_PARMANENTLY:             /* 301 */
  case UDM_HTTP_STATUS_MOVED_TEMPORARILY:             /* 302 */
  case UDM_HTTP_STATUS_SEE_OTHER:                     /* 303 */
  case UDM_HTTP_STATUS_USE_PROXY:                     /* 305 */
  case UDM_HTTP_STATUS_BAD_REQUEST:                   /* 400 */
  case UDM_HTTP_STATUS_UNAUTHORIZED:                  /* 401 */
  case UDM_HTTP_STATUS_PAYMENT_REQUIRED:              /* 402 */
  case UDM_HTTP_STATUS_FORBIDDEN:                     /* 403 */
  case UDM_HTTP_STATUS_NOT_FOUND:                     /* 404 */
  case UDM_HTTP_STATUS_METHOD_NOT_ALLOWED:            /* 405 */
  case UDM_HTTP_STATUS_NOT_ACCEPTABLE:                /* 406 */
  case UDM_HTTP_STATUS_PROXY_AUTHORIZATION_REQUIRED:  /* 407 */
  case UDM_HTTP_STATUS_REQUEST_TIMEOUT:               /* 408 */
  case UDM_HTTP_STATUS_CONFLICT:                      /* 409 */
  case UDM_HTTP_STATUS_GONE:                          /* 410 */
  case UDM_HTTP_STATUS_LENGTH_REQUIRED:               /* 411 */
  case UDM_HTTP_STATUS_PRECONDITION_FAILED:           /* 412 */
  case UDM_HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE:      /* 413 */
  case UDM_HTTP_STATUS_REQUEST_URI_TOO_LONG:          /* 414 */  
  case UDM_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE:        /* 415 */
  case UDM_HTTP_STATUS_NOT_IMPLEMENTED:               /* 501 */
  case UDM_HTTP_STATUS_BAD_GATEWAY:                   /* 502 */
  case UDM_HTTP_STATUS_NOT_SUPPORTED:                 /* 505 */  
    /*
      Remove words and outgoing links from database.
      Keep "urlinfo" and "url" values for DBMode=blob searches,
      as well as for cached search results. Information from
      these two tables will be deleted later, after "HoldBadHref"
    */
    action= UDM_URL_ACTION_DUPDATE;
    break;
  
  case UDM_HTTP_STATUS_INTERNAL_SERVER_ERROR:         /* 500 */
  case UDM_HTTP_STATUS_SERVICE_UNAVAILABLE:           /* 503 */
  case UDM_HTTP_STATUS_GATEWAY_TIMEOUT:               /* 504 */
  
    /* Keep words in database                */
    /* We'll retry later, maybe host is down */
    UdmHostErrorIncrement(&Indexer->Conf->Hosts, Doc->connp.hostname);
    next_index_time=time(NULL)+Doc->Spider.net_error_delay_time;
    action= UDM_URL_ACTION_SUPDATE;
    break;
  
  default: /* Unknown status, retry later */
    UdmLog(Indexer,UDM_LOG_WARN,"HTTP %d We don't yet know how to handle it, skipped",status);
    action= UDM_URL_ACTION_SUPDATE;
    break;
  }

update:

  UdmTime_t2HttpStr(next_index_time,dbuf);
  UdmVarListReplaceStr(&Doc->Sections,"Next-Index-Time",dbuf);  
  rc= UdmURLActionNoLock(Indexer, Doc, action);
  return rc;
}


static int UdmDocUpdate(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc)
{
  size_t    maxsize;
  size_t    sec;
  int    flush=0;
  int    rc=UDM_OK;
  UDM_RESULT  *I = &Indexer->Indexed;

  UDM_LOCK_CHECK_OWNER(Indexer, UDM_LOCK_CONF);
  maxsize = UdmVarListFindInt(&Indexer->Conf->Vars,"DocMemCacheSize",0) * 1024 * 1024;

  if (maxsize > 0 && I->memused > 0) UdmLog(Indexer, UDM_LOG_EXTRA, "DocCacheSize: %d/%d", I->memused, maxsize);
  if (Doc)
  {
    I->memused += sizeof(UDM_DOCUMENT);
    /* Aproximation for Words memory usage  */
    I->memused += Doc->Words.nwords * (sizeof(UDM_WORD) + 5);
    /* Aproximation for CrossWords memory usage */
    I->memused += Doc->CrossWords.ncrosswords * (sizeof(UDM_CROSSWORD) + 35);
    /* Aproximation for Sections memory usage */
    for(sec = 0; sec < Doc->Sections.nvars; sec++) {
      I->memused += sizeof(UDM_VAR);
      I->memused += Doc->Sections.Var[sec].maxlen * 3 + 10;
    }
    I->memused += (sizeof(UDM_HREF) + 35) * Doc->Hrefs.nhrefs;
    if (I->memused >= maxsize) flush = 1;
    if (I->num_rows >= 1024) flush = 1;
  } else flush = 1;

  if (flush)
  {
    size_t  docnum;

    if (I->num_rows)
      UdmLog(Indexer, UDM_LOG_EXTRA, "Flush %d document(s)", I->num_rows + ((Doc != NULL) ? 1 : 0));
    
    if (Doc)
    {
      UDM_THREADINFO(Indexer, "Updating", UdmVarListFindStr(&Doc->Sections, "URL", ""));
      if(UDM_OK != (rc = DocUpdate(Indexer, Doc))) return rc;
      UdmDocFree(Doc);
    }
    
    for (docnum = 0; docnum < I->num_rows; docnum++)
    {
      /* Flush all hrefs from cache in-memory    */
      /* cache into database. Note, this must    */
      /* be done before call of  StoreCrossWords */
      /* because we need to know all new URL IDs */
      
      UDM_THREADINFO(Indexer, "Updating", UdmVarListFindStr(&I->Doc[docnum].Sections, "URL", ""));
      if(UDM_OK != (rc = DocUpdate(Indexer, &I->Doc[docnum])))
        return rc;
    }
    
    if (Indexer->Indexed.num_rows) UdmResultFree(&Indexer->Indexed);
  }
  else
  {
    /* Add document into cache */
    I->Doc=(UDM_DOCUMENT*)UdmRealloc(I->Doc, (I->num_rows + 1) * sizeof(UDM_DOCUMENT));
    I->Doc[I->num_rows] = Doc[0];
    I->Doc[I->num_rows].freeme = 0;
    I->num_rows++;
  }
  return rc;
}


size_t
UdmDBNumBySeed(UDM_ENV *Env, udmhash32_t seed)
{
  return seed * Env->dbl.nitems / 256;
}


/*
  There are a few ways to detect which database to work with:
  
- When passed through URL: http://hostname/search.cgi?cc=1&dbnum=2
- When passed from command line: indexer -Edumpdata -D1
- Autodetection from URL using hash

*/
int UdmURLActionNoLock(UDM_AGENT *A, UDM_DOCUMENT *D, int cmd)
{
  int res=UDM_ERROR, execflag = 0;
  size_t i, dbfrom = 0, dbto;
  UDM_DB  *db;
  int dbnum = (cmd == UDM_URL_ACTION_GET_CACHED_COPY ? UdmVarListFindInt(&D->Sections, "dbnum", 0) : -1);

  UDM_LOCK_CHECK_OWNER(A, UDM_LOCK_CONF);

  if(cmd == UDM_URL_ACTION_FLUSH)
    return UdmDocUpdate(A, D);

  /* indexer -Edumpdata -Dxxx */
  if (cmd == UDM_URL_ACTION_DUMPDATA)
    dbnum= UdmVarListFindInt(&A->Conf->Vars, "DBLimit", 0) - 1;

#ifdef USE_TRACE
  fprintf(A->TR, "[%d] URLAction: %d\n", A->handle, cmd);
#endif
  
  dbto =  A->Conf->dbl.nitems;
  if (dbnum < 0 && D != NULL)
  {
    udmhash32_t url_id=UdmVarListFindInt(&D->Sections,"URL_ID", 0);
    udmhash32_t seed= (((url_id) ? url_id :
                       UdmStrHash32(UdmVarListFindStr(&D->Sections, "URL", ""))) & 0xFF);
    dbfrom= dbto= UdmDBNumBySeed(A->Conf, seed);
    dbto++;
  }

  for (i = dbfrom; i < dbto; i++)
  {
    if (dbnum >= 0 && dbnum != (int) i) continue;
    db = &A->Conf->dbl.db[i];

    UDM_GETLOCK(A, UDM_LOCK_DB);
    switch(db->DBDriver)
    {
      case UDM_DB_SEARCHD:
        res = UdmSearchdURLAction(A, D, cmd, db);
        execflag = 1;
        break;
      
#ifdef HAVE_SQL
      default:
        res=UdmURLActionSQL(A,D,cmd,db);
        if (cmd == UDM_URL_ACTION_EXPIRE)
        {
          UDM_FREE(db->where);  /* clear db->where for next parameters */
          UDM_FREE(db->from);
        }
        execflag = 1;
        break;
#endif
    }
    
    if (res != UDM_OK && execflag)
    {
      UdmLog (A, UDM_LOG_ERROR, "%s", db->errstr);
    }
    UDM_RELEASELOCK(A, UDM_LOCK_DB);
    if (res != UDM_OK) break;
  }
  
  if ((res != UDM_OK) && !execflag)
  {
    UdmLog(A, UDM_LOG_ERROR, "no supported DBAddr specified");
  }
  return res;
}


/*
  URL action with automatic UDM_LOCK_CONF locking
*/
__C_LINK int __UDMCALL UdmURLAction(UDM_AGENT *A, UDM_DOCUMENT *D, int cmd)
{
  int rc;
  UDM_GETLOCK(A, UDM_LOCK_CONF);
  rc= UdmURLActionNoLock(A, D, cmd);
  UDM_RELEASELOCK(A, UDM_LOCK_CONF);
  return rc;
}



__C_LINK int __UDMCALL UdmTargets(UDM_AGENT *A)
{
  int  res=UDM_ERROR;
  size_t i, dbfrom = 0, dbto;

  UDM_LOCK_CHECK_OWNER(A, UDM_LOCK_CONF);
  dbto =  A->Conf->dbl.nitems;
  UdmResultFree(&A->Conf->Targets);

  for (i= dbfrom; i < dbto; i++)
  {
    UDM_DB *db= &A->Conf->dbl.db[i];
    
    if (!UdmDBIsActive(A, i))
      continue;
    UDM_GETLOCK(A, UDM_LOCK_DB);
#ifdef HAVE_SQL
    res= UdmTargetsSQL(A, db);
#endif
    if(res != UDM_OK)
    {
      UdmLog(A, UDM_LOG_ERROR, "%s", db->errstr);
    }
    UDM_RELEASELOCK(A, UDM_LOCK_DB);
    if (res != UDM_OK) break;
  }
  return res;
}

__C_LINK int __UDMCALL UdmResAction(UDM_AGENT *A, UDM_RESULT *R, int cmd)
{
  int  rc= UDM_ERROR;
  UDM_DB  *db;
  size_t i, dbfrom = 0, dbto;
  
  dbto =  A->Conf->dbl.nitems;

  for (i = dbfrom; i < dbto; i++)
  {
    db = &A->Conf->dbl.db[i];
    UDM_GETLOCK(A, UDM_LOCK_DB);
    switch(db->DBDriver)
    {
      case UDM_DB_SEARCHD:
        rc= UDM_OK;
        break;

#ifdef HAVE_SQL
      default:
        rc= UdmResActionSQL(A, R, cmd, db, i);
#endif
    }
    if(rc != UDM_OK)
    {
      UdmLog(A, UDM_LOG_ERROR, "%s", db->errstr);
    }
    UDM_RELEASELOCK(A, UDM_LOCK_DB);
    if (rc != UDM_OK) break;
  }
  return rc;
}


__C_LINK int __UDMCALL UdmCatAction(UDM_AGENT *A, UDM_CATEGORY *C, int cmd)
{
  UDM_DB  *db;
  int  res=UDM_ERROR;
  size_t i, dbfrom = 0, dbto;

  UDM_GETLOCK(A, UDM_LOCK_CONF);
  dbto =  A->Conf->dbl.nitems;
  UDM_RELEASELOCK(A, UDM_LOCK_CONF);

  for (i = dbfrom; i < dbto; i++)
  {
    db = &A->Conf->dbl.db[i];
    UDM_GETLOCK(A, UDM_LOCK_DB);
    switch(db->DBDriver)
    {
      case UDM_DB_SEARCHD:
        res = UdmSearchdCatAction(A, C, cmd, db);
        break;
#ifdef HAVE_SQL
      default:
        res=UdmCatActionSQL(A,C,cmd,db);
#endif
    }
    if(res != UDM_OK)
    {
      UdmLog(A, UDM_LOG_ERROR, "%s", db->errstr);
    }
    UDM_RELEASELOCK(A, UDM_LOCK_DB);
    if (res != UDM_OK) break;
  }
  return res;
}

__C_LINK int __UDMCALL UdmSrvAction(UDM_AGENT *A, UDM_SERVERLIST *S, int cmd)
{
  UDM_DB  *db;
  int  res=UDM_ERROR;
  size_t i, dbfrom = 0, dbto;
  
  UDM_GETLOCK(A, UDM_LOCK_CONF);
  dbto =  A->Conf->dbl.nitems;

  for (i = dbfrom; i < dbto; i++)
  {
    db = &A->Conf->dbl.db[i];

    UDM_GETLOCK(A, UDM_LOCK_DB); 
#ifdef HAVE_SQL
    res = UdmSrvActionSQL(A, S, cmd, db);
#endif
    if(res != UDM_OK){
    UdmLog(A, UDM_LOG_ERROR, "%s", db->errstr);
    }
    UDM_RELEASELOCK(A, UDM_LOCK_DB);
    if (res != UDM_OK) break;
  }
  UDM_RELEASELOCK(A, UDM_LOCK_CONF);
  return res;
}

static const int search_cache_size=1000;

/* Allocate an array for documents information */
static int
UdmResAllocDoc(UDM_RESULT *Res)
{
  size_t i;
  if (!Res->Doc && Res->num_rows > 0)
  {
    Res->Doc = (UDM_DOCUMENT*)UdmMalloc(sizeof(UDM_DOCUMENT) * (Res->num_rows));
    for (i= 0; i < Res->num_rows; i++)
      UdmDocInit(&Res->Doc[i]);
  }
  return UDM_OK;
}



static int
UdmFindWordsDB(UDM_AGENT *A, UDM_RESULT *CurRes, UDM_DB *db,
               size_t num_best_rows)
{
  int rc= UDM_OK;
  const char *dbaddr= UdmVarListFindStr(&db->Vars,"DBAddr","<noaddr>");
  udm_timer_t ticks= UdmStartTimer();
  
  UdmLog(A, UDM_LOG_DEBUG, "Start FindWordsDB for %s", dbaddr);
  
  switch(db->DBDriver)
    {
    case UDM_DB_SEARCHD:
      rc= UdmFindWordsSearchd(A, CurRes, db);
      break;
#ifdef HAVE_SQL
    default:
      if (UDM_OK != (rc= UdmQueryCacheGetSQL(A, CurRes, db)))
        return rc;
      if (!CurRes->URLData.nitems)
      {
        rc= UdmFindWordsSQL(A, CurRes, db, num_best_rows);
        if (rc == UDM_OK && CurRes->URLData.nitems)
          rc= UdmQueryCachePutSQL(A, CurRes, db);
      }
      break;
#endif
  }
  UdmLog(A, UDM_LOG_DEBUG, "%-30s%.2f", "Stop  FindWordsDB:", UdmStopTimer(&ticks));
  return rc;
}

typedef struct search_param_st
{
  UDM_AGENT *Agent;
  UDM_RESULT *Res;
  UDM_DB *db;
  int rc;
  void *thd;
  size_t num_best_rows;
} UDM_SPRM;


static
void *SearchThread(void *arg)
{
  UDM_SPRM *sprm= (UDM_SPRM*) arg;
  sprm->rc= UdmFindWordsDB(sprm->Agent, sprm->Res, sprm->db, sprm->num_best_rows);
  return NULL;
}


static int
UdmResultSetMachineNumber(UDM_URLDATALIST *List, size_t num)
{
  size_t i;
  UDM_URLDATA *Item= List->Item;

  for(i= 0; i < List->nitems; i++)
  {
    /* 
      We use (256-i) to sort a document from the first database
      before the same document from the second database.
    */
    Item[i].score = (Item[i].score << 8) + (255 - (num & 255));
   }
  return UDM_OK;
}


static int
UdmResultJoin(UDM_AGENT *A,
              UDM_RESULT *TmpRes, size_t nresults,
              UDM_RESULT *Res)
{
  size_t i, max_total_found= 0;
  udm_timer_t ticks;
  
  for (Res->URLData.nitems= 0,
       Res->num_rows= 0,
       Res->total_found= 0, i= 0; i < nresults; i++)
  {
    size_t j;
    if (max_total_found < TmpRes[i].total_found)
      max_total_found= TmpRes[i].total_found;
    Res->total_found+= TmpRes[i].total_found;
    Res->num_rows+= TmpRes[i].num_rows;
    Res->URLData.nitems+= TmpRes[i].URLData.nitems;
    for (j= 0; j < TmpRes[i].WWList.nwords; j++)
      UdmWideWordListAddForStat(&Res->WWList, &TmpRes[i].WWList.Word[j]);
  }

  ticks= UdmStartTimer();
  UdmLog(A, UDM_LOG_DEBUG, "Start joining results from %d dbs", nresults);

  if (Res->URLData.nitems > 0)
  {
    size_t offs;

    Res->URLData.Item= (UDM_URLDATA*)UdmMalloc(sizeof(UDM_URLDATA)
                                               * Res->URLData.nitems);
    for (offs= 0, i= 0; i < nresults; offs+= TmpRes[i].URLData.nitems, i++)
    {
      UDM_RESULT  *CurRes= &TmpRes[i];
      UDM_URLDATA *CurData= TmpRes[i].URLData.Item;
      size_t ncoords= CurRes->URLData.nitems;
      if(CurData)
      {
        UdmResultSetMachineNumber(&CurRes->URLData, i);
        memcpy(Res->URLData.Item + offs, CurData,
               sizeof(CurData[0]) * ncoords);
      }
      UDM_FREE(CurRes->URLData.Item);
    }
  }

  if (nresults > 1 && Res->URLData.nitems > 0)
  {
    int group_by_site= UdmVarListFindBool(&A->Conf->Vars, "GroupBySite", 0) &&
                       (UdmVarListFindInt(&A->Conf->Vars, "site", 0) == 0);
    int group_by_site_rank=  !strcmp(UdmVarListFindStr(&A->Conf->Vars, "GroupBySite", "no"), "rank");
    if (group_by_site)
    {
      UdmURLDataSortBySite(&Res->URLData);
      UdmURLDataGroupBySite(&Res->URLData);
      /*
        TODO: add better approximation of total results
        found with cluster. This number will be between
        max_total_found and Res->total_found, and will
        depend on overlap between the sites:
        (Res->URLData.nitems / Res->total_found)
      */
      Res->total_found= Res->URLData.nitems;
    }
    else if (group_by_site_rank)
    {
      UdmURLDataSortBySite(&Res->URLData);
      UdmURLDataApplySiteRank(A, &Res->URLData, 1);
    }
    if (Res->URLData.nitems)
      UdmURLDataSortByPattern(&Res->URLData,
                              UdmVarListFindStr(&A->Conf->Vars, "s", "R"));
  }

  UdmLog(A, UDM_LOG_DEBUG, "Stop  joining results:\t%.2f", UdmStopTimer(&ticks));

  return UDM_OK;
}


static int
UdmFindWordsMulDB(UDM_AGENT *A, UDM_RESULT *TmpRes,
                  UDM_RESULT *Res, size_t num_best_rows)
{
  size_t i, ndatabases= A->Conf->dbl.nitems;
  UDM_SPRM search_param[256];
  int use_threads= A->Conf->ThreadCreate != NULL && ndatabases > 1;
  int rc= UDM_OK;
  
  /* Check if all drivers are thread safe */
  for (i= 0; i < ndatabases; i++)
  {
    UDM_DB *db = &A->Conf->dbl.db[i];
    /* TODO: link against libmysqlclient_r */
    if (db->DBDriver != UDM_DB_SEARCHD)
    {
      use_threads= 0;
      break;
    }
  }
    
  for (i= 0; i < ndatabases; i++)
  {
    UDM_RESULT *CurRes= &TmpRes[i];
    UDM_DB *db = &A->Conf->dbl.db[i];
    CurRes[0]= Res[0];    /* Copy bool items */
    UdmWideWordListCopy(&CurRes->WWList, &Res->WWList);

    if (use_threads)
    {
      void *thd;
      UdmLog(A, UDM_LOG_DEBUG, "Starting thread[%d]", i + 1);
      search_param[i].Agent= UdmAgentInit(NULL, A->Conf, i + 1);
      search_param[i].Res= CurRes;
      search_param[i].db= db;
      search_param[i].num_best_rows= num_best_rows;
      A->Conf->ThreadCreate(&thd, SearchThread, (void*) &search_param[i]);
      search_param[i].thd= thd;
    }
    else
    {
      if (UDM_OK != (rc= UdmFindWordsDB(A, CurRes, db, num_best_rows)))
        return rc;
    }
  }
    
  if (use_threads)
  {
    udm_timer_t ticks= UdmStartTimer();
    UdmLog(A, UDM_LOG_DEBUG, "Creating threads");
    for (i= 0; i < ndatabases; i++)
    {
      void *thd= search_param[i].thd;
      UdmLog(A, UDM_LOG_DEBUG, "Joining thread[%d]", i + 1);
      A->Conf->ThreadJoin(thd);
    }
    UdmLog(A, UDM_LOG_DEBUG, "Threads finished: %.2f", UdmStopTimer(&ticks));
  }
  
  if (use_threads)
  {
    for (i= 0; i < ndatabases; i++)
      UdmAgentFree(search_param[i].Agent);
  }

  UdmResultJoin(A, TmpRes, ndatabases, Res);
  
  return rc;
}


static int
UdmFindWords(UDM_AGENT *A, UDM_RESULT *TmpRes,
             UDM_RESULT *Res, size_t num_best_rows)
{
  const char  *cache_mode= UdmVarListFindStr(&A->Conf->Vars, "Cache", "no");
  int res=    UDM_OK;

  if( strcasecmp(cache_mode, "yes") || UdmSearchCacheFind(A, Res))
  {
    if (A->Conf->dbl.nitems > 1)
    {
      if (UDM_OK  != (res= UdmFindWordsMulDB(A, TmpRes, Res, num_best_rows)))
        return res;
    }
    else
    {
      UDM_DB *db = &A->Conf->dbl.db[0];
      if (UDM_OK != (res= UdmFindWordsDB(A, Res, db, num_best_rows)))
        return res;
      UdmResultSetMachineNumber(&Res->URLData, 0);
    }

    if((!strcasecmp(cache_mode,"yes"))&&(search_cache_size>-1))
    {
      fflush(stdout);
      fflush(stderr);
      UdmSearchCacheStore(A, Res);
    }
  }
  return res;
}


static int CreateAlias(UDM_ENV *Conf, UDM_DOCUMENT *Doc)
{
  char *alcopy, *aliastr;
  UDM_MATCH    *Alias;
  UDM_MATCH_PART Parts[10];

  /* Create "Alias" variable */
  alcopy=UdmRemoveHiLightDup(UdmVarListFindStr(&Doc->Sections,"URL",""));
  if ((Alias=UdmMatchListFind(&Conf->Aliases,alcopy,10,Parts)))
  {
    size_t nbytes= strlen(Alias->arg) + strlen(alcopy) + 64;
    aliastr= (char*)UdmMalloc(nbytes);
    UdmMatchApply(aliastr, nbytes, alcopy, Alias->arg, Alias, 10, Parts);
  }
  else
  {
    aliastr = (char*)UdmStrdup(alcopy);
  }
  UdmVarListReplaceStr(&Doc->Sections,"Alias", aliastr);
  UdmFree(aliastr);
  UdmFree(alcopy);
  return UDM_OK;
}


static int
UdmAddCategoryInfo(UDM_AGENT *Agent)
{
  UDM_CATEGORY C;
  UDM_ENV *Env= Agent->Conf;
  size_t catcolumns= (size_t)atoi(UdmVarListFindStr(&Env->Vars,"CatColumns",""));

  if (!catcolumns)
    return UDM_OK;
  
  bzero((void*)&C, sizeof(C));
  strcpy(C.addr,UdmVarListFindStr(&Env->Vars,"cat",""));

  if(UDM_OK == UdmCatAction(Agent, &C, UDM_CAT_ACTION_LIST))
  {
    size_t n=1, l= 0, i;
    char *catlist;
    
    for(i= 0; i < C.ncategories; i++)
      l+= 128 + strlen(C.Category[i].path) + strlen(C.Category[i].name);
      
    if (l > 0  && (catlist= (char*)UdmMalloc(l)))
    {
      sprintf(catlist, "<table>\n");
      for(i = 0; i < C.ncategories; i++)
      {
        sprintf(catlist+strlen(catlist),
                "%s<td><a href=\"?cat=%s\">%s</A></td><td width=60>&nbsp;</td>\n",
                n==1 ? "<tr>\n" : "",
                C.Category[i].path, C.Category[i].name);
      
        if(n == catcolumns)
        {
          sprintf(catlist+strlen(catlist),"</tr>\n");
          n=1;
        }
        else
        {
          n++;
        }
      }
      sprintf(catlist + strlen(catlist), "</table>\n");
      UdmVarListAddStr(&Env->Vars, "CS", catlist);
      UDM_FREE(catlist);
    }
  }
  else
  {
    return UDM_ERROR;
  }
    
  UDM_FREE(C.Category);
  bzero((void*)&C, sizeof(C));
  strcpy(C.addr,UdmVarListFindStr(&Env->Vars,"cat",""));
  
  if(UDM_OK == UdmCatAction(Agent, &C, UDM_CAT_ACTION_PATH))
  {
    char *catpath = NULL;
    size_t i, l= 0;
    
    for(i= 0; i < C.ncategories; i++)
      l+= 32 + strlen(C.Category[i].path) + strlen(C.Category[i].name);
    
    if (l > 0 && (catpath= (char*)UdmMalloc(l)))
    {
      catpath[0]= '\0';
      for(i = 0; i < C.ncategories; i++)
      {
        sprintf(catpath+strlen(catpath),"/<a href=\"?cat=%s\">%s</A>",
                (C.Category[i].path) ? C.Category[i].path : "",
                (C.Category[i].name) ? C.Category[i].name : "");
      }
      UdmVarListAddStr(&Env->Vars, "CP", catpath);
      UDM_FREE(catpath);
    }
  }
  else
  {
    return UDM_ERROR;
  }
  
  UDM_FREE(C.Category);
  return UDM_OK;
}


static int
UdmResultAddCachedCopyLinks(UDM_AGENT *Agent,
                            UDM_RESULT *Res)
{
  UDM_ENV *Env= Agent->Conf;
  char *searchwords= NULL, *storedstr= NULL; 
  size_t i, swlen, storedlen, catcolumns= 0;
  
  for (i= swlen= 0; i < Res->WWList.nwords; i++)
    swlen+= (8 * Res->WWList.Word[i].len) + 2;
  
  if ((searchwords= UdmXmalloc(swlen)) != NULL)
  {
    int z;
    for (z= i= 0; i < Res->WWList.nwords; i++)
    {
      if (Res->WWList.Word[i].count > 0)
      {
        sprintf(UDM_STREND(searchwords), (z)?"+%s":"%s", Res->WWList.Word[i].word);
        z++;
      }
    }
  }
  storedstr= UdmRealloc(storedstr, storedlen= 1024 + 10 * swlen);
  
  for(i= 0; i < Res->num_rows; i++)
  {
    UDM_DOCUMENT *Doc=&Res->Doc[i];
    UDM_CATEGORY C;
    const char   *stored_href;
    int dbnum= UdmVarListFindInt(&Doc->Sections, "dbnum", 0);
    
    bzero((void*)&C, sizeof(C));
    strcpy(C.addr,UdmVarListFindStr(&Doc->Sections,"Category",""));
    if(catcolumns && !UdmCatAction(Agent, &C, UDM_CAT_ACTION_PATH))
    {
      char *catpath;
      size_t c, l = 0;
      
      for(c = 0; c < C.ncategories; c++)
        l+= 32 + strlen(C.Category[c].path) + strlen(C.Category[c].name);
      
      if (l > 0 && (catpath= (char*)UdmMalloc(l)))
      {
        *catpath = '\0';
        for(c = 0; c < C.ncategories; c++)
          sprintf(catpath+strlen(catpath)," &gt; <A HREF=\"?cat=%s\">%s</A> ",
                  C.Category[c].path,C.Category[c].name);
        UdmVarListReplaceStr(&Env->Vars,"DY",catpath);
        UDM_FREE(catpath);
      }
    }
    UDM_FREE(C.Category);
    
    if ((!(stored_href= UdmVarListFindStr(&Doc->Sections, "stored_href", NULL)) ||
         !stored_href[0]) &&
         UdmVarListFindStr(&Doc->Sections, "CachedCopy", NULL))
    {
      const char *u= UdmVarListFindStr(&Doc->Sections, "URL", "");
      char *eu= (char*)UdmMalloc(strlen(u)*10 + 30);
      UdmEscapeURL(eu, u);
      if (dbnum)
      {
        udm_snprintf(storedstr, storedlen, "?cc=1&amp;dbnum=%d&amp;URL=%s&amp;q=%s&amp;wm=%s",
                     dbnum, eu, searchwords,
                     UdmVarListFindStr(&Env->Vars, "wm", "wrd"));
      }
      else
      {
        udm_snprintf(storedstr, storedlen, "?cc=1&amp;URL=%s&amp;q=%s&amp;wm=%s",
                     eu, searchwords,
                     UdmVarListFindStr(&Env->Vars, "wm", "wrd"));
      }
      UdmFree(eu);
      UdmVarListReplaceStr(&Doc->Sections, "stored_href",  storedstr);
    }
    else
    {
      /* add dbnum to stored_href returned from searchd */
      if (stored_href && dbnum)
      {
        char tmp[256];
        udm_snprintf(tmp, sizeof(tmp), "?dbnum=%d&%s", dbnum, stored_href + 1);
        UdmVarListReplaceStr(&Doc->Sections, "stored_href", tmp);
      }
    }
  }
  UDM_FREE(searchwords);
  UDM_FREE(storedstr);
  
  return UDM_OK;
}




UDM_RESULT * __UDMCALL UdmFind2(UDM_AGENT *Agent, const char *query_string)
{
  UdmParseQueryString(Agent, &Agent->Conf->Vars, query_string);
  return UdmFind(Agent);
}


UDM_RESULT * __UDMCALL UdmFind(UDM_AGENT *A)
{
  UDM_RESULT  *Res, *TmpRes;
  int    res=UDM_OK;
  udm_timer_t ticks= UdmStartTimer(), ticks_;
  size_t  i, nbytes, numdatabases=  A->Conf->dbl.nitems;
  size_t  page_number= (size_t) UdmVarListFindInt(&A->Conf->Vars, "np", 0);
  size_t  page_size=   (size_t) UdmVarListFindInt(&A->Conf->Vars, "ps", 10);
  size_t  offs=        (size_t) UdmVarListFindInt(&A->Conf->Vars, "offs", 0);
  size_t  ExcerptSize= (size_t)UdmVarListFindInt(&A->Conf->Vars, "ExcerptSize", 256);
  size_t  ExcerptPadding = (size_t)UdmVarListFindInt(&A->Conf->Vars, "ExcerptPadding", 40);
  int     UseLocalCachedCopy= UdmVarListFindBool(&A->Conf->Vars, "UseLocalCachedCopy", 0);
  size_t  ResultsLimit= 1000;
  UDM_VAR *ResultsLimitVar= UdmVarListFind(&A->Conf->Vars, "ResultsLimit");
  char    str[128];

  new_version= UdmVarListFindBool(&A->Conf->Vars, "NewVersion",
               UdmVarListFindBool(&A->Conf->Vars, "ENV.NEWVERSION", 0));

  /*
    Make sure we use only search.htm command,
    do not allow to overwrite in query string.
    FIXME: Using template command and query string parameter
    at the same time makes limit equal to 1000.
    This should be fixed to use the template value instead.
    This needed when the template value is smaller than 1000.
  */
  if (ResultsLimitVar && (ResultsLimitVar->section == 0))
  {
    ResultsLimit= UdmVarIntVal(ResultsLimitVar);
  }

  UdmLog(A, UDM_LOG_EXTRA, "Start UdmFind");
  
  ticks_= UdmStartTimer();
  UdmLog(A, UDM_LOG_DEBUG, "Start Prepare");  
  nbytes= sizeof(UDM_RESULT) * numdatabases;
  TmpRes= (UDM_RESULT*) UdmMalloc(nbytes);
  bzero((void*) TmpRes, sizeof(UDM_RESULT) * numdatabases);
  Res=UdmResultInit(NULL);
  if (UDM_OK != (res= UdmPrepare(A, Res)) ||
      UDM_OK != UdmAddCategoryInfo(A))
    goto conv;
  UdmVarListAddStr(&A->Conf->Vars, "orig_m", UdmVarListFindStr(&A->Conf->Vars, "m", "all"));
  UdmLog(A, UDM_LOG_DEBUG, "%-30s%.2f", "Stop  Prepare", UdmStopTimer(&ticks_));

  Res->first = offs ? offs : page_number * page_size;  
  if (ResultsLimit && (Res->first + page_size > ResultsLimit))
  {
    UdmLog(A, UDM_LOG_DEBUG, "Too large ps or np: offset=%d limit=%d\n",
           Res->first + page_size, ResultsLimit);
    goto conv;
  }

  ticks_= UdmStartTimer();
  UdmLog(A, UDM_LOG_DEBUG, "Start FindWords");  
  if (!Res->WWList.nwords ||
      UDM_OK != (res= UdmFindWords(A, TmpRes, Res, Res->first + page_size)))
    goto conv;
  UdmLog(A, UDM_LOG_DEBUG, "%-30s%.2f", "Stop  FindWords", UdmStopTimer(&ticks_));
  
  if (!Res->total_found)
  {
    int suggest= UdmVarListFindBool(&A->Conf->Vars, "Suggest", 0);
    if (suggest)
    {
      if(UDM_OK != (res= UdmResAction(A, Res, UDM_RES_ACTION_SUGGEST)))
        goto conv;
    }
  }
  
  UdmVarListReplaceStr(&A->Conf->Vars, "m", UdmVarListFindStr(&A->Conf->Vars, "orig_m", "all"));
  UdmVarListDel(&A->Conf->Vars, "orig_m");
  UdmVarListReplaceInt(&A->Conf->Vars, "CurrentTimestamp", (int) time(0));


  if(Res->first >= Res->URLData.nitems)
  {
    Res->last= Res->first;
    Res->num_rows= 0;
    goto conv; /* jump to converting variables into BrowserCharset */
  }
  if(Res->first + page_size > Res->URLData.nitems)
  {
    Res->num_rows= Res->URLData.nitems - Res->first;
  }
  else
  {
    Res->num_rows= page_size;
  }
  Res->last= Res->first + Res->num_rows - 1;

  ticks_= UdmStartTimer();
  UdmLog(A, UDM_LOG_DEBUG, "Start AddDocInfo");

  UdmResAllocDoc(Res);
  
  for (i= 0; i < numdatabases; i++)
  {
    UDM_DB *db= &A->Conf->dbl.db[i];
    switch(db->DBDriver){
    case UDM_DB_SEARCHD:
      res = UdmResAddDocInfoSearchd(A, TmpRes, db, Res, i);
      break;
#ifdef HAVE_SQL
    default:
      res = UdmResAddDocInfoSQL(A, db, Res, i);
      break;
#endif
    }
  }

  /* Copy url_id and coord to result */
  for(i=0;i<Res->num_rows;i++)
  {
    UDM_URLDATA *Data= &Res->URLData.Item[i + Res->first];
    uint4 score= Data->score;
    UDM_VARLIST *Sections= &Res->Doc[i].Sections;
    UdmVarListReplaceUnsigned(Sections, "PerSite", Data->per_site);
    udm_snprintf(str, 128, "%.3f%%", ((double)(score >> 8)) / 1000);
    UdmVarListReplaceStr(Sections, "Score", str);
    UdmVarListReplaceInt(Sections,"Order",(int)(i + Res->first + 1));
    UdmVarListReplaceInt(Sections, "dbnum", UDM_COORD2DBNUM(score));
    /* Add a fake cached copy to have search.cgi display stored_href */
    if (UseLocalCachedCopy && !UdmVarListFind(Sections, "CachedCopy"))
      UdmVarListAddStr(Sections, "CachedCopy", "Fake-CachedCopy");
  }
  
  
  for (i= 0; i < Res->num_rows; i++)
    CreateAlias(A->Conf, &Res->Doc[i]);
  
  UdmLog(A, UDM_LOG_DEBUG, "%-30s%.2f", "Stop  AddDocInfo", UdmStopTimer(&ticks_));
  
  if (UdmVarListFindInt(&A->Conf->Vars, "DetectClones", 0))
  {
    size_t num=Res->num_rows;
    ticks_=UdmStartTimer();
    UdmLog(A, UDM_LOG_DEBUG, "Start Clones");
  
    for(i=0;i<num;i++)
    {
      UDM_RESULT *Cl = UdmCloneList(A, &Res->Doc[i]);
      if (Cl)
      {
        size_t c;
        UdmVarListReplaceInt(&Res->Doc[i].Sections, "nclones", Cl->num_rows);
        for (c= 0; c < Cl->num_rows; c++)
        {
          char name[32];
          sprintf(name, "Clone%d", c);
          UdmVarListReplaceLst(&Res->Doc[i].Sections,
                               &Cl->Doc[c].Sections, name, "*");
        }
        UdmResultFree(Cl);
      }
    }
    UdmLog(A, UDM_LOG_DEBUG, "%-30s%.2f", "Stop  Clones:", UdmStopTimer(&ticks_));
  }
  
  /* first and last begins from 0, make it begin from 1 */
  Res->first++;
  Res->last++;
  
conv:

#ifndef NO_ADVANCE_CONV
  ticks_=UdmStartTimer();
  UdmLog(A, UDM_LOG_DEBUG, "Start UdmConvert");
  UdmConvert(A->Conf, Res, A->Conf->lcs, A->Conf->bcs);
  UdmLog(A, UDM_LOG_DEBUG, "%-30s%.2f", "Stop  UdmConvert:", UdmStopTimer(&ticks_));
#endif

  ticks_=UdmStartTimer();
  UdmLog(A, UDM_LOG_DEBUG, "Start Excerpts");
  for (i= 0; i < Res->num_rows; i++)
  {
    char *Excerpt;
    if ((Excerpt= UdmExcerptDoc(A, Res, &Res->Doc[i], ExcerptSize, ExcerptPadding)))
    {
      UdmVarListReplaceStr(&Res->Doc[i].Sections, "body", Excerpt);
      UdmFree(Excerpt);
    }
  }
  UdmLog(A, UDM_LOG_DEBUG, "%-30s%.2f", "Stop  Excerpts:", UdmStopTimer(&ticks_));

  UdmLog(A, UDM_LOG_DEBUG, "Start WordInfo");
  ticks_=UdmStartTimer();
  UdmResWordInfo(A->Conf, Res);
  UdmResultAddCachedCopyLinks(A, Res);
  UdmLog(A, UDM_LOG_DEBUG, "%-30s%.2f", "Stop  WordInfo:", UdmStopTimer(&ticks_));

  Res->work_time= ticks= UdmStartTimer() - ticks;

  UdmTrack(A, Res);
  UdmLog(A, UDM_LOG_EXTRA, "%-30s%.2f", "Stop  UdmFind:", (float)ticks/1000);

  UdmFree(TmpRes);
  if(res!=UDM_OK)
  {
    UdmResultFree(Res);
    Res=NULL;
  }
  return Res;
}


static int UdmStr2DBMode(const char * str1)
{
  int m = -1;
  if(!strncasecmp(str1,"single",6))m=UDM_DBMODE_SINGLE;
  else if(!strncasecmp(str1,"multi",5))m=UDM_DBMODE_MULTI;
  else if(!strncasecmp(str1,"blob",4))m=UDM_DBMODE_BLOB;
  else if(!strncasecmp(str1,"rawblob",7))m=UDM_DBMODE_RAWBLOB;
  return(m);
}

__C_LINK const char* __UDMCALL UdmDBTypeToStr(int dbtype)
{
  switch(dbtype)
  {
    case UDM_DB_MYSQL:   return "mysql";
    case UDM_DB_PGSQL:   return "pgsql";
    case UDM_DB_IBASE:   return "ibase";
    case UDM_DB_MSSQL:   return "mssql";
    case UDM_DB_ORACLE8: return "oracle";
    case UDM_DB_SQLITE:  return "sqlite";
    case UDM_DB_SQLITE3: return "sqlite";
    case UDM_DB_MIMER:   return "mimer";
    case UDM_DB_VIRT:    return "virtuoso";
    case UDM_DB_ACCESS:  return "access";
    case UDM_DB_DB2:     return "db2";
    case UDM_DB_CACHE:   return "cache";
    case UDM_DB_SYBASE:  return "sybase";
    case UDM_DB_MONETDB: return "monetdb";
  }
  return "unknown_dbtype";
}


__C_LINK const char* __UDMCALL UdmDBModeToStr(int dbmode)
{
  switch(dbmode) 
  {
    case UDM_DBMODE_SINGLE:  return "single";
    case UDM_DBMODE_MULTI:   return "multi";
    case UDM_DBMODE_BLOB:    return "blob";
    case UDM_DBMODE_RAWBLOB: return "rawblob";
  }
  return "unknown_dbmode";
}


__C_LINK const char* __UDMCALL UdmIndCmdStr(enum udm_indcmd cmd)
{
  switch(cmd)
  {
    case UDM_IND_CREATE: return "create";
    case UDM_IND_DROP:   return "drop";
    default: return "";
  }
  return "unknown_cmd";
}


static int UdmDBSetParam(UDM_DB *db, char *param)
{
  char *tok, *lt;
  
  for(tok = udm_strtok_r(param, "&",&lt) ; tok ; 
      tok = udm_strtok_r(NULL,"&",&lt))
  {
    char * val;
    if((val=strchr(tok,'=')))
    {
      *val++='\0';
      UdmVarListReplaceStr(&db->Vars, tok, val);
    }
    else
    {
      UdmVarListReplaceStr(&db->Vars, tok, "");
    }
  }
  return UDM_OK;
}


typedef struct udm_sqldb_driver_st
{
  const char *name;
  int DBType;
  int DBDriver;
  int DBSQL_IN;
  int flags;
  UDM_SQLDB_HANDLER *handler;
} UDM_SQLDB_DRIVER;


static UDM_SQLDB_DRIVER SQLDriver[]=
{
#if (HAVE_ORACLE8)
  {
    "oracle8", UDM_DB_ORACLE8, UDM_DB_ORACLE8, 1, 
    UDM_SQL_HAVE_GROUPBY   | UDM_SQL_HAVE_TRUNCATE |
    UDM_SQL_HAVE_SUBSELECT | UDM_SQL_HAVE_BIND |
    UDM_SQL_HAVE_ROWNUM    | UDM_SQL_HAVE_GOOD_COMMIT | UDM_SQL_HAVE_TRANSACT |
    UDM_SQL_HAVE_RENAME    | UDM_SQL_HAVE_CREATE_LIKE,
    &udm_sqldb_oracle_handler
  },
  {
    "oracle", UDM_DB_ORACLE8, UDM_DB_ORACLE8, 1, 
    UDM_SQL_HAVE_GROUPBY   | UDM_SQL_HAVE_TRUNCATE |
    UDM_SQL_HAVE_SUBSELECT | UDM_SQL_HAVE_BIND |
    UDM_SQL_HAVE_ROWNUM    | UDM_SQL_HAVE_GOOD_COMMIT | UDM_SQL_HAVE_TRANSACT |
    UDM_SQL_HAVE_RENAME    | UDM_SQL_HAVE_CREATE_LIKE,
    &udm_sqldb_oracle_handler
  },
#endif
#if (HAVE_CTLIB)
  {
    "mssql", UDM_DB_MSSQL, UDM_DB_MSSQL, 1,
    UDM_SQL_HAVE_GROUPBY | UDM_SQL_HAVE_TRUNCATE |
    UDM_SQL_HAVE_TOP     | UDM_SQL_HAVE_0xHEX | UDM_SQL_HAVE_TRANSACT |
    UDM_SQL_HAVE_RENAME  | UDM_SQL_HAVE_CREATE_LIKE,
    &udm_sqldb_ctlib_handler
  },
  {
    "sybase", UDM_DB_SYBASE, UDM_DB_MSSQL, 1,
    UDM_SQL_HAVE_GROUPBY | /*UDM_SQL_HAVE_TRUNCATE |*/
    /*
      Don't use TRUNCATE with Sybase.
      It gives error:
      'TRUNCATE TABLE command not allowed within multi-statement
      transaction.
      TODO: modify the code to use TRUNCATE outside a transaction
    */
    UDM_SQL_HAVE_TOP     | UDM_SQL_HAVE_0xHEX    |
    UDM_SQL_HAVE_GOOD_COMMIT | UDM_SQL_HAVE_TRANSACT
    /*
    Something goes wrong with sp_rename!
    UDM_SQL_HAVE_RENAME  | UDM_SQL_HAVE_CREATE_LIKE
    */
     ,
    &udm_sqldb_ctlib_handler
  },
#endif
#if (HAVE_MYSQL)
  { 
    "mysql", UDM_DB_MYSQL, UDM_DB_MYSQL, 1,
    UDM_SQL_HAVE_BIND  |
    UDM_SQL_HAVE_LIMIT | UDM_SQL_HAVE_GROUPBY | UDM_SQL_HAVE_0xHEX |
    UDM_SQL_HAVE_RENAME | UDM_SQL_HAVE_CREATE_LIKE |
    UDM_SQL_HAVE_DROP_IF_EXISTS,
    &udm_sqldb_mysql_handler
  },
#endif
#if (HAVE_PGSQL)
  {
    "pgsql", UDM_DB_PGSQL, UDM_DB_PGSQL, 1,
    UDM_SQL_HAVE_BIND  |
    UDM_SQL_HAVE_LIMIT | UDM_SQL_HAVE_GROUPBY |
    UDM_SQL_HAVE_SUBSELECT | UDM_SQL_HAVE_TRANSACT |
    UDM_SQL_HAVE_RENAME | UDM_SQL_HAVE_CREATE_LIKE,
    /* UDM_SQL_HAVE_DROP_IF_EXISTS depends on server version */
    &udm_sqldb_pgsql_handler,
  },
#endif
#if (HAVE_IBASE)
  {
    "ibase", UDM_DB_IBASE, UDM_DB_IBASE, 0,
    /* 
    while indexing large sites and using the SQL in statement 
    interbase will fail when the items in the in IN statements
    are more then 1500. We'd better have to fix code to avoid 
    big INs instead of hidding DBSQL_IN.
    */
    UDM_SQL_HAVE_GROUPBY | UDM_SQL_HAVE_BIND |
    UDM_SQL_HAVE_FIRST_SKIP | UDM_SQL_HAVE_TRANSACT,
    &udm_sqldb_ibase_handler,
  },
#endif
#if (HAVE_SQLITE)
  {
    "sqlite",
    UDM_DB_SQLITE, UDM_DB_SQLITE, 1,
    UDM_SQL_HAVE_BIND  |
    UDM_SQL_HAVE_LIMIT | UDM_SQL_HAVE_GROUPBY | UDM_SQL_HAVE_TRANSACT,
    &udm_sqldb_sqlite_handler,
  },
#endif
#if (HAVE_SQLITE3)
  {
    "sqlite3",
    UDM_DB_SQLITE3, UDM_DB_SQLITE3, 1,
    UDM_SQL_HAVE_BIND   |
    UDM_SQL_HAVE_LIMIT  | UDM_SQL_HAVE_GROUPBY |
    UDM_SQL_HAVE_STDHEX | UDM_SQL_HAVE_GOOD_COMMIT | UDM_SQL_HAVE_TRANSACT |
    UDM_SQL_HAVE_DROP_IF_EXISTS | UDM_SQL_HAVE_RENAME |
    UDM_SQL_HAVE_CREATE_LIKE,
    &udm_sqldb_sqlite3_handler,
  },
#endif
#if (HAVE_ODBC)
  {
    "odbc-solid", UDM_DB_SOLID, UDM_DB_ODBC, 1,
    UDM_SQL_HAVE_GROUPBY | UDM_SQL_HAVE_BIND | UDM_SQL_HAVE_TRANSACT,
    &udm_sqldb_odbc_handler,
  },
  {
    "odbc-sapdb", UDM_DB_SAPDB, UDM_DB_ODBC, 1,
    UDM_SQL_HAVE_GROUPBY | UDM_SQL_HAVE_BIND | UDM_SQL_HAVE_TRANSACT,
    &udm_sqldb_odbc_handler,
  },
  {
    "odbc-db2", UDM_DB_DB2, UDM_DB_ODBC, 1,
    UDM_SQL_HAVE_GROUPBY | UDM_SQL_HAVE_BIND |
    UDM_SQL_HAVE_BIND    | UDM_SQL_HAVE_STDHEX | UDM_SQL_HAVE_TRANSACT |
    UDM_SQL_HAVE_RENAME | UDM_SQL_HAVE_CREATE_LIKE,    
    &udm_sqldb_odbc_handler,
  },
  {
    "odbc-access", UDM_DB_ACCESS, UDM_DB_ODBC, 1,
    UDM_SQL_HAVE_GROUPBY | UDM_SQL_HAVE_SUBSELECT |
    UDM_SQL_HAVE_0xHEX   | UDM_SQL_HAVE_TRANSACT,
    &udm_sqldb_odbc_handler,
  },
  {
    "odbc-mimer", UDM_DB_MIMER, UDM_DB_ODBC, 1,
    UDM_SQL_HAVE_GROUPBY | UDM_SQL_HAVE_SUBSELECT |
    UDM_SQL_HAVE_BIND | UDM_SQL_HAVE_STDHEX | UDM_SQL_HAVE_TRANSACT,
    &udm_sqldb_odbc_handler,
  },
  {
    "odbc-cache", UDM_DB_CACHE, UDM_DB_ODBC, 1,
    UDM_SQL_HAVE_GROUPBY | UDM_SQL_HAVE_SUBSELECT |
    UDM_SQL_HAVE_BIND    | UDM_SQL_HAVE_TRANSACT,
    &udm_sqldb_odbc_handler,
  },
  { 
    "odbc-virtuoso", UDM_DB_VIRT, UDM_DB_ODBC, 1,
    UDM_SQL_HAVE_GROUPBY | UDM_SQL_HAVE_SUBSELECT |
    UDM_SQL_HAVE_BIND    | UDM_SQL_HAVE_TRANSACT | UDM_SQL_HAVE_TOP,
    &udm_sqldb_odbc_handler,
  },
  {
    "odbc-oracle", UDM_DB_ORACLE8, UDM_DB_ODBC, 1,
    UDM_SQL_HAVE_GROUPBY   | UDM_SQL_HAVE_TRUNCATE |
    UDM_SQL_HAVE_SUBSELECT | UDM_SQL_HAVE_BIND |
    UDM_SQL_HAVE_ROWNUM    | UDM_SQL_HAVE_GOOD_COMMIT |
    UDM_SQL_HAVE_TRANSACT  |
    UDM_SQL_HAVE_RENAME    | UDM_SQL_HAVE_CREATE_LIKE,
    &udm_sqldb_odbc_handler,
  },
  {
    "odbc-oracle8", UDM_DB_ORACLE8, UDM_DB_ODBC, 1,
    UDM_SQL_HAVE_GROUPBY   | UDM_SQL_HAVE_TRUNCATE |
    UDM_SQL_HAVE_SUBSELECT | UDM_SQL_HAVE_BIND | UDM_SQL_HAVE_ROWNUM |
    UDM_SQL_HAVE_TRANSACT  |
    UDM_SQL_HAVE_RENAME    | UDM_SQL_HAVE_CREATE_LIKE,
    &udm_sqldb_odbc_handler,
  },
  {
    "odbc-mssql", UDM_DB_MSSQL, UDM_DB_ODBC, 1,
    UDM_SQL_HAVE_GROUPBY | UDM_SQL_HAVE_TRUNCATE |
    UDM_SQL_HAVE_TOP     | UDM_SQL_HAVE_0xHEX | UDM_SQL_HAVE_TRANSACT |
    UDM_SQL_HAVE_RENAME  | UDM_SQL_HAVE_CREATE_LIKE,
    &udm_sqldb_odbc_handler,
  },
  {
    "odbc-sybase", UDM_DB_SYBASE, UDM_DB_ODBC, 1,
    UDM_SQL_HAVE_GROUPBY | /*UDM_SQL_HAVE_TRUNCATE |*/
    UDM_SQL_HAVE_TOP     | UDM_SQL_HAVE_0xHEX |
    UDM_SQL_HAVE_TRANSACT| UDM_SQL_HAVE_GOOD_COMMIT,
    &udm_sqldb_odbc_handler,
  },
  {
    "odbc-mysql", UDM_DB_MYSQL, UDM_DB_ODBC, 1,
    UDM_SQL_HAVE_BIND  |
    UDM_SQL_HAVE_LIMIT | UDM_SQL_HAVE_GROUPBY | UDM_SQL_HAVE_0xHEX |
    UDM_SQL_HAVE_RENAME | UDM_SQL_HAVE_CREATE_LIKE |
    UDM_SQL_HAVE_DROP_IF_EXISTS,
    &udm_sqldb_odbc_handler,
  },
  {
    /* Bind does not seem to work with BYTEA in Windows */
    "odbc-pgsql", UDM_DB_PGSQL, UDM_DB_ODBC, 1,
    UDM_SQL_HAVE_LIMIT      | UDM_SQL_HAVE_GROUPBY |
    UDM_SQL_HAVE_SUBSELECT  /*| UDM_SQL_HAVE_BIND*/|
    UDM_SQL_HAVE_TRANSACT,
    &udm_sqldb_odbc_handler,
  },
  {
    "odbc-ibase", UDM_DB_IBASE, UDM_DB_ODBC, 0,
    /* 
    while indexing large sites and using the SQL in statement 
    interbase will fail when the items in the in IN statements
    are more then 1500. We'd better have to fix code to avoid 
    big INs instead of hidding DBSQL_IN.
    */
    UDM_SQL_HAVE_GROUPBY | UDM_SQL_HAVE_BIND |
    UDM_SQL_HAVE_FIRST_SKIP | UDM_SQL_HAVE_TRANSACT,
    &udm_sqldb_odbc_handler,
  },
  {
    "odbc-monetdb", UDM_DB_MONETDB, UDM_DB_ODBC, 1,
    UDM_SQL_HAVE_GROUPBY | UDM_SQL_HAVE_SUBSELECT | UDM_SQL_HAVE_LIMIT |
    UDM_SQL_HAVE_BIND |
    /*UDM_SQL_HAVE_BIND_TEXT | */UDM_SQL_HAVE_BLOB_AS_HEX | UDM_SQL_HAVE_TRANSACT,
    &udm_sqldb_odbc_handler,
  },
#endif
  {
    NULL, 0, 0, 0, 0, NULL
  }
};


static UDM_SQLDB_DRIVER *UdmSQLDriverByName(const char *name)
{
  UDM_SQLDB_DRIVER *drv;
  for (drv= SQLDriver; drv->name; drv++)
  {
    if (!strcasecmp(name, drv->name))
      return drv;
    if (!strncasecmp(drv->name, "odbc-", 5) &&
        !strcasecmp(drv->name + 5, name))
      return drv;
  }
  return NULL;
}


static UDM_DBMODE_HANDLER *
UdmDBModeHandlerByID(int DBMode)
{
#ifdef HAVE_SQL
  switch (DBMode)
  {
    case UDM_DBMODE_BLOB:
      return &udm_dbmode_handler_blob;
    case UDM_DBMODE_SINGLE:
      return &udm_dbmode_handler_single;
    case UDM_DBMODE_MULTI:
      return &udm_dbmode_handler_multi;
    case UDM_DBMODE_RAWBLOB:
      return &udm_dbmode_handler_rawblob;
  }
  UDM_ASSERT(0);
#endif
  return NULL;
}

int UdmDBSetAddr(UDM_DB *db, const char *dbaddr, int mode)
{
  UDM_URL    addr;
  char     *s;
  const char *v;
  int        rc= UDM_OK;
  
  UdmVarListFree(&db->Vars);
  UDM_FREE(db->DBName);
  UDM_FREE(db->where);
  UDM_FREE(db->from);
  db->DBMode= UDM_DBMODE_BLOB;
  
  UdmVarListReplaceStr(&db->Vars, "DBAddr", dbaddr);
  
  UdmURLInit(&addr);
  
  if((!dbaddr) || UdmURLParse(&addr, dbaddr) || (!addr.schema))
  {
    rc= UDM_ERROR; /* Invalid DBAddr */
    goto ret;
  }
  
  if (addr.auth)
  {
    /*
      Unescape user and password to allow URL specific
      characters like '"<>@#? to be used as user or password part.
      
      It's safe to spoil addr.auth here, as we don't
      need it anymore after setting DBUser and DBPass
    */
    
    if ((s= strchr(addr.auth,':')))
    {
      *s++= 0;
      UdmUnescapeCGIQuery(s, s);
      UdmVarListReplaceStr(&db->Vars, "DBPass", s);
    }
    UdmUnescapeCGIQuery(addr.auth, addr.auth);
    UdmVarListReplaceStr(&db->Vars, "DBUser", addr.auth);
  }
  
  UdmVarListReplaceStr(&db->Vars, "DBHost", addr.hostname);
  if (addr.port)
    UdmVarListReplaceInt(&db->Vars, "DBPort", addr.port);
  
  if((s = strchr(UDM_NULL2EMPTY(addr.filename), '?')))
  {
    *s++='\0';
    if (UDM_OK != UdmDBSetParam(db, s))
    {
      rc= UDM_ERROR;
      goto ret;
    }
    UdmVarListReplaceStr(&db->Vars, "filename", addr.filename);
  }
  else
  {
    UdmVarListReplaceStr(&db->Vars, "filename", addr.filename);
  }
  
  if(!strcasecmp(addr.schema, "searchd") ||
     !strcasecmp(addr.schema, "http") ||
     !strcasecmp(addr.schema, "file"))
  {
    db->DBType=UDM_DB_SEARCHD;
    db->DBDriver=UDM_DB_SEARCHD;
  }
  else
  {
    UDM_SQLDB_DRIVER *drv= UdmSQLDriverByName(addr.schema);
    if (!drv)
    {
      rc= UDM_UNSUPPORTED; /* Unsupported DBAddr */
      goto ret;
    }
    
    db->DBType= drv->DBType;
    db->DBDriver= drv->DBDriver;
    db->DBSQL_IN= drv->DBSQL_IN;
    db->flags= drv->flags;
    db->sql= drv->handler;
  }
  
  if((v= UdmVarListFindStr(&db->Vars,"numtables",NULL)))
  {
    db->numtables= atoi(v);
    if(!db->numtables)
      db->numtables=1;
  }

  if((v= UdmVarListFindStr(&db->Vars,"dbmode",NULL)))
  {
    if ((db->DBMode=UdmStr2DBMode(v)) < 0) 
    return UDM_ERROR;
  }
  
  db->dbmode_handler= UdmDBModeHandlerByID(db->DBMode);
  
  if((v= UdmVarListFindStr(&db->Vars,"dbmodesearch",NULL)))
  {
    int DBMode;
    if ((DBMode=UdmStr2DBMode(v)) < 0) 
      return UDM_ERROR;
    if (DBMode == UDM_DBMODE_BLOB  &&
        db->DBType != UDM_DB_MYSQL &&
        db->DBType != UDM_DB_SYBASE &&
        db->DBType != UDM_DB_MSSQL &&
        db->DBType != UDM_DB_MIMER &&
        db->DBType != UDM_DB_ORACLE8 &&
        db->DBType != UDM_DB_DB2 &&
        db->DBType != UDM_DB_PGSQL &&
        db->DBType != UDM_DB_IBASE &&
        db->DBType != UDM_DB_VIRT &&
        db->DBType != UDM_DB_SQLITE3 &&
        db->DBType != UDM_DB_MONETDB)
      return UDM_ERROR;
  }

  if((v= UdmVarListFindStr(&db->Vars, "debugsql", "no")))
  {
    if (!strcasecmp(v, "yes")) 
      db->flags |= UDM_SQL_DEBUG_QUERY;
  }

  if(db->DBDriver == UDM_DB_IBASE ||
     db->DBDriver == UDM_DB_SQLITE ||
     db->DBDriver == UDM_DB_SQLITE3)
  {
    /*
      Ibase is a special case:
      It's database name consists of
      full path and file name        
    */
    db->DBName = (char*)UdmStrdup(UDM_NULL2EMPTY(addr.path));
  }
  else
  {
    /*
      ODBC Data Source Names may contain space and
      other tricky characters, let's unescape them.
    */
    size_t len= strlen(UDM_NULL2EMPTY(addr.path));
    char  *src= (char*)UdmMalloc(len+1);
    src[0]= '\0';
    sscanf(UDM_NULL2EMPTY(addr.path), "/%[^/]s", src);
    db->DBName= (char*)UdmMalloc(len+1);
    UdmUnescapeCGIQuery(db->DBName, src);
    UdmFree(src);
  }
  
  if (UdmVarListFindInt(&db->Vars, "ps", 0) == 123)
  {
#ifdef HAVE_SQL
    db->sql->SQLPrepare= UdmSQLPrepareGeneric;
    db->sql->SQLBind= UdmSQLBindGeneric; 
    db->sql->SQLExec= UdmSQLExecGeneric;
    db->sql->SQLStmtFree= UdmSQLStmtFreeGeneric;
    db->flags|= UDM_SQL_HAVE_BIND;
#endif
  }
  else if ((db->DBType == UDM_DB_MSSQL   ||
            db->DBType == UDM_DB_SYBASE  ||
            db->DBType == UDM_DB_MYSQL   ||
            db->DBType == UDM_DB_PGSQL   ||
            db->DBType == UDM_DB_SQLITE  ||
            db->DBType == UDM_DB_SQLITE3)&&
            !strcasecmp(UdmVarListFindStr(&db->Vars, "ps", ""), "none"))
  {
    db->flags&= (0x7FFFFFFF ^ UDM_SQL_HAVE_BIND);
  }
  else if ((db->DBDriver == UDM_DB_MYSQL ||
            db->DBDriver == UDM_DB_PGSQL ||
            db->DBDriver == UDM_DB_ODBC) &&
            db->sql->SQLExec &&
            UdmVarListFindBool(&db->Vars, "ps", 0))
  {
    db->flags|= UDM_SQL_HAVE_BIND;
  }


ret:
  UdmURLFree(&addr);
  return rc;
}


__C_LINK int __UDMCALL UdmStatAction(UDM_AGENT *A, UDM_STATLIST *S)
{
  UDM_DB  *db;
  int  res=UDM_ERROR;
  size_t i, dbfrom = 0, dbto;

  UDM_GETLOCK(A, UDM_LOCK_CONF);
  dbto=  A->Conf->dbl.nitems;
  S->nstats = 0;
  S->Stat = NULL;

  for (i = dbfrom; i < dbto; i++)
  {
    if (!UdmDBIsActive(A, i))
      continue;
    db = &A->Conf->dbl.db[i];
#ifdef HAVE_SQL
    UDM_GETLOCK(A, UDM_LOCK_DB);
    res = UdmStatActionSQL(A, S, db);
    UDM_RELEASELOCK(A, UDM_LOCK_DB);
#endif
    if (res != UDM_OK)
    {
      strcpy(A->Conf->errstr, db->errstr);
      db->errcode= 0;
      break;
    }
  }
  UDM_RELEASELOCK(A, UDM_LOCK_CONF);
  return res;
}

unsigned int UdmGetCategoryId(UDM_ENV *Conf, char *category)
{
  UDM_DB  *db;
  unsigned int rc = 0;
  size_t i, dbfrom = 0, dbto =  Conf->dbl.nitems;

  for (i = dbfrom; i < dbto; i++)
  {
    db = &Conf->dbl.db[i];
#ifdef HAVE_SQL
    rc = UdmGetCategoryIdSQL(Conf, category, db);
    if (rc != 0) return rc;
#endif
  }
  return rc;
}


int UdmTrack(UDM_AGENT * query, UDM_RESULT *Res)
{
  int rc = UDM_OK;
#ifdef HAVE_SQL
  size_t i, dbfrom = 0, dbto=  query->Conf->dbl.nitems; 
  char * env= getenv("REMOTE_ADDR");
  UdmVarListAddStr(&query->Conf->Vars, "IP", env ? env : "");
  
  for (i = dbfrom; i < dbto; i++)
  {
    const char *v;
    UDM_DB *db = &query->Conf->dbl.db[i];
    if((v= UdmVarListFindStr(&db->Vars,"trackquery",NULL)))
      rc = UdmTrackSQL(query, Res, db);
  }
#endif
  return rc;
}


UDM_RESULT * UdmCloneList(UDM_AGENT * Indexer, UDM_DOCUMENT *Doc)
{
  size_t i, dbfrom = 0, dbto =  Indexer->Conf->dbl.nitems;
  UDM_DB    *db;
  UDM_RESULT  *Res;
  int    rc = UDM_OK;

  Res = UdmResultInit(NULL);
  
  for (i = dbfrom; i < dbto; i++)
  {
    db = &Indexer->Conf->dbl.db[i];
    switch(db->DBDriver)
    {
      case UDM_DB_SEARCHD:
        rc = UdmCloneListSearchd(Indexer, Doc, Res, db);
        break;
#ifdef HAVE_SQL
     default:
        rc = UdmCloneListSQL(Indexer, Doc, Res, db);
        break;
#endif
    }
    if (rc != UDM_OK) break;
  }
  if (Res->num_rows > 0) return Res;
  UdmResultFree(Res);
  return NULL;
}


int UdmCheckUrlid(UDM_AGENT *Agent, urlid_t id)
{
  size_t i, dbfrom = 0, dbto;
  UDM_DB    *db;
  int    rc = 0;

  UDM_GETLOCK(Agent, UDM_LOCK_CONF);
  dbto =  Agent->Conf->dbl.nitems;

  for (i = dbfrom; i < dbto; i++)
  {
    db = &Agent->Conf->dbl.db[i];
    UDM_GETLOCK(Agent, UDM_LOCK_DB); 
    switch(db->DBDriver)
    {
#ifdef HAVE_SQL
      default:
        rc = UdmCheckUrlidSQL(Agent, db, id);
        break;
#endif
    }
    UDM_RELEASELOCK(Agent, UDM_LOCK_DB);
    if (rc != 0) break;
  }
  UDM_RELEASELOCK(Agent, UDM_LOCK_CONF);
  return rc;
}


/********************************************************/


UDM_DBLIST * UdmDBListInit(UDM_DBLIST * List)
{
  bzero((void*)List, sizeof(*List));
  return(List);
}

size_t UdmDBListAdd(UDM_DBLIST *List, const char * addr, int mode)
{
  UDM_DB  *db;
  int res;
  db=List->db=(UDM_DB*)UdmRealloc(List->db,(List->nitems+1)*sizeof(UDM_DB));
  db+=List->nitems;
  UdmDBInit(db);
  res = UdmDBSetAddr(db, addr, mode);
  if (res == UDM_OK) List->nitems++;
  return res;
}

void UdmDBListFree(UDM_DBLIST *List)
{
  size_t  i;
  UDM_DB  *db=List->db;
  
  for(i = 0; i < List->nitems; i++)
  {
    UdmDBFree(&db[i]);
  }
  UDM_FREE(List->db);
  UdmDBListInit(List);
}


/******************************************/


int UdmMulti2Blob (UDM_AGENT *Indexer)
{
#ifdef HAVE_SQL
  size_t i;
  udm_timer_t ticks;

  UdmLog(Indexer,UDM_LOG_ERROR,"Converting to blob");
  ticks=UdmStartTimer();

  for (i = 0; i < Indexer->Conf->dbl.nitems; i++)
  {
    int rc;
    UDM_DB *db = &Indexer->Conf->dbl.db[i];
    if (!UdmDBIsActive(Indexer, i))
      continue;
    UDM_GETLOCK(Indexer, UDM_LOCK_DB);
    rc= UdmConvert2BlobSQL(Indexer, db);
    UDM_RELEASELOCK(Indexer, UDM_LOCK_DB);
    if (rc != UDM_OK)
    {
      UdmLog(Indexer,UDM_LOG_ERROR,"%s",db->errstr); 
      return rc;
    }
  }

  UdmLog(Indexer,UDM_LOG_ERROR,"Converting to blob finished\t%.2f", UdmStopTimer(&ticks));
#endif
  return UDM_OK;
}


int UdmExport (UDM_AGENT *Indexer)
{
  int rc= UDM_OK;
#ifdef HAVE_SQL
  size_t i;
  udm_timer_t ticks;

  UdmLog(Indexer,UDM_LOG_ERROR,"Starting export");
  ticks=UdmStartTimer();

  for (i = 0; i < Indexer->Conf->dbl.nitems; i++)
  {
    UDM_DB *db = &Indexer->Conf->dbl.db[i];
    if (!UdmDBIsActive(Indexer, i))
      continue;
    UDM_GETLOCK(Indexer, UDM_LOCK_DB);
    rc= UdmExportSQL(Indexer, db);
    UDM_RELEASELOCK(Indexer, UDM_LOCK_DB);
    if (rc != UDM_OK)
    {
      UdmLog(Indexer,UDM_LOG_ERROR,"%s",db->errstr); 
      break;
    }
  }
  
  UdmLog(Indexer,UDM_LOG_ERROR,"Export finished\t%.2f",UdmStopTimer(&ticks));
#endif
  return rc;
}

int new_version= 0;
