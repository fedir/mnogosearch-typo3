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



/************** some forward declarations ********************/
static int UdmDeleteURL(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc,UDM_DB *db);
static int UdmDeleteBadHrefs(UDM_AGENT *Indexer, 
                             UDM_DOCUMENT *Doc,
                             UDM_DB *db,
                             urlid_t url_id);
static int UdmDeleteLinks(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc, UDM_DB *db);

static int UdmDeleteWordFromURL(UDM_AGENT *A, UDM_DOCUMENT *D, UDM_DB *db);


/*********************** helper functions **********************/

static size_t
WhereConditionAddAnd(char *where, const char *add)
{
  if (add[0])
  {
    const char *and= where[0] ? " AND " : "";
    return sprintf(where + strlen(where), "%s%s", and, add);
  }
  return 0;
}


static void
WhereConditionDSTRAddAnd(char *where, UDM_DSTR *add)
{
  if (add->size_data)
  {
    if (where[0])
      strcat(where, " AND ");
    strcat(where, add->data);
  }
}


/* Prepare to add a new condition into a class */
static int
UdmSQLWhereAddJoiner(UDM_DSTR *dstr, const char *joiner)
{
  if (dstr->size_data)  /* Second (or more) condition of the same class */
  {
    dstr->size_data--;
    UdmDSTRAppendSTR(dstr, joiner);
  }
  else                 /* First condition of this class */
  {
    UdmDSTRAppend(dstr, "(", 1);
  }
  return UDM_OK;
}


static int
UdmSQLWhereIntParam(UDM_DB *db,
                    UDM_DSTR *dstr, const char *sqlname, const char *val)
{
  const char *range= strchr(val, '-');
  UdmDSTRRealloc(dstr, dstr->size_data + strlen(val) + 50);
  if(db->DBSQL_IN && !range) /* Single value */
  {
    if(!dstr->size_data) /* First parameter */
    {
      UdmDSTRAppendf(dstr, " %s IN (%d)", sqlname, atoi(val));
    }
    else /* Second or higher parameter */
    {
      dstr->size_data--;
      UdmDSTRAppendf(dstr, ",%d)", atoi(val));
    }
  }
  else /* Range */
  {
    int first, second;
    UdmSQLWhereAddJoiner(dstr, " OR ");
    if (range && 2 == sscanf(val, "%d-%d", &first, &second))
    {
      UdmDSTRAppendf(dstr,
                     "%s>=%d AND %s<=%d)",
                     sqlname, first, sqlname, second);
    }
    else
    {
      UdmDSTRAppendf(dstr, "%s=%d)", sqlname, atoi(val));
    }
  }
  return UDM_OK;
}


#define UDM_ADD_PARAM_NEG                1  /* If reverse condition      */
#define UDM_ADD_PARAM_WITH_TAIL_PERCENT  2  /* If the trailing % needed  */
#define UDM_ADD_PARAM_CHECK_URL_SCHEMA   4  /* Detect URL prefix         */

#define UDM_ADD_PARAM_URL     (UDM_ADD_PARAM_CHECK_URL_SCHEMA|UDM_ADD_PARAM_WITH_TAIL_PERCENT)
#define UDM_ADD_PARAM_URL_NEG (UDM_ADD_PARAM_NEG|UDM_ADD_PARAM_URL)


static int
UdmSQLWhereStrParam(UDM_DB *db, UDM_DSTR *dstr,
                    const char *sqlname, const char *val, int flag)
{
  const char *first= "";
  const char *last= (flag & UDM_ADD_PARAM_WITH_TAIL_PERCENT) ? "%" : "";
  const char *joiner= (flag & UDM_ADD_PARAM_NEG) ? " AND " : " OR ";
  const char *not= (flag & UDM_ADD_PARAM_NEG) ? "NOT " : "";

  if (flag & UDM_ADD_PARAM_CHECK_URL_SCHEMA)
  {
    UDM_URL URL;
    UdmURLInit(&URL);
    UdmURLParse(&URL,val);
    /* Check if URL prefix is not given / given */
    first= (URL.schema == NULL) ? "%" : "";
    UdmURLFree(&URL);
  }

  UdmSQLWhereAddJoiner(dstr, joiner);
  UdmDSTRAppendf(dstr, "%s %sLIKE '%s%s%s')", sqlname, not, first, val, last);
  return UDM_OK;
}


typedef struct udm_date_param_st
{
  int dt;
  int dx;
  int dm;
  int dy;
  int dd;
  time_t dp;
  int DB;
  int DE;
  int dstmp;
} UDM_DATE_PARAM;


static void
UdmDateParamInit(UDM_DATE_PARAM *d)
{
  d->dt= UDM_DT_UNKNOWN;
  d->dx= 1;
  d->dm= 0;
  d->dy= 1970;
  d->dd= 1;
  d->dp= (time_t) 0;
  d->DB= 0;
  d->DE= time(NULL);
  d->dstmp= 0;
}


static int
UdmCheckDateParam(UDM_DATE_PARAM *d,
                  const char *var, const char *val)
{
  int intval= atoi(val);
  int longval= atol(val);

  if (!strcmp(var, "dt"))
  {
    if(!strcasecmp(val, "back")) d->dt= UDM_DT_BACK;
    else if (!strcasecmp(val, "er")) d->dt= UDM_DT_ER;
    else if (!strcasecmp(val, "range")) d->dt= UDM_DT_RANGE;
  }
  else if (!strcmp(var, "dx"))
  {
    if (intval == 1 || intval == -1) d->dx= intval;
    else d->dx= 1;
  }
  else if (!strcmp(var, "dm"))
  {
    d->dm= (intval) ? intval : 1;
  }
  else if (!strcmp(var, "dy"))
  {
    d->dy= (intval) ? intval : 1970;
  }
  else if (!strcmp(var, "dd"))
  {
    d->dd= (intval) ? intval : 1;
  }
  else if (!strcmp(var, "dstmp"))
  {
    d->dstmp= longval ? longval : 0;
  }
  else if (!strcmp(var, "dp"))
  {
    d->dp= Udm_dp2time_t(val);
  }
  else if (!strcmp(var, "db"))
  {
    struct tm tm;
    bzero((void*) &tm, sizeof(tm));
    sscanf(val, "%d/%d/%d", &tm.tm_mday, &tm.tm_mon, &tm.tm_year);
    tm.tm_year -= 1900; tm.tm_mon--;
    d->DB= mktime(&tm);
  }
  else if (!strcmp(var, "de"))
  {
    struct tm tm;
    bzero((void*) &tm, sizeof(tm));
    sscanf(val, "%d/%d/%d", &tm.tm_mday, &tm.tm_mon, &tm.tm_year);
    tm.tm_year -= 1900; tm.tm_mon--;
    d->DE= mktime(&tm) + 86400; /* Including the given date */
  }
  else
    return UDM_ERROR;
  return UDM_OK;
}


static void
UdmSQLWhereDateParam(UDM_DSTR *cond, UDM_DATE_PARAM *d)
{
  switch(d->dt)
  {
    case UDM_DT_BACK:
      if (d->dp)
        UdmDSTRAppendf(cond, "url.last_mod_time >= %li",
                       (long int) time(NULL) - d->dp);
      break;
    case UDM_DT_ER:
      {
      struct tm tm;
        bzero((void*) &tm, sizeof(tm));
        tm.tm_mday= d->dd;
        tm.tm_mon= d->dm;
        tm.tm_year= d->dy - 1900;
        UdmDSTRAppendf(cond, "url.last_mod_time %s %li",
                       (d->dx == 1) ? ">=" : "<=",
                       (long int) (d->dstmp ? d->dstmp : mktime(&tm)));
      }
      break;
    case UDM_DT_RANGE:
      UdmDSTRAppendf(cond,
                     "url.last_mod_time >= %li AND url.last_mod_time <= %li",
                     (long int) d->DB, (long int) d->DE);
      break;
    case UDM_DT_UNKNOWN:
    default:
      break;
  }
}


const char*
UdmSQLBuildWhereCondition(UDM_ENV * Conf,UDM_DB *db)
{
  size_t  i;
  int     fromserver = 1, fromurlinfo_lang = 1, fromurlinfo_type = 1, fromurlinfo = 1;
  UDM_DATE_PARAM datep;
  struct tm tm;
  UDM_DSTR ue, seed, status, site, from, server, url, tag, lang, cat, type;
  UDM_DSTR urlinfo, timecond;
  int need_new= UdmVarListFindBool(&Conf->Vars, "delta", 0);
  const char *need_new_str= need_new ?
    "url.rec_id IN (SELECT url_id FROM bdicti WHERE state=1)" : "";
  
  if(db->where)return(db->where);
  
  bzero((void*)&tm, sizeof(struct tm));
  UdmDateParamInit(&datep);
  UdmDSTRInit(&from, 64);
  UdmDSTRInit(&server, 64);
  UdmDSTRInit(&ue, 64);
  UdmDSTRInit(&seed, 64);
  UdmDSTRInit(&status, 64);
  UdmDSTRInit(&site, 64);
  UdmDSTRInit(&url, 64);
  UdmDSTRInit(&tag, 64);
  UdmDSTRInit(&lang, 64);
  UdmDSTRInit(&cat, 64);
  UdmDSTRInit(&type, 64);
  UdmDSTRInit(&urlinfo, 64);
  UdmDSTRInit(&timecond, 64);
  
  for(i=0;i<Conf->Vars.nvars;i++)
  {
    const char *var=Conf->Vars.Var[i].name?Conf->Vars.Var[i].name:"";
    const char *val=Conf->Vars.Var[i].val?Conf->Vars.Var[i].val:"";
    int intval=atoi(val);
    char varbuf[64];
    char valbuf[64];
    size_t varlen, vallen;
    
    if(!val[0] ||
       (varlen= strlen(var)) >= sizeof(varbuf) ||
       (vallen= strlen(val)) >= sizeof(valbuf))
      continue;
    
    /* Protection against SQL injection */
    var= UdmSQLEscStrSimple(db, varbuf, var, varlen);
    val= UdmSQLEscStrSimple(db, valbuf, val, vallen);
    
    if (!strcmp(var, "status"))
      UdmSQLWhereIntParam(db, &status, "url.status", val);
    
    if (!strcmp(var, "seed"))
      UdmSQLWhereIntParam(db, &seed, "url.seed", val);
    
    if (!strcmp(var, "site") && intval != 0)
      UdmSQLWhereIntParam(db, &site, "url.site_id", val);
    
    if(!strcmp(var,"ul"))
      UdmSQLWhereStrParam(db, &url, "url.url", val, UDM_ADD_PARAM_URL);

    if(!strcmp(var,"ue"))
      UdmSQLWhereStrParam(db, &ue, "url.url", val, UDM_ADD_PARAM_URL_NEG);
    
    if(!strcmp(var,"u"))
      UdmSQLWhereStrParam(db, &url, "url.url", val, 0);
    
    if(!strcmp(var,"tag") || !strcmp(var,"t"))
    {
      UdmSQLWhereStrParam(db, &tag, "s.tag", val, 0);
      if (fromserver)
      {
        fromserver = 0;
        UdmDSTRAppendSTR(&from, ", server s");
        UdmDSTRAppendSTR(&server, " AND s.rec_id=url.server_id");
      }
    }
    
    if(!strcmp(var,"lang") || !strcmp(var,"g"))
    {
      UdmSQLWhereStrParam(db, &lang, "il.sval", val, 0);
      if (fromurlinfo_lang)
      {
        fromurlinfo_lang = 0;
        UdmDSTRAppendSTR(&from, ", urlinfo il");
        UdmDSTRAppendSTR(&server, " AND il.url_id=url.rec_id AND il.sname='Content-Language'");
      }
    }

    if(!strncmp(var, "sl.", 3))
    {
      UdmSQLWhereAddJoiner(&urlinfo, " AND ");
      UdmDSTRAppendf(&urlinfo,"isl%d.sname='%s' AND isl%d.sval LIKE '%s')",fromurlinfo,var+3,fromurlinfo,val);
      UdmDSTRAppendf(&from, ", urlinfo isl%d", fromurlinfo);
      UdmDSTRAppendf(&server, " AND isl%d.url_id=url.rec_id", fromurlinfo);
      fromurlinfo++;
    }
    
    if(!strcmp(var,"cat"))
    {
      UDM_ASSERT(val[0]);
      UdmSQLWhereStrParam(db, &cat, "c.path", val, UDM_ADD_PARAM_WITH_TAIL_PERCENT);
      if (fromserver)
      {
        fromserver = 0;
        UdmDSTRAppendSTR(&from, ", server s, categories c");
        UdmDSTRAppendSTR(&server, " AND s.rec_id=url.server_id AND s.category=c.rec_id");
      }
    }
    if(!strcmp(var,"type") || !strcmp(var, "typ"))
    {
      /* 
         "type" is a reserved word in ASP,
         so "typ" is also added as a workaround
      */
      UdmSQLWhereStrParam(db, &type, "it.sval", val, 0);
      if (fromurlinfo_type)
      {
        fromurlinfo_type = 0;
        UdmDSTRAppendSTR(&from, ", urlinfo it");
        UdmDSTRAppendSTR(&server, " AND it.url_id=url.rec_id AND it.sname='Content-Type'");
      }
    }
    
    UdmCheckDateParam(&datep, var, val);
  }


  UdmSQLWhereDateParam(&timecond, &datep);


  if(!url.size_data && !tag.size_data && cat.size_data && !lang.size_data &&
     !type.size_data && !timecond.size_data &&
     !urlinfo.size_data && !need_new_str[0] &&
     !from.size_data && !server.size_data && 
     !status.size_data && !site.size_data && !ue.size_data && !seed.size_data)
  {
    db->where = (char*)UdmStrdup("");
    db->from = (char*)UdmStrdup("");
    goto ret;
  }
  i= url.size_data + tag.size_data + cat.size_data + 
     lang.size_data + type.size_data + server.size_data + 
     timecond.size_data + urlinfo.size_data + strlen(need_new_str) +
     status.size_data + site.size_data + ue.size_data + seed.size_data;
  db->where=(char*)UdmMalloc(i+100);
  db->where[0] = '\0';
  UDM_FREE(db->from);
  db->from = (char*) UdmStrdup(from.size_data ? from.data : "");
  
  if(url.size_data)
  {
    strcat(db->where, url.data);
  }
  
  WhereConditionDSTRAddAnd(db->where, &ue);
  WhereConditionDSTRAddAnd(db->where, &seed);
  WhereConditionDSTRAddAnd(db->where, &status);
  WhereConditionDSTRAddAnd(db->where, &site);
  WhereConditionDSTRAddAnd(db->where, &tag);
  WhereConditionDSTRAddAnd(db->where, &lang);
  WhereConditionDSTRAddAnd(db->where, &cat);
  WhereConditionDSTRAddAnd(db->where, &type);
  WhereConditionDSTRAddAnd(db->where, &urlinfo);
  WhereConditionDSTRAddAnd(db->where, &timecond);
  
  if(server.size_data)
  {
    if(!db->where[0]) strcat(db->where, " 1=1 ");
    strcat(db->where, server.data);
  }

  WhereConditionAddAnd(db->where, need_new_str);

ret:
  UdmDSTRFree(&from);
  UdmDSTRFree(&server);
  UdmDSTRFree(&url);
  UdmDSTRFree(&ue);
  UdmDSTRFree(&seed);
  UdmDSTRFree(&status);
  UdmDSTRFree(&site);
  UdmDSTRFree(&tag);
  UdmDSTRFree(&lang);
  UdmDSTRFree(&cat);
  UdmDSTRFree(&type);
  UdmDSTRFree(&urlinfo);
  UdmDSTRFree(&timecond);

  /* Need this for test purposes */
  UdmVarListReplaceStr(&Conf->Vars, "WhereCondition", db->where);
  return db->where;
}


static int
UdmVarListSQLEscape(UDM_VARLIST *dst, UDM_VARLIST *src, UDM_DB *db)
{
  size_t i, nbytes= 0;
  char *tmp= NULL;
  for (i= 0; i < src->nvars; i++)
  {
    UDM_VAR *V= &src->Var[i];
    size_t len= V->curlen;
    if (nbytes < len * 2 + 1)
    {
      nbytes= len * 2 + 1;
      tmp= (char*) UdmRealloc(tmp, nbytes);
    }
    UdmSQLEscStr(db, tmp, V->val ? V->val : "", len);  /* doc Section */
    UdmVarListAddStr(dst, V->name, tmp);
  }
  UdmFree(tmp);
  return UDM_OK;
}



/******************** Popularity *****************************************/

static int
UdmPopRankCalculate(UDM_AGENT *A, UDM_DB *db)
{
  UDM_SQLRES  SQLres, Res, POPres;
  char    qbuf[1024];
  int             rc = UDM_ERROR, u;
  size_t    i, nrows;
  int    skip_same_site = !strcasecmp(UdmVarListFindStr(&A->Conf->Vars, "PopRankSkipSameSite","no"),"yes");
  int    feed_back = !strcasecmp(UdmVarListFindStr(&A->Conf->Vars, "PopRankFeedBack", "no"), "yes");
  int    use_tracking = !strcasecmp(UdmVarListFindStr(&A->Conf->Vars, "PopRankUseTracking", "no"), "yes");
  int    use_showcnt = !strcasecmp(UdmVarListFindStr(&A->Conf->Vars, "PopRankUseShowCnt", "no"), "yes");
  double          ratio = UdmVarListFindDouble(&A->Conf->Vars, "PopRankShowCntWeight", 0.01);
  const char *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";
  const char *typespec= (db->DBType == UDM_DB_PGSQL) ? "::float" : "";

  if (feed_back || use_tracking)
  {
    if (use_tracking) UdmLog(A, UDM_LOG_EXTRA, "Will calculate servers weights by tracking");
    if (feed_back) UdmLog(A, UDM_LOG_EXTRA, "Will calculate feed back servers weights");

    if(UDM_OK != (rc = UdmSQLQuery(db, &Res, "SELECT rec_id FROM server WHERE command='S'")))
      goto Calc_unlock;

    nrows = UdmSQLNumRows(&Res);
    for (i = 0; i < nrows; i++)
    {

      if (use_tracking)
      {
        udm_snprintf(qbuf, sizeof(qbuf), "SELECT COUNT(*) FROM qinfo WHERE name='site' AND value='%s'", UdmSQLValue(&Res, i, 0) );
        if(UDM_OK != (rc = UdmSQLQuery(db, &SQLres, qbuf))) goto Calc_unlock;
        u = (UDM_ATOI(UdmSQLValue(&SQLres, 0, 0)) == 0);
      }
      if (feed_back && (u || !use_tracking))
      {
        udm_snprintf(qbuf, sizeof(qbuf), "SELECT SUM(pop_rank) FROM url WHERE site_id=%s%s%s", qu, UdmSQLValue(&Res, i, 0), qu);
        if(UDM_OK != (rc = UdmSQLQuery(db, &SQLres, qbuf))) goto Calc_unlock;
      }
      if (*UdmSQLValue(&SQLres, 0, 0))
      {
        udm_snprintf(qbuf, sizeof(qbuf), "UPDATE server SET weight=%s WHERE rec_id=%s%s%s", UdmSQLValue(&SQLres, 0, 0), 
         qu, UdmSQLValue(&Res, i, 0), qu);
        UdmSQLQuery(db, NULL, qbuf);
      }
      UdmSQLFree(&SQLres);
    }
    UdmSQLFree(&Res);
    UdmSQLQuery(db, NULL, "UPDATE server SET weight=1 WHERE weight=0 AND command='S'");
  }


  if(UDM_OK != (rc = UdmSQLQuery(db, &SQLres, "SELECT rec_id, url, weight FROM server WHERE command='S'")))
    goto Calc_unlock;
  
  nrows = UdmSQLNumRows(&SQLres);

  for (i = 0; i < nrows; i++)
  {
    udm_snprintf(qbuf, sizeof(qbuf), "SELECT COUNT(*) FROM url WHERE site_id=%s%s%s", qu, UdmSQLValue(&SQLres, i, 0), qu);
    if(UDM_OK != (rc = UdmSQLQuery(db, &Res, qbuf)))
    goto Calc_unlock;
    UdmLog(A, UDM_LOG_EXTRA, "Site_id: %s URL: %s Weight: %s URL count: %s",
           UdmSQLValue(&SQLres, i, 0),
           UdmSQLValue(&SQLres, i, 1),
           UdmSQLValue(&SQLres, i, 2),
           UdmSQLValue(&Res, 0, 0)); 
    if (atoi(UdmSQLValue(&Res, 0, 0)) > 0)
    {
      udm_snprintf(qbuf, sizeof(qbuf),
                  "UPDATE server SET pop_weight=(%s/%s%s) WHERE rec_id=%s%s%s",
                  UdmSQLValue(&SQLres, i, 2),
                  UdmSQLValue(&Res, 0, 0),
                  typespec,
                  qu, UdmSQLValue(&SQLres, i, 0), qu);
      UdmSQLQuery(db, NULL, qbuf);
    }
    UdmSQLFree(&Res);

  }
  UdmSQLFree(&SQLres);


  UdmLog(A, UDM_LOG_EXTRA, "update links and pages weights");
  if (skip_same_site)  UdmLog(A, UDM_LOG_EXTRA, "Will skip links from same site");
  if (use_showcnt)  UdmLog(A, UDM_LOG_EXTRA, "Will add show count");

        udm_snprintf(qbuf, sizeof(qbuf), "SELECT rec_id, site_id  FROM url ORDER BY rec_id");

  if(UDM_OK != (rc = UdmSQLQuery(db, &Res, qbuf))) goto Calc_unlock;
  nrows = UdmSQLNumRows(&Res);
  for (i = 0; i < nrows; i++)
  {

    if (skip_same_site)
    {
      udm_snprintf(qbuf, sizeof(qbuf), "SELECT count(*) FROM links l, url uo, url uk WHERE uo.rec_id=l.ot AND uk.rec_id=l.k AND uo.site_id <> uk.site_id AND l.ot=%s%s%s", qu, UdmSQLValue(&Res, i, 0), qu);
    }
    else
    {
      udm_snprintf(qbuf, sizeof(qbuf), "SELECT count(*) FROM links WHERE ot=%s%s%s", qu, UdmSQLValue(&Res, i, 0), qu);
    }
    if(UDM_OK != (rc = UdmSQLQuery(db, &SQLres, qbuf))) goto Calc_unlock;
    if (*UdmSQLValue(&SQLres, 0, 0)) 
    {
      if (UDM_ATOI(UdmSQLValue(&SQLres, 0, 0)))
      {
        udm_snprintf(qbuf, sizeof(qbuf), "SELECT pop_weight FROM server WHERE rec_id=%s%s%s", qu, UdmSQLValue(&Res, i, 1), qu);
        if(UDM_OK != (rc = UdmSQLQuery(db, &POPres, qbuf))) goto Calc_unlock;
        if (UdmSQLNumRows(&POPres) != 1)
        { 
          UdmSQLFree(&POPres);
          UdmSQLFree(&SQLres);
          continue;
        }

        udm_snprintf(qbuf, sizeof(qbuf),
                     "UPDATE links SET weight = (%s/%d.0) WHERE ot=%s%s%s",
                     UdmSQLValue(&POPres, 0, 0),
                     atoi(UdmSQLValue(&SQLres, 0, 0)),
                     qu, UdmSQLValue(&Res, i, 0), qu);
        UdmSQLFree(&POPres);
        UdmSQLQuery(db, NULL, qbuf);
      }
    }
    UdmSQLFree(&SQLres);

    if (skip_same_site)
    {
      udm_snprintf(qbuf, sizeof(qbuf), "SELECT SUM(weight) FROM links l, url uo, url uk WHERE uo.rec_id=l.ot AND uk.rec_id=l.k AND uo.site_id <> uk.site_id AND l.k=%s%s%s", qu, UdmSQLValue(&Res, i, 0), qu);
    }
    else
    {
      udm_snprintf(qbuf, sizeof(qbuf), "SELECT SUM(weight) FROM links WHERE k=%s%s%s", qu, UdmSQLValue(&Res, i, 0), qu);
    }
    if(UDM_OK != (rc = UdmSQLQuery(db, &SQLres, qbuf))) goto Calc_unlock;
    if (UdmSQLValue(&SQLres,0,0) && *UdmSQLValue(&SQLres, 0, 0))
    {
      if (use_showcnt)
      {
        udm_snprintf(qbuf, sizeof(qbuf), "UPDATE url SET pop_rank=%s + (shows * %f) WHERE rec_id=%s%s%s", 
        UdmSQLValue(&SQLres, 0, 0), ratio, qu, UdmSQLValue(&Res, i, 0), qu );
      }
      else
      {
        udm_snprintf(qbuf, sizeof(qbuf), "UPDATE url SET pop_rank=%s WHERE rec_id=%s%s%s", 
        UdmSQLValue(&SQLres, 0, 0), qu, UdmSQLValue(&Res, i, 0), qu );
      }
    } else {
      udm_snprintf(qbuf, sizeof(qbuf), "UPDATE url SET pop_rank=(shows * %f) WHERE rec_id=%s%s%s", 
                   ratio, qu, UdmSQLValue(&Res, i, 0), qu );
    }
    UdmSQLQuery(db, NULL, qbuf);
    UdmSQLFree(&SQLres);
  }
  UdmSQLFree(&Res);

  rc = UDM_OK;

Calc_unlock:
  UdmLog(A, UDM_LOG_EXTRA, "Popularity rank done.");
  return rc;
}


