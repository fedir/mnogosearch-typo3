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

#ifdef HAVE_SQL


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#ifdef WIN32
#include <time.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_spell.h"
#include "udm_robots.h"
#include "udm_mutex.h"
#include "udm_db.h"
#include "udm_unicode.h"
#include "udm_unidata.h"
#include "udm_url.h"
#include "udm_log.h"
#include "udm_proto.h"
#include "udm_conf.h"
#include "udm_hash.h"
#include "udm_xmalloc.h"
#include "udm_boolean.h"
#include "udm_searchtool.h"
#include "udm_searchcache.h"
#include "udm_server.h"
#include "udm_stopwords.h"
#include "udm_doc.h"
#include "udm_result.h"
#include "udm_vars.h"
#include "udm_agent.h"
#include "udm_store.h"
#include "udm_hrefs.h"
#include "udm_word.h"
#include "udm_db_int.h"
#include "udm_sqldbms.h"
#include "udm_match.h"
#include "udm_indexer.h"
#include "udm_textlist.h"
#include "udm_parsehtml.h"

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif


static int
UdmFetchCachedQuery(UDM_AGENT *A,
                    UDM_RESULT *Res,
                    UDM_DB *db, const char *query, int *tm)
{
  int rc;
  UDM_SQLRES SQLRes;
  UDM_URLDATALIST *DataList= &Res->URLData;
  
  if(UDM_OK != (rc= UdmSQLQuery(db,&SQLRes,query)))
      return rc;
  if (UdmSQLNumRows(&SQLRes) == 1)
  {
    size_t i, nbytes;
    const char *p;
    DataList->nitems= UdmSQLLen(&SQLRes, 0, 0) / 12;
    nbytes= DataList->nitems * sizeof(UDM_URLDATA);
    DataList->Item= (UDM_URLDATA*) UdmMalloc(nbytes);
    bzero((void*) DataList->Item, nbytes);
    for (p= UdmSQLValue(&SQLRes, 0, 0), i= 0; i < DataList->nitems; i++)
    {
      UDM_URLDATA *D= &DataList->Item[i];
      D->url_id= udm_get_int4(p); p+= 4;
      D->score= udm_get_int4(p);  p+= 4;
      D->per_site= udm_get_int4(p); p+= 4;
    }
    if (UdmSQLNumCols(&SQLRes) >= 2)
    {
      UdmResultFromXML(A, Res, UdmSQLValue(&SQLRes, 0, 1),
                       UdmSQLLen(&SQLRes, 0,1), A->Conf->lcs);
      if (UdmSQLNumCols(&SQLRes) >= 3)
        *tm= atoi(UdmSQLValue(&SQLRes,0,2));
    }
  }
  UdmSQLFree(&SQLRes);
  return rc;
}


static int
UdmLoadCachedQueryWords(UDM_AGENT *query, UDM_RESULT *Res,
                        UDM_DB *db, const char *pqid)
{
  char qbuf[128], bqid[32];
  char *tm, *end;
  int iid, itm;
  UDM_URLDATALIST *DataList= &Res->URLData;
  char top[32], limit[32], rownum[32];
  UDM_SQL_TOP_CLAUSE Top;
  
  bzero(DataList, sizeof(*DataList));
  if (!pqid)
    return UDM_OK;
  
  udm_snprintf(bqid, sizeof(bqid), "%s", pqid);
  if (!(tm= strchr(bqid, '-')))
    return UDM_OK;
  *tm++= '\0';
  iid= (int) strtoul(bqid, &end, 16);
  itm= (int) strtol(tm, &end, 16);
  
  UdmSQLTopClause(db, 1, &Top);
  udm_snprintf(qbuf, sizeof(qbuf),
               "SELECT %sdoclist FROM qcache "
               "WHERE id=%d AND tm=%d %s ORDER BY tm DESC %s",
               top, iid, itm, rownum, limit);

  return UdmFetchCachedQuery(query, Res, db, qbuf, NULL);
}


const char QCACHEID[]=
"${q}.${pqid}.${SearchMode}.${orig_m}.${fl}.${wm}.${o}.${t}."
"${cat}.${ul}.${wf}.${g}.${tmplt}.${GroupBySite}.${site}."
"${type}.${sp}.${sy}.${dt}.${dp}.${dx}.${dm}.${dy}.${db}.${de}.${s}"
"${ss}.${us}.${su}.${sl.*}";