/************* Servers ******************************************/

#define UDM_SERVER_TABLE_COLUMNS \
  "rec_id,url,tag,category,command,weight,ordre,parent,enabled "

static void
UdmServerInitFromRecord(UDM_SERVER *S, UDM_SQLRES *SQLRes, size_t row)
{
  const char *val;
  S->site_id= UDM_ATOI(UdmSQLValue(SQLRes, row, 0));
  val= UdmSQLValue(SQLRes, row, 1);
  S->Match.pattern= UdmStrdup(val ? val : "");
  S->ordre= UDM_ATOI(UdmSQLValue(SQLRes, row, 6));
  S->command= *UdmSQLValue(SQLRes, row, 4);
  S->weight= UDM_ATOF(UdmSQLValue(SQLRes, row, 5));
  
  if ((val= UdmSQLValue(SQLRes, row, 2)) && val[0])
    UdmVarListReplaceStr(&S->Vars, "Tag", val);
  
  if((val= UdmSQLValue(SQLRes, row, 3)) && val[0])
    UdmVarListReplaceStr(&S->Vars, "Category", val);
  S->parent= UDM_ATOI(UdmSQLValue(SQLRes, row, 7));
  S->enabled= UDM_ATOI(UdmSQLValue(SQLRes, row, 8));
}


static int
UdmServerNeedsUpdate(UDM_SERVER *a, UDM_SERVER *b)
{
  /* Note: we don't check "srvinfo" content */
  if (a->site_id != b->site_id ||
      strcmp(a->Match.pattern, b->Match.pattern) ||
      a->command != b->command ||
      strcmp(UdmVarListFindStr(&a->Vars, "Tag", ""), UdmVarListFindStr(&b->Vars, "Tag", "")) ||
      strcmp(UdmVarListFindStr(&a->Vars, "Category", "0"), UdmVarListFindStr(&b->Vars, "Category", "0")) ||
      a->weight != b->weight ||
      a->ordre != b->ordre ||
      a->parent != a->parent ||
      a->enabled != b->enabled)
    return 1;
  return 0;
}


static int
UdmLoadServerTable(UDM_AGENT * Indexer, UDM_SERVERLIST *S, UDM_DB *db)
{
  size_t    rows, i, j, jrows;
  UDM_SQLRES  SQLRes, SRes;
  UDM_HREF  Href;
  char    qbuf[1024];
  const char *filename= UdmVarListFindStr(&db->Vars, "filename", NULL);
  const char *name = (filename && filename[0]) ? filename : "server";
  const char *infoname = UdmVarListFindStr(&db->Vars, "srvinfo", "srvinfo");
  int   rc= UDM_OK;
  const char  *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";
  
  udm_snprintf(qbuf,sizeof(qbuf)-1,"SELECT " UDM_SERVER_TABLE_COLUMNS
                                   "FROM %s "
                                   "WHERE enabled=1 AND parent=%s0%s "
                                   "ORDER BY ordre", name, qu, qu);

  if (UDM_OK != (rc=UdmSQLQuery(db, &SQLRes, qbuf)))
    return rc;
  
  bzero((void*)&Href, sizeof(Href));
  
  rows= UdmSQLNumRows(&SQLRes);
  for(i= 0; i < rows; i++)
  {
    UDM_SERVER *Server= Indexer->Conf->Cfg_Srv;
    
    UdmServerInitFromRecord(Server, &SQLRes, i);

    sprintf(qbuf,"SELECT sname,sval FROM %s WHERE srv_id=%s%i%s", infoname, qu, Server->site_id, qu);
    if(UDM_OK != (rc= UdmSQLQuery(db, &SRes, qbuf)))
      return rc;
    jrows= UdmSQLNumRows(&SRes);
    for(j= 0; j < jrows; j++)
    {
      const char *sname = UdmSQLValue(&SRes, j, 0);
      const char *sval = UdmSQLValue(&SRes, j, 1);
      UdmVarListReplaceStr(&Server->Vars, sname, sval);
    }
    UdmSQLFree(&SRes);

    Server->follow= UdmVarListFindInt(&Server->Vars, "Follow", UDM_FOLLOW_DEFAULT);
    Server->method= UdmMethod(UdmVarListFindStr(&Server->Vars, "Method", UdmMethodStr(UDM_METHOD_DEFAULT)));
    Server->Match.match_type  = UdmVarListFindInt(&Server->Vars, "Match_type", UDM_MATCH_BEGIN);
    Server->Match.case_sense= UdmVarListFindInt(&Server->Vars, "Case_sense", UDM_CASE_INSENSITIVE);
    Server->Match.nomatch  = UdmVarListFindInt(&Server->Vars, "Nomatch", 0);
    Server->Match.arg  = (char *)UdmStrdup(UdmVarListFindStr(&Server->Vars, "Arg", "Disallow"));

    if (Server->command == 'S')
    {
      UdmServerAdd(Indexer, Server, 0);
      if((Server->Match.match_type==UDM_MATCH_BEGIN) &&
         (Indexer->flags & UDM_FLAG_ADD_SERVURL))
      {
        Href.url = Server->Match.pattern;
        Href.method=UDM_METHOD_GET;
        Href.site_id = Server->site_id;
        Href.server_id = Server->site_id;
        Href.hops= (uint4) UdmVarListFindInt(&Server->Vars, "StartHops", 0);
        UdmHrefListAdd(&Indexer->Conf->Hrefs, &Href);
      }
    }
    else
    {
      char errstr[128];
      rc= UdmMatchListAdd(Indexer, &Indexer->Conf->Filters,
                          &Server->Match, errstr, sizeof(errstr), Server->ordre);
      if (rc != UDM_OK)
      {
        udm_snprintf(db->errstr, sizeof(db->errstr),
                    "Error while loading ServerTable '%s' at row %d: %s",
                    name, i, errstr);
        break;
      }
    }
    UDM_FREE(Server->Match.pattern);
  }
  UdmSQLFree(&SQLRes);
  return rc;
}


static int
UdmServerTableFlush(UDM_DB *db)
{
  int rc;
  const char      *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";
  char str[128];
  
  udm_snprintf(str, sizeof(str),  "UPDATE server SET enabled=0 WHERE parent=%s0%s", qu, qu);
  rc = UdmSQLQuery(db, NULL, str);
  return rc;
}


static int
UdmServerTableUpdateSrvInfo(UDM_DB *db, UDM_SERVER *S, char *arg)
{
  int rc;
  size_t i;
  UDM_DSTR d;
  const char *E= (db->DBType == UDM_DB_PGSQL && db->version >= 80101) ? "E" :"";
  UdmDSTRInit(&d, 64);
  
  UDM_ASSERT(db->connected); /* make sure E is set to a correct value */
  
  UdmDSTRAppendf(&d, "DELETE FROM srvinfo WHERE srv_id=%i", S->site_id);
  if (UDM_OK != (rc = UdmSQLQuery(db, NULL, d.data)))
    goto ex;

  for (i= 0; i < S->Vars.nvars; i++)
  {
    UDM_VAR *Sec = &S->Vars.Var[i];
    if(Sec->val && Sec->name && 
       strcasecmp(Sec->name, "Category") &&
       strcasecmp(Sec->name, "Tag"))
    {
      UdmSQLEscStr(db, arg, Sec->val,strlen(Sec->val)); /* srvinfo */
      UdmDSTRReset(&d);
      UdmDSTRAppendf(&d, "INSERT INTO srvinfo (srv_id,sname,sval) "
                         "VALUES (%i,'%s',%s'%s')",
                     S->site_id, Sec->name, E, arg);
      if(UDM_OK != (rc = UdmSQLQuery(db, NULL, d.data)))
        goto ex;
    }
  }
ex:
  UdmDSTRFree(&d);
  return rc;
}


static int
UdmServerTableUpdateWithLock(UDM_DB *db, UDM_SERVER *S,
                             const char *buf, char *arg)
{
  int rc;
  if (UDM_OK != (rc= UdmSQLLockOrBegin(db, "server WRITE, srvinfo WRITE")) ||
      UDM_OK != (rc= UdmSQLQuery(db, NULL, buf)) ||
      UDM_OK != (rc= UdmServerTableUpdateSrvInfo(db, S, arg)) ||
      UDM_OK != (rc= UdmSQLUnlockOrCommit(db)))
    return rc;
  return UDM_OK;
}


static int
UdmServerTableAdd(UDM_SERVERLIST *S, UDM_DB *db)
{
  UDM_SQLRES  SQLRes;
  int    res = UDM_OK, found;
  const char  *alias=UdmVarListFindStr(&S->Server->Vars,"Alias",NULL);
  const char *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";
  size_t    i, len= 0;
  char    *buf, *arg;
  UDM_VARLIST  *Vars= &S->Server->Vars;
  UDM_SERVER Old;

  UdmServerInit(&Old);

  S->Server->site_id= UdmStrHash32(S->Server->Match.pattern);
  
  for (i=0; i < Vars->nvars; i++)
    len= udm_max(len, Vars->Var[i].curlen);
  
  len+= S->Server->Match.pattern ? strlen(S->Server->Match.pattern) : 0;
  len+= alias ? strlen(alias) : 0;
  len+= 2048;
  
  buf = (char*)UdmMalloc(len);
  arg = (char*)UdmMalloc(len);
  if (buf == NULL || arg == NULL)
  {
    UDM_FREE(buf);
    UDM_FREE(arg);
    strcpy(db->errstr, "Out of memory");
    db->errcode = 1;
    return UDM_ERROR;
  }

  for (found= 0; ; S->Server->site_id++)
  {
    udm_snprintf(buf, len, "SELECT " UDM_SERVER_TABLE_COLUMNS
                           "FROM server WHERE rec_id=%s%d%s",
                           qu, S->Server->site_id, qu);
    if (UDM_OK != (res= UdmSQLQuery(db, &SQLRes, buf)))
      goto ex;
    
    if (!UdmSQLNumRows(&SQLRes))
      break; /* Not found */
    
    UdmServerInitFromRecord(&Old, &SQLRes, 0);
    found= !strcasecmp(S->Server->Match.pattern, UdmSQLValue(&SQLRes, 0, 1));
    UdmSQLFree(&SQLRes);
    
    if (found)
      break;
  }

  if (S->Server->follow != UDM_FOLLOW_DEFAULT)
    UdmVarListReplaceInt(&S->Server->Vars, "Follow",  S->Server->follow);
  if (S->Server->method != UDM_METHOD_DEFAULT)
    UdmVarListReplaceStr(&S->Server->Vars, "Method",  UdmMethodStr(S->Server->method));
  if (S->Server->Match.match_type != UDM_MATCH_BEGIN)
    UdmVarListReplaceInt(&S->Server->Vars, "Match_type",  S->Server->Match.match_type);
  if (S->Server->Match.case_sense != UDM_CASE_INSENSITIVE)
    UdmVarListReplaceInt(&S->Server->Vars, "Case_sense",  S->Server->Match.case_sense);
  if (S->Server->Match.nomatch != 0)
    UdmVarListReplaceInt(&S->Server->Vars, "Nomatch",  S->Server->Match.nomatch);
  if (S->Server->command == 'F' && S->Server->Match.arg != NULL)
  {
    UdmVarListReplaceStr(&S->Server->Vars, "Arg", S->Server->Match.arg);
  }

  UdmSQLEscStr(db, arg,  /* Server pattern */
               UDM_NULL2EMPTY(S->Server->Match.pattern),
               strlen(UDM_NULL2EMPTY(S->Server->Match.pattern)));
  
  if (!found)
  {
    udm_snprintf(buf, len,
                 "INSERT INTO server (rec_id, enabled, tag, category,"
                 " command, parent, ordre, weight, url, pop_weight) "
                 " VALUES "
                 "(%s%d%s, 1, '%s', %s, '%c', %s%d%s, %d, %f, '%s', 0)",
         qu, S->Server->site_id, qu,
         UdmVarListFindStr(&S->Server->Vars, "Tag", ""),
         UdmVarListFindStr(&S->Server->Vars, "Category", "0"),
         S->Server->command,
         qu, S->Server->parent, qu,
         S->Server->ordre,
         S->Server->weight,
         arg
     );
    if (UDM_OK != (res= UdmServerTableUpdateWithLock(db, S->Server, buf, arg)))
      goto ex;
  }
  else
  {
    if (UdmServerNeedsUpdate(S->Server, &Old))
    {
      udm_snprintf(buf, len,
                   "UPDATE server SET enabled=1, tag='%s', category=%s,"
                   "command='%c', parent=%s%i%s, ordre=%d, weight=%f "
                   "WHERE rec_id=%s%d%s",
                    UdmVarListFindStr(&S->Server->Vars, "Tag", ""),
                    UdmVarListFindStr(&S->Server->Vars, "Category", "0"),
                    S->Server->command,
                    qu, S->Server->parent, qu,
                    S->Server->ordre,
                    S->Server->weight,
                    qu, S->Server->site_id, qu);
      if (UDM_OK != (res= UdmServerTableUpdateWithLock(db, S->Server, buf, arg)))
        goto ex;
    }
  }
  
  UDM_ASSERT(res == UDM_OK);

ex:
  UDM_FREE(buf);
  UDM_FREE(arg);
  UdmServerFree(&Old);
  return res;
}


static int
UdmServerTableGetId(UDM_AGENT *Indexer, UDM_SERVERLIST *S, UDM_DB *db)
{
  UDM_SQLRES  SQLRes;
  size_t len = ((S->Server->Match.pattern)?strlen(S->Server->Match.pattern):0) + 1024;
  char *buf = (char*)UdmMalloc(len);
  char *arg = (char*)UdmMalloc(len);
  int res, id = 0, i;
  const char      *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";
  

  if (buf == NULL)
  {
    UdmLog(Indexer, UDM_LOG_ERROR, "Out of memory");
    return UDM_ERROR;
  }
  if (arg == NULL)
  {
    UDM_FREE(buf);
    UdmLog(Indexer, UDM_LOG_ERROR, "Out of memory");
    return UDM_ERROR;
  }

  for (i = 0; i < UDM_SERVERID_CACHE_SIZE; i++)
  {
    if (Indexer->ServerIdCacheCommand[i] == S->Server->command)
      if (!strcmp(Indexer->ServerIdCache[i], S->Server->Match.pattern))
      {
        S->Server->site_id = id = Indexer->ServerIdCacheId[i];
        break;
      }
  }

  if (id == 0)
  {
    udmhash32_t     rec_id;
    int done = 1;
  
    udm_snprintf(buf, len, "SELECT rec_id FROM server WHERE command='%c' AND url='%s'",
           S->Server->command,
           UDM_NULL2EMPTY(S->Server->Match.pattern)
     );
    res = UdmSQLQuery(db, &SQLRes, buf);
    if ((res == UDM_OK) && UdmSQLNumRows(&SQLRes))
    {
      id = S->Server->site_id = UDM_ATOI(UdmSQLValue(&SQLRes, 0, 0));
      UDM_FREE(Indexer->ServerIdCache[Indexer->pServerIdCache]);
      Indexer->ServerIdCache[Indexer->pServerIdCache] = (char*)UdmStrdup(S->Server->Match.pattern);
      Indexer->ServerIdCacheCommand[Indexer->pServerIdCache] = S->Server->command;
      Indexer->ServerIdCacheId[Indexer->pServerIdCache] = id;
      Indexer->pServerIdCache = (Indexer->pServerIdCache + 1) % UDM_SERVERID_CACHE_SIZE;
      UDM_FREE(buf); UDM_FREE(arg);
      UdmSQLFree(&SQLRes);
      return UDM_OK;
    }
    UdmSQLFree(&SQLRes);
    rec_id = UdmStrHash32(S->Server->Match.pattern);
    while(done) 
    {
      udm_snprintf(buf, len, "SELECT rec_id, url FROM server WHERE rec_id=%s%i%s", qu, rec_id, qu);
      if (UDM_OK != (res = UdmSQLQuery(db, &SQLRes, buf)))
        return res;
      
      if (UdmSQLNumRows(&SQLRes) && (strcasecmp(S->Server->Match.pattern,UdmSQLValue(&SQLRes, 0, 1)) != 0))
      {
        rec_id++;
      } else done = 0;
      UdmSQLFree(&SQLRes);
    }
    udm_snprintf(buf, len, "SELECT enabled,tag,category,ordre FROM server WHERE rec_id=%s%i%s", qu, S->Server->parent, qu);
    res = UdmSQLQuery(db, &SQLRes, buf);
    if (res != UDM_OK)
    {
      UDM_FREE(buf); UDM_FREE(arg);
      UdmSQLFree(&SQLRes);
      return res;
    }

     UdmSQLEscStr(db, arg,  /* Server pattern */
                  UDM_NULL2EMPTY(S->Server->Match.pattern),
                  strlen(UDM_NULL2EMPTY(S->Server->Match.pattern)));

    udm_snprintf(buf, len, "\
INSERT INTO server (rec_id, enabled, tag, category, command, parent, ordre, weight, url) \
VALUES (%s%d%s, %d, '%s', %s, '%c', %s%d%s, %d, %f, '%s')\
",
           qu, rec_id, qu,
           UDM_ATOI(UdmSQLValue(&SQLRes, 0, 0)),
           UdmSQLValue(&SQLRes, 0, 1),
           UdmSQLValue(&SQLRes, 0, 2),
           S->Server->command,
           qu, S->Server->parent, qu,
           UDM_ATOI(UdmSQLValue(&SQLRes, 0, 3)),
           S->Server->weight,
           arg
     );
    res = UdmSQLQuery(db, NULL, buf);
    UdmSQLFree(&SQLRes);

    S->Server->site_id = id = rec_id;
    UDM_FREE(Indexer->ServerIdCache[Indexer->pServerIdCache]);
    Indexer->ServerIdCache[Indexer->pServerIdCache] = (char*)UdmStrdup(S->Server->Match.pattern);
    Indexer->ServerIdCacheCommand[Indexer->pServerIdCache] = S->Server->command;
    Indexer->ServerIdCacheId[Indexer->pServerIdCache] = id;
    Indexer->pServerIdCache = (Indexer->pServerIdCache + 1) % UDM_SERVERID_CACHE_SIZE;
  }
  UDM_FREE(buf); UDM_FREE(arg);
  return UDM_OK;
}


int
UdmSrvActionSQL(UDM_AGENT *A, UDM_SERVERLIST *S, int cmd,UDM_DB *db)
{
  switch(cmd){
    case UDM_SRV_ACTION_TABLE:
      return UdmLoadServerTable(A,S,db);
    case UDM_SRV_ACTION_FLUSH:
      return UdmServerTableFlush(db);
    case UDM_SRV_ACTION_ADD:
      return UdmServerTableAdd(S, db);
    case UDM_SRV_ACTION_ID:
      return UdmServerTableGetId(A, S, db);
    case UDM_SRV_ACTION_POPRANK:
      return UdmPopRankCalculate(A, db);
    default:
      UdmLog(A, UDM_LOG_ERROR, "Unsupported Srv Action SQL");
      return UDM_ERROR;
  }
}


/********** Searching for URL_ID by various parameters ****************/

int
UdmFindURL(UDM_AGENT *Indexer, UDM_DOCUMENT * Doc, UDM_DB *db)
{
  UDM_SQLRES  SQLRes;
  const char  *url=UdmVarListFindStr(&Doc->Sections,"URL","");
  udmhash32_t  id = 0;
  int    use_crc32_url_id;
  int    rc=UDM_OK;
  
  use_crc32_url_id = !strcasecmp(UdmVarListFindStr(&Indexer->Conf->Vars, "UseCRC32URLId", "no"), "yes");

  if(use_crc32_url_id)
  {
    /* Auto generation of rec_id */
    /* using CRC32 algorythm     */
    id = UdmStrHash32(url);
  }else{
    const char *o;
    char *qbuf, *e_url;
    size_t i, l, url_length= strlen(url);
    
    /* Escape URL string */
    if ((e_url = (char*)UdmMalloc(l = (8 * url_length + 1))) == NULL ||
        (qbuf = (char*)UdmMalloc( l + 100 )) == NULL)
    {
      UDM_FREE(e_url);
      UdmLog(Indexer, UDM_LOG_ERROR, "Out of memory");
      return UDM_ERROR;
    }
    UdmSQLEscStr(db,e_url,url,url_length);


    for(i = 0; i < UDM_FINDURL_CACHE_SIZE; i++)
    {
      if (Indexer->UdmFindURLCache[i])
        if (!strcmp(e_url, Indexer->UdmFindURLCache[i]))
        {
          id = Indexer->UdmFindURLCacheId[i];
          break;
        }
    }

    if (id == 0)
    {
      udm_snprintf(qbuf, l + 100, "SELECT rec_id FROM url WHERE url='%s'",e_url);
      if(UDM_OK!=(rc=UdmSQLQuery(db,&SQLRes,qbuf)))
      {
        UDM_FREE(e_url);
        UDM_FREE(qbuf);
        return rc;
      }
      for(i=0;i<UdmSQLNumRows(&SQLRes);i++)
      {
        if((o=UdmSQLValue(&SQLRes,i,0)))
        {
          id=atoi(o);
          break;
        }
      }
      UdmSQLFree(&SQLRes);
      UDM_FREE(Indexer->UdmFindURLCache[Indexer->pURLCache]);
      Indexer->UdmFindURLCache[Indexer->pURLCache] = (char*)UdmStrdup(e_url);
      Indexer->UdmFindURLCacheId[Indexer->pURLCache] = id;
      Indexer->pURLCache = (Indexer->pURLCache + 1) % UDM_FINDURL_CACHE_SIZE;
    }
    UDM_FREE(e_url);
    UDM_FREE(qbuf);
  }
  UdmVarListReplaceInt(&Doc->Sections, "ID", id);
  return  rc;
}


static int
UdmFindMessage(UDM_AGENT *Indexer, UDM_DOCUMENT * Doc, UDM_DB *db)
{
  size_t     i, len;
  char     *qbuf;
  char     *eid;
  UDM_SQLRES  SQLRes;
  const char  *message_id=UdmVarListFindStr(&Doc->Sections,"Header.Message-ID",NULL);
  int    rc;
  
  if(!message_id)
    return UDM_OK;
  
  len = strlen(message_id);
  eid = (char*)UdmMalloc(4 * len + 1);
  if (eid == NULL) return UDM_ERROR;
  qbuf = (char*)UdmMalloc(4 * len + 128);
  if (qbuf == NULL)
  { 
    UDM_FREE(eid);
    return UDM_ERROR;
  }

  /* Escape URL string */
  UdmSQLEscStr(db, eid, message_id, len); /* Message ID */
  
  udm_snprintf(qbuf, 4 * len + 128, 
     "SELECT rec_id FROM url u, urlinfo i WHERE u.rec_id=i.url_id AND i.sname='Message-ID' AND i.sval='%s'", eid);
  rc = UdmSQLQuery(db,&SQLRes,qbuf);
  UDM_FREE(qbuf);
  UDM_FREE(eid);
  if (UDM_OK != rc)
    return rc;
  
  for(i=0;i<UdmSQLNumRows(&SQLRes);i++)
  {
    const char * o;
    if((o=UdmSQLValue(&SQLRes,i,0)))
    {
      UdmVarListReplaceInt(&Doc->Sections,"ID", UDM_ATOI(o));
      break;
    }
  }
  UdmSQLFree(&SQLRes);
  return(UDM_OK);
}


/********************* Limits ********************/


static int
cmp_data_urls(UDM_URLDATA *d1, UDM_URLDATA *d2)
{
  if (d1->url_id > d2->url_id) return 1;
  if (d1->url_id < d2->url_id) return -1;
  return 0;
}


int
UdmLoadSlowLimit(UDM_AGENT *A, UDM_DB *db, UDM_URLID_LIST *list, const char *q)
{
  udm_timer_t ticks= UdmStartTimer();
  size_t i;
  int rc;
  UDM_SQLRES SQLRes;
  int exclude= list->exclude;
  bzero((void*) list, sizeof(UDM_URLID_LIST));
  list->exclude= exclude;
  if (UDM_OK != (rc= UdmSQLQuery(db, &SQLRes, q)))
    goto ret;

  if (!(list->nurls= UdmSQLNumRows(&SQLRes)))
    goto sqlfree;

  if (!(list->urls= UdmMalloc(sizeof(urlid_t) * list->nurls)))
  {
    rc= UDM_ERROR;
    list->nurls= 0;
    goto ret;
  }
  for (i= 0; i < list->nurls; i++)
  {
    list->urls[i]= atoi(UdmSQLValue(&SQLRes, i, 0));
  }
  
sqlfree:
  UdmLog(A, UDM_LOG_DEBUG, "Limit query retured %d rows: %.2f",
         list->nurls, UdmStopTimer(&ticks));
  UdmSQLFree(&SQLRes);
ret:
  return rc;
}


static int
UdmLoadSlowLimitWithSort(UDM_AGENT *A, UDM_DB *db,
                         UDM_URLID_LIST *list, const char *q)
{
  int rc= UdmLoadSlowLimit(A, db, list, q);
  if (rc == UDM_OK)
    rc= UdmURLIdListSort(list);
  return rc;
}


static int
UdmSlowLimitLoadForConv(UDM_AGENT *A,
                        UDM_DB *db,
                        UDM_URLID_LIST *fl_urls,
                        const char *fl)
{
  int rc= UDM_OK; 
  udm_timer_t ticks= UdmStartTimer();
  char name[64];
  const char *q;

  bzero((void*) fl_urls, sizeof(*fl_urls));

  UdmLog(A, UDM_LOG_INFO, "Loading fast limit '%s'", fl);
  if ((fl_urls->exclude= (fl[0] == '-')))
    fl++;

  udm_snprintf(name, sizeof(name), "Limit.%s", fl);
  if (!(q= UdmVarListFindStr(&A->Conf->Vars, name, NULL)))
  {
    UdmLog(A, UDM_LOG_ERROR, "Limit '%s' not specified", fl);
    return UDM_ERROR;
  }

  if (UDM_OK != (rc= UdmLoadSlowLimitWithSort(A, db, fl_urls, q)))
    return rc;
  UdmLog(A, UDM_LOG_DEBUG, "Limit '%s' loaded%s, %d records, %.2f sec",
         fl, fl_urls->exclude ? " type=excluding" : "", fl_urls->nurls,
         UdmStopTimer(&ticks));
  return rc;
}



/******************** Orders ********************************/

/*
  Apply a sorted UserOrder to UDM_URLDATALIST
*/
static int
UdmApplyFastOrderToURLDataList(UDM_URLDATALIST *Data,
                               UDM_URL_INT4_LIST *Order)
{
  UDM_URLDATA *d= Data->Item;
  UDM_URLDATA *de= Data->Item + Data->nitems;

  if (!Order->nitems)
    return UDM_OK;
  
  for ( ; d < de; d++)
  {
    UDM_URL_INT4 *found;
    if ((found= (UDM_URL_INT4*) UdmBSearch(&d->url_id,
                                           Order->Item,
                                           Order->nitems,
                                           sizeof(UDM_URL_INT4),
                                           (udm_qsort_cmp)UdmCmpURLID)))
    {
      char buf[64];
      sprintf(buf, "%08X", found->param);
      d->section= UdmStrdup(buf);
    }
    else
    {
      d->section= UdmStrdup("00000001");
    }
  }
  return UDM_OK;
}


static int
UdmFastOrderLoadAndApplyToURLDataList(UDM_AGENT *Agent,
                                      UDM_DB *db,
                                      UDM_URLDATALIST *Data,
                                      const char *name,
                                      size_t *norder)
{
  UDM_URL_INT4_LIST Order;
  int rc;
  
  if ((UDM_OK != (rc= UdmBlobLoadFastOrder(Agent, db, &Order, name))) ||
      !Order.nitems)
    goto ret;

  rc= UdmApplyFastOrderToURLDataList(Data, &Order);
  
ret:
  *norder= Order.nitems;
  UDM_FREE(Order.Item);
  return rc;
}



/******************** URLData *******************************/


static int
UdmLoadURLDataFromURLForConv(UDM_AGENT *A,
                             UDM_DB *db,
                             UDM_URLDATALIST *C,
                             UDM_URLID_LIST *fl_urls)
{
  int rc;
  udm_timer_t ticks= UdmStartTimer();
  char qbuf[4*1024];
  const char *url= (db->from && db->from[0]) ? "url." : "";
  size_t nbytes, i, j;
  UDM_SQLRES  SQLres;

  bzero((void*)C, sizeof(*C));

  UdmLog(A, UDM_LOG_INFO, "Loading URL list");
  
  udm_snprintf(qbuf, sizeof(qbuf),
              "SELECT %srec_id, site_id, pop_rank, last_mod_time"
              " FROM url%s%s%s",
              url, db->from, db->where[0] ? " WHERE " : "", db->where);

  if (UDM_OK != (rc = UdmSQLQuery(db, &SQLres, qbuf)))
    goto fin;
  
  C->nitems= UdmSQLNumRows(&SQLres);
  nbytes= C->nitems * sizeof(UDM_URLDATA);
  C->Item= (UDM_URLDATA*) UdmMalloc(nbytes);

  for (i= 0, j= 0; i < C->nitems; i++)
  {
    urlid_t url_id= UDM_ATOI(UdmSQLValue(&SQLres, i, 0));
    if (fl_urls->nurls)
    {
      void *found= UdmBSearch(&url_id, fl_urls->urls, fl_urls->nurls,
                              sizeof(urlid_t), (udm_qsort_cmp)UdmCmpURLID);
      if (found && fl_urls->exclude)
        continue;
      if (!found && !fl_urls->exclude)
        continue;
    }
    C->Item[j].url_id= url_id;
    C->Item[j].score= 0;
    C->Item[j].per_site= 0;
    C->Item[j].site_id= UDM_ATOI(UdmSQLValue(&SQLres, i, 1));
    C->Item[j].pop_rank= UDM_ATOF(UdmSQLValue(&SQLres, i, 2));
    C->Item[j].last_mod_time= UDM_ATOI(UdmSQLValue(&SQLres, i, 3));
    C->Item[j].url= NULL;
    C->Item[j].section= NULL;
    j++;
  }
  C->nitems= j;
  UdmSQLFree(&SQLres);

  if (C->nitems)
    UdmSort(C->Item, C->nitems, sizeof(UDM_URLDATA), (udm_qsort_cmp) cmp_data_urls);

fin:
  UdmLog(A, UDM_LOG_DEBUG,
         "URL list loaded: %d urls, %.2f sec", C->nitems, UdmStopTimer(&ticks));
  return rc;
}


static int
UdmLoadURLDataFromURLAndSlowLimitForConv(UDM_AGENT *A,
                                         UDM_DB *db,
                                         UDM_URLDATALIST *C)
{
  int rc= UDM_OK;
  const char *fl= UdmVarListFindStr(&A->Conf->Vars, "fl", NULL);
  UDM_URLID_LIST fl_urls;

  bzero((void*)&fl_urls, sizeof(fl_urls));
  bzero((void*)C, sizeof(*C));
  
  if (fl && (UDM_OK != (rc= UdmSlowLimitLoadForConv(A, db, &fl_urls, fl))))
    return rc;

  rc= UdmLoadURLDataFromURLForConv(A, db, C, &fl_urls);
  
  UDM_FREE(fl_urls.urls);

  return rc;
}


/*
  Load the section with the given name from the table "urlinfo",
  for sorting by section: "s=S&su=section".
*/
static int
UdmLoadURLDataFromURLInfoUsingIN(UDM_AGENT *A,
                                 UDM_URLDATALIST *DataList,
                                 UDM_DB *db,
                                 size_t num_best_rows,
                                 const char *esu)
{
  int rc= UDM_OK;
  size_t offs;
  char qbuf[4*1024];

  for (offs= 0; offs < num_best_rows; offs+= 256)
  {  
    size_t nrows, s, i;
    int notfirst= 0;
    UDM_SQLRES SQLres;
    char *end= qbuf + sprintf(qbuf, "SELECT url_id, sval"
                              " FROM urlinfo"
                              " WHERE sname='%s' AND url_id IN (", esu);
    
    for (i= 0; (i < 256) && (offs + i < DataList->nitems); i++)
    {
      end+= sprintf(end, "%s%i", (notfirst) ? "," : "",
                    DataList->Item[offs + i].url_id);
      notfirst= 1;
    }
    end+= sprintf(end, ") ORDER BY url_id");
  
    if (UDM_OK != (rc= UdmSQLQuery(db, &SQLres, qbuf)))
      goto fin;
  
    nrows= UdmSQLNumRows(&SQLres);

    for(i= 0, s= i + offs; i < nrows; s++)
    {
      if (s == DataList->nitems)
        break;
      if (DataList->Item[s].url_id != (urlid_t) UDM_ATOI(UdmSQLValue(&SQLres, i, 0)))
      {
        DataList->Item[s].section= UdmStrdup("");
      }
      else
      {
        DataList->Item[s].section= UdmStrdup(UdmSQLValue(&SQLres, i, 1));
        i++;
      }
    }
    UdmSQLFree(&SQLres);
  }

fin:
  return rc;
}


/*
  Load URL data from "url" for sorting by:
  site_id
  pop_rank
  last_mod_time
  url
  section
*/
static int
UdmLoadURLDataFromURLUsingIN(UDM_AGENT *A,
                             UDM_URLDATALIST *DataList,
                             UDM_DB *db,
                             size_t num_best_rows,
                             int flag)
{
  int need_url= (flag & UDM_URLDATA_URL);
  int rc= UDM_OK;
  char qbuf[4*1024];
  UDM_SQLRES SQLres;
  UDM_PSTR row[5];
  const char *su= UdmVarListFindStr(&A->Conf->Vars, "su", NULL);
  char *esu=su ? UdmSQLEscStrSimple(db, NULL, su, strlen(su)) : NULL; /* User sort name */
  size_t j;
  const char *hi_priority= db->DBType == UDM_DB_MYSQL ? "HIGH_PRIORITY " : " ";

  for (j= 0; j < num_best_rows; j+= 256)
  {
    size_t i;
    int notfirst = 0;
    udm_snprintf(qbuf, sizeof(qbuf),
            "SELECT %srec_id, site_id, pop_rank, last_mod_time%s"
            " FROM url"
            " WHERE rec_id IN (",
            hi_priority,
            need_url ? ",url" : "");
    for (i= 0; (i < 256) && (j + i < num_best_rows); i++)
    {
      sprintf(UDM_STREND(qbuf), "%s%i", (notfirst) ? "," : "", DataList->Item[j + i].url_id);
      notfirst= 1;
    }
    sprintf(UDM_STREND(qbuf), ") ORDER BY rec_id");
    if (UDM_OK != (rc= UdmSQLExecDirect(db, &SQLres, qbuf)))
      goto fin;
    for (i= 0; db->sql->SQLFetchRow(db, &SQLres, row) == UDM_OK; i++)
    {
      UDM_URLDATA *D= &DataList->Item[i + j];
      if (D->url_id != (urlid_t) UDM_ATOI(row[0].val))
      {
        UdmLog(A, UDM_LOG_ERROR, "Dat url_id (%d) != SQL url_id (%d)", 
               D->url_id, UDM_ATOI(row[0].val));
      }
      D->site_id= UDM_ATOI(row[1].val);
      D->pop_rank= UDM_ATOF(row[2].val);
      D->last_mod_time= UDM_ATOI(row[3].val);
      D->url= need_url ? UdmStrdup(row[4].val) : NULL;
      D->section= NULL;
    }
    UdmSQLFree(&SQLres);

  }

  if ((flag & UDM_URLDATA_SU) && su && su[0])
  {
    if (UDM_OK != (rc= UdmLoadURLDataFromURLInfoUsingIN(A, DataList, db,
                                                        num_best_rows, esu)))
      goto fin;
  }

fin:
  UDM_FREE(esu);
  return rc;
}


static int
UdmLoadURLDataFromURLUsingLoop(UDM_AGENT *A,
                               UDM_URLDATALIST *DataList,
                               UDM_DB *db,
                               size_t num_best_rows, int flag)
{
  int rc= UDM_OK;
  char qbuf[256];
  size_t i;
  int need_url= (flag & UDM_URLDATA_URL);
  const char *hi_priority= db->DBType == UDM_DB_MYSQL ? "HIGH_PRIORITY" : "";

  for (i = 0; i < num_best_rows; i++)
  {
    UDM_SQLRES SQLres;
    UDM_URLDATA *D= &DataList->Item[i];
    udm_snprintf(qbuf, sizeof(qbuf),
                 "SELECT %s site_id, pop_rank, last_mod_time%s"
                 " FROM url WHERE rec_id=%i",
                 hi_priority,
                 need_url ? ",url" : "",
                 DataList->Item[i].url_id);
    if (UDM_OK != (rc = UdmSQLQuery(db, &SQLres, qbuf)))
      goto fin;
    if(UdmSQLNumRows(&SQLres))
    {
      D->url_id= DataList->Item[i].url_id;
      D->site_id= UDM_ATOI(UdmSQLValue(&SQLres, 0, 0));
      D->pop_rank= UDM_ATOF(UdmSQLValue(&SQLres, 0, 1));
      D->last_mod_time= UDM_ATOI(UdmSQLValue(&SQLres, 0, 2));
      D->url= need_url ? UdmStrdup(UdmSQLValue(&SQLres, 0, 3)) : NULL;
      D->section= NULL;
    }
    UdmSQLFree(&SQLres);
  }

fin:
  return rc;
}


int
UdmLoadURLDataFromURL(UDM_AGENT *A,
                      UDM_URLDATALIST *DataList,
                      UDM_DB *db,
                      size_t num_best_rows, int flag)
{
  int rc= UDM_OK;
  int group_by_site= (flag & UDM_URLDATA_SITE);
  int use_urlbasicinfo= UdmVarListFindBool(&A->Conf->Vars, "LoadURLBasicInfo", 1);

  /* Check that DataList is not empty and is sorted by url_id */
  UDM_ASSERT(num_best_rows && (DataList->Item[0].url_id <= DataList->Item[num_best_rows-1].url_id));
  
  if (!use_urlbasicinfo)
  {
    UdmLog(A,UDM_LOG_DEBUG,"Not using basic URL data from url");
    rc= UdmURLDataListClearParams(DataList, num_best_rows);
  }
  else if (db->DBSQL_IN)
  {
    UdmLog(A,UDM_LOG_DEBUG,"Trying to load URL data from url");
    rc= UdmLoadURLDataFromURLUsingIN(A, DataList, db, num_best_rows, flag);
  }
  else
  {
    UdmLog(A,UDM_LOG_DEBUG,"Trying to load URL data from url, not using IN");
    rc= UdmLoadURLDataFromURLUsingLoop(A, DataList, db, num_best_rows, flag);
  }

  if (rc == UDM_OK && group_by_site)
    rc= UdmURLDataListGroupBySiteUsingSort(A, DataList, db);
  return rc;
}



/****************************** User score **************************/ 

int
UdmUserScoreListLoad(UDM_AGENT *A, UDM_DB *db,
                     UDM_URL_INT4_LIST *List, const char *q)
{
  size_t i;
  int rc;
  UDM_SQLRES SQLRes;
  udm_timer_t ticks= UdmStartTimer();
  
  bzero((void*) List, sizeof(UDM_URL_INT4_LIST));

  if (UDM_OK != (rc= UdmSQLQuery(db, &SQLRes, q)))
    goto ret;

  if (!(List->nitems= UdmSQLNumRows(&SQLRes)))
    goto sqlfree;

  if (2 != UdmSQLNumCols(&SQLRes))
  {
    udm_snprintf(db->errstr, sizeof(db->errstr),
                 "User Score query must return 2 columns, returned %d columns",
                 UdmSQLNumCols(&SQLRes));
    rc= UDM_ERROR;
    db->errcode= 1;
    goto sqlfree;
  }

  if (!(List->Item= UdmMalloc(sizeof(UDM_URL_INT4) * List->nitems)))
  {
    rc= UDM_ERROR;
    List->nitems= 0;
    goto sqlfree;
  }
  for (i= 0; i < List->nitems; i++)
  {
    List->Item[i].url_id= atoi(UdmSQLValue(&SQLRes, i, 0));
    List->Item[i].param= atoi(UdmSQLValue(&SQLRes, i, 1));
  }
  UdmSort(List->Item, List->nitems, sizeof(UDM_URL_INT4), (udm_qsort_cmp)UdmCmpURLID);

  UdmLog(A, UDM_LOG_DEBUG,
         "UserScore query returned %d columns, %d rows: %.2f",
         UdmSQLNumCols(&SQLRes), List->nitems, UdmStopTimer(&ticks));
  

sqlfree:
  UdmSQLFree(&SQLRes);
ret:
  return rc;
}


static int
UdmUserScoreListLoadAndApplyToURLScoreList(UDM_AGENT *Agent,
                                           UDM_URLSCORELIST *List,
                                           UDM_DB *db)
{
  char name[128];
  const char *us, *query;
  UDM_URL_INT4_LIST UserScoreList;
  int rc;
  int UserScoreFactor= UdmVarListFindInt(&Agent->Conf->Vars, "UserScoreFactor", 0);
  udm_timer_t ticks= UdmStartTimer();
  
  if (!UserScoreFactor ||
      !(us= UdmVarListFindStr(&Agent->Conf->Vars, "us", NULL)))
    return UDM_OK;
  
  UdmLog(Agent, UDM_LOG_DEBUG, "Start loading UserScore '%s'", us);
  
  udm_snprintf(name, sizeof(name), "Score.%s", us);
  query= UdmVarListFindStr(&Agent->Conf->Vars, name, NULL);

  if (UDM_OK != (rc= query ?
                 UdmUserScoreListLoad(Agent, db, &UserScoreList, query) :
                 UdmBlobLoadFastScore(Agent, db, &UserScoreList, us)) ||
      !UserScoreList.nitems)
    goto ret;

  rc= UdmUserScoreListApplyToURLScoreList(Agent, List,
                                          &UserScoreList, UserScoreFactor);

ret:  
  UdmLog(Agent, UDM_LOG_DEBUG, "%-30s%.2f (%d docs found)",
         "Stop  loading UserScore",  UdmStopTimer(&ticks), UserScoreList.nitems);
  UDM_FREE(UserScoreList.Item);
  return rc;
}


int
UdmUserSiteScoreListLoadAndApplyToURLDataList(UDM_AGENT *Agent,
                                              UDM_URLDATALIST *List,
                                              UDM_DB *db)
{
  char name[128];
  const char *us, *query;
  UDM_URL_INT4_LIST UserScoreList;
  int rc;
  int UserScoreFactor= UdmVarListFindInt(&Agent->Conf->Vars, "UserScoreFactor", 0);

  if (!UserScoreFactor ||
      !(us= UdmVarListFindStr(&Agent->Conf->Vars, "ss", NULL)))
    return UDM_OK;
  udm_snprintf(name, sizeof(name), "SiteScore.%s", us);
  if (!(query= UdmVarListFindStr(&Agent->Conf->Vars, name, NULL)))
    return UDM_OK;
  
  if ((UDM_OK != (rc= UdmUserScoreListLoad(Agent, db,
                                           &UserScoreList, query))) ||
      !UserScoreList.nitems)
    goto ret;

  rc= UdmUserScoreListApplyToURLDataList(Agent, List,
                                         &UserScoreList, UserScoreFactor);

ret:
  UDM_FREE(UserScoreList.Item);
  return rc;
}



/*********************** Creating fast index ******************/

/*
  Write limits with COMMIT and timestamp
*/
int
UdmBlobWriteLimits(UDM_AGENT *A, UDM_DB *db, const char *table, int use_deflate)
{
  int rc;
  if (UDM_OK != (rc= UdmSQLBegin(db)) ||
      UDM_OK != (rc= UdmBlobWriteLimitsInternal(A, db, table, use_deflate)) ||
      UDM_OK != (rc= UdmBlobWriteTimestamp(A, db, table)) ||
      UDM_OK != (rc= UdmSQLCommit(db)))
    return rc;
  return UDM_OK;
}


int UdmConvert2BlobSQL(UDM_AGENT *Indexer, UDM_DB *db)
{
  int rc;
  UDM_URLDATALIST List;
  
  UdmSQLBuildWhereCondition(Indexer->Conf, db);
  
  if (UDM_OK != (rc= UdmLoadURLDataFromURLAndSlowLimitForConv(Indexer, db, &List)))
    return rc;
  
  UDM_ASSERT(db->dbmode_handler->ToBlob != NULL);
  rc= db->dbmode_handler->ToBlob(Indexer, db, &List);
  
  UdmFree(List.Item);
  return rc;
}


static int
UdmWordListAddPairs(UDM_DOCUMENT *Doc)
{
  size_t i, nwords, maxdelta= 1;
  UDM_WORDLIST *L= &Doc->Words;
  for (i= 0, nwords= L->nwords; i < nwords; i++)
  {
    size_t j;
    for (j= i; j < nwords; j++)
    {
      UDM_WORD *W1= &L->Word[i]; /* Must be inside the loop - realloc */
      UDM_WORD *W2= &L->Word[j];
      size_t delta;
      /*
        We probably need word-to-itself pairs,
        to be able to search for phrases like "x x".
        
        TODO: MaxWordLen*2 limit.
      */
      if (W1->pos < W2->pos &&  W1->secno == W2->secno)
      {
        if ((delta= W2->pos - W1->pos) <= maxdelta)
        {
          char str[256];
          size_t secno= W1->secno; /* Realloc */
          udm_snprintf(str, sizeof(str), "%s-%s", W1->word, W2->word);
          UdmWordListAdd(Doc, str, secno);
        }
        else if (delta > 64)
          break;
      }
    }
  }
  return UDM_OK;
}


static int
UdmStoreWords(UDM_AGENT *Indexer, UDM_DB *db, UDM_DOCUMENT *Doc)
{
  UDM_ASSERT(db->dbmode_handler->StoreWords != NULL);
  if (UdmVarListFindBool(&Indexer->Conf->Vars, "StorePairs", 0))
    UdmWordListAddPairs(Doc);
  return db->dbmode_handler->StoreWords(Indexer, db, Doc);
}


/********************* Inserting URLs and Links ***************************/

static int
InsertLink(UDM_AGENT *A, UDM_DB *db, urlid_t from, urlid_t to)
{
  char qbuf[128];
  const char *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";
  if (from == to) /* Don't collect self links */
    return UDM_OK;
  udm_snprintf(qbuf, sizeof(qbuf),
               "INSERT INTO links (ot,k,weight) VALUES (%s%i%s,%s%d%s,0.0)",
               qu, from, qu, qu, to, qu);
  return UdmSQLQuery(db, NULL, qbuf);
}


static int
UdmAddURL(UDM_AGENT *Indexer,UDM_DOCUMENT * Doc,UDM_DB *db)
{
  char    *e_url, *qbuf;
  const char  *url;
  int    url_seed;
  int    use_crc32_url_id;
  int    usehtdburlid;
  int    rc=UDM_OK;
  size_t          len;
  const char      *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";
  const char *sql_export= UdmVarListFindStr(&Doc->Sections, "SQLExportHref", NULL);
  urlid_t rec_id = 0;

  if (sql_export)
  {
    char *part, *lt, *sql_export_copy= UdmStrdup(sql_export);
    UDM_DSTR d;
    UDM_VARLIST Vars;
    UdmVarListInit(&Vars);
    UdmDSTRInit(&d,256);
    
    UdmVarListSQLEscape(&Vars, &Doc->Sections, db);
    for (part= udm_strtok_r(sql_export_copy, ";", &lt) ;
         part ;
         part= udm_strtok_r(NULL, ";", &lt))
    {
      UdmDSTRParse(&d, part, &Vars);
      if(UDM_OK!= (rc= UdmSQLQuery(db, NULL, d.data)))
        return rc;
      UdmDSTRReset(&d);
    }
    UdmVarListFree(&Vars);
    UdmDSTRFree(&d);
    UdmFree(sql_export_copy);
  }
  url = UdmVarListFindStr(&Doc->Sections,"URL","");
  use_crc32_url_id = !strcasecmp(UdmVarListFindStr(&Indexer->Conf->Vars, "UseCRC32URLId", "no"), "yes");
  usehtdburlid = UdmVarListFindInt(&Indexer->Conf->Vars, "UseHTDBURLId", 0);

  len = strlen(url);
  e_url = (char*)UdmMalloc(4 * len + 1);
  if (e_url == NULL) return UDM_ERROR;
  qbuf = (char*)UdmMalloc(4 * len + 512);
  if (qbuf == NULL)
  { 
    UDM_FREE(e_url);
    return UDM_ERROR;
  }
  
  url_seed = UdmStrHash32(url) & 0xFF;
  
  /* Escape URL string */
  UdmSQLEscStr(db, e_url, url, len);
  
  if(use_crc32_url_id || usehtdburlid)
  {
    /* Auto generation of rec_id */
    /* using CRC32 algorythm     */
    if (use_crc32_url_id) rec_id = UdmStrHash32(url);
    else rec_id = UdmVarListFindInt(&Doc->Sections, "HTDB_URL_ID", 0);
    
    udm_snprintf(qbuf, 4 * len + 512, "INSERT INTO url (rec_id,url,referrer,hops,crc32,next_index_time,status,seed,bad_since_time,site_id,server_id,docsize,last_mod_time,shows,pop_rank) VALUES (%s%i%s,'%s',%s%i%s,%d,0,%d,0,%d,%d,%s%i%s,%s%i%s,%s%i%s,%li,0,0.0)",
           qu, rec_id, qu,
           e_url,
           qu, UdmVarListFindInt(&Doc->Sections,"Referrer-ID",0), qu,
           UdmVarListFindInt(&Doc->Sections,"Hops",0),
           (int)time(NULL),
           url_seed, (int)time(NULL),
           qu, UdmVarListFindInt(&Doc->Sections, "Site_id", 0), qu,
           qu, UdmVarListFindInt(&Doc->Sections, "Server_id", 0), qu,
           qu, UdmVarListFindInt(&Doc->Sections, "Content-Length", 0), qu,
           UdmHttpDate2Time_t(UdmVarListFindStr(&Doc->Sections, "Last-Modified",
                              UdmVarListFindStr(&Doc->Sections, "Date", "")))
       );
  }else{
    /* Use dabatase generated rec_id */
    /* It depends on used DBType     */
    switch(db->DBType)
    {
    case UDM_DB_SOLID:
    case UDM_DB_ORACLE8:
    case UDM_DB_SAPDB:
      /* FIXME: Dirty hack for stupid too smart databases 
       Change this for config parameter checking */
/*      if (strlen(e_url)>UDM_URLSIZE)e_url[UDM_URLSIZE]=0;*/
      /* Use sequence next_url_id.nextval */
      udm_snprintf(qbuf, 4 * len + 512, "INSERT INTO url (url,referrer,hops,rec_id,crc32,next_index_time,status,seed,bad_since_time,site_id,server_id) VALUES ('%s',%i,%d,next_url_id.nextval,0,%d,0,%d,%d,%i,%i)",
        e_url,
        UdmVarListFindInt(&Doc->Sections,"Referrer-ID",0),
        UdmVarListFindInt(&Doc->Sections,"Hops",0),
        (int)time(NULL),
         url_seed, (int)time(NULL),
         UdmVarListFindInt(&Doc->Sections, "Site_id", 0),
         UdmVarListFindInt(&Doc->Sections, "Server_id", 0)
         );
      break;
    case UDM_DB_MIMER:
      udm_snprintf(qbuf, 4 * len + 512, "INSERT INTO url (url,referrer,hops,rec_id,crc32,next_index_time,status,seed,bad_since_time,site_id,server_id) VALUES ('%s',%i,%d,NEXT_VALUE OF rec_id_GEN,0,%d,0,%d,%d,%i,%i)",
        e_url,
        UdmVarListFindInt(&Doc->Sections,"Referrer-ID",0),
        UdmVarListFindInt(&Doc->Sections,"Hops",0),
        (int)time(NULL),
         url_seed, (int)time(NULL),
         UdmVarListFindInt(&Doc->Sections, "Site_id", 0),
         UdmVarListFindInt(&Doc->Sections, "Server_id", 0)
         );
      break;    
    case UDM_DB_IBASE:
      udm_snprintf(qbuf, 4 * len + 512, "INSERT INTO url (url,referrer,hops,rec_id,crc32,next_index_time,status,seed,bad_since_time,site_id,server_id) VALUES ('%s',%i,%d,GEN_ID(rec_id_GEN,1),0,%d,0,%d,%d,%i,%i)",
        e_url,
        UdmVarListFindInt(&Doc->Sections,"Referrer-ID",0),
        UdmVarListFindInt(&Doc->Sections,"Hops",0),
        (int)time(NULL),
         url_seed, (int)time(NULL),
         UdmVarListFindInt(&Doc->Sections, "Site_id", 0),
         UdmVarListFindInt(&Doc->Sections, "Server_id", 0)
         );
      break;
    case UDM_DB_MYSQL:
      /* MySQL generates itself */
    default:  
      udm_snprintf(qbuf, 4 * len + 512, "INSERT INTO url (url,referrer,hops,crc32,next_index_time,status,seed,bad_since_time,site_id,server_id,docsize,last_mod_time,shows,pop_rank) VALUES ('%s',%s%i%s,%d,0,%d,0,%d,%d,%s%i%s,%s%i%s,%s%i%s,%li,0,0.0)",
             e_url,
             qu, UdmVarListFindInt(&Doc->Sections,"Referrer-ID",0), qu,
             UdmVarListFindInt(&Doc->Sections,"Hops",0),
             (int)time(NULL),
             url_seed, (int)time(NULL),
             qu, UdmVarListFindInt(&Doc->Sections, "Site_id", 0), qu,
             qu, UdmVarListFindInt(&Doc->Sections, "Server_id", 0), qu,
             qu, UdmVarListFindInt(&Doc->Sections, "Content-Length", 0), qu,
             UdmHttpDate2Time_t(UdmVarListFindStr(&Doc->Sections, "Last-Modified",
             UdmVarListFindStr(&Doc->Sections, "Date", "")))
         );
    }
  }

  /* Exec INSERT now */
  if(UDM_OK!=(rc=UdmSQLQuery(db, NULL, qbuf)))
    goto ex;

ex:

  UDM_FREE(qbuf);
  UDM_FREE(e_url);
  return rc;
}