static int
QueryCacheID(UDM_AGENT *A)
{
  UDM_VARLIST *Vars= &A->Conf->Vars;
  const char *fmt= UdmVarListFindStr(Vars, "QueryCacheID", QCACHEID);
  UDM_DSTR d;
  int res;
  UdmDSTRInit(&d, 256);
  UdmDSTRParse(&d, fmt, Vars);
  res= (int) UdmStrHash32(d.data);
  UdmVarListReplaceStr(&A->Conf->Vars, "QueryCacheIDValue", d.data);
  UdmDSTRFree(&d);
  return res;
}


int
UdmQueryCacheGetSQL(UDM_AGENT *A, UDM_RESULT *Res, UDM_DB *db)
{
  int use_qcache= UdmVarListFindBool(&db->Vars, "qcache", 0);
  int tm, rc, id, qtm;
  char qbuf[128];
  UDM_SQL_TOP_CLAUSE Top;
  
  if (!use_qcache)
    return UDM_OK;
  
  if (db->DBMode != UDM_DBMODE_BLOB)
    return UDM_OK;
  if (UDM_OK != (rc= UdmBlobReadTimestamp(A, db, &tm, (int) time(0))))
    return rc;
  id= QueryCacheID(A);

  UdmSQLTopClause(db, 1, &Top);

  udm_snprintf(qbuf, sizeof(qbuf),
               "SELECT %sdoclist, wordinfo, tm "
               "FROM qcache "
               "WHERE id=%d AND tm>=%d %s"
               "ORDER BY tm DESC%s", Top.top, id, tm, Top.rownum, Top.limit);
  rc= UdmFetchCachedQuery(A, Res, db, qbuf, &qtm);
  if (Res->URLData.nitems)
  {
    UdmLog(A, UDM_LOG_DEBUG,
           "Fetched from qcache %d documents, %d total found",
           (int) Res->URLData.nitems, (int) Res->total_found);
    udm_snprintf(qbuf, sizeof(qbuf), "QCache:%08X-%08X", id, qtm);
    UdmVarListReplaceStr(&A->Conf->Vars, "ResultSource", qbuf);
    udm_snprintf(qbuf, sizeof(qbuf), "%08X-%08X", id, qtm);
    UdmVarListAddStr(&A->Conf->Vars, "qid", qbuf);
  }
  return UDM_OK;
}


static int
UdmExecuteUserCacheQuery(UDM_AGENT *A, UDM_RESULT *Res, UDM_DB *db)
{
  int rc= UDM_OK;
  const char *usercache= UdmVarListFindStr(&db->Vars, "usercache", "");
  const char *UserCacheQuery= UdmVarListFindStr(&A->Conf->Vars,
                                                "UserCacheQuery", NULL);

  if (UserCacheQuery && UserCacheQuery[0])
  {
    size_t i;
    UDM_DSTR dstr;
    UDM_VARLIST vars;
    UdmVarListInit(&vars);
    UdmVarListAddLst(&vars, &A->Conf->Vars, NULL, "*");
    UdmVarListReplaceInt(&vars, "total", Res->total_found);
    UdmDSTRInit(&dstr, 64);
    for (i= 0; i < Res->URLData.nitems; i++)
    {
      UDM_URLDATA *Item= &Res->URLData.Item[i];
      UdmVarListReplaceInt(&vars, "url_id", Item->url_id);
      UdmVarListReplaceInt(&vars, "score", Item->score);
      UdmVarListReplaceInt(&vars, "rank", i);
      UdmDSTRParse(&dstr, UserCacheQuery, &vars);
      if (UDM_OK != (rc= UdmSQLQuery(db, NULL, dstr.data)))
        break;
      UdmDSTRReset(&dstr);
    }
    UdmDSTRFree(&dstr);
    UdmVarListFree(&vars);
    if (rc != UDM_OK)
      return rc;
  }


  /* Built-in UserCacheQuery */
  if (usercache[0] && strcasecmp(usercache, "no"))
  {
    size_t i;
    for (i= 0; i < Res->URLData.nitems; i++)
    {
      char qbuf[64];
      udm_snprintf(qbuf, sizeof(qbuf),
                   "INSERT INTO %s VALUES(%d, %d)",
                    usercache,
                    Res->URLData.Item[i].url_id,
                    Res->URLData.Item[i].score);
      if (UDM_OK != (rc= UdmSQLQuery(db, NULL, qbuf)))
        return rc;
    }
  }
  return rc;
}