static int
UdmAddLink(UDM_AGENT *Indexer, UDM_DOCUMENT * Doc, UDM_DB *db)
{
  char    *e_url, *qbuf;
  UDM_SQLRES  SQLRes;
  const char  *url;
  int    use_crc32_url_id;
  int    rc=UDM_OK;
  size_t          len;
  urlid_t rec_id = 0;

  url = UdmVarListFindStr(&Doc->Sections,"URL","");
  use_crc32_url_id = !strcasecmp(UdmVarListFindStr(&Indexer->Conf->Vars, "UseCRC32URLId", "no"), "yes");

  len = strlen(url);
  e_url = (char*)UdmMalloc(4 * len + 1);
  if (e_url == NULL) return UDM_ERROR;
  qbuf = (char*)UdmMalloc(4 * len + 512);
  if (qbuf == NULL) 
  { 
    UDM_FREE(e_url); 
    return UDM_ERROR;
  }
  
  if (use_crc32_url_id)
  {
    rec_id = UdmStrHash32(url);
  }
  else
  {
    /* Escape URL string */
    UdmSQLEscStr(db, e_url, url, len);
  
    udm_snprintf(qbuf, 4 * len + 512, "SELECT rec_id FROM url WHERE url='%s'", e_url);
    if(UDM_OK != (rc = UdmSQLQuery(db, &SQLRes, qbuf))) 
      goto ex;
    if (UdmSQLNumRows(&SQLRes))
    {
      rec_id = UDM_ATOI(UdmSQLValue(&SQLRes, 0, 0));
    }
    UdmSQLFree(&SQLRes);
  }

  if (rec_id)
  {
    urlid_t from= UdmVarListFindInt(&Doc->Sections, "Referrer-ID", 0);
    UdmVarListReplaceInt(&Doc->Sections, "ID", rec_id);
    if(UDM_OK != (rc = InsertLink(Indexer, db, from, rec_id)))
      goto ex;
  }
  else
  {
    UdmLog(Indexer, UDM_LOG_ERROR, "URL not found: %s", e_url);
  }

ex:

  UDM_FREE(qbuf);
  UDM_FREE(e_url);
  return UDM_OK;
}


/******************* Cached Copy *********************/

static void
SQLResToDoc(UDM_ENV *Conf, UDM_DOCUMENT *D, UDM_SQLRES *sqlres, size_t i)
{
  time_t    last_mod_time;
  char    dbuf[128];
  const char  *format = UdmVarListFindStr(&Conf->Vars, "DateFormat", "%a, %d %b %Y, %X %Z");
  double          pr;
  
  UdmVarListReplaceStr(&D->Sections,"URL",UdmSQLValue(sqlres,i,1));
  UdmVarListReplaceInt(&D->Sections, "URL_ID", UdmStrHash32(UdmSQLValue(sqlres,i,1)));
  last_mod_time=atol(UdmSQLValue(sqlres,i,2));
  UdmVarListReplaceInt(&D->Sections, "Last-Modified-Timestamp", (int) last_mod_time);
  if (strftime(dbuf, 128, format, localtime(&last_mod_time)) == 0)
  {
    UdmTime_t2HttpStr(last_mod_time, dbuf);
  }
  UdmVarListReplaceStr(&D->Sections,"Last-Modified",dbuf);
  UdmVarListReplaceStr(&D->Sections,"Content-Length",UdmSQLValue(sqlres,i,3));
  pr= atof(UdmSQLValue(sqlres,i,3)) / 1024;
  sprintf(dbuf, "%.2f", pr);
  UdmVarListReplaceStr(&D->Sections,"Content-Length-K",dbuf);  
  last_mod_time=atol(UdmSQLValue(sqlres,i,4));
  if (strftime(dbuf, 128, format, localtime(&last_mod_time)) == 0)
  {
    UdmTime_t2HttpStr(last_mod_time, dbuf);
  }
  UdmVarListReplaceStr(&D->Sections,"Next-Index-Time",dbuf);
  UdmVarListReplaceInt(&D->Sections, "Referrer-ID", UDM_ATOI(UdmSQLValue(sqlres,i,5)));
  UdmVarListReplaceInt(&D->Sections,"crc32",atoi(UdmSQLValue(sqlres,i,6)));
  UdmVarListReplaceStr(&D->Sections, "Site_id", UdmSQLValue(sqlres, i, 7));

#if BAR_COMMA_PERIOD_ORACLE_PROBLEM
  {
	char *num= UdmSQLValue(sqlres, i, 8);
	char *comma= strchr(num, ',');
	if (comma)
	  *comma= '.';
  }
#endif

  pr = atof(UdmSQLValue(sqlres, i, 8));
  snprintf(dbuf, 128, "%.5f", pr);
  UdmVarListReplaceStr(&D->Sections, "Pop_Rank", dbuf);
}


static int
UdmGetURLInfoOneDoc(UDM_AGENT *Indexer, UDM_DB *db,
                    UDM_DOCUMENT *Doc, int unpack_cached_copy)
{
  int rc;
  char buf[64];
  size_t i;
  UDM_SQLRES SQLRes;
  int CachedCopy_found= 0;
  
  udm_snprintf(buf, sizeof(buf), "SELECT sname, sval FROM urlinfo WHERE url_id=%d", UDM_ATOI(UdmVarListFindStr(&Doc->Sections, "ID", "0")));
  if (UDM_OK != (rc= UdmSQLQuery(db, &SQLRes, buf)))
    return rc;

  for (i= 0; i < UdmSQLNumRows(&SQLRes); i++)
  {
    const char *sname= UdmSQLValue(&SQLRes, i, 0);
    const char *sval= UdmSQLValue(&SQLRes, i, 1);
    size_t l= UdmSQLLen(&SQLRes, i, 1);
    
    if (!sname)
      continue;
    if (!sval)
      sval = "";

    if (!strcmp(sname, "CachedCopy")) 
    {
      if (unpack_cached_copy)
      {
        if (Doc->Buf.content)
          continue;

        UdmVarListReplaceStr(&Doc->Sections, "CachedCopyBase64", sval);
        Doc->Buf.buf= UdmMalloc(UDM_MAXDOCSIZE);
        if (UDM_OK != UdmCachedCopyUnpack(Doc, sval, l))
          return UDM_ERROR;
        CachedCopy_found= 1;
      }
      else
      {
        UdmVarListReplaceStr(&Doc->Sections, "CachedCopyBase64", sval);
      }
    }
    else
    {
      UdmVarListReplaceStr(&Doc->Sections, sname, sval);
    }
  }
  UdmSQLFree(&SQLRes);
  
  if (unpack_cached_copy && !CachedCopy_found)
  {
    const char *url= UdmVarListFindStr(&Doc->Sections, "url", NULL);
    UDM_LOCK_CHECK_OWNER(Indexer, UDM_LOCK_CONF);
    UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);
    UdmGetURLSimple(Indexer, Doc, url);
    UDM_GETLOCK(Indexer, UDM_LOCK_CONF);
  }
  
  return UDM_OK;
}


static int
UdmGetCachedCopy(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc, UDM_DB *db)
{
#ifdef HAVE_ZLIB
  UDM_SQLRES SQLRes;
  char buf[1024];
  int rc;

  UdmFindURL(Indexer, Doc, db);
  udm_snprintf(buf, sizeof(buf), "SELECT rec_id,url,last_mod_time,docsize,next_index_time,referrer,crc32,site_id,pop_rank FROM url WHERE rec_id=%d", UDM_ATOI(UdmVarListFindStr(&Doc->Sections, "ID", "0")));
  if (UDM_OK != (rc= UdmSQLQuery(db, &SQLRes, buf)))
    return rc;
  
  if (! UdmSQLNumRows(&SQLRes)) return(UDM_ERROR);
  SQLResToDoc(Indexer->Conf, Doc, &SQLRes, 0);
  UdmSQLFree(&SQLRes);

  if (UDM_OK != (rc= UdmGetURLInfoOneDoc(Indexer, db, Doc, 1)))
    return rc;
 
  return(UDM_OK);
#else
  return(UDM_ERROR);
#endif
}


/********************** Reindexing "indexer -a" *************************/

static int
UdmMarkForReindex(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc, UDM_DB *db)
{
  char    qbuf[1024];
  const char  *where;
  UDM_SQLRES   SQLRes;
  size_t          i, j;
  int             rc;
  const char      *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";
  UDM_DSTR buf;

  UDM_LOCK_CHECK_OWNER(Indexer, UDM_LOCK_CONF);
  where = UdmSQLBuildWhereCondition(Indexer->Conf, db);
  
  if (db->flags & UDM_SQL_HAVE_SUBSELECT &&
      db->DBType != UDM_DB_MYSQL)
  {
    udm_snprintf(qbuf,sizeof(qbuf),"UPDATE url SET next_index_time=%d WHERE rec_id IN (SELECT url.rec_id FROM url%s %s %s)",
       (int)time(NULL), db->from, (where[0]) ? "WHERE" : "", where);
    return UdmSQLQuery(db,NULL,qbuf);
  }

  udm_snprintf(qbuf, sizeof(qbuf), "SELECT url.rec_id FROM url%s %s %s", db->from, (where[0]) ? "WHERE" : "", where);
  if(UDM_OK != (rc = UdmSQLQuery(db, &SQLRes, qbuf))) return rc;

  UdmDSTRInit(&buf, 4096);
  if (db->DBSQL_IN) 
  {
    for (i = 0; i < UdmSQLNumRows(&SQLRes); i += 512) 
    {
      UdmDSTRReset(&buf);
      UdmDSTRAppendf(&buf, "UPDATE url SET next_index_time=%d WHERE rec_id IN (", (int)time(NULL));
      for (j = 0; (j < 512) && (i + j < UdmSQLNumRows(&SQLRes)); j++) 
      {
        UdmDSTRAppendf(&buf, "%s%s%s%s", (j) ? "," : "", qu, UdmSQLValue(&SQLRes, i + j, 0), qu);
      }
      UdmDSTRAppendf(&buf, ")");
      if(UDM_OK != (rc = UdmSQLQuery(db, NULL, buf.data))) 
      {
        UdmSQLFree(&SQLRes);
	UdmDSTRFree(&buf);
        return rc;
      }
    }
  } else {
    for (i = 0; i < UdmSQLNumRows(&SQLRes); i++) 
    {
      UdmDSTRReset(&buf);
      UdmDSTRAppendf(&buf, "UPDATE url SET next_index_time=%d WHERE rec_id=%s", (int)time(NULL),  UdmSQLValue(&SQLRes, i, 0));
      if(UDM_OK != (rc = UdmSQLQuery(db, NULL, buf.data))) 
      {
        UdmSQLFree(&SQLRes);
	UdmDSTRFree(&buf);
        return rc;
      }
    }
  }
  UdmDSTRFree(&buf);
  UdmSQLFree(&SQLRes);
  return UDM_OK;
}


/************** Child - for new extensions ****************/

static int
UdmRegisterChild(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc,UDM_DB *db)
{
  char  qbuf[1024];
  urlid_t  url_id = UdmVarListFindInt(&Doc->Sections,"ID",0);
  urlid_t  parent_id = UdmVarListFindInt(&Doc->Sections,"Parent-ID",0);
  const char      *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";
  
  udm_snprintf(qbuf,sizeof(qbuf),"insert into links (ot,k,weight) values(%s%i%s,%s%i%s,0.0)", qu, parent_id, qu, qu, url_id, qu);
  return UdmSQLQuery(db,NULL,qbuf);
}


/*********************** Update URL ***********************/

static int
UdmUpdateUrl(UDM_AGENT *Indexer,UDM_DOCUMENT *Doc,UDM_DB *db)
{
  char qbuf[256];
  urlid_t  url_id = UdmVarListFindInt(&Doc->Sections, "ID", 0);
  int  status=UdmVarListFindInt(&Doc->Sections,"Status",0);
  int  prevStatus = UdmVarListFindInt(&Doc->Sections, "PrevStatus", 0);
  int  next_index_time=UdmHttpDate2Time_t(UdmVarListFindStr(&Doc->Sections,"Next-Index-Time",""));
  int  res;
  const char      *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";
  
  if (prevStatus != status && status > 300 && status != 304)
    sprintf(qbuf, "UPDATE url SET status=%d,next_index_time=%d,bad_since_time=%d,site_id=%s%i%s,server_id=%s%i%s WHERE rec_id=%s%i%s",
            status, next_index_time, (int)time(NULL), qu, UdmVarListFindInt(&Doc->Sections, "Site_id", 0), qu,
           qu, UdmVarListFindInt(&Doc->Sections, "Server_id",0), qu, qu, url_id, qu);
  else
    sprintf(qbuf,"UPDATE url SET status=%d,next_index_time=%d, site_id=%s%i%s,server_id=%s%i%s WHERE rec_id=%s%i%s",
            status, next_index_time, qu, UdmVarListFindInt(&Doc->Sections, "Site_id", 0), qu,
            qu, UdmVarListFindInt(&Doc->Sections, "Server_id",0), qu, qu, url_id, qu);

  if(UDM_OK!=(res=UdmSQLQuery(db,NULL,qbuf)))return res;
  
  /* remove all old broken hrefs from this document to avoid broken link collecting */
  return UdmDeleteBadHrefs(Indexer,Doc,db,url_id);
}

static int
UdmUpdateUrlWithLangAndCharset(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc,UDM_DB *db)
{
  char  *qbuf;
  int  rc;
  const char  *charset;
  UDM_VAR    *var;
  int    status, prevStatus;
  urlid_t         url_id;
  size_t    i, len = 0;
  char    qsmall[64];
  const char      *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";
  int IndexTime= UdmVarListFindInt(&Indexer->Conf->Vars, "IndexTime", 0);
  
  status = UdmVarListFindInt(&Doc->Sections, "Status", 0);
  prevStatus = UdmVarListFindInt(&Doc->Sections, "PrevStatus", 0);
  url_id = UdmVarListFindInt(&Doc->Sections, "ID", 0);
  
  if((var=UdmVarListFind(&Doc->Sections,"Content-Language")))
  {
    if (var->val == NULL)
    {
      var->val = (char*)UdmStrdup(UdmVarListFindStr(&Doc->Sections, "DefaultLang", "en"));
    }
    len=strlen(var->val);
    for(i = 0; i < len; i++)
    {
      var->val[i] = tolower(var->val[i]);
    }
  }
  
  charset = UdmVarListFindStr(&Doc->Sections, "Charset", 
            UdmVarListFindStr(&Doc->Sections, "RemoteCharset", "iso-8859-1"));
  charset = UdmCharsetCanonicalName(charset);
  UdmVarListReplaceStr(&Doc->Sections, "Charset", charset);
  
  if (prevStatus != status && status > 300 && status != 304)
    udm_snprintf(qsmall, 64, ", bad_since_time=%d", (int)time(NULL));
  else qsmall[0] = '\0';

  if (IndexTime)
  {
    if (! prevStatus) udm_snprintf(UDM_STREND(qsmall), 64, ",last_mod_time=%li", time(NULL));
  }
  else
  {
    const char *lmsrc= UdmVarListFindStr(&Doc->Sections, "User.Date",
                       UdmVarListFindStr(&Doc->Sections, "Last-Modified",
                       UdmVarListFindStr(&Doc->Sections, "Date", "")));
    udm_snprintf(UDM_STREND(qsmall), 64, ",last_mod_time=%li", UdmHttpDate2Time_t(lmsrc));
  }
  qbuf=(char*)UdmMalloc(1024);
  
  
  udm_snprintf(qbuf, 1023, "\
UPDATE url SET \
status=%d,\
next_index_time=%li,\
docsize=%d,\
crc32=%d%s, site_id=%s%i%s, server_id=%s%i%s \
WHERE rec_id=%s%i%s",
  status,
  UdmHttpDate2Time_t(UdmVarListFindStr(&Doc->Sections,"Next-Index-Time","")),
  UdmVarListFindInt(&Doc->Sections,"Content-Length",0),
  UdmVarListFindInt(&Doc->Sections,"crc32",0),
  qsmall,
  qu, UdmVarListFindInt(&Doc->Sections,"Site_id",0), qu,
  qu, UdmVarListFindInt(&Doc->Sections, "Server_id",0), qu,
  qu, url_id, qu);
  
  rc=UdmSQLQuery(db,NULL,qbuf);
  UDM_FREE(qbuf);
  return rc;
}


static int
UdmDocInsertSectionsUsingBind(UDM_DB *db, UDM_DOCUMENT *Doc)
{
  int rc= UDM_OK;
  size_t i;
  char qbuf[256];
  urlid_t    url_id = UdmVarListFindInt(&Doc->Sections, "ID", 0);

  UDM_ASSERT(db->sql->SQLPrepare);

  udm_snprintf(qbuf, sizeof(qbuf),
               "INSERT INTO urlinfo (url_id,sname,sval) "
               "VALUES(%s, %s, %s)",
                UdmSQLParamPlaceHolder(db, 1),
                UdmSQLParamPlaceHolder(db, 2),
                UdmSQLParamPlaceHolder(db, 3));

  if (UDM_OK != (rc= UdmSQLPrepare(db, qbuf)))
    return rc;
  
  for(i= 0; i< Doc->Sections.nvars; i++)
  {
    UDM_VAR *Sec=&Doc->Sections.Var[i];
    if (Sec->val && Sec->name &&
        ((Sec->curlen && Sec->maxlen) || (!strcmp(Sec->name, "Z"))))
    {
      int bindtype= UdmSQLLongVarCharBindType(db);
      UDM_ASSERT(sizeof(url_id) == 4);
      if (UDM_OK != (rc= UdmSQLBindParameter(db, 1,
                                             &url_id, (int) sizeof(url_id),
                                             UDM_SQLTYPE_INT32)) ||
          UDM_OK != (rc= UdmSQLBindParameter(db, 2,
                                             Sec->name, (int) strlen(Sec->name),
                                             UDM_SQLTYPE_VARCHAR)) ||
          UDM_OK != (rc= UdmSQLBindParameter(db, 3,
                                             Sec->val, (int) strlen(Sec->val),
                                             bindtype)) ||
          UDM_OK != (rc= UdmSQLExecute(db)))
        return rc;
    }
  }

  return UdmSQLStmtFree(db);
}


static int
UdmDocInsertSectionsUsingEscapeBuildQuery(UDM_DB *db,
                                          size_t url_id,
                                          const char *name,
                                          const char *val,
                                          size_t vallen,
                                          UDM_DSTR *qbuf)
{
  const char *E= (db->DBDriver == UDM_DB_PGSQL && db->version >= 80101) ? "E" : "";
  char *arg;
  size_t newlen, esclen;
  UdmDSTRReset(qbuf);
  UdmDSTRAppendf(qbuf, "INSERT INTO urlinfo (url_id,sname,sval) VALUES(");
  if (url_id)
    UdmDSTRAppendf(qbuf, "%d,", url_id);
  else
    UdmDSTRAppendSTR(qbuf, "last_insert_id(),");
  UdmDSTRAppendf(qbuf, "'%s',", name);
  UdmDSTRAppendf(qbuf, "%s'", E);
  
  newlen= qbuf->size_data + vallen * (db->DBType == UDM_DB_PGSQL ? 4 : 2);
  UdmDSTRRealloc(qbuf, newlen);
  arg= qbuf->data + qbuf->size_data;
  esclen= UdmSQLEscStr(db, arg, val, vallen);
  qbuf->size_data+= esclen;
  UdmDSTRAppend(qbuf, "')", 2);
  return UDM_OK;
}


static int
UdmDocInsertSectionsUsingEscape(UDM_DB *db, UDM_DOCUMENT *Doc,
                                urlid_t url_id)
{
  int rc= UDM_OK;
  size_t i, len, esc_multiply = (db->DBType == UDM_DB_PGSQL) ? 4 : 2;
  UDM_DSTR qbuf;

  /* Calculate maximum arg length */
  for(len= 0, i= 0; i < Doc->Sections.nvars; i++)
  {
    size_t l= Doc->Sections.Var[i].curlen +
              (Doc->Sections.Var[i].name ?
               strlen(Doc->Sections.Var[i].name) : 0);
    if (len < l)
      len= l;
  }
  if (!len)
    return UDM_OK;

  UdmDSTRInit(&qbuf, 256);
  UdmDSTRAlloc(&qbuf, esc_multiply * len + 128);
  
  for(i= 0; i< Doc->Sections.nvars; i++)
  {
    UDM_VAR *Sec=&Doc->Sections.Var[i];
    if (Sec->val && Sec->name &&
        ((Sec->curlen && Sec->maxlen) || (!strcmp(Sec->name, "Z"))))
    {
      UdmDocInsertSectionsUsingEscapeBuildQuery(db, url_id, Sec->name,
                                                Sec->val, strlen(Sec->val),
                                                &qbuf);
      if(UDM_OK != (rc= UdmSQLQuery(db, NULL, qbuf.data)))
        break;
    }
  }
  UdmDSTRFree(&qbuf);
  return rc;
}


static int
UdmLongUpdateURL(UDM_AGENT *Indexer,UDM_DOCUMENT *Doc,UDM_DB *db)
{
  int     rc= UDM_OK;
  urlid_t url_id = UdmVarListFindInt(&Doc->Sections, "ID", 0);
  const char *c;
  int use_crosswords= UdmVarListFindBool(&Indexer->Conf->Vars, "CrossWords", 0);
  int use_tnx=  (db->DBType != UDM_DB_MYSQL)    && /* OK */
                (db->DBType != UDM_DB_VIRT)     && /* TODO: check */
                (db->DBType != UDM_DB_ACCESS)   && /* TODO: check */
                (db->DBType != UDM_DB_DB2)      && /* TODO: check */
                (db->DBType != UDM_DB_CACHE);      /* TODO: check */
  
  /*
   TNX works fine: Sybase: ASE-15.0.2 Dev Edition + UnixODBC.
  */
  
  if (use_tnx && UDM_OK != (rc= UdmSQLBegin(db)))
    return rc;
  
  /* Now store words and crosswords */
  if(UDM_OK != (rc= UdmStoreWords(Indexer, db, Doc)))
    return rc;
  
  if(use_crosswords)
    if(UDM_OK != (rc= UdmStoreCrossWords(Indexer, Doc, db)))
      return rc;
  

  /* Copy default languages, if not given by server and not guessed */
  if (!(c= UdmVarListFindStr(&Doc->Sections,"Content-Language",NULL)))
  {
    if ((c= UdmVarListFindStr(&Doc->Sections,"DefaultLang",NULL)))
      UdmVarListReplaceStr(&Doc->Sections,"Content-Language",c);
  }
  

  if(UDM_OK != (rc= UdmUpdateUrlWithLangAndCharset(Indexer, Doc, db)))
    return rc;
  
  /* remove all old broken hrefs from this document to avoid broken link collecting */
  if (UDM_OK != (rc= UdmDeleteBadHrefs(Indexer,Doc,db,url_id)))
    return rc;
  
  /* Remove old URLInfo only if PrevStatus != 0 */
  if (UdmVarListFindInt(&Doc->Sections, "PrevStatus", 1))
  {
    char    qsmall[128];
    sprintf(qsmall,"DELETE FROM urlinfo WHERE url_id=%i", url_id);
    if (UDM_OK != (rc= UdmSQLQuery(db, NULL, qsmall)))
      return rc;
  }

/* No need delete from links here, it has been done before */
  
  if (!Doc->Sections.nvars)
    goto commit;



  if (db->flags & UDM_SQL_HAVE_BIND_TEXT)
  {
    rc= UdmDocInsertSectionsUsingBind(db, Doc);
  }
  else
  {
    rc= UdmDocInsertSectionsUsingEscape(db, Doc, url_id);
  }

commit:

  if(use_tnx && rc == UDM_OK)
    rc= UdmSQLCommit(db);

  return rc;
}


static int
UdmUpdateClone(UDM_AGENT *Indexer,UDM_DOCUMENT *Doc,UDM_DB *db)
{
  int rc;
  int use_crosswords= UdmVarListFindBool(&Indexer->Conf->Vars, "CrossWords", 0);

  if (UDM_OK != (rc= UdmDeleteWordFromURL(Indexer, Doc, db)))
    return rc;
  if(use_crosswords)
  {
    if (UDM_OK != (rc= UdmDeleteCrossWordFromURL(Indexer, Doc, db)))
      return rc;
  }
  rc= UdmUpdateUrlWithLangAndCharset(Indexer, Doc, db);
  return rc;
}



/************************ Clones stuff ***************************/
static int
UdmFindOrigin(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc,UDM_DB *db)
{
  size_t    i=0;
  char    qbuf[256]="";
  UDM_SQLRES  SQLRes;
  urlid_t    origin_id = 0;
  int    scrc32=UdmVarListFindInt(&Doc->Sections,"crc32",0);
  int    rc;
  
  if (scrc32==0)return UDM_OK;
  
  if (db->DBSQL_IN)
    sprintf(qbuf,"SELECT rec_id FROM url WHERE crc32=%d AND status IN (200,304,206)",scrc32);
  else
    sprintf(qbuf,"SELECT rec_id FROM url WHERE crc32=%d AND (status=200 OR status=304 OR status=206)",scrc32);
  
  if(UDM_OK!=(rc=UdmSQLQuery(db,&SQLRes,qbuf)))
    return rc;
  
  for(i=0;i<UdmSQLNumRows(&SQLRes);i++)
  {
    const char *o;
    if((o=UdmSQLValue(&SQLRes,i,0)))
      if((!origin_id) || (origin_id > UDM_ATOI(o)))
        origin_id = UDM_ATOI(o);
  }
  UdmSQLFree(&SQLRes);
  UdmVarListReplaceInt(&Doc->Sections, "Origin-ID", origin_id);
  return(UDM_OK);
}


int
UdmCloneListSQL(UDM_AGENT * Indexer, UDM_DOCUMENT *Doc, UDM_RESULT *Res, UDM_DB *db)
{
  size_t    i, nr, nadd;
  char    qbuf[256];
  UDM_SQLRES  SQLres;
  int    scrc32=UdmVarListFindInt(&Doc->Sections,"crc32",0);
  urlid_t    origin_id = UdmVarListFindInt(&Doc->Sections, "ID", 0);
  int    rc;
  const char      *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";
  const char  *format = UdmVarListFindStr(&Indexer->Conf->Vars, "DateFormat", "%a, %d %b %Y, %X %Z");

  if (Res->num_rows > 4) return UDM_OK;
  
  if (!scrc32)
    return UDM_OK;
  
  sprintf(qbuf,"SELECT rec_id,url,last_mod_time,docsize FROM url WHERE crc32=%d AND (status=200 OR status=304 OR status=206) AND rec_id<>%s%i%s", scrc32, qu, origin_id, qu);
  if(UDM_OK!=(rc=UdmSQLQuery(db,&SQLres,qbuf)))
    return UDM_OK;
  
  nr = UdmSQLNumRows(&SQLres);
  if( nr == 0)
  {
    UdmSQLFree(&SQLres);
    return UDM_OK;
  }
  nadd = 5 - Res->num_rows;
  if(nr < nadd) nadd = nr;
  
  Res->Doc = (UDM_DOCUMENT*)UdmRealloc(Res->Doc, (Res->num_rows + nadd) * sizeof(UDM_DOCUMENT));
  
  for(i = 0; i < nadd; i++)
  {
    time_t    last_mod_time;
    char    buf[128];
    UDM_DOCUMENT  *D = &Res->Doc[Res->num_rows + i];
    
    UdmDocInit(D);
    UdmVarListAddInt(&D->Sections, "ID", UDM_ATOI(UdmSQLValue(&SQLres,i,0)));
    UdmVarListAddStr(&D->Sections,"URL",UdmSQLValue(&SQLres,i,1));
    UdmVarListReplaceInt(&D->Sections, "URL_ID", UdmStrHash32(UdmSQLValue(&SQLres,i,1)));
    last_mod_time=atol(UdmSQLValue(&SQLres,i,2));
    if (strftime(buf, 128, format, localtime(&last_mod_time)) == 0)
    {
      UdmTime_t2HttpStr(last_mod_time, buf);
    }
    UdmVarListAddStr(&D->Sections,"Last-Modified",buf);
    UdmVarListAddInt(&D->Sections,"Content-Length",atoi(UdmSQLValue(&SQLres,i,3)));
    UdmVarListAddInt(&D->Sections,"crc32",scrc32);
    UdmVarListAddInt(&D->Sections, "Origin-ID", origin_id);
  }
  Res->num_rows += nadd;
  UdmSQLFree(&SQLres);
  return UDM_OK;
}


/************** Get Target to be indexed ***********************/



static void
UdmSQLTopInit(UDM_SQL_TOP_CLAUSE *Top)
{
  Top->rownum[0]= 0;
  Top->limit[0]= 0;
  Top->top[0]= 0;
}



void
UdmSQLTopClause(UDM_DB *db, size_t top_num, UDM_SQL_TOP_CLAUSE *Top)
{
  UdmSQLTopInit(Top);
  if(db->flags & UDM_SQL_HAVE_LIMIT)
  {
    udm_snprintf(Top->limit, UDM_SQL_TOP_BUF_SIZE, " LIMIT %d", top_num);
  }
  else if (db->flags & UDM_SQL_HAVE_TOP)
  {
    udm_snprintf(Top->top, UDM_SQL_TOP_BUF_SIZE, " TOP %d ", top_num);
  }
  else if (db->flags & UDM_SQL_HAVE_FIRST_SKIP)
  {
    udm_snprintf(Top->top, UDM_SQL_TOP_BUF_SIZE, " FIRST %d ", top_num);
  }
  else if (db->DBType == UDM_DB_ORACLE8)
  {
#if HAVE_ORACLE8
    if(db->DBDriver==UDM_DB_ORACLE8)
    {
      udm_snprintf(Top->rownum, UDM_SQL_TOP_BUF_SIZE, " AND ROWNUM<=%d", top_num); 
    }
#endif
    if(!Top->rownum[0])
      udm_snprintf(Top->rownum, UDM_SQL_TOP_BUF_SIZE, " AND ROWNUM<=%d", top_num); 
  }
}


static const  char select_url_str[]=
"url.url,url.rec_id,docsize,status,hops,crc32,last_mod_time,seed";

static const char select_url_str_for_dump[]=
"url.url,url.rec_id,docsize,status,hops,crc32,last_mod_time,seed,"
"next_index_time,bad_since_time,server_id,site_id"
;

/*
  The columns that are dumped:
  - status
  - docsize
  - last_mod_time
  - hops
  - crc32
  - seed
  - url
  - next_index_time
  - bad_since_time
  - site_id
  - server_id
  
  The columns that don't need to be dumped for restore purposes:
  - rec_id
  - shows
  - sop_rank
  - referrer
*/
static int
UdmTargetSQLResDump(UDM_AGENT *Indexer, UDM_DB *db,
                    UDM_DOCUMENT *Doc,
                    UDM_SQLRES *SQLRes, size_t rownum,
                    UDM_DSTR *eurl)
{
  int seed= UDM_ATOI(UdmSQLValue(SQLRes, rownum, 7));
  UdmVarListAddInt(&Doc->Sections, "ID", UDM_ATOI(UdmSQLValue(SQLRes,rownum,1)));
  printf("--seed=%d\n", seed);
  printf("INSERT INTO url ");
  printf("(url,docsize,status,hops,crc32,last_mod_time,seed,next_index_time,bad_since_time,server_id,site_id) VALUES (");
  if (UDM_OK != UdmSQLEscDSTR(db, eurl,
                              UdmSQLValue(SQLRes, rownum, 0),
                              UdmSQLLen(SQLRes, rownum, 0)))
    return UDM_ERROR;
  printf("'%s',", eurl->data);
  printf("%s,", UdmSQLValue(SQLRes, rownum, 2));
  printf("%s,", UdmSQLValue(SQLRes, rownum, 3));
  printf("%s,", UdmSQLValue(SQLRes, rownum, 4));
  printf("%s,", UdmSQLValue(SQLRes, rownum, 5));
  printf("%s,", UdmSQLValue(SQLRes, rownum, 6));
  printf("%s,", UdmSQLValue(SQLRes, rownum, 7));
  printf("%s,", UdmSQLValue(SQLRes, rownum, 8));
  printf("%s,", UdmSQLValue(SQLRes, rownum, 9));
  printf("%s,", UdmSQLValue(SQLRes, rownum, 10));
  printf("%s",  UdmSQLValue(SQLRes, rownum, 11));
  printf(");\n");
  return UDM_OK;
}


static void
UdmTargetSQLResToDoc(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc,
                     UDM_SQLRES *SQLRes, size_t i)
{
  char buf[64]= "";
  time_t last_mod_time;
  UdmVarListAddStr(&Doc->Sections,"URL",UdmSQLValue(SQLRes,i,0));
  UdmVarListAddInt(&Doc->Sections, "ID", UDM_ATOI(UdmSQLValue(SQLRes,i,1)));
  UdmVarListAddInt(&Doc->Sections,"Content-Length",atoi(UdmSQLValue(SQLRes,i,2)));
  UdmVarListAddInt(&Doc->Sections,"Status",atoi(UdmSQLValue(SQLRes,i,3)));
  UdmVarListAddInt(&Doc->Sections,"Hops",atoi(UdmSQLValue(SQLRes,i,4)));
  UdmVarListAddInt(&Doc->Sections,"crc32",atoi(UdmSQLValue(SQLRes,i,5)));
  last_mod_time= (time_t) atol(UdmSQLValue(SQLRes,i,6));
  UdmTime_t2HttpStr(last_mod_time, buf);
  if (last_mod_time != 0 && strlen(buf) > 0)
  {
    UdmVarListReplaceStr(&Doc->Sections, "Last-Modified", buf);
  }
}

/*
  Setting extending sections - only needed for targets
*/
static void
UdmTargetSQLResToDoc_Extra(UDM_AGENT *A, UDM_DOCUMENT *Doc,
                           UDM_SQLRES *SQLRes, size_t i)
{
  UdmVarListReplaceInt(&Doc->Sections, "URL_ID", UdmStrHash32(UdmSQLValue(SQLRes,i,0)));
  UdmVarListAddInt(&Doc->Sections,"PrevStatus",atoi(UdmSQLValue(SQLRes,i,3)));
}


int
UdmTargetsSQL(UDM_AGENT *Indexer, UDM_DB *db)
{
  char    sortstr[128]= "";
  char    updstr[64]="";
  char    tblhint[64]="";
  UDM_SQL_TOP_CLAUSE Top;
  size_t    i = 0, j, start, nrows, qbuflen;
  size_t    url_num;
  UDM_SQLRES   SQLRes;
  char    smallbuf[128];
  int    rc=UDM_OK;
  const char  *where;
  char    *qbuf=NULL;
  const char      *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";
  int skip_lock= UdmVarListFindBool(&Indexer->Conf->Vars, "URLSelectSkipLock", 0);

  url_num= UdmVarListFindInt(&Indexer->Conf->Vars, "URLSelectCacheSize", URL_SELECT_CACHE);
  if (Indexer->Conf->url_number < url_num)
    url_num= Indexer->Conf->url_number;
  where= UdmSQLBuildWhereCondition(Indexer->Conf, db);
  qbuflen= 1024 + 4 * strlen(where);
  
  if ((qbuf = (char*)UdmMalloc(qbuflen + 2)) == NULL)
  {
      UdmLog(Indexer, UDM_LOG_ERROR, "Out of memory");
      return UDM_ERROR;
  }

  if ((Indexer->flags & (UDM_FLAG_SORT_HOPS | UDM_FLAG_SORT_EXPIRED)) ||
      !(Indexer->flags & UDM_FLAG_DONTSORT_SEED))
  {    
    sprintf(sortstr, " ORDER BY %s%s%s", 
      (Indexer->flags & UDM_FLAG_SORT_HOPS) ? "hops" : "",
      (Indexer->flags & UDM_FLAG_DONTSORT_SEED) ? "" : ((Indexer->flags & UDM_FLAG_SORT_HOPS) ? ",seed" : "seed"),
      (Indexer->flags & UDM_FLAG_SORT_EXPIRED) ? 
      ( ((Indexer->flags & UDM_FLAG_SORT_HOPS) || !(Indexer->flags & UDM_FLAG_DONTSORT_SEED)  ) ? 
        ",next_index_time" : "next_index_time") : "");
  }

  UdmSQLTopClause(db, url_num, &Top);

  if(1)
  {
    switch(db->DBType)
    {
      case UDM_DB_MYSQL:
        udm_snprintf(qbuf, qbuflen,
                     "INSERT INTO udm_url_tmp "
                     "SELECT url.rec_id FROM url%s "
                     "WHERE next_index_time<=%d %s%s%s%s",
                    db->from,
                    (int)time(NULL), where[0] ? "AND " : "",  where,
                    sortstr, Top.limit);
        if (UDM_OK != (rc= UdmSQLDropTableIfExists(db, "udm_url_tmp")) ||
            UDM_OK != (rc= UdmSQLQuery(db, NULL, "CREATE TEMPORARY TABLE udm_url_tmp (rec_id int not null)")) ||
            (!skip_lock &&
             UDM_OK != (rc= UdmSQLQuery(db,NULL,"LOCK TABLES udm_url_tmp WRITE, url WRITE, urlinfo AS it WRITE, urlinfo AS il WRITE, server AS s WRITE, categories AS c WRITE"))) ||
            UDM_OK != (rc= UdmSQLQuery(db, NULL, qbuf)))
          return rc;
        break;
      case UDM_DB_PGSQL:
        rc=UdmSQLQuery(db,NULL,"BEGIN WORK");
        sprintf(updstr, " FOR UPDATE ");
/*        rc=UdmSQLQuery(db,NULL,"LOCK url");*/
        break;
      case UDM_DB_ORACLE8:
        sprintf(updstr, " FOR UPDATE ");
        break;
      case UDM_DB_MSSQL:
        strcpy(tblhint, " (TABLOCKX)");
        rc= UdmSQLBegin(db);
        break;
      case UDM_DB_SAPDB:
        sprintf(updstr, " WITH LOCK ");
        break;
      default:
        break;
    }
    if(rc!=UDM_OK)return rc;
  }
  
  db->res_limit=url_num;
  if (db->DBType == UDM_DB_MYSQL)
    udm_snprintf(qbuf, qbuflen, "SELECT %s FROM url, udm_url_tmp "
                                "WHERE url.rec_id=udm_url_tmp.rec_id",
                                select_url_str);
  else
    udm_snprintf(qbuf, qbuflen, "SELECT %s%s "
                                "FROM url%s%s "
                                "WHERE next_index_time<=%d %s%s%s"
                                "%s%s%s",
                 Top.top, select_url_str, tblhint, db->from,
                 (int)time(NULL), where[0] ? "AND " : "",  where, Top.rownum,
                 sortstr, updstr, Top.limit);
  
  if(UDM_OK != (rc= UdmSQLQuery(db,&SQLRes, qbuf)))
    goto commit;
  
  if(!(nrows = UdmSQLNumRows(&SQLRes)))
  {
    UdmSQLFree(&SQLRes);
    goto commit;
  }

  start = Indexer->Conf->Targets.num_rows;
  Indexer->Conf->Targets.num_rows += nrows;
  
  Indexer->Conf->Targets.Doc = 
    (UDM_DOCUMENT*)UdmRealloc(Indexer->Conf->Targets.Doc, sizeof(UDM_DOCUMENT)*(Indexer->Conf->Targets.num_rows + 1));
  if (Indexer->Conf->Targets.Doc == NULL)
  {
    UdmLog(Indexer, UDM_LOG_ERROR, "Out of memory at realloc %s[%d]", __FILE__, __LINE__);
    rc= UDM_ERROR;
    goto commit;
  }
  
  for(i = 0; i < nrows; i++)
  {
    UDM_DOCUMENT  *Doc = &Indexer->Conf->Targets.Doc[start + i];
    UdmDocInit(Doc);
    UdmTargetSQLResToDoc(Indexer, Doc, &SQLRes, i);
    UdmTargetSQLResToDoc_Extra(Indexer, Doc, &SQLRes, i);
  }
  UdmSQLFree(&SQLRes);
  
  
  if (db->DBSQL_IN)
  {
    char  *urlin=NULL;
    
    if ( (qbuf = (char*)UdmRealloc(qbuf, qbuflen = qbuflen + 35 * URL_SELECT_CACHE)) == NULL)
    {
      UDM_FREE(qbuf);
      UdmLog(Indexer, UDM_LOG_ERROR, "Out of memory");
      rc= UDM_ERROR;
      goto commit;
    }
    
    if ( (urlin = (char*)UdmMalloc(35 * URL_SELECT_CACHE)) == NULL)
    {
      UDM_FREE(qbuf);
      UdmLog(Indexer, UDM_LOG_ERROR, "Out of memory");
      rc = UDM_ERROR;
      goto commit;
    }
    urlin[0]=0;
    
    for(i = 0; i < nrows; i+= URL_SELECT_CACHE)
    {

      urlin[0] = 0;

      for (j = 0; (j < URL_SELECT_CACHE) && (i + j < nrows) ; j++)
      {

      UDM_DOCUMENT  *Doc = &Indexer->Conf->Targets.Doc[start + i + j];
      urlid_t    url_id = UdmVarListFindInt(&Doc->Sections, "ID", 0);
      
      if(urlin[0])strcat(urlin,",");
      sprintf(urlin+strlen(urlin), "%s%i%s", qu, url_id, qu);
      }
      udm_snprintf(qbuf, qbuflen, "UPDATE url SET next_index_time=%d WHERE rec_id in (%s)",
             (int)(time(NULL) + URL_LOCK_TIME), urlin);
      if (UDM_OK != (rc= UdmSQLQuery(db,NULL,qbuf)))
        goto commit;
    }
    UDM_FREE(urlin);
  }
  else
  {
    for(i = 0; i < nrows; i++)
    {
      UDM_DOCUMENT  *Doc = &Indexer->Conf->Targets.Doc[start + i];
      urlid_t    url_id = UdmVarListFindInt(&Doc->Sections, "ID", 0);
      
      udm_snprintf(smallbuf, 128, "UPDATE url SET next_index_time=%d WHERE rec_id=%i",
             (int)(time(NULL) + URL_LOCK_TIME), url_id);
      if(UDM_OK!=(rc=UdmSQLQuery(db,NULL,smallbuf)))
        goto commit;
    }
  }


commit:

  if (rc != UDM_OK)
  {
    UdmLog(Indexer, UDM_LOG_ERROR, "UdmTargetsSQL: DB error: %s", db->errstr);
  }
  if(1)
  {
    switch(db->DBType)
    {
      case UDM_DB_MYSQL:
        if (!skip_lock)
          rc= UdmSQLQuery(db,NULL,"UNLOCK TABLES");
        break;
      case UDM_DB_PGSQL:
        rc=UdmSQLQuery(db,NULL,"END WORK");
        break;
      case UDM_DB_MSSQL:
        rc= UdmSQLCommit(db);
      default:
        break;
    }
  }
  UDM_FREE(qbuf);
  return rc;
}



/******************* Truncate database ********************/

static int
UdmTruncateURL(UDM_AGENT *Indexer,UDM_DB *db)
{
  int  rc;
  
  rc= UdmSQLTableTruncateOrDelete(db, "url");
  if(rc!=UDM_OK)return rc;
  
  rc= UdmSQLTableTruncateOrDelete(db, "links");
  if(rc != UDM_OK) return rc;
  
  rc= UdmSQLTableTruncateOrDelete(db, "urlinfo");
  return rc;
}


static int
UdmTruncateDict(UDM_AGENT *Indexer,UDM_DB *db)
{
  UDM_ASSERT(db->dbmode_handler->Truncate != NULL);
  return db->dbmode_handler->Truncate(Indexer, db);
}


static int
UdmTruncateDB(UDM_AGENT *Indexer,UDM_DB *db)
{
  int rc, use_crosswords;
  UDM_GETLOCK(Indexer, UDM_LOCK_CONF);
  use_crosswords= UdmVarListFindBool(&Indexer->Conf->Vars, "CrossWords", 0);
  UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);

  if(use_crosswords)
  {
    if ((UDM_OK != (rc= UdmTruncateCrossDict(Indexer,db))))
      return rc;
  }
  if((UDM_OK != (rc= UdmTruncateDict(Indexer,db))) ||
     (UDM_OK != (rc= UdmTruncateURL(Indexer,db))))
    return rc ;
  return rc;
}


/******************* Clear database with condition ********/

static int
UdmDeleteWordsAndLinks(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc, UDM_DB *db)
{
  int  rc;
  int use_crosswords= UdmVarListFindBool(&Indexer->Conf->Vars, "CrossWords", 0);
  if (use_crosswords)
    if (UDM_OK!= (rc= UdmDeleteCrossWordFromURL(Indexer,Doc,db)))
      return rc;

  if (UDM_OK != (rc= UdmDeleteWordFromURL(Indexer,Doc,db)))
    return rc;

  if (Doc->Spider.collect_links)
  {
    if (UDM_OK != (rc= UdmDeleteLinks(Indexer, Doc, db)))
      return rc;
  }

  /* Set status, bad_since_time, etc */
  if (UDM_OK != (rc= UdmUpdateUrl(Indexer, Doc, db)))
    return rc;

  return rc;
}


static int
UdmDeleteWordFromURL(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc, UDM_DB *db)
{
  urlid_t url_id= UdmVarListFindInt(&Doc->Sections, "ID", 0);

  if (!UdmVarListFindInt(&Doc->Sections, "PrevStatus", 0))
    return UDM_OK;

  UDM_ASSERT(db->dbmode_handler->DeleteWordsFromURL != NULL);
  return db->dbmode_handler->DeleteWordsFromURL(Indexer, db, url_id);
}


static int
UdmDeleteBadHrefs(UDM_AGENT *Indexer,
                  UDM_DOCUMENT *Doc,
                  UDM_DB *db,
                  urlid_t url_id)
{
  UDM_DOCUMENT  rDoc;
  UDM_SQLRES  SQLRes;
  char    q[256];
  size_t    i;
  size_t    nrows;
  int    rc=UDM_OK;
  int    hold_period= UdmVarListFindInt(&Doc->Sections,"HoldBadHrefs",0);
  const char      *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";
  
  if (hold_period <= 0)
    return UDM_OK;
  
  udm_snprintf(q, sizeof(q), "SELECT rec_id FROM url WHERE status > 300 AND status<>304 AND referrer=%s%i%s AND bad_since_time<%d",
    qu, url_id, qu, (int)time(NULL) - hold_period);
  if(UDM_OK!=(rc=UdmSQLQuery(db,&SQLRes,q)))return rc;
  
  nrows = UdmSQLNumRows(&SQLRes);
  
  UdmDocInit(&rDoc);
  for(i = 0; i < nrows ; i++)
  {
    UdmVarListReplaceStr(&rDoc.Sections,"ID", UdmSQLValue(&SQLRes,i,0));
    if(UDM_OK!=(rc=UdmDeleteURL(Indexer, &rDoc, db)))
      break;
  }
  UdmDocFree(&rDoc);
  UdmSQLFree(&SQLRes);
  return rc;
}