/*
  TODO: reuse a function from cluster
*/
static int
UdmResToXML(UDM_RESULT *Res, UDM_DSTR *wwl)
{
  size_t i;
  UdmDSTRAppendf(wwl, "<result>"); 
  UdmDSTRAppendf(wwl, "<totalResults>%d</totalResults>",
                 (int) Res->total_found);
  UdmDSTRAppendf(wwl,"<wordinfo>");
  for (i= 0; i < Res->WWList.nwords; i++)
  {
    UDM_WIDEWORD *ww= &Res->WWList.Word[i];
    UdmDSTRAppendf(wwl, "<word id='%d' order='%d' count='%d' len='%d' "
                         "origin='%d' weight='%d' match='%d' "
                         "secno='%d' phrlen='%d' phrpos='%d'>%s</word>",
                    (int) i, (int) ww->order, (int) ww->count, (int) ww->len,
                    ww->origin, ww->weight, ww->match,
                    (int) ww->secno, (int) ww->phrlen, (int) ww->phrpos,
                    ww->word);
  }
  UdmDSTRAppendf(wwl, "</wordinfo></result>");
  return UDM_OK;
}


/*
  Collect coord information into dstr
*/
static int
UdmURLDataListEncode(UDM_URLDATALIST *URLData, UDM_DB *db, UDM_DSTR *buf)
{
  size_t i;
  for (i= 0; i < URLData->nitems; i++)
  {
    UDM_URLDATA *D= &URLData->Item[i];
    char tmp[4*3];
    udm_put_int4(D->url_id,   tmp + 0);
    udm_put_int4(D->score,    tmp + 4);
    udm_put_int4(D->per_site, tmp + 8);
    if (!db)
    {
      UdmDSTRAppend(buf, tmp, 12);
    }
    else if (db->DBType == UDM_DB_PGSQL)
    {
      size_t len= UdmSQLBinEscStr(db, buf->data + buf->size_data, buf->size_total, tmp, 12);
      buf->size_data+= len;
    }
    else
      UdmDSTRAppendHex(buf, tmp, 12);
  }
  return UDM_OK;
}