static int
UdmDeleteURL(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc,UDM_DB *db)
{
  char  qbuf[128];
  int  rc;
  urlid_t  url_id  =UdmVarListFindInt(&Doc->Sections, "ID", 0);
  int  use_crosswords= UdmVarListFindBool(&Indexer->Conf->Vars, "CrossWords", 0);
  const char *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";

  if(use_crosswords)
    if(UDM_OK!=(rc=UdmDeleteCrossWordFromURL(Indexer,Doc,db)))return(rc);
  
  if(UDM_OK!=(rc=UdmDeleteWordFromURL(Indexer,Doc,db)))return(rc);
  
  sprintf(qbuf,"DELETE FROM url WHERE rec_id=%s%i%s", qu, url_id, qu);
  if(UDM_OK!=(rc=UdmSQLQuery(db,NULL,qbuf)))return rc;
  
  sprintf(qbuf,"DELETE FROM urlinfo WHERE url_id=%s%i%s", qu, url_id, qu);
  if(UDM_OK!=(rc=UdmSQLQuery(db,NULL,qbuf)))return rc;
  
  sprintf(qbuf,"DELETE FROM links WHERE ot=%s%i%s", qu, url_id, qu);
  if(UDM_OK!=(rc=UdmSQLQuery(db,NULL,qbuf)))return rc;
  
  sprintf(qbuf,"DELETE FROM links WHERE k=%s%i%s", qu, url_id, qu);
  if(UDM_OK!=(rc=UdmSQLQuery(db,NULL,qbuf)))return rc;
  
  /* remove all old broken hrefs from this document to avoid broken link collecting */
  if (UDM_OK != (rc=UdmDeleteBadHrefs(Indexer,Doc, db, url_id))) return rc;

  sprintf(qbuf,"UPDATE url SET referrer=%s0%s WHERE referrer=%s%i%s", qu, qu, qu, url_id, qu);
  return UdmSQLQuery(db,NULL,qbuf);
}


static int
UdmDeleteLinks(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc, UDM_DB *db)
{
  char  qbuf[128];
  urlid_t  url_id = UdmVarListFindInt(&Doc->Sections, "ID", 0);
  const char *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";

  sprintf(qbuf,"DELETE FROM links WHERE ot=%s%i%s", qu, url_id, qu);
  return UdmSQLQuery(db, NULL, qbuf);
}


static int
UdmClearDBUsingIN(UDM_AGENT *Indexer, UDM_DB *db, UDM_URLID_LIST *list)
{
  UDM_DSTR qbuf, urlin;
  int rc= UDM_OK; /* if list if empty */
  size_t part;
  size_t url_num = UdmVarListFindInt(&Indexer->Conf->Vars, "URLSelectCacheSize", URL_DELETE_CACHE);
  
  UdmDSTRInit(&qbuf, 4096);
  UdmDSTRInit(&urlin, 4096);
  
  for (part= 0; part < list->nurls; part+= url_num)
  {
    size_t offs;
    urlid_t *item= &list->urls[part];
    UdmDSTRReset(&urlin);
    for(offs= 0; (offs < url_num) && ((part + offs) < list->nurls); offs++)
    {
      if(offs) UdmDSTRAppend(&urlin,",", 1);
      UdmDSTRAppendf(&urlin, "%d", item[offs]);
    }
    
    if(UDM_OK != (rc= UdmSQLBegin(db)))
      goto ret;
    
    switch(db->DBMode)
    {
      case UDM_DBMODE_BLOB:
        UdmDSTRReset(&qbuf);
        UdmDSTRAppendf(&qbuf,
                       "DELETE FROM bdicti WHERE state=1 AND url_id IN (%s)",
                       urlin.data);
        if (UDM_OK != (rc= UdmSQLQuery(db, NULL, qbuf.data)))
          goto ret;

        UdmDSTRReset(&qbuf);
        UdmDSTRAppendf(&qbuf,
                       "UPDATE bdicti SET state=0 WHERE state=2 AND url_id IN (%s)",
                       urlin.data);
        if (UDM_OK != (rc= UdmSQLQuery(db, NULL, qbuf.data)))
          goto ret;
        break;
      
      case UDM_DBMODE_MULTI:
        {
          size_t dictnum;
          for (dictnum= 0; dictnum <= MULTI_DICTS; dictnum++)
          {
            UdmDSTRReset(&qbuf);
            UdmDSTRAppendf(&qbuf,"DELETE FROM dict%02X WHERE url_id in (%s)",
                           dictnum, urlin.data);
            if(UDM_OK != (rc= UdmSQLQuery(db, NULL, qbuf.data)))
              goto ret;
          }
        }
        break;
        
      default:
        UdmDSTRReset(&qbuf);
        UdmDSTRAppendf(&qbuf,"DELETE FROM dict WHERE url_id in (%s)",urlin.data);
        if (UDM_OK != (rc=UdmSQLQuery(db,NULL,qbuf.data)))
          goto ret;
        break;
    }
    
    UdmDSTRReset(&qbuf);
    UdmDSTRAppendf(&qbuf,"DELETE FROM url WHERE rec_id in (%s)",urlin.data);
    if (UDM_OK != (rc= UdmSQLQuery(db,NULL,qbuf.data)))
      goto ret;
    
    UdmDSTRReset(&qbuf);
    UdmDSTRAppendf(&qbuf,"DELETE FROM urlinfo WHERE url_id in (%s)",urlin.data);
    if (UDM_OK != (rc= UdmSQLQuery(db,NULL,qbuf.data)))
      goto ret;
    
    UdmDSTRReset(&qbuf);
    UdmDSTRAppendf(&qbuf,"DELETE FROM links WHERE ot in (%s)",urlin.data);
    if (UDM_OK != (rc= UdmSQLQuery(db,NULL,qbuf.data)))
      goto ret;
    
    UdmDSTRReset(&qbuf);
    UdmDSTRAppendf(&qbuf,"DELETE FROM links WHERE k in (%s)",urlin.data);
    if (UDM_OK != (rc= UdmSQLQuery(db,NULL,qbuf.data)))
      goto ret;

    if(UDM_OK != (rc= UdmSQLCommit(db)))
      goto ret;
  }
  
ret:
  UdmDSTRFree(&qbuf);
  UdmDSTRFree(&urlin);
  return rc;
}


static int
UdmClearDBUsingLoop(UDM_AGENT *Indexer, UDM_DB *db, UDM_URLID_LIST *list)
{
  int rc= UDM_OK;
  size_t i;
  UDM_DOCUMENT Doc;
  bzero((void*)&Doc, sizeof(Doc));

  for(i=0; i < list->nurls; i++)
  {
    UdmVarListReplaceInt(&Doc.Sections, "ID", list->urls[i]);
    if(UDM_OK != (rc= UdmDeleteURL(Indexer, &Doc, db)))
      break;
  }
  UdmDocFree(&Doc);
  return rc;
}


int
UdmClearDBSQL(UDM_AGENT *Indexer,UDM_DB *db)
{
  int rc, use_crosswords;
  const char *where, *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";
  char ClearDBHook[128];

  UDM_GETLOCK(Indexer, UDM_LOCK_CONF);
  where = UdmSQLBuildWhereCondition(Indexer->Conf, db);
  use_crosswords= UdmVarListFindBool(&Indexer->Conf->Vars, "CrossWords", 0);
  udm_snprintf(ClearDBHook, sizeof(ClearDBHook),
               UdmVarListFindStr(&Indexer->Conf->Vars, "SQLClearDBHook", ""));
  UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);
  
  if (ClearDBHook[0] && (UDM_OK != (rc= UdmSQLQuery(db, NULL, ClearDBHook))))
    return rc;
  
  if(!where[0])
  {
    return UdmTruncateDB(Indexer, db);
  }
  else
  {
    UDM_URLID_LIST urllist;
    UDM_DSTR qbuf;
    UdmDSTRInit(&qbuf, 4096);
    
    bzero((void*) &urllist, sizeof(urllist));
    UdmDSTRAppendf(&qbuf,"SELECT url.rec_id, url.url FROM url%s WHERE url.rec_id<>%s0%s AND %s", 
                   db->from, qu, qu,  where);
    
    if (UDM_OK != (rc= UdmLoadSlowLimitWithSort(Indexer, db, &urllist, qbuf.data)))
      goto fin;
    
    rc= db->DBSQL_IN ? UdmClearDBUsingIN(Indexer, db, &urllist) :
                       UdmClearDBUsingLoop(Indexer, db, &urllist);

fin:
    UdmFree(urllist.urls);
    UdmDSTRFree(&qbuf);
  }
  return rc;
}


/******************** Hrefs ****************************/

#define MAXHSIZE	1023*4	/* TUNE */

int
UdmStoreHrefsSQL(UDM_AGENT *Indexer)
{
  size_t i;
  int rc= UDM_OK, tnx_started= 0;
  UDM_DOCUMENT  Doc;
  UDM_HREFLIST *Hrefs;
  UDM_DB *db= Indexer->Conf->dbl.nitems == 1 ?
              &Indexer->Conf->dbl.db[0] : (UDM_DB*) NULL;

  if (db && !(db->flags & UDM_SQL_HAVE_GOOD_COMMIT))
    db= NULL;
  
  UDM_LOCK_CHECK_OWNER(Indexer, UDM_LOCK_CONF);
 
  UdmDocInit(&Doc);

  Hrefs= &Indexer->Conf->Hrefs;
  
  for (i= 0; i < Hrefs->nhrefs; i++)
  {  
    UDM_HREF *H = &Hrefs->Href[i];
    if (!H->stored)
    {
      int need_add_url=  (i >= Hrefs->dhrefs);
      int need_add_link= (H->collect_links);
      
      H->stored= 1;
      if (!need_add_url && !need_add_link)
        continue;
      
      if (!tnx_started && db && UDM_OK != UdmSQLBegin(db))
        goto ret;
      tnx_started= 1;
      UdmVarListAddLst(&Doc.Sections, &H->Vars, NULL, "*");
      UdmVarListReplaceInt(&Doc.Sections, "Referrer-ID", H->referrer);
      UdmVarListReplaceUnsigned(&Doc.Sections,"Hops", H->hops);
      UdmVarListReplaceStr(&Doc.Sections,"URL",H->url?H->url:"");
      UdmVarListReplaceInt(&Doc.Sections, "URL_ID", UdmStrHash32(H->url ? H->url : ""));
      UdmVarListReplaceInt(&Doc.Sections,"Site_id", H->site_id);
      UdmVarListReplaceInt(&Doc.Sections,"Server_id", H->server_id);
      UdmVarListReplaceInt(&Doc.Sections, "HTDB_URL_ID", H->rec_id);
      if (need_add_url)
      {
        if(UDM_OK != (rc= UdmURLActionNoLock(Indexer, &Doc, UDM_URL_ACTION_ADD)))
          goto ret;
      }
      if (need_add_link)
      {
        if(UDM_OK != (rc= UdmURLActionNoLock(Indexer, &Doc, UDM_URL_ACTION_ADD_LINK)))
          goto ret;
      }
      UdmVarListFree(&Doc.Sections);
    }
  }

  if (tnx_started && db && UDM_OK != UdmSQLCommit(db))
    goto ret;

ret:

  UdmDocFree(&Doc);
  
  /*
    Remember last stored URL num
    Note that it will became 0
    after next sort in AddUrl   
  */
  Hrefs->dhrefs = Hrefs->nhrefs;
  
  /*
    We should not free URL list with onw database
    to avoid double indexing of the same document
    So, do it if compiled with SQL only          
  */
  
  /* FIXME: this is incorrect with both SQL and built-in compiled */
  if(Hrefs->nhrefs > MAXHSIZE)
    UdmHrefListFree(&Indexer->Conf->Hrefs);

  return rc;
}


/********************* Categories ************************************/
static int
UdmCatList(UDM_AGENT * Indexer,UDM_CATEGORY *Cat,UDM_DB *db)
{
  size_t    i, rows;
  char    qbuf[1024];
  UDM_SQLRES  SQLres;
  int    rc;
  
  if(db->DBType==UDM_DB_SAPDB)
  {
    udm_snprintf(qbuf,sizeof(qbuf)-1,"SELECT rec_id,path,lnk,name FROM categories WHERE path LIKE '%s__'",Cat->addr);
  }else{
    udm_snprintf(qbuf,sizeof(qbuf)-1,"SELECT rec_id,path,link,name FROM categories WHERE path LIKE '%s__'",Cat->addr);
  }
  
  if(UDM_OK!=(rc=UdmSQLQuery(db,&SQLres,qbuf)))
    return rc;
  
  if( (rows = UdmSQLNumRows(&SQLres)) )
  {
    size_t nbytes;
    
    nbytes = sizeof(UDM_CATITEM) * (rows + Cat->ncategories);
    Cat->Category=(UDM_CATITEM*)UdmRealloc(Cat->Category,nbytes);
    for(i=0;i<rows;i++)
    {
      UDM_CATITEM *r = &Cat->Category[Cat->ncategories];
      r[i].rec_id=atoi(UdmSQLValue(&SQLres,i,0));
      strcpy(r[i].path,UdmSQLValue(&SQLres,i,1));
      strcpy(r[i].link,UdmSQLValue(&SQLres,i,2));
      strcpy(r[i].name,UdmSQLValue(&SQLres,i,3));
    }
    Cat->ncategories+=rows;
  }
  UdmSQLFree(&SQLres);
  return UDM_OK;
}


static int
UdmCatPath(UDM_AGENT *Indexer,UDM_CATEGORY *Cat,UDM_DB *db)
{
  size_t    i,l;
  char    qbuf[1024];
  int    rc;
  char            *head = NULL;
  
  l=(strlen(Cat->addr)/2)+1;
  Cat->Category=(UDM_CATITEM*)UdmRealloc(Cat->Category,sizeof(UDM_CATITEM)*(l+Cat->ncategories));
  head = (char *)UdmMalloc(2 * l + 1);

  if (head != NULL)
  {
    UDM_CATITEM  *r = &Cat->Category[Cat->ncategories];

    for(i = 0; i < l; i++)
    {
      UDM_SQLRES  SQLres;

      strncpy(head,Cat->addr,i*2);head[i*2]=0;

      if(db->DBType==UDM_DB_SAPDB)
      {
        udm_snprintf(qbuf,sizeof(qbuf)-1,"SELECT rec_id,path,lnk,name FROM categories WHERE path='%s'",head);
      }
      else
      {
        udm_snprintf(qbuf,sizeof(qbuf)-1,"SELECT rec_id,path,link,name FROM categories WHERE path='%s'",head);
      }
    
      if(UDM_OK!=(rc=UdmSQLQuery(db,&SQLres,qbuf)))
        return rc;
    
      if(UdmSQLNumRows(&SQLres))
      {
        r[i].rec_id=atoi(UdmSQLValue(&SQLres,0,0));
        strcpy(r[i].path,UdmSQLValue(&SQLres,0,1));
        strcpy(r[i].link,UdmSQLValue(&SQLres,0,2));
        strcpy(r[i].name,UdmSQLValue(&SQLres,0,3));
        Cat->ncategories++;
      }
      UdmSQLFree(&SQLres);
    }
    UDM_FREE(head);
  }
  return UDM_OK;
}


unsigned int
UdmGetCategoryIdSQL(UDM_ENV *Conf, char *category, UDM_DB *db)
{
  UDM_SQLRES Res;
  char qbuf[128];
  unsigned int rc = 0;

  udm_snprintf(qbuf, 128, "SELECT rec_id FROM categories WHERE path LIKE '%s'", category);
  if(UDM_OK != (rc = UdmSQLQuery(db, &Res, qbuf))) return rc;
  if ( UdmSQLNumRows(&Res) > 0)
  {
    sscanf(UdmSQLValue(&Res, 0, 0), "%u", &rc);
  }
  UdmSQLFree(&Res);
  return rc;
}


/******************* Search *************************/
static int
cmp_score_urlid(UDM_URL_SCORE *s1, UDM_URL_SCORE *s2)
{
  if (s1->url_id > s2->url_id) return(1);
  if (s1->url_id < s2->url_id) return(-1);
  return 0;
}
      

static void
UdmScoreListToURLData(UDM_URLDATA *D, UDM_URL_SCORE *C, size_t num)
{
  for ( ; num > 0; num--, D++, C++)
  {
    D->url_id= C->url_id;
    D->score= C->score;
  }
}


#ifdef HAVE_DEBUG
static void
UdmURLScoreListPrint(UDM_URLSCORELIST *List)
{
  size_t i;
  for (i= 0; i < List->nitems; i++)
  {
    UDM_URL_SCORE *Item= &List->Item[i];
    fprintf(stderr, "%d:%d\n", Item->url_id, Item->score);
  }
}
#endif

static int 
UdmSortAndGroupByURL(UDM_AGENT *A,
                     UDM_RESULT *Res,
                     UDM_SECTIONLIST *SectionList,
                     UDM_DB *db, size_t num_best_rows)
{
  UDM_URLSCORELIST ScoreList;
  UDM_URLDATALIST DataList;
  udm_timer_t ticks=UdmStartTimer();
  const char *pattern= UdmVarListFindStr(&A->Conf->Vars, "s", "R");
  size_t nbytes;
  int flags= 0, rc= UDM_OK;
  const char *p;
  int RelevancyFactor= UdmVarListFindInt(&A->Conf->Vars, "RelevancyFactor", 255);
  int DateFactor= UdmVarListFindInt(&A->Conf->Vars, "DateFactor", 0);
  const char *su= UdmVarListFindStr(&A->Conf->Vars, "su", NULL);
  int group_by_site= UdmVarListFindBool(&A->Conf->Vars, "GroupBySite", 0) 
                     && UdmVarListFindInt(&A->Conf->Vars, "site", 0) == 0 ?
                     UDM_URLDATA_SITE : 0;
  int group_by_site_rank= !strcmp(UdmVarListFindStr(&A->Conf->Vars, "GroupBySite", "no"), "rank");
  size_t BdictThreshold= (size_t) UdmVarListFindInt(&A->Conf->Vars,
                                                    "URLDataThreshold", 0);
  size_t MaxResults= (size_t) UdmVarListFindInt(&db->Vars, "MaxResults", 0);
  int use_qcache= UdmVarListFindBool(&db->Vars, "qcache", 0);
  urlid_t debug_url_id= UdmVarListFindInt(&A->Conf->Vars, "DebugURLId", 0);

  flags|= group_by_site ? UDM_URLDATA_SITE : 0;
  flags|= group_by_site_rank ? UDM_URLDATA_SITE_RANK : 0;
  flags|= DateFactor ? UDM_URLDATA_LM : 0;
  flags|= use_qcache ? UDM_URLDATA_CACHE : 0;
  
  for (p = pattern; *p; p++)
  {
    if (*p == 'U' || *p == 'u') flags|= UDM_URLDATA_URL;
    if (*p == 'P' || *p == 'p') flags|= UDM_URLDATA_POP;
    if (*p == 'D' || *p == 'd') flags|= UDM_URLDATA_LM;
    if (*p == 'S' || *p == 's') flags|= (su && su[0]) ? UDM_URLDATA_SU : 0;
  }
  
  ticks=UdmStartTimer();
  bzero((void*) &ScoreList, sizeof(ScoreList));

  UdmLog(A,UDM_LOG_DEBUG, "Start GroupByURL %d sections", SectionList->nsections);
  UdmGroupByURL2(A, db, Res, SectionList, &ScoreList);

  UdmLog(A, UDM_LOG_DEBUG, "%-30s%.2f (%d docs found)",
         "Stop  GroupByURL", UdmStopTimer(&ticks), ScoreList.nitems);

#ifdef HAVE_DEBUG
  if (UdmVarListFindBool(&A->Conf->Vars, "DebugGroupByURL", 0))
  {
    UdmURLScoreListPrint(&ScoreList);
  }
#endif

  UdmApplyCachedQueryLimit(A, &ScoreList, db);
  if (ScoreList.nitems == 0)
    goto ex;
  
  if (UDM_OK != (rc=  UdmUserScoreListLoadAndApplyToURLScoreList(A, &ScoreList, db)))
    goto ex;

  UdmLog(A,UDM_LOG_DEBUG,"Start load url data %d docs (%d best needed)",
         ScoreList.nitems, num_best_rows);
  ticks=UdmStartTimer();

  nbytes= UdmHashSize(ScoreList.nitems) * sizeof(UDM_URLDATA);
  DataList.Item = (UDM_URLDATA*)UdmMalloc(nbytes);
  bzero((void*) DataList.Item, nbytes);
  DataList.nitems= ScoreList.nitems;

  /* Use full sort in case if DebugURLId is specified */
  if (debug_url_id)
    num_best_rows= ScoreList.nitems;
  
  if (num_best_rows > ScoreList.nitems)
    num_best_rows= ScoreList.nitems;
  
  /* Try fast sorting if sorting is on score */
  if (num_best_rows < 256 && !flags)
  {
    udm_timer_t ticks1;
    
    Res->total_found= ScoreList.nitems;
    UdmLog(A,UDM_LOG_DEBUG,"Start SortByScore %d docs", ScoreList.nitems);
    ticks1=UdmStartTimer();
    if (ScoreList.nitems > 1000)
    {
      UdmURLScoreListSortByScoreThenURLTop(&ScoreList, 1000);
    }
    else
    {
      UdmURLScoreListSortByScoreThenURL(&ScoreList);
    }
    UdmSort((void*) ScoreList.Item, num_best_rows,
            sizeof(UDM_URL_SCORE), (udm_qsort_cmp) cmp_score_urlid);
    UdmScoreListToURLData(DataList.Item, ScoreList.Item, num_best_rows);
    UdmLog(A,UDM_LOG_DEBUG,"%-30s%.2f", "Stop  SortByScore:", UdmStopTimer(&ticks1));
    DataList.nitems= num_best_rows; /* Put only num_best_rows into DataList */
    goto date_factor;
  }

  UdmScoreListToURLData(DataList.Item, ScoreList.Item, DataList.nitems);
  
  /* Sort by a user defined section, if given */
  if (flags & UDM_URLDATA_SU)
  {
    size_t norder;
    udm_timer_t ticks1= UdmStartTimer();
    UdmLog(A, UDM_LOG_DEBUG, "Trying to load fast section order '%s'", su);
    rc= UdmFastOrderLoadAndApplyToURLDataList(A, db, &DataList, su, &norder);
    UdmLog(A, UDM_LOG_DEBUG, "Loading fast order '%s' done, %d docs found, %.2f sec",
           su, norder, UdmStopTimer(&ticks1));
    if (norder)
      flags^= UDM_URLDATA_SU;
  }
  
  if (db->DBMode == UDM_DBMODE_BLOB &&
      !(flags & UDM_URLDATA_URL)    &&
      !(flags & UDM_URLDATA_SU)     &&
        BdictThreshold < ScoreList.nitems)
  {
    rc= UdmLoadURLDataFromBdict(A, &DataList, db, DataList.nitems, flags);
  }
  else
  {
    rc= UdmLoadURLDataFromURL(A, &DataList, db, DataList.nitems, flags);
  }

  if (flags & UDM_URLDATA_SITE_RANK)
  {
    udm_timer_t ticks1= UdmStartTimer();
    UdmLog(A, UDM_LOG_DEBUG, "Start applying in-site-rank");
    UdmURLDataSortBySite(&DataList);
    UdmURLDataApplySiteRank(A, &DataList, 0);
    UdmLog(A, UDM_LOG_DEBUG, "Stop applying in-site-rank:   %.2f sec", UdmStopTimer(&ticks1));
  }

  Res->total_found= DataList.nitems;


date_factor:
  
  if (rc != UDM_OK)
    goto ex;

  /* TODO: check whether limit by site works fine */
  if (!RelevancyFactor || DateFactor)
    UdmURLDataListApplyRelevancyFactors(A, &DataList, RelevancyFactor, DateFactor);

  UdmLog(A,UDM_LOG_DEBUG,"%-30s%.2f", "Stop  load url data:", UdmStopTimer(&ticks));
  
  UdmLog(A,UDM_LOG_DEBUG,"Start SortByPattern %d docs", DataList.nitems);
  ticks=UdmStartTimer();
  if (DataList.nitems)
    UdmURLDataSortByPattern(&DataList, pattern);
  UdmLog(A,UDM_LOG_DEBUG,"%-30s%.2f", "Stop  SortByPattern:", UdmStopTimer(&ticks));

  Res->URLData= DataList;

  if (debug_url_id)
  {
    size_t i;
    for (i= 0; i < DataList.nitems; i++)
    {
      if (DataList.Item[i].url_id == debug_url_id)
      {
        char tmp[256];
        const char *s= UdmVarListFindStr(&A->Conf->Vars, "DebugScore", "");
        udm_snprintf(tmp, sizeof(tmp), "%s rank=%d", s, i + 1);
        UdmVarListReplaceStr(&A->Conf->Vars, "DebugScore", tmp);
        break;
      }
    }
  }
  
  if (MaxResults && MaxResults < Res->total_found)
  {
    UdmLog(A, UDM_LOG_DEBUG, "Applying MaxResults=%d, total_found=%d\n",
           (int) MaxResults, (int) Res->total_found);
    Res->total_found= MaxResults;
    if (Res->URLData.nitems > MaxResults)
      Res->URLData.nitems= MaxResults;
  }

ex:
  UdmFree(ScoreList.Item);
  return rc;
}