int
UdmQueryCachePutSQL(UDM_AGENT *Indexer, UDM_RESULT *Res, UDM_DB *db)
{
  int prevcache= UdmVarListFindBool(&db->Vars, "qcache", 0);
  int rc= UDM_OK;
  char idbuf[64];
  UDM_DSTR buf, wwl, bindata;
  size_t nbytes= Res->URLData.nitems * 24, wwl_length_escaped;
  int id, tm= time(0);
  const char *prefix= (db->flags & UDM_SQL_HAVE_STDHEX) ? "X'" : 
                      (db->flags & UDM_SQL_HAVE_BLOB_AS_HEX) ? "'" : "0x";
  const char *suffix= (db->flags & UDM_SQL_HAVE_STDHEX) ? "'"  : 
                      (db->flags & UDM_SQL_HAVE_BLOB_AS_HEX) ? "'" : "";
  udm_timer_t ticks= UdmStartTimer();


  /* Put information into a user-defined cache */
  /* TODO: add its own debug logging inside this func: */
  if (UDM_OK != (rc= UdmExecuteUserCacheQuery(Indexer, Res, db)))
    return rc;
   
  if (!prevcache)
    return UDM_OK;

  UdmLog(Indexer, UDM_LOG_DEBUG,
         "Start UdmQueryCachePut %d documents", (int) Res->URLData.nitems);
    
  if (db->DBType == UDM_DB_PGSQL)
  {
    prefix= "'";
    suffix= "'";
  }
  
  id= QueryCacheID(Indexer);
  sprintf(idbuf, "%08X-%08X", id, tm);

  UdmDSTRInit(&wwl, 256);
  UdmResToXML(Res, &wwl);

  wwl_length_escaped= wwl.size_data * 5;

  UdmDSTRInit(&bindata, 2048);
  UdmDSTRInit(&buf, 256);
  UdmDSTRRealloc(&buf, nbytes + 128 + wwl_length_escaped);
  UdmDSTRAppendf(&buf,
                 "INSERT INTO qcache (id, tm, doclist, wordinfo) "
                 "VALUES (%d, %d, ", id, tm);

  if (db->flags & UDM_SQL_HAVE_BIND_BINARY)
  {
    int bindtype= UDM_SQLTYPE_LONGVARBINARY;
    
    UdmDSTRAppendSTR(&buf, UdmSQLParamPlaceHolder(db, 1));
    UdmDSTRAppendSTR(&buf, ",");
    UdmDSTRAppendSTR(&buf, UdmSQLParamPlaceHolder(db, 2));
    UdmDSTRAppendSTR(&buf, ")");

    UdmURLDataListEncode(&Res->URLData, NULL, &bindata);
    
    if (UDM_OK != (rc= UdmSQLPrepare(db, buf.data)) ||
        UDM_OK != (rc= UdmSQLBindParameter(db, 1,
                                           bindata.data,
                                           (int) bindata.size_data,
                                           bindtype)) ||
        UDM_OK != (rc= UdmSQLBindParameter(db, 2,
                                           wwl.data, (int) wwl.size_data,
                                           UDM_SQLTYPE_VARCHAR)) ||
        UDM_OK != (rc= UdmSQLExecute(db)) ||
        UDM_OK != (rc= UdmSQLStmtFree(db)))
      goto ret;
  }
  else
  {
    UdmDSTRAppendSTR(&buf, prefix);
    
    /* Append coord information */
    UdmURLDataListEncode(&Res->URLData, db, &buf);

    UdmDSTRAppendSTR(&buf, suffix);

    /* Append word information */
    UdmDSTRAppend(&buf, ",'", 2);
    UdmSQLEscStr(db, buf.data + buf.size_data, wwl.data, wwl.size_data); /* WWList XML*/
    nbytes= strlen(buf.data + buf.size_data);
    buf.size_data+= nbytes;
    UdmDSTRAppend(&buf, "')", 2);
    
    if (UDM_OK == (rc= UdmSQLQuery(db, NULL, buf.data)))
      UdmVarListAddStr(&Indexer->Conf->Vars, "qid", idbuf);
  }

ret:

  UdmDSTRFree(&wwl);
  UdmDSTRFree(&buf);
  UdmDSTRFree(&bindata);

  UdmLog(Indexer, UDM_LOG_DEBUG, "Stop  UdmQueryCachePut: %.2f",
         UdmStopTimer(&ticks));

  return rc;
}


/* TODO: merge cmp_data_urls with the same function in  sql.c */
static int
cmp_data_urls(UDM_URLDATA *d1, UDM_URLDATA *d2)
{
  if (d1->url_id > d2->url_id) return 1;
  if (d1->url_id < d2->url_id) return -1;
  return 0;
}


int 
UdmApplyCachedQueryLimit(UDM_AGENT *query, UDM_URLSCORELIST *ScoreList, UDM_DB *db)
{
  UDM_RESULT CachedResult;
  UDM_URLDATALIST *CachedData= &CachedResult.URLData;
  const char *pqid= UdmVarListFindStr(&query->Conf->Vars, "pqid", NULL);
  UdmResultInit(&CachedResult);
  if (pqid && UDM_OK == UdmLoadCachedQueryWords(query, &CachedResult, db, pqid))
  {
    UdmLog(query, UDM_LOG_DEBUG,
           "Start applying pqid limit: %d docs", (int) CachedData->nitems);
    if (CachedData->nitems)
    {
      size_t i, to;
      UdmSort(CachedData->Item, CachedData->nitems, sizeof(UDM_URLDATA), (udm_qsort_cmp) cmp_data_urls);
      for (to= 0, i= 0; i < ScoreList->nitems; i++)
      {
        if (UdmURLDataListSearch(CachedData, ScoreList->Item[i].url_id))
        {
          if (to != i)
            ScoreList->Item[to]= ScoreList->Item[i];
          to++;
        }
      }
      ScoreList->nitems= to;
    }
    else
    {
      ScoreList->nitems= 0;
    }
    UdmLog(query, UDM_LOG_DEBUG,
           "Stop applying pqid limit: %d docs", (int) ScoreList->nitems);
  }
  UdmResultFree(&CachedResult);
  return UDM_OK;
}

#endif /* HAVE_SQL */