static int /* WHERE limit */
LoadURL(UDM_DB *db, const char *where, UDM_URLID_LIST *buf)
{
  int rc;
  UDM_SQLRES SQLRes;
  char qbuf[1024 * 4];
  size_t nrows;
  urlid_t *tmp;
  size_t i;

  if (!*where)
    return UDM_OK;

  /* TODO: reuse LoadSlowLimitWithSort() here */
  udm_snprintf(qbuf, sizeof(qbuf),
               "SELECT url.rec_id FROM url%s WHERE %s", db->from, where);
  if  (UDM_OK != (rc= UdmSQLQuery(db, &SQLRes, qbuf)))
    return rc;

  if (!(nrows= UdmSQLNumRows(&SQLRes)))
  {
    buf->empty= 1;
    UdmSQLFree(&SQLRes);
    return(UDM_OK);
  }

  tmp= UdmMalloc(sizeof(urlid_t) * nrows);
  buf->urls= UdmMalloc(sizeof(urlid_t) * nrows);
  if (!tmp || !buf->urls)
  {
    UDM_FREE(buf->urls);
    UDM_FREE(tmp);
    goto ex;
  }
  
  for (i= 0; i < nrows; i++)
  {
    tmp[i]= (urlid_t) UDM_ATOI(UdmSQLValue(&SQLRes, i, 0));
  }
  UdmSort(tmp, nrows, sizeof(urlid_t), (udm_qsort_cmp)UdmCmpURLID);
  
  /* Remove duplicates */
  for (i= 0; i < nrows; )
  {
    while (++i < nrows && tmp[i] == tmp[i - 1]);
    buf->urls[buf->nurls++] = tmp[i - 1];
  }
  UDM_FREE(tmp);
  if ((tmp= UdmRealloc(buf->urls, sizeof(urlid_t) * buf->nurls)))
    buf->urls = tmp;

ex:
  UdmSQLFree(&SQLRes);
  return UDM_OK;
}


int
UdmBuildCmpArgSQL(UDM_DB *db, int match, const char *word,
                  char *cmparg, size_t maxlen)
{
  char escwrd[1000];
  UdmSQLEscStr(db, escwrd, word, strlen(word)); /* Search word */
  switch(match)
  {  
    case UDM_MATCH_BEGIN:
      udm_snprintf(cmparg, maxlen, " LIKE '%s%%'", escwrd);
      break;
    case UDM_MATCH_END:
      udm_snprintf(cmparg, maxlen, " LIKE '%%%s'", escwrd);
      break;
    case UDM_MATCH_SUBSTR:
      udm_snprintf(cmparg, maxlen, " LIKE '%%%s%%'", escwrd);
      break;
    case UDM_MATCH_NUMERIC_LT:
      udm_snprintf(cmparg, maxlen, " < %d", atoi(escwrd));
      break;
    case UDM_MATCH_NUMERIC_GT:
      udm_snprintf(cmparg, maxlen, " > %d", atoi(escwrd));
      break;
    case UDM_MATCH_FULL:
    default:
      udm_snprintf(cmparg, maxlen, "='%s'", escwrd);
      break;
  }
  return(UDM_OK);
}


static int
UdmFindOneWordSQL (UDM_FINDWORD_ARGS *args)
{
  char cmparg[256];
  UdmBuildCmpArgSQL(args->db,
                    args->Word.match, args->Word.word,
                    cmparg, sizeof(cmparg));
  args->cmparg= cmparg;
  
  UDM_ASSERT(args->db->dbmode_handler->FindWord != NULL);
  return args->db->dbmode_handler->FindWord(args);
}


static int
UdmFindMultiWordSQL (UDM_FINDWORD_ARGS *args)
{
  char *lt, *tmp_word, *tok;
  int rc= UDM_OK;
  UDM_SECTIONLISTLIST OriginalSectionListList;
  size_t orig_wordnum;
  size_t nparts= 0;
  const char *w;
  char *orig_word= args->Word.word;
  char delim[]= " \r\t_-./";

  /* Check if the word really multi-part */
  for (w= args->Word.word; ; w++)
  {
    if (!*w)
      return UdmFindOneWordSQL(args); /* No delimiters found */
    
    if (strchr(delim, *w)) /* Delimiter found */
      break;
  }
  
  if (!(tmp_word= UdmStrdup(args->Word.word)))
    return(UDM_ERROR);

  UdmLog(args->Agent, UDM_LOG_DEBUG,
         "Start searching for multiword '%s'", args->Word.word);
  OriginalSectionListList= args->SectionListList;
  UdmSectionListListInit(&args->SectionListList);
  orig_wordnum= args->Word.order;
  args->need_coords= 1; /* Force immediate coord unpacking */
  
  for (tok= udm_strtok_r(tmp_word, delim, &lt) ; tok ;
       tok= udm_strtok_r(NULL, delim, &lt))
  {
    udm_timer_t ticks1= UdmStartTimer();
    args->Word.word= tok;
    UdmLog(args->Agent, UDM_LOG_DEBUG,
           "Searching for subword '%s'", args->Word.word);
    rc= UdmFindOneWordSQL(args);
    UdmLog(args->Agent, UDM_LOG_DEBUG,
           "Stop searching for subword '%s' %d coords found: %.2f",
           args->Word.word, args->Word.count, UdmStopTimer(&ticks1));
    /* If the next word wasn't found - no need to search for others. */
    if (rc != UDM_OK || !args->Word.count)
      goto ret;
    nparts++;
    args->Word.order++;
  }
  
  /* All parts returned results. Check phrase */
  UdmMultiWordAdd(args, &OriginalSectionListList, orig_wordnum, nparts);
  
  
ret:
  UdmFree(tmp_word);
  UdmSectionListListFree(&args->SectionListList);
  args->SectionListList= OriginalSectionListList;
  args->Word.word= orig_word;
  args->need_coords= 0;
  UdmLog(args->Agent, UDM_LOG_DEBUG,
         "Stop searching for multiword '%s'", args->Word.word);
  return rc;
}


static int
UdmFindAlwaysFoundWordSQL(UDM_FINDWORD_ARGS *args)
{
  int rc= UDM_OK;
  UDM_SQLRES SQLRes;
  char qbuf[1024 * 4];
  size_t nrows;
  size_t i;
  UDM_URLCRDLIST CoordList;

  bzero((void*) &CoordList, sizeof(CoordList));

  if (*args->where)
     udm_snprintf(qbuf, sizeof(qbuf), "SELECT url.rec_id FROM url%s WHERE %s",
                  args->db->from, args->where);
  else
  {
    if (args->urls.nurls)
    {
      /*
        A fast limit is loaded.
        No needs to do "SELECT FROM url".
        Populate CoordList from the fast limit instead.
      */
      for (i= 0; i < args->urls.nurls; i++)
      {
        if (UDM_OK != (rc= UdmAddOneCoord(&CoordList, args->urls.urls[i],
                                          0x00010100,
                                          (args->Word.order & 0xFF))))
          return UDM_ERROR;
      }
      UdmURLCRDListListAddWithSort2(args, &CoordList);
      return UDM_OK;
    }
    udm_snprintf(qbuf, sizeof(qbuf), "SELECT url.rec_id FROM url");
  }

  if ((rc= UdmSQLQuery(args->db, &SQLRes, qbuf)) != UDM_OK)
    return(rc);
  /* Note that rc is implicitly set to UDM_OK at this point. */
  if (! (nrows= UdmSQLNumRows(&SQLRes)))
    goto err;

  for (i = 0; i < nrows; i++)
    if (UDM_OK != (rc= UdmAddOneCoord(&CoordList,
                              (urlid_t)UDM_ATOI(UdmSQLValue(&SQLRes, i, 0)),
                              0x00010100,
                              (args->Word.order & 0xFF))))
      break;

  if (args->urls.nurls)
    UdmApplyFastLimit(&CoordList, &args->urls);
  if (CoordList.ncoords)
    UdmURLCRDListListAddWithSort2(args, &CoordList);

err:
  UdmSQLFree(&SQLRes);
  return(rc);
}


static int
UdmCheckIndex(UDM_AGENT *query, UDM_DB *db)
{
  int tm, rc;
  if (UDM_OK != (rc= UdmBlobReadTimestamp(query, db, &tm, 0)))
    return rc;
  if (tm)
    return UDM_OK;
#ifdef WIN32
  sprintf(query->Conf->errstr, "Inverted word index not found. Probably you forgot to run 'Create fast index'.");
#else
  sprintf(query->Conf->errstr, "Inverted word index not found. Probably you forgot to run 'indexer -Eblob'.");
#endif
  return UDM_ERROR;
}


static int
UdmMergeWords(UDM_FINDWORD_ARGS *args, UDM_SECTIONLIST *SectionList)
{
  udm_timer_t ticks= UdmStartTimer();
  UDM_AGENT *A= args->Agent;

  UdmLog(A, UDM_LOG_DEBUG, "Start merging %d lists", args->SectionListList.nitems);
  UdmSectionListListMergeSorted(&args->SectionListList, SectionList, 1);
  UdmLog(A, UDM_LOG_DEBUG, "%-30s%.2f (%d sections)",
         "Stop  merging:", UdmStopTimer(&ticks), SectionList->nsections);

  if (!SectionList->nsections && args->db->DBMode == UDM_DBMODE_BLOB)
    return UdmCheckIndex(A, args->db);
  return UDM_OK;
}


static void
UdmSearchParamInit(UDM_FINDWORD_ARGS *args,
                   UDM_AGENT *A,
                   UDM_RESULT *Res,
                   UDM_DB *db)
{

  UDM_LOCK_CHECK_OWNER(A, UDM_LOCK_CONF);

  bzero((void*) args, sizeof(*args));
  UdmWideWordListInit(&args->CollationMatches);

  args->Agent= A;
  args->db= db;
  args->WWList= &Res->WWList;
  args->Word.match= UdmMatchMode(UdmVarListFindStr(&A->Conf->Vars, "wm", "wrd"));
  args->where= UdmSQLBuildWhereCondition(A->Conf, db);
  args->save_section_size= UdmVarListFindBool(&A->Conf->Vars, "SaveSectionSize", 1);
  args->live_updates= UdmVarListFindBool(&db->Vars, "LiveUpdates", 0);
  args->use_crosswords= UdmVarListFindBool(&A->Conf->Vars, "CrossWords", 0);
  args->use_qcache= UdmVarListFindBool(&db->Vars, "qcache", 0);
}


static void
UdmSearchParamFree(UDM_FINDWORD_ARGS *args)
{
  UDM_FREE(args->urls.urls);
  UDM_FREE(args->live_update_active_urls.urls);
  UDM_FREE(args->live_update_deleted_urls.urls);
  UdmWideWordListFree(&args->CollationMatches);
  UdmSectionListListFree(&args->SectionListList);
  UdmSQLResListFree(&args->SQLResults);
}


/*
  Load WHERE and fl limits from the database at search time.
*/
static int
UdmFindLoadLimits(UDM_AGENT *query,
                  UDM_DB *db,
                  UDM_FINDWORD_ARGS *args,
                  const char *fl)
{
  int rc= UDM_OK;
  UDM_URLID_LIST fl_urls;
  udm_timer_t ticks= UdmStartTimer();

  bzero((void*) &fl_urls, sizeof(fl_urls));

  UdmLog(query,UDM_LOG_DEBUG, "Start loading limits");
  ticks= UdmStartTimer();
  if (*args->where)
  {
    LoadURL(db, args->where, &args->urls);
    UdmLog(query, UDM_LOG_DEBUG, "WHERE limit loaded. %d URLs found", args->urls.nurls);
  }
  if (!args->urls.empty && fl[0])
  {
    char name[64];
    const char *q;
    if ((fl_urls.exclude= (fl[0] == '-')))
      fl++;
    udm_snprintf(name, sizeof(name), "Limit.%s", fl);
    if (UDM_OK != (rc= ((q= UdmVarListFindStr(&query->Conf->Vars, name, NULL)) ?
                       UdmLoadSlowLimitWithSort(query, db, &fl_urls, q) :
                       UdmBlobLoadFastURLLimit(query, db, fl, &fl_urls))))
      goto ret;
    UdmLog(query,UDM_LOG_DEBUG, "Limit '%s' loaded%s%s %d URLs",
           fl, fl_urls.exclude ? " type=excluding" : "",
           q ? " source=slow":"", fl_urls.nurls);

    UdmURLIdListMerge(&args->urls, &fl_urls);
  }
  UdmLog(query,UDM_LOG_DEBUG,"%-30s%.2f (%d URLs found)",
         "Stop  loading limits", UdmStopTimer(&ticks), args->urls.nurls);
ret:
  UDM_FREE(fl_urls.urls);
  return rc;
}


/*
  Load word information from the database
*/
static int
UdmFindWordsFetch(UDM_FINDWORD_ARGS *args,
                  UDM_RESULT *Res,
                  const char *always_found_word)
{
  size_t wordnum;
  int rc= UDM_OK;
  udm_timer_t ticks0= UdmStartTimer();
  
  UdmLog(args->Agent, UDM_LOG_DEBUG, "Start fetching words");
  
  /* Now find each word */
  for(wordnum=0; wordnum < Res->WWList.nwords; wordnum++)
  {
    udm_timer_t ticks= UdmStartTimer();
    UDM_WIDEWORD *W=&Res->WWList.Word[wordnum];
    char quoted_word[64];
    udm_snprintf(quoted_word, sizeof(quoted_word), "'%s'", W->word);

    if (W->origin == UDM_WORD_ORIGIN_STOP) continue;

    UdmLog(args->Agent, UDM_LOG_DEBUG, "Start search for %s", quoted_word);

    args->Word.order= wordnum;
    args->Word.count= 0;
    args->Word.word= W->word;
    args->Word.match= W->match;
    args->Word.secno= W->secno;

    /*
       For now SYNONYMs only are treated as a possible multi-word
       origin. Probably it will be changed in future, so we will
       use this feature for phrase search.
     */
    if (always_found_word && !strcmp(W->word, always_found_word))
      rc= UdmFindAlwaysFoundWordSQL(args);
    else if (W->origin == UDM_WORD_ORIGIN_SYNONYM || W->phrwidth > 0)
      rc= UdmFindMultiWordSQL(args);
    else
      rc= UdmFindOneWordSQL(args);

    if (rc != UDM_OK)
      goto ret;

    /*
      If CollationMatches is not empty, then we should skip
      updating word statistics here - it will be updated in
      the loop after UdmSortAndGroupByURL().
     */
    if (!args->CollationMatches.nwords)
      Res->WWList.Word[wordnum].count+= args->Word.count;

    UdmLog(args->Agent, UDM_LOG_DEBUG,
           "Stop  search for %-13s%.2f (%u coords found)",
           quoted_word, UdmStopTimer(&ticks), args->Word.count);

    if (args->use_crosswords)
    {
      UdmLog(args->Agent, UDM_LOG_DEBUG,
             "Start search for crossword '%s'", W->word);
      args->Word.count= 0;
      if (UDM_OK != (rc= UdmFindCrossWord(args)))
        goto ret;
      Res->WWList.Word[wordnum].count+= args->Word.count;
      
      UdmLog(args->Agent, UDM_LOG_DEBUG,
            "Stop search for crossword '%s'\t%.2f %d found",
             args->Word.word, UdmStopTimer(&ticks), args->Word.count);
    }
  }
  UdmLog(args->Agent, UDM_LOG_DEBUG,
         "%-30s%.2f", "Stop  fetching words:", UdmStopTimer(&ticks0));
ret:
  return rc;
}


int
UdmFindWordsSQL(UDM_AGENT *query, UDM_RESULT *Res, UDM_DB *db,
                size_t num_best_rows)
{
  char   wf[256];
  const char  *always_found_word, *fl;
  int    rc= UDM_OK;
  UDM_FINDWORD_ARGS args;
  UDM_SECTIONLIST SectionList;

  bzero((void*) &SectionList, sizeof(SectionList));

  UDM_GETLOCK(query, UDM_LOCK_CONF);
  {
    UdmSearchParamInit(&args, query, Res, db);

    always_found_word= UdmVarListFindStr(&query->Conf->Vars, "AlwaysFoundWord", NULL);
    fl= UdmVarListFindStr(&query->Conf->Vars, "fl", UdmVarListFindStr(&db->Vars, "fl", ""));

    UdmWeightFactorsInit2(wf, &query->Conf->Vars, &db->Vars, "wf");
    args.wf= wf;
  }
  UDM_RELEASELOCK(query, UDM_LOCK_CONF);

  if ((db->DBMode == UDM_DBMODE_BLOB && args.where) || fl[0])
  {
    if (UDM_OK != UdmFindLoadLimits(query, db, &args, fl))
      goto ret;

    if (args.urls.empty)
      goto ret;
  }


  if (db->DBMode == UDM_DBMODE_BLOB)
  {
    /* Load update limit when necessary */
    if (UDM_OK != (rc= UdmBlobLoadLiveUpdateLimit(&args, query, db)))
      goto ret;
  }


  if (UDM_OK != (rc= UdmFindWordsFetch(&args, Res, always_found_word)))
    goto ret;

  if (UDM_OK != (rc= UdmMergeWords(&args, &SectionList)))
    goto ret;
  
  if (UDM_OK != (rc= UdmSortAndGroupByURL(query, Res,
                                          &SectionList, db, num_best_rows)))
    goto ret;

  /* 
     We cannot add collation matches before
     UdmSortAndGroupByURL - to use optimized groupping
     functions when WWList->nwords==1
  */
  if (args.CollationMatches.nwords)
  {
    size_t i;
    for (i= 0; i < args.CollationMatches.nwords; i++)
    {
      UdmWideWordListAdd(&Res->WWList, &args.CollationMatches.Word[i]);
    }
  }


ret:
  UdmSectionListFree(&SectionList);
  UdmSearchParamFree(&args);
  return rc;
}


/****************** Track ***********************************/

int UdmTrackSQL(UDM_AGENT * query, UDM_RESULT *Res, UDM_DB *db)
{
  UDM_VARLIST Vars;
  char    *qbuf;
  char    *text_escaped;
  const char  *words=UdmVarListFindStr(&query->Conf->Vars,"q",""); /* "q-lc" was here */
  const char      *IP = UdmVarListFindStr(&query->Conf->Vars, "IP", "");
  size_t          i, escaped_len, qbuf_len;
  int             res, qtime, rec_id;
  const char      *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";
  const char      *value= (db->DBType == UDM_DB_IBASE ||
                           db->DBType == UDM_DB_MIMER ||
                           db->DBType == UDM_DB_DB2   ||
                           db->DBType == UDM_DB_ORACLE8) ? "sval" : "value";
  
  if (*words == '\0') return UDM_OK; /* do not store empty queries */

  escaped_len = 4 * strlen(words);
  qbuf_len = escaped_len + 4096;

  if ((qbuf = (char*)UdmMalloc(qbuf_len)) == NULL) return UDM_ERROR;
  if ((text_escaped = (char*)UdmMalloc(escaped_len)) == NULL)
  { 
    UDM_FREE(qbuf);
    return UDM_ERROR;
  }
  
  UdmVarListInit(&Vars);
  UdmVarListSQLEscape(&Vars, &query->Conf->Vars, db);
  
  /* Escape text to track it  */
  UdmSQLEscStr(db, text_escaped, words, strlen(words)); /* query for tracking */
  
  if (db->DBType == UDM_DB_IBASE ||
      db->DBType == UDM_DB_MIMER ||
      db->DBType == UDM_DB_ORACLE8)
  {
    const char *next;
    switch (db->DBType)
    {
      case UDM_DB_IBASE: next= "SELECT GEN_ID(qtrack_GEN,1) FROM rdb$database"; break;
      case UDM_DB_MIMER: next= "SELECT NEXT_VALUE OF qtrack_GEN FROM system.onerow"; break;
      case UDM_DB_ORACLE8: next= "SELECT qtrack_seq.nextval FROM dual"; break;
    }
    if (UDM_OK != (res= UdmSQLQueryOneRowInt(db, &rec_id, next)))
      goto UdmTrack_exit;
    udm_snprintf(qbuf, qbuf_len - 1,
                 "INSERT INTO qtrack (rec_id,ip,qwords,qtime,wtime,found) "
                 "VALUES "
                 "(%d,'%s','%s',%d,%d,%d)",
                 rec_id, IP, text_escaped, qtime= (int)time(NULL),
                 Res->work_time, Res->total_found);
    if (UDM_OK != (res = UdmSQLQuery(db, NULL, qbuf)))
      goto UdmTrack_exit;
  }
  else
  {
    /* FOUND is a keyword in Virtuoso */
    const char *quote_found= (db->DBType == UDM_DB_VIRT) ? "\"" : "";
    udm_snprintf(qbuf, qbuf_len - 1,
                 "INSERT INTO qtrack (ip,qwords,qtime,wtime,%sfound%s) "
                 "VALUES "
                 "('%s','%s',%d,%d,%d)",
                 quote_found, quote_found,
                 IP, text_escaped, qtime= (int)time(NULL),
                 Res->work_time, Res->total_found);
  
    if (UDM_OK != (res= UdmSQLQuery(db, NULL, qbuf)))
      goto UdmTrack_exit;
    
    if (db->DBType == UDM_DB_MYSQL)
      udm_snprintf(qbuf, qbuf_len - 1, "SELECT last_insert_id()");
    else
      udm_snprintf(qbuf, qbuf_len - 1, "SELECT rec_id FROM qtrack WHERE ip='%s' AND qtime=%d", IP, qtime);
    if (UDM_OK != (res= UdmSQLQueryOneRowInt(db, &rec_id, qbuf)))
      goto UdmTrack_exit;
  }
  
  for (i = 0; i < Vars.nvars; i++)
  {
    UDM_VAR *Var = &Vars.Var[i];
    if (strncasecmp(Var->name, "query.",6)==0 && strcasecmp(Var->name, "query.q") && strcasecmp(Var->name, "query.BrowserCharset")
       && strcasecmp(Var->name, "query.IP")  && Var->val != NULL && *Var->val != '\0') 
    {
      udm_snprintf(qbuf, qbuf_len,
                   "INSERT INTO qinfo (q_id,name,%s) "
                   "VALUES "
                   "(%s%i%s,'%s','%s')", 
      value, qu, rec_id, qu, &Var->name[6], Var->val);
      res= UdmSQLQuery(db, NULL, qbuf);
      if (res != UDM_OK) goto UdmTrack_exit;
    }
  }
UdmTrack_exit:
  UdmVarListFree(&Vars);
  UDM_FREE(text_escaped);
  UDM_FREE(qbuf);
  return res;
}


/********************* Adding URLInfo to Res *********************/

static int UpdateShows(UDM_DB *db, urlid_t url_id)
{
  char qbuf[64];
  udm_snprintf(qbuf, sizeof(qbuf), "UPDATE url SET shows = shows + 1 WHERE rec_id = %s%i%s",
               (db->DBType == UDM_DB_PGSQL) ? "'" : "",
               url_id,
               (db->DBType == UDM_DB_PGSQL) ? "'" : "");
  return UdmSQLQuery(db, NULL, qbuf);
}

static void SQLResToSection(UDM_SQLRES *R, UDM_VARLIST *S, size_t row)
{
  const char *sname=UdmSQLValue(R,row,1);
  const char *sval=UdmSQLValue(R,row,2);
  UdmVarListAddStr(S, sname, sval);
}


static size_t
UdmDBNum(UDM_RESULT *Res, size_t n)
{
  UDM_URLDATA *Data= &Res->URLData.Item[n + Res->first];
  return UDM_COORD2DBNUM(Data->score);
}


static int
UdmResAddURLInfoUsingIN(UDM_RESULT *Res, UDM_DB *db, size_t dbnum,
                        const char *qbuf)
{
  int rc;
  UDM_SQLRES SQLres;
  size_t j, sqlrows;
  
  if(UDM_OK!=(rc=UdmSQLQuery(db,&SQLres,qbuf)))
    return rc;
    
  for(sqlrows= UdmSQLNumRows(&SQLres), j=0; j<Res->num_rows; j++)
  {
    if (UdmDBNum(Res, j) == dbnum)
    {
      size_t i;
      UDM_DOCUMENT *D=&Res->Doc[j];
      urlid_t      url_id = UdmVarListFindInt(&D->Sections, "ID", 0);
      for(i = 0; i < sqlrows; i++)
      {
        if(url_id == UDM_ATOI(UdmSQLValue(&SQLres,i,0)))
          SQLResToSection(&SQLres, &D->Sections, i);
      }
    }
  }
  UdmSQLFree(&SQLres);
  return UDM_OK;
}


static int
UdmDocAddURLInfo(UDM_DOCUMENT *D, UDM_DB *db, const char *qbuf)
{
  UDM_SQLRES SQLres;
  int rc;
  size_t row;
  
  if(UDM_OK != (rc= UdmSQLQuery(db,&SQLres,qbuf)))
    return rc;

  for(row= 0; row < UdmSQLNumRows(&SQLres); row++)
    SQLResToSection(&SQLres, &D->Sections, row);
  UdmSQLFree(&SQLres);
  return rc;
}



int
UdmResAddDocInfoSQL(UDM_AGENT *query, UDM_DB *db, UDM_RESULT *Res, size_t dbnum)
{
  size_t      i;
  UDM_SQLRES  SQLres;
  int         rc= UDM_OK;
  int         use_showcnt = !strcasecmp(UdmVarListFindStr(&query->Conf->Vars, "PopRankUseShowCnt", "no"), "yes");
  int         use_category = UdmVarListFindStr(&query->Conf->Vars, "cat", NULL) ? 1 : 0;
  double      pr, ratio = 0.0;
  const       char *hi_priority= db->DBType == UDM_DB_MYSQL ? "HIGH_PRIORITY" : "";
  int         use_urlinfo= UdmVarListFindBool(&query->Conf->Vars, "LoadURLInfo", 1);
  int         use_urlbasicinfo= UdmVarListFindBool(&query->Conf->Vars, "LoadURLBasicInfo", 1);
  int         use_taginfo= UdmVarListFindBool(&query->Conf->Vars, "LoadTagInfo", 0);
  if(!Res->num_rows)return UDM_OK;
  if (use_showcnt) ratio = UdmVarListFindDouble(&query->Conf->Vars, "PopRankShowCntRatio", 25.0);
  
  if (use_showcnt)
    UdmLog(query, UDM_LOG_DEBUG, "use_showcnt: %d  ratio: %f", use_showcnt, ratio);
  
  for (i= 0; i < Res->num_rows; i++)
  {
    UdmVarListReplaceInt(&Res->Doc[i].Sections, "id",
                         Res->URLData.Item[i + Res->first].url_id);
  }
  
  if(db->DBSQL_IN)
  {
    size_t  j, sqlrows;
    UDM_DSTR  in_list, qq;
    
    UdmDSTRInit(&in_list, 1024);
    UdmDSTRInit(&qq, 1024);
    
    /* Compose IN string and set to zero url_id field */
    for(i=0; i < Res->num_rows; i++)
    {
      if (UdmDBNum(Res, i) == dbnum)
      {
        const char *comma= in_list.size_data ? "," : "";
        const char *squot= db->DBType == UDM_DB_PGSQL ? "'" : "";
        UdmDSTRAppendf(&in_list, "%s%s%i%s", comma, squot,
                UdmVarListFindInt(&Res->Doc[i].Sections, "ID", 0), squot);
      }
    }
    
    if (!in_list.size_data)
      goto ret_in;
    
    if (use_urlbasicinfo)
    {
      UdmDSTRReset(&qq);
      UdmDSTRAppendf(&qq,
                     "SELECT %s"
                     " rec_id,url,last_mod_time,docsize,"
                     " next_index_time,referrer,crc32,site_id,pop_rank"
                     " FROM url WHERE rec_id IN (%s)", hi_priority, in_list.data);
      if (UDM_OK != (rc= UdmSQLQuery(db, &SQLres, qq.data)))
        goto ret_in;
      
      for(sqlrows= UdmSQLNumRows(&SQLres), j=0; j<Res->num_rows; j++)
      {
        if (UdmDBNum(Res, j) == dbnum)
        {
          UDM_DOCUMENT *D= &Res->Doc[j];
          urlid_t      url_id= UdmVarListFindInt(&D->Sections, "ID", 0);
          for(i = 0; i < sqlrows; i++)
          {
            if(url_id == UDM_ATOI(UdmSQLValue(&SQLres,i,0)))
            {
              SQLResToDoc(query->Conf, D, &SQLres, i);
              if (use_showcnt &&
                  (pr= atof(UdmVarListFindStr(&D->Sections, "Score", "0.0"))) >= ratio)
                  UpdateShows(db, url_id);
              break;
            }
          }
        }
      } 
      UdmSQLFree(&SQLres);
    }
    
    if (use_category)
    {
      UdmDSTRReset(&qq);
      UdmDSTRAppendf(&qq,
                     "SELECT u.rec_id,'Category' as sname,c.path "
                     "FROM url u,server s,categories c "
                     "WHERE u.rec_id "
                     "IN (%s) AND u.server_id=s.rec_id AND s.category=c.rec_id",
                     in_list.data); 
      if (UDM_OK != (rc= UdmResAddURLInfoUsingIN(Res, db, dbnum, qq.data)))
        goto ret_in;
    }
    
    if (use_taginfo)
    {
      UdmDSTRReset(&qq);
      UdmDSTRAppendf(&qq,
                     "SELECT u.rec_id, 'tag', tag FROM url u, server s "
                     "WHERE  u.rec_id in (%s) AND u.server_id=s.rec_id",
                     in_list.data); 
      if (UDM_OK != (rc= UdmResAddURLInfoUsingIN(Res, db, dbnum, qq.data)))
        return rc;
    }
    
    if (use_urlinfo)
    {
      UdmDSTRReset(&qq);
      UdmDSTRAppendf(&qq,
                     "SELECT url_id,sname,sval "
                     "FROM urlinfo WHERE url_id IN (%s)",
                     in_list.data);
      if (UDM_OK != (rc= UdmResAddURLInfoUsingIN(Res, db, dbnum, qq.data)))
        return rc;
    }
    
ret_in:
      UdmDSTRFree(&in_list);
      UdmDSTRFree(&qq);
      return rc;
  }
  else
  {
    char  qbuf[1024*4];

    for(i=0;i<Res->num_rows;i++)
    {
      UDM_DOCUMENT  *D=&Res->Doc[i];
      urlid_t  url_id= UdmVarListFindInt(&D->Sections, "ID", 0);
      
      if (UdmDBNum(Res, i) != dbnum)
        continue;
      
      sprintf(qbuf,"SELECT rec_id,url,last_mod_time,docsize,next_index_time,referrer,crc32,site_id,pop_rank FROM url WHERE rec_id=%i", url_id);
      if(UDM_OK!=(rc=UdmSQLQuery(db,&SQLres,qbuf)))
        return rc;
      
      if(UdmSQLNumRows(&SQLres))
      {
        SQLResToDoc(query->Conf, D, &SQLres, 0);
          if (use_showcnt &&
              (pr= atof(UdmVarListFindStr(&D->Sections, "Score", "0.0"))) >= ratio)
              UpdateShows(db, url_id);
      }
      UdmSQLFree(&SQLres);
      
      if (use_category)
      {
        sprintf(qbuf,"SELECT u.rec_id,c.path FROM url u,server s,categories c WHERE rec_id=%i AND u.server_id=s.rec_id AND s.category=c.rec_id", url_id);
        if(UDM_OK!=(rc=UdmSQLQuery(db,&SQLres,qbuf)))
          return rc;
        if(UdmSQLNumRows(&SQLres))
        {
          UdmVarListReplaceStr(&D->Sections, "Category", UdmSQLValue(&SQLres, i, 1));
        }
        UdmSQLFree(&SQLres);
      }
      
      if (use_taginfo)
      {
        udm_snprintf(qbuf, sizeof(qbuf),
                     "SELECT u.rec_id, 'tag', tag FROM url u, server s "
                     "WHERE  u.rec_id=%d AND u.server_id=s.rec_id", url_id); 
        if(UDM_OK != (rc= UdmDocAddURLInfo(D, db, qbuf)))
          return rc;
      }
      
      if (use_urlinfo)
      {
        sprintf(qbuf,"SELECT url_id,sname,sval FROM urlinfo WHERE url_id=%i", url_id);
        if(UDM_OK != (rc= UdmDocAddURLInfo(D, db, qbuf)))
          return rc;
      }
    }
  }
  return(UDM_OK);
}




/************************* Misc *******************************************/


int
UdmResActionSQL(UDM_AGENT *Agent, UDM_RESULT *Res, int cmd, UDM_DB *db, size_t dbnum)
{
  switch(cmd)
  {
    case UDM_RES_ACTION_DOCINFO:
      return UdmResAddDocInfoSQL(Agent, db, Res, dbnum);
    case UDM_RES_ACTION_SUGGEST:
      return UdmResSuggest(Agent, db, Res, dbnum);
    default:
      UdmLog(Agent, UDM_LOG_ERROR, "Unsupported Res Action SQL");
      return UDM_ERROR;
  }
}


int
UdmCatActionSQL(UDM_AGENT *Agent, UDM_CATEGORY *Cat, int cmd,UDM_DB *db)
{
  switch(cmd)
  {
    case UDM_CAT_ACTION_LIST:
      return UdmCatList(Agent,Cat,db);
    case UDM_CAT_ACTION_PATH:
      return UdmCatPath(Agent,Cat,db);
    default:
              UdmLog(Agent, UDM_LOG_ERROR, "Unsupported Cat Action SQL");
      return UDM_ERROR;
  }
}


int
UdmCheckUrlidSQL(UDM_AGENT *Agent, UDM_DB *db, urlid_t id)
{
  UDM_SQLRES SQLRes;
  char qbuf[128];
  unsigned int rc = 0;

  udm_snprintf(qbuf, sizeof(qbuf), "SELECT rec_id FROM url WHERE rec_id=%d", id);
  rc = UdmSQLQuery(db, &SQLRes, qbuf);
  if(UDM_OK != rc)
  {
    rc = 1;
  }
  else
  {
    if (UdmSQLNumRows(&SQLRes) != 0) rc = 1;
    else rc = 0;
  }
  UdmSQLFree(&SQLRes);
  return rc;
}


int
UdmExportSQL(UDM_AGENT *Indexer, UDM_DB *db)
{
  UDM_SQLRES SQLRes;
  int rc;
  UDM_PSTR row[24];

  printf("<database>\n");
  printf("<urlList>\n");
  rc= UdmSQLExecDirect(db, &SQLRes, "SELECT * FROM url");
  if (rc != UDM_OK) return(rc);
  while (db->sql->SQLFetchRow(db, &SQLRes, row) == UDM_OK)
  {
    printf(
      "<url "
      "rec_id=\"%s\" "
      "status=\"%s\" "
      "docsize=\"%s\" "
      "next_index_time=\"%s\" "
      "last_mod_time=\"%s\" "
      "referrer=\"%s\" "
      "hops=\"%s\" "
      "crc32=\"%s\" "
      "seed=\"%s\" "
      "bad_since_time=\"%s\" "
      "site_id=\"%s\" "
      "server_id=\"%s\" "
      "shows=\"%s\" "
      "pop_rank=\"%s\" "
      "url=\"%s\" "
      "/>\n",
      row[0].val, row[1].val, row[2].val, row[3].val,
      row[4].val, row[5].val, row[6].val, row[7].val,
      row[8].val, row[9].val, row[10].val, row[11].val,
      row[12].val, row[13].val, row[14].val);
  }
  UdmSQLFree(&SQLRes);
  printf("</urlList>\n");

  printf("<linkList>\n");
  rc= UdmSQLExecDirect(db, &SQLRes, "SELECT * FROM links");
  if (rc != UDM_OK) return(rc);
  while (db->sql->SQLFetchRow(db, &SQLRes, row) == UDM_OK)
  {
    printf(
      "<link "
      "ot=\"%s\" "
      "k=\"%s\" "
      "weight=\"%s\" "
      "/>\n",
      row[0].val, row[1].val, row[2].val);
  }
  UdmSQLFree(&SQLRes);
  printf("</linkList>\n");

  printf("</database>\n");
  return(0);
}


static int
UdmDocPerSite(UDM_AGENT *A, UDM_DOCUMENT *D, UDM_DB *db)
{
  char qbuf[1024];
  const char *s, *hostinfo= UdmVarListFindStr(&D->Sections, "Hostinfo", NULL);
  int rc, num, prevnum= UdmVarListFindInt(&D->Sections, "DocPerSite", 0);
  UDM_SQLRES SQLRes;
  
  if (!hostinfo)
    return UDM_OK;
  
  for (s= hostinfo; s[0]; s++)
  {
    /*
      Host name good characters: digits, letters, hyphen (-).
      Just check the worst characters.
    */
    if (*s == '\'' || *s == '\"')
    {
      num= 1000000;
      goto ret;
    }
  }
  udm_snprintf(qbuf, sizeof(qbuf),
               "SELECT COUNT(*) FROM url WHERE url LIKE '%s%%'", hostinfo);
  
  if(UDM_OK!= (rc= UdmSQLQuery(db, &SQLRes, qbuf)))
    return rc;
  num= prevnum + atoi(UdmSQLValue(&SQLRes, 0, 0));
  UdmSQLFree(&SQLRes);
ret:
  UdmVarListReplaceInt(&D->Sections, "DocPerSite", num);
  return UDM_OK;
}

 
static int
UdmImportSection(UDM_AGENT *A, UDM_DOCUMENT *D, UDM_DB *db)
{
  UDM_TEXTITEM Item;
  UDM_VAR *Sec;
  UDM_VARLIST Vars;
  UDM_SQLRES SQLRes;
  UDM_DSTR d;
  int rc;
  size_t row, rows, cols;
  const char *fmt= UdmVarListFindStr(&D->Sections, "SQLImportSection", NULL);
  
  if (!fmt)
    return UDM_OK;
  
  UdmDSTRInit(&d, 1024);
  UdmVarListInit(&Vars);
  UdmVarListSQLEscape(&Vars, &D->Sections, db);
  UdmDSTRParse(&d, fmt, &Vars);
  UdmVarListFree(&Vars);
  if(UDM_OK!= (rc= UdmSQLQuery(db, &SQLRes, d.data)))
    return rc;

  cols= UdmSQLNumCols(&SQLRes);
  bzero((void*)&Item, sizeof(Item));
  for (row=0, rows= UdmSQLNumRows(&SQLRes); row < rows; row++)
  {
    size_t col;
    for (col= 0; col + 1 < cols; col+= 2)
    {
      Item.section_name= (char*) UdmSQLValue(&SQLRes, row, col);
      if ((Sec= UdmVarListFind(&D->Sections, Item.section_name)))
      {
        Item.str= (char*) UdmSQLValue(&SQLRes, row, col + 1);
        Item.section= Sec->section;
        UdmTextListAdd(&D->TextList, &Item);
      }
    }
  }
  
  UdmDSTRFree(&d);
  UdmSQLFree(&SQLRes);
  return rc;
}


static int
UdmGetReferers(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc, UDM_DB *db)
{
  size_t    i,j;
  char    qbuf[2048];
  UDM_SQLRES  SQLres;
  const char  *where;
  int    rc;

  UDM_LOCK_CHECK_OWNER(Indexer, UDM_LOCK_CONF);
  where = UdmSQLBuildWhereCondition(Indexer->Conf, db);
  
  udm_snprintf(qbuf,sizeof(qbuf),"SELECT url.status,url2.url,url.url FROM url,url url2%s WHERE url.referrer=url2.rec_id %s %s",
    db->from, where[0] ? "AND" : "", where);
  
  if(UDM_OK!=(rc=UdmSQLQuery(db,&SQLres,qbuf)))return rc;
  
  j=UdmSQLNumRows(&SQLres);
  for(i=0;i<j;i++)
  {
    if(Indexer->Conf->RefInfo)Indexer->Conf->RefInfo(
      atoi(UdmSQLValue(&SQLres,i,0)),
      UdmSQLValue(&SQLres,i,2),
      UdmSQLValue(&SQLres,i,1)
    );
  }
  UdmSQLFree(&SQLres);
  return rc;
}


static int
UdmGetDocCount(UDM_AGENT * Indexer, UDM_DOCUMENT *Doc, UDM_DB *db)
{
  char    qbuf[200]="";
  UDM_SQLRES  SQLres;
  int    rc;
  
  sprintf(qbuf,NDOCS_QUERY);
  if(UDM_OK!=(rc=UdmSQLQuery(db,&SQLres,qbuf)))return rc;
  
  if(UdmSQLNumRows(&SQLres))
  {
    const char * s;
    s=UdmSQLValue(&SQLres,0,0);
    if(s)Indexer->doccount += atoi(s);
  }
  UdmSQLFree(&SQLres);
  return(UDM_OK);
}


int
UdmStatActionSQL(UDM_AGENT *Indexer,UDM_STATLIST *Stats,UDM_DB *db)
{
  size_t    i,j,n;
  char    qbuf[2048];
  UDM_SQLRES  SQLres;
  int    have_group= (db->flags & UDM_SQL_HAVE_GROUPBY);
  const char  *where;
  int    rc=UDM_OK;
  
  if(db->DBType==UDM_DB_IBASE)
    have_group=0;

  UDM_LOCK_CHECK_OWNER(Indexer, UDM_LOCK_CONF);
  where = UdmSQLBuildWhereCondition(Indexer->Conf, db);

  if(have_group)
  {
    char func[128];
    
    switch(db->DBType)
    {
      case UDM_DB_MYSQL:
        udm_snprintf(func,sizeof(func)-1, "next_index_time<=%d", Stats->time);
        break;
    
      case UDM_DB_PGSQL:
      case UDM_DB_MSSQL:
      case UDM_DB_SYBASE:
      case UDM_DB_DB2:
      case UDM_DB_SQLITE:
      case UDM_DB_SQLITE3:
      default:
        udm_snprintf(func,sizeof(func)-1, "case when next_index_time<=%d then 1 else 0 end", Stats->time);
        break;

      case UDM_DB_ACCESS:
        udm_snprintf(func,sizeof(func)-1, "IIF(next_index_time<=%d, 1, 0)", Stats->time);
        break;

      case UDM_DB_ORACLE8:
      case UDM_DB_SAPDB:
        udm_snprintf(func,sizeof(func)-1, "DECODE(SIGN(%d-next_index_time),-1,0,1,1)", Stats->time);
        break;
    }

    udm_snprintf(qbuf, sizeof(qbuf) - 1,
                 "SELECT status, SUM(%s), count(*) FROM url%s %s%s GROUP BY status ORDER BY status",
                 func, db->from, where[0] ? "WHERE " : "", where);

    if(UDM_OK!=(rc=UdmSQLQuery(db,&SQLres,qbuf)))return rc;
    
    if((n = UdmSQLNumRows(&SQLres)))
    {
      for(i = 0; i < n; i++)
      {
        for(j=0;j<Stats->nstats;j++)
        {
          if(Stats->Stat[j].status==atoi(UdmSQLValue(&SQLres,i,0)))
          {
            Stats->Stat[j].expired += atoi(UdmSQLValue(&SQLres,i,1));
            Stats->Stat[j].total += atoi(UdmSQLValue(&SQLres,i,2));
            break;
          }
        }
        if(j==Stats->nstats)
        {
          UDM_STAT  *S;
        
          Stats->Stat=(UDM_STAT*)UdmRealloc(Stats->Stat,(Stats->nstats+1)*sizeof(Stats->Stat[0]));
          S=&Stats->Stat[Stats->nstats];
          S->status=atoi(UdmSQLValue(&SQLres,i,0));
          S->expired=atoi(UdmSQLValue(&SQLres,i,1));
          S->total=atoi(UdmSQLValue(&SQLres,i,2));
          Stats->nstats++;
        }
      }
    }
    UdmSQLFree(&SQLres);
    
  }else{
/*  
  FIXME: learn how to get it from SOLID and IBASE
  (HAVE_IBASE || HAVE_SOLID || HAVE_VIRT )
*/
    
    udm_snprintf(qbuf,sizeof(qbuf)-1,"SELECT status,next_index_time FROM url%s %s%s ORDER BY status",
      db->from, where[0] ? "WHERE " : "", where);

    if(UDM_OK!=(rc=UdmSQLQuery(db,&SQLres,qbuf)))return rc;
    
    for(i=0;i<UdmSQLNumRows(&SQLres);i++)
    {
      for(j=0;j<Stats->nstats;j++)
      {
        if(Stats->Stat[j].status==atoi(UdmSQLValue(&SQLres,i,0)))
        {
          if((time_t)UDM_ATOU(UdmSQLValue(&SQLres,i,1)) <= Stats->time)
            Stats->Stat[j].expired++;
          Stats->Stat[j].total++;
          break;
        }
      }
      if(j==Stats->nstats)
      {
        Stats->Stat=(UDM_STAT *)UdmRealloc(Stats->Stat,sizeof(UDM_STAT)*(Stats->nstats+1));
        Stats->Stat[j].status = UDM_ATOI(UdmSQLValue(&SQLres,i,0));
        Stats->Stat[j].expired=0;
        if((time_t)UDM_ATOU(UdmSQLValue(&SQLres,i,1)) <= Stats->time)
          Stats->Stat[j].expired++;
        Stats->Stat[j].total=1;
        Stats->nstats++;
      }
    }
    UdmSQLFree(&SQLres);
  }
  return rc;
}


static int
UdmURLInfoDumpDoc(UDM_AGENT *Indexer, UDM_DB *db, UDM_DOCUMENT *Doc)
{
  int rc;
  char buf[64];
  size_t i;
  UDM_SQLRES SQLRes;
  UDM_DSTR dbuf;
  urlid_t url_id= UdmVarListFindInt(&Doc->Sections, "ID", 0);

  udm_snprintf(buf, sizeof(buf),
               "SELECT sname, sval FROM urlinfo WHERE url_id=%d", url_id);
  if (UDM_OK != (rc= UdmSQLQuery(db, &SQLRes, buf)))
    return rc;
  
  UdmDSTRInit(&dbuf, 256);
  
  for (i= 0; i < UdmSQLNumRows(&SQLRes); i++)
  {
    const char *sname= UdmSQLValue(&SQLRes, i, 0);
    const char *sval= UdmSQLValue(&SQLRes, i, 1);
    size_t slen= UdmSQLLen(&SQLRes, i, 1);
    UdmDocInsertSectionsUsingEscapeBuildQuery(db, 0, sname, sval, slen, &dbuf);
    printf("%s;\n", dbuf.data);
  }
  UdmSQLFree(&SQLRes);
  UdmDSTRFree(&dbuf);
  return UDM_OK;
}


static int
UdmDumpData(UDM_AGENT *A, UDM_DOCUMENT *DocUnunsed, UDM_DB *db)
{
  char buf[256];
  UDM_SQLRES SQLRes;
  size_t i, nrows;
  int rc;
  const char *where= UdmSQLBuildWhereCondition(A->Conf, db);
  UDM_DSTR eurl;
  
  UDM_ASSERT(db->dbmode_handler->DumpWordInfo != NULL);
  
  UdmDSTRInit(&eurl, 256);
  udm_snprintf(buf, sizeof(buf),
               "SELECT %s FROM url%s%s", select_url_str_for_dump,
               where[0] ? " WHERE " : "", where);
  if (UDM_OK != (rc= UdmSQLQuery(db, &SQLRes, buf)))
    goto ret;
  
  nrows= UdmSQLNumRows(&SQLRes);
  
  for(i= 0; i < nrows; i++)
  {
    UDM_DOCUMENT Doc;
    UdmDocInit(&Doc);
    if (UDM_OK != UdmTargetSQLResDump(A, db, &Doc, &SQLRes, i, &eurl))
      goto ret;
    if (UDM_OK != (rc= UdmURLInfoDumpDoc(A, db, &Doc)))
      goto ret;
    if (UDM_OK != (rc= db->dbmode_handler->DumpWordInfo(A, db, &Doc)))
      goto ret;
    UdmDocFree(&Doc);
  }
  
ret:
  UdmDSTRFree(&eurl);
  return rc;
}


static int
UdmRestoreData(UDM_AGENT *A, UDM_DOCUMENT *Doc, UDM_DB *db)
{
  size_t i;
  int rc;
  
  for (i= 0; i < Doc->Sections.nvars; i++)
  {
    UDM_VAR *S= &Doc->Sections.Var[i];
    printf("%s[%d]=%s\n", S->name, S->curlen, S->val);
    S->maxlen= S->curlen; /* TODO: init sections from indexer.conf */
  }

  if (UDM_OK != (rc= UdmAddURL(A, Doc, db)))
    goto ex;
  if (UDM_OK != (rc= UdmFindURL(A, Doc, db)))
    goto ex;
  if (UDM_OK != (rc= UdmLongUpdateURL(A, Doc, db)))
    goto ex;

  printf("\n");

ex:
  return rc;
}

/******************* URL handlers **************************/

int (*udm_url_action_handlers[])(UDM_AGENT *A, UDM_DOCUMENT *D, UDM_DB *db)=
{
  UdmDeleteURL,           /* UDM_URL_ACTION_DELETE */
  UdmAddURL,              /* UDM_URL_ACTION_ADD */
  UdmUpdateUrl,           /* UDM_URL_ACTION_SUPDATE */
  UdmLongUpdateURL,       /* UDM_URL_ACTION_LUPDATE */
  UdmDeleteWordsAndLinks, /* UDM_URL_ACTION_DUPDATE */
  UdmUpdateClone,         /* UDM_URL_ACTION_UPDCLONE */
  UdmRegisterChild,       /* UDM_URL_ACTION_REGCHILD */
  UdmFindURL,             /* UDM_URL_ACTION_FINDBYURL */
  UdmFindMessage,         /* UDM_URL_ACTION_FINDBYMSG */
  UdmFindOrigin,          /* UDM_URL_ACTION_FINDORIG */
  UdmMarkForReindex,      /* UDM_URL_ACTION_EXPIRE */
  UdmGetReferers,         /* UDM_URL_ACTION_REFERERS */
  UdmGetDocCount,         /* UDM_URL_ACTION_DOCCOUNT */
  UdmDeleteLinks,         /* UDM_URL_ACTION_LINKS_DELETE */
  UdmAddLink,             /* UDM_URL_ACTION_ADD_LINK */
  UdmGetCachedCopy,       /* UDM_URL_ACTION_GET_CACHED_COPY */
  UdmWordStatCreate,      /* UDM_URL_ACTION_WRDSTAT */
  UdmDocPerSite,          /* UDM_URL_ACTION_DOCPERSITE */
  UdmImportSection,       /* UDM_URL_ACTION_SQLIMPORTSEC */
  NULL,                   /* UDM_URL_ACTION_FLUSH */
  UdmDumpData,            /* UDM_URL_ACTION_DUMPDATA */
  UdmRestoreData          /* UDM_URL_ACTION_RESTOREDATA */
};



#endif /* HAVE_SQL */

