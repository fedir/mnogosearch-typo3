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
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>

#ifdef WIN32
#include <time.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_LIBUTIL_H
#include <libutil.h>
#endif

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_log.h"
#include "udm_conf.h"
#include "udm_indexer.h"
#include "udm_robots.h"
#include "udm_db.h"
#include "udm_url.h"
#include "udm_parser.h"
#include "udm_proto.h"
#include "udm_hrefs.h"
#include "udm_mutex.h"
#include "udm_crc32.h"
#include "udm_hash.h"
#include "udm_xmalloc.h"
#include "udm_http.h"
#include "udm_host.h"
#include "udm_server.h"
#include "udm_alias.h"
#include "udm_word.h"
#include "udm_crossword.h"
#include "udm_parsehtml.h"
#include "udm_parsexml.h"
#include "udm_spell.h"
#include "udm_execget.h"
#include "udm_agent.h"
#include "udm_match.h"
#include "udm_doc.h"
#include "udm_result.h"
#include "udm_parsedate.h"
#include "udm_unicode.h"
#include "udm_contentencoding.h"
#include "udm_vars.h"
#include "udm_guesser.h"
#include "udm_textlist.h"
#include "udm_id3.h"
#include "udm_stopwords.h"
#include "udm_wild.h"
#ifdef HAVE_ZLIB
#include "udm_store.h"
#endif

/* This should be last include */
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define UDM_THREADINFO(A,s,m)	if(A->Conf->ThreadInfo)A->Conf->ThreadInfo(A,s,m)

/***************************************************************************/

static size_t UdmDocPerSiteCached(UDM_AGENT *Indexer,
                                  const char *hostinfo, size_t hostinfo_length)
{
  size_t i, res= 0;
  
  UDM_HREFLIST *Hrefs;
  for (Hrefs= &Indexer->Conf->Hrefs, i= 0; i < Hrefs->nhrefs; i++)
  {
    UDM_HREF *Href= &Hrefs->Href[i];
    if (Href->stored && Href->method != UDM_METHOD_DISALLOW &&
        !strncmp(Href->url, hostinfo, hostinfo_length))
      res++;
  }
  return res;
}

static int UdmCheckDocPerSite(UDM_AGENT *Indexer, UDM_HREF *Href, size_t *res,
                              const char *hostinfo, size_t hostinfo_length)
{
  int rc= UDM_OK;

  UDM_LOCK_CHECK_OWNER(Indexer, UDM_LOCK_CONF);
  *res= UdmDocPerSiteCached(Indexer, hostinfo, hostinfo_length);

  if (*res < Href->max_doc_per_site)
  {
    size_t doc_per_site_db;
    UDM_DOCUMENT Doc;
    UdmDocInit(&Doc);
    UdmVarListAddStr(&Doc.Sections, "Hostinfo", hostinfo);
    rc= UdmURLActionNoLock(Indexer, &Doc, UDM_URL_ACTION_DOCPERSITE);
    doc_per_site_db= UdmVarListFindInt(&Doc.Sections, "DocPerSite", 0);
    UdmDocFree(&Doc);
    *res+= doc_per_site_db;
  }
  return rc;
}


static int
UdmHrefsCheckDocPerSite(UDM_AGENT *Indexer)
{
  size_t i;
  int rc= UDM_OK;
  UDM_HREFLIST *Hrefs;
  size_t doc_per_site= 0;
  size_t hostinfo_length= 0;
  char hostinfo[128]= "";
  
  UDM_LOCK_CHECK_OWNER(Indexer, UDM_LOCK_CONF);
  
  Hrefs= &Indexer->Conf->Hrefs;

  for (i= 0; i < Hrefs->nhrefs; i++)
  {
    UDM_HREF *H = &Hrefs->Href[i];
    if (!H->stored)
    {
      if (H->max_doc_per_site)
      {
        if (!hostinfo[0] || strncmp(hostinfo, H->url, hostinfo_length))
        {
          /* New site */
          UDM_URL url;
          UdmURLInit(&url);
          UdmURLParse(&url, H->url);
          hostinfo_length= udm_snprintf(hostinfo, sizeof(hostinfo),
                                       "%s://%s/", url.schema, url.hostinfo);
          rc= UdmCheckDocPerSite(Indexer, H, &doc_per_site,
                                 hostinfo, hostinfo_length);
          UdmLog(Indexer, UDM_LOG_DEBUG, "DocPerSite: %d/%d",
                 (int) doc_per_site, (int) H->max_doc_per_site);
          UdmURLFree(&url);
      
          if (rc != UDM_OK)
            goto ret;
        }
        else
        {
          /* Same site */
          doc_per_site++;
        }
        if (doc_per_site > H->max_doc_per_site)
        {
          UdmLog(Indexer, UDM_LOG_DEBUG,
                 "Too many docs (%d) per site, skip it", (int) doc_per_site);
          H->method = UDM_METHOD_DISALLOW;
          H->stored= 1;
        }
      }
    }
  }
ret:

  return rc;
}


__C_LINK int __UDMCALL
UdmStoreHrefs(UDM_AGENT *Indexer)
{
  int rc= UDM_ERROR;
  
  UDM_GETLOCK(Indexer, UDM_LOCK_CONF);

  if (UDM_OK != (rc= UdmHrefsCheckDocPerSite(Indexer)))
    goto ret; 
  
#ifdef HAVE_SQL
  rc= UdmStoreHrefsSQL(Indexer);
#endif

ret:

  UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);
  return rc;
}


static void RelLink(UDM_URL *curURL, UDM_URL *newURL, char **str)
{
  const char  *schema = newURL->schema ? newURL->schema : curURL->schema;
  const char  *hostinfo = newURL->hostinfo ? newURL->hostinfo : curURL->hostinfo;
  const char  *path = (newURL->path && newURL->path[0]) ? newURL->path : curURL->path;
  const char  *fname = ((newURL->filename && newURL->filename[0]) || 
                       (newURL->path && newURL->path[0])) ? 
                       newURL->filename : curURL->filename;
  char    *pathfile;

  if (newURL->filename && *newURL->filename == '?' && (! newURL->path || ! newURL->path[0]))
  {
    char *qm = strchr(UDM_NULL2EMPTY(curURL->filename), '?');
    char qf = 0;
    if (qm) { *qm = 0; qf = 1; }
    pathfile = (char*)UdmMalloc(strlen(UDM_NULL2EMPTY(path)) + strlen(UDM_NULL2EMPTY(fname)) + strlen(UDM_NULL2EMPTY(curURL->filename)) + 5);
    if (pathfile == NULL) return;
    sprintf(pathfile, "/%s%s%s",  UDM_NULL2EMPTY(path), UDM_NULL2EMPTY(curURL->filename), UDM_NULL2EMPTY(fname));
    if (qf) *qm = '?';
  }
  else
  {
    pathfile = (char*)UdmMalloc(strlen(UDM_NULL2EMPTY(path)) + strlen(UDM_NULL2EMPTY(fname)) + 5);
    if (pathfile == NULL) return;
    sprintf(pathfile, "/%s%s",  UDM_NULL2EMPTY(path), UDM_NULL2EMPTY(fname));
  }

  
  UdmURLNormalizePath(pathfile);

  if (!strcasecmp(UDM_NULL2EMPTY(schema), "mailto") || 
      !strcasecmp(UDM_NULL2EMPTY(schema), "javascript")) 
  {
    *str = (char*)UdmMalloc(strlen(UDM_NULL2EMPTY(schema)) + strlen(UDM_NULL2EMPTY(newURL->specific)) + 4);
    sprintf(*str, "%s:%s", UDM_NULL2EMPTY(schema), UDM_NULL2EMPTY(newURL->specific));
  }
  else if(!strcasecmp(UDM_NULL2EMPTY(schema), "htdb"))
  {
    *str = (char*)UdmMalloc(strlen(UDM_NULL2EMPTY(schema)) + strlen(pathfile) + 4);
    sprintf(*str, "%s:%s", UDM_NULL2EMPTY(schema), pathfile);
  }
  else
  {
    *str = (char*)UdmMalloc(strlen(UDM_NULL2EMPTY(schema)) + strlen(pathfile) + strlen(UDM_NULL2EMPTY(hostinfo)) + 8);
    sprintf(*str, "%s://%s%s", UDM_NULL2EMPTY(schema), UDM_NULL2EMPTY(hostinfo), pathfile);
  }
  
  if(!strncmp(*str, "ftp://", 6) && (strstr(*str, ";type=")))
    *(strstr(*str, ";type")) = '\0';
  UDM_FREE(pathfile);
}

static int UdmDocBaseHref(UDM_AGENT *Indexer,UDM_DOCUMENT *Doc){
  const char  *basehref=UdmVarListFindStr(&Doc->Sections,"base.href",NULL);
  /*
    <BASE HREF="xxx"> stuff
    Check that URL is properly formed baseURL is just temporary variable
    If parsing  fails we'll use old base href, passed via CurURL
    Note that we will not check BASE if delete_no_server is unset
    This is  actually dirty hack. We must check that hostname is the same
  */
  
  if(basehref /*&& (Doc->Spider.follow==UDM_FOLLOW_WORLD)*/)
  {
    UDM_URL    baseURL;
    int    parse_res;
    
    UdmURLInit(&baseURL);
    
    if (UDM_URL_OK == (parse_res= UdmURLParse(&baseURL, basehref)) &&
        UDM_URL_OK == (parse_res= (baseURL.schema ? UDM_URL_OK : UDM_URL_BAD)))
    {
      UdmURLParse(&Doc->CurURL, basehref);
      UdmLog(Indexer, UDM_LOG_DEBUG, "BASE HREF '%s'", basehref);
    }
    else
    {
      UdmLog(Indexer, UDM_LOG_ERROR, "BASE HREF: %s: '%s'",
                                      UdmURLErrorStr(parse_res), basehref);
    }
    UdmURLFree(&baseURL);
  }
  return UDM_OK;
}


static int
UdmFilterFind(UDM_MATCHLIST *L,const char *newhref,char *reason)
{
  UDM_MATCH  *M;
  int  res= UDM_METHOD_GET;
  
  UdmDebugEnter();
  
  if ((M= UdmMatchListFind(L, newhref, 0, NULL)))
  {
    sprintf(reason,"%s %s %s '%s'",
            M->arg, UdmMatchTypeStr(M->match_type),
            M->case_sense == UDM_CASE_INSENSITIVE ? "InSensitive":"Sensitive",
            M->pattern);
    res=UdmMethod(M->arg);
  }
  else
  {
    sprintf(reason,"Allow by default");
  }
  UdmDebugReturn(res);
}


static UDM_MATCH *
UdmMatchSectionListFindWithSubst(UDM_MATCHLIST *L, UDM_DOCUMENT *Doc,
                                 size_t nparts, UDM_MATCH_PART *Parts)
{
  UDM_DSTR dstr;
  size_t i, srclen;
  UdmDSTRInit(&dstr, 128);
  for(i=0;i<L->nmatches;i++)
  {
    UDM_MATCH *M=&L->Match[i];
    const char *src;
    if (strchr(M->section, '$'))
    {
      UdmDSTRReset(&dstr);
      UdmDSTRParse(&dstr, M->section, &Doc->Sections);
      src= dstr.data;
      srclen= dstr.size_data;
    }
    else
    {
      src= UdmVarListFindStr(&Doc->Sections, M->section, "");
      srclen= strlen(src);
    }
    if(!UdmMatchExec(M, src, srclen, src, nparts, Parts))
    {
      UdmDSTRFree(&dstr);
      return M;
    }
  }
  UdmDSTRFree(&dstr);
  return NULL;
}


static int
UdmSectionFilterFind(UDM_MATCHLIST *L, UDM_DOCUMENT *Doc, char *reason)
{
  UDM_MATCH  *M;
  int    res=UDM_METHOD_INDEX;

  UdmDebugEnter();

  if((M=UdmMatchSectionListFindWithSubst(L, Doc, 0, NULL)))
  {
    sprintf(reason,"%s %s %s %s '%s' '%s'", M->arg,
            M->nomatch ? "NoMatch" : "Match",
            UdmMatchTypeStr(M->match_type),
            M->case_sense == UDM_CASE_INSENSITIVE ? "InSensitive" : "Sensitive",
            M->section,
            M->pattern);
    res= UdmMethod(M->arg);
  }
  else
  {
    sprintf(reason,"Allow by default");
  }
  UdmDebugReturn(res);
}


int UdmConvertHref(UDM_AGENT *Indexer, UDM_URL *CurURL,
                   UDM_SPIDERPARAM *Spider, UDM_HREF *Href)
{
  int    parse_res, cascade;
  UDM_URL    newURL;
  char    *newhref = NULL;
  UDM_MATCH  *Alias;
  char    *alias = NULL;
  size_t    aliassize, nparts = 10;
  UDM_MATCH_PART  Parts[10];
  UDM_SERVER  *Srv;
  char    reason[1024]="";
  int rc= UDM_OK;

  UdmDebugEnter();

  UDM_LOCK_CHECK_OWNER(Indexer, UDM_LOCK_CONF);
  
  UdmURLInit(&newURL);

  if ((parse_res= UdmURLParse(&newURL, Href->url)))
  {
    UdmLog(Indexer, UDM_LOG_DEBUG, "%s: '%s'",
                                   UdmURLErrorStr(parse_res), Href->url);
    Href->method= UDM_METHOD_DISALLOW;
    goto ret;
  }
  
  RelLink(CurURL, &newURL, &newhref);
  
  UdmLog(Indexer,UDM_LOG_DEBUG,"Link '%s' %s",Href->url,newhref);
  for(cascade = 0; ((Alias=UdmMatchListFind(&Indexer->Conf->ReverseAliases,newhref,nparts,Parts))) && (cascade < 1024); cascade++) {
          aliassize = strlen(Alias->arg) + strlen(Alias->pattern) + strlen(newhref) + 8;
    alias = (char*)UdmRealloc(alias, aliassize);
    if (alias == NULL) {
      UdmLog(Indexer, UDM_LOG_ERROR, "No memory (%d bytes). %s line %d",
             (int) aliassize, __FILE__, __LINE__);
      goto ret;
    }
    UdmMatchApply(alias,aliassize,newhref,Alias->arg,Alias,nparts,Parts);
    if(alias[0]){
      UdmLog(Indexer,UDM_LOG_DEBUG,"ReverseAlias%d: '%s'", cascade, alias);
      UDM_FREE(newhref);
      newhref = (char*)UdmStrdup(alias);
    } else break;
  }
  
    
  UdmURLParse(&newURL, newhref);
  
  Href->method= UDM_METHOD_GET;
  Href->site_id = 0;
  if(!(Srv = UdmServerFind(Indexer->Conf, &Indexer->Conf->Servers, newhref, NULL))) {
    UdmLog(Indexer,UDM_LOG_DEBUG,"no Server, skip it");
    Href->method=UDM_METHOD_DISALLOW;
    goto ret;
  }
  {
    if (Srv->follow == UDM_FOLLOW_SITE &&
        Spider->follow != UDM_FOLLOW_URLLIST &&
        (strcasecmp(newURL.schema, CurURL->schema) ||
         strcasecmp(newURL.hostinfo, CurURL->hostinfo)))
    {
      UdmLog(Indexer,UDM_LOG_DEBUG, "Skip: sites differ: %s %s",
             newURL.hostinfo, CurURL->hostinfo);
      Href->method= UDM_METHOD_DISALLOW;
    }
  }

  if (Href->method == UDM_METHOD_DISALLOW)
    goto ret;
  
  if (!strcasecmp(UDM_NULL2EMPTY(newURL.schema), "mailto") || !strcasecmp(UDM_NULL2EMPTY(newURL.schema), "javascript")) {
    UdmLog(Indexer,UDM_LOG_DEBUG,"'%s' schema, skip it", newURL.schema);
    Href->method=UDM_METHOD_DISALLOW;
    goto ret;
  }
  
  if (Href->hops > Srv->MaxHops) {
    UdmLog(Indexer, UDM_LOG_DEBUG, "too many hops (%d), skip it", Href->hops);
    Href->method = UDM_METHOD_DISALLOW;
    goto ret;
  }

  UdmLog(Indexer, UDM_LOG_DEBUG, " Server applied: site_id: %d URL: %s", Srv->site_id, Srv->Match.pattern);
  
  /* Check Allow/Disallow/CheckOnly stuff */
  Href->method=UdmFilterFind(&Indexer->Conf->Filters,newhref,reason);
  if(Href->method==UDM_METHOD_DISALLOW){
    UdmLog(Indexer,UDM_LOG_DEBUG,"%s, skip it",reason);
    goto ret;
  }else{
    UdmLog(Indexer,UDM_LOG_DEBUG,"%s",reason);
  }
  
  Href->max_doc_per_site= UdmVarListFindInt(&Srv->Vars, "MaxDocPerSite", 0);
  
  {
    const char *val= UdmVarListFindStr(&Srv->Vars, "SQLExportHref", NULL);
    if (val)
    {
      UdmVarListAddStr(&Href->Vars, "SQLExportHref", val);
    }
  }

  /* FIXME: add MaxHops, Robots */
  UDM_FREE(Href->url);
  Href->url = (char*)UdmStrdup(newhref);
  Href->server_id = Srv->site_id;
ret:  
  UDM_FREE(newhref);
  UDM_FREE(alias);
  UdmURLFree(&newURL);
  UdmDebugReturn(rc);
}

static int UdmDocConvertHrefs(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc)
{
  size_t  i;
  int     hops=UdmVarListFindInt(&Doc->Sections,"Hops",0);
  urlid_t url_id = (urlid_t)UdmVarListFindInt(&Doc->Sections, "ID", 0);
  uint4   maxhops = UdmVarListFindUnsigned(&Doc->Sections, "MaxHops", 255);

  UDM_LOCK_CHECK_OWNER(Indexer, UDM_LOCK_CONF);
  
  for(i=0;i<Doc->Hrefs.nhrefs;i++){
    UDM_HREF  *Href=&Doc->Hrefs.Href[i];
    Href->hops=hops+1;
    UdmConvertHref(Indexer, &Doc->CurURL, &Doc->Spider, Href);
    Href->referrer=url_id;
    Href->collect_links= Doc->Spider.collect_links;
    if (maxhops >= Href->hops) {
      Href->stored = 0;
    } else {
      Href->stored = 1;
      Href->method = UDM_METHOD_DISALLOW;
    }
  }
  return UDM_OK;
}


int UdmDocStoreHrefs(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc)
{
  size_t i;

  if(Doc->method==UDM_METHOD_HEAD)
    return UDM_OK;

  UdmDocBaseHref(Indexer,Doc);
  
  UDM_GETLOCK(Indexer, UDM_LOCK_CONF);  
  UdmDocConvertHrefs(Indexer,Doc);

  for(i=0;i<Doc->Hrefs.nhrefs;i++)
  {
    UDM_HREF  *Href=&Doc->Hrefs.Href[i];
    if(Href->method != UDM_METHOD_DISALLOW)
      UdmHrefListAdd(&Indexer->Conf->Hrefs, Href);
  }
  UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);

  return UDM_OK;
}

/*********************** 'UrlFile' stuff (for -f option) *******************/

__C_LINK int __UDMCALL UdmURLFile(UDM_AGENT *Indexer,
                                  const char *fname, int action)
{
  FILE *url_file;
  char str[1024]="";
  char str1[1024]="";
  int result= UDM_OK, res;
  UDM_URL myurl;
  UDM_HREF Href;
  UdmDebugEnter();
  
  UdmURLInit(&myurl);
  
  UDM_GETLOCK(Indexer, UDM_LOCK_CONF);
  
  /* Read lines and clear/insert/check URLs                     */
  /* We've already tested in main.c to make sure it can be read */
  /* FIXME !!! Checking should be done here surely              */
  
  if(!strcmp(fname,"-"))
    url_file=stdin;
  else
    url_file=fopen(fname,"r");
  
  while(fgets(str1,sizeof(str1),url_file)){
    char *end;
    if(!str1[0])continue;
    end=str1+strlen(str1)-1;
    while((end>=str1)&&(*end=='\r'||*end=='\n')){
      *end=0;if(end>str1)end--;
    }
    if(!str1[0])continue;
    if(str1[0]=='#')continue;

    if(*end=='\\'){
      *end=0;strcat(str,str1);
      continue;
    }
    strcat(str,str1);
    strcpy(str1,"");

    switch(action){
    case UDM_URL_FILE_REINDEX:
      UdmVarListReplaceStr(&Indexer->Conf->Vars, "u", str);
      result = UdmURLActionNoLock(Indexer, NULL, UDM_URL_ACTION_EXPIRE);
      if(result!=UDM_OK)
        goto ex;
      UdmVarListDel(&Indexer->Conf->Vars, "u");
      break;
    case UDM_URL_FILE_CLEAR:
      UdmVarListReplaceStr(&Indexer->Conf->Vars, "u", str);
      result=UdmClearDatabase(Indexer);
      if(result!=UDM_OK)
        goto ex;
      UdmVarListDel(&Indexer->Conf->Vars, "u");
      break;
    case UDM_URL_FILE_INSERT:
      UdmHrefInit(&Href);
      Href.url=str;
      Href.method=UDM_METHOD_GET;
      UdmHrefListAdd(&Indexer->Conf->Hrefs, &Href);
      break;
    case UDM_URL_FILE_PARSE:
      if ((UDM_URL_OK != (res= UdmURLParse(&myurl, str))) ||
          (UDM_URL_OK != (res= myurl.schema ? UDM_URL_OK : UDM_URL_BAD)))
      {
        UdmLog(Indexer,UDM_LOG_ERROR,"%s: '%s'", UdmURLErrorStr(res), str);
        result= UDM_ERROR;
        goto ex;
      }
      break;
    }
    str[0]=0;
  }
  if(url_file!=stdin)
    fclose(url_file);
ex:
  UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);
  UdmURLFree(&myurl);
  UdmDebugReturn(result);
}




/*******************************************************************/


static int UdmDocAlias(UDM_AGENT *Indexer,UDM_DOCUMENT *Doc){
  UDM_MATCH  *Alias;
  UDM_MATCH_PART  Parts[10];
  size_t    alstrlen, nparts=10;
  const char  *alias_prog=UdmVarListFindStr(&Indexer->Conf->Vars,"AliasProg",NULL);
  char    *alstr;
  int    result=UDM_OK;
  const char  *url=UdmVarListFindStr(&Doc->Sections,"URL","");

  alstrlen = 256 + strlen(url);
  if ((alstr = (char*)UdmMalloc(alstrlen)) == NULL) return UDM_ERROR;
  alstr[0] = '\0';
  if(alias_prog){
    result = UdmAliasProg(Indexer, alias_prog, url, alstr, alstrlen - 1);
    UdmLog(Indexer,UDM_LOG_EXTRA,"AliasProg result: '%s'",alstr);
    if(result!=UDM_OK) {
                        UDM_FREE(alstr);
                        return(result);
                }
  }

  /* Find alias when aliastr is empty, i.e.     */
  /* when there is no alias in "Server" command */
  /* and no AliasProg                           */
  if(! alstr[0] && (Alias=UdmMatchListFind(&Indexer->Conf->Aliases,url,nparts,Parts))){
    UdmMatchApply(alstr, alstrlen - 1, url, Alias->arg, Alias, nparts, Parts);
  }
  if(alstr[0]){
    UdmVarListReplaceStr(&Doc->Sections,"Alias",alstr);
  }
  UDM_FREE(alstr);
  return UDM_OK;
}




static int
UdmDocLookupConn(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc)
{
  const char *proxy;
  int u;
  
  if((proxy=UdmVarListFindStr(&Doc->RequestHeaders,"Proxy",NULL))){
    char *port;

    UdmLog(Indexer, UDM_LOG_DEBUG, "Using Proxy: %s", proxy);
    Doc->connp.hostname = (char*)UdmStrdup(proxy);
    if((port=strchr(Doc->connp.hostname,':'))){
      *port++='\0';
      Doc->connp.port=atoi(port);
    }else{
      Doc->connp.port=3128;
    }
  }else{
    if (Doc->CurURL.hostname){
      Doc->connp.hostname = (char*)UdmStrdup(Doc->CurURL.hostname);
      Doc->connp.port=Doc->CurURL.port?Doc->CurURL.port:Doc->CurURL.default_port;
    }
  }

#ifdef WIN32
  if (Doc->CurURL.schema && !strcasecmp(Doc->CurURL.schema, "file"))
	return UDM_OK; /* Indexing Win LAN, no needs to resolve */
#endif

  u= UdmHostLookup2(Indexer, &Indexer->Conf->Hosts, &Doc->connp);
  
  if(Doc->CurURL.hostname != NULL && *Doc->CurURL.hostname != '\0' && u) {
    UdmLog(Indexer,UDM_LOG_WARN,"Can't resolve host '%s'",Doc->connp.hostname);
    Doc->method = UDM_METHOD_VISITLATER;
    UdmVarListReplaceInt(&Doc->Sections, "Status", UDM_HTTP_STATUS_SERVICE_UNAVAILABLE);
  }
  return UDM_OK;
}



static int UdmDocCheck(UDM_AGENT *Indexer,UDM_SERVER *CurSrv,UDM_DOCUMENT *Doc){
  char    reason[1024]="";
  int    hops=UdmVarListFindInt(&Doc->Sections,"Hops",0);
  const char *method= UdmMethodStr(CurSrv->method);
  const char *follow= UdmFollowStr(CurSrv->follow);
  
  switch(CurSrv->Match.match_type){
    case UDM_MATCH_WILD:
      UdmLog(Indexer,UDM_LOG_DEBUG, "Realm %s %s wild '%s'", follow, method, CurSrv->Match.pattern);
      break;
    case UDM_MATCH_REGEX:
      UdmLog(Indexer,UDM_LOG_DEBUG, "Realm %s %s regex '%s'", follow, method, CurSrv->Match.pattern);
      break;
    case UDM_MATCH_SUBNET:
      UdmLog(Indexer,UDM_LOG_DEBUG, "Subnet %s %s '%s'", follow, method, CurSrv->Match.pattern);
      break;
    case UDM_MATCH_BEGIN:
    default:
      UdmLog(Indexer,UDM_LOG_DEBUG, "Server %s %s '%s'", follow, method, CurSrv->Match.pattern);
      break;
  }
  
  if((Doc->method=UdmMethod(method)) == UDM_METHOD_GET){
    /* Check Allow/Disallow/CheckOnly stuff */
    Doc->method=UdmFilterFind(&Indexer->Conf->Filters,UdmVarListFindStr(&Doc->Sections,"URL",""),reason);
    UdmLog(Indexer,UDM_LOG_DEBUG,"%s",reason);
  }
  
  if(Doc->method==UDM_METHOD_DISALLOW)return UDM_OK;
  
  /* Check that hops is less than MaxHops */
  if(hops>Doc->Spider.maxhops){
    UdmLog(Indexer,UDM_LOG_WARN,"Too many hops (%d)",hops);
    Doc->method=UDM_METHOD_DISALLOW;
    return UDM_OK;
  }
  
  return UDM_OK;
}


static void UdmAppendTarget(UDM_AGENT *Indexer, const char *url, const char *lang, const int hops, int parent) {
  UDM_DOCUMENT *Doc, *Save;

  UDM_GETLOCK(Indexer, UDM_LOCK_THREAD);
  UDM_GETLOCK(Indexer, UDM_LOCK_CONF);
  
  if (Indexer->Conf->Targets.num_rows > 0)
  {
    UDM_DOCUMENT *First= Indexer->Conf->Targets.Doc;
    UDM_DOCUMENT *Last= First + Indexer->Conf->Targets.num_rows - 1;
    for (Doc= Last; Doc >= First; Doc--)
    {
      if (!strcasecmp(UdmVarListFindStr(&Doc->Sections, "URL", ""), url) &&
          !strcmp(UdmVarListFindStr(&Doc->RequestHeaders, "Accept-Language", ""), lang))
        goto ret;
    }
  }
  if ((Indexer->Conf->Targets.Doc= 
       UdmRealloc(Save= Indexer->Conf->Targets.Doc,
                  (Indexer->Conf->Targets.num_rows + 1) *
                  sizeof(UDM_DOCUMENT))) == NULL)
  {
    Indexer->Conf->Targets.Doc= Save;
    goto ret;
  }
  Doc= &Indexer->Conf->Targets.Doc[Indexer->Conf->Targets.num_rows++];
  UdmDocInit(Doc);
  UdmVarListAddStr(&Doc->Sections, "URL", url);
  UdmVarListAddInt(&Doc->Sections, "Hops", hops);
  UdmVarListReplaceInt(&Doc->Sections, "URL_ID", UdmStrHash32(url));
  UdmVarListReplaceInt(&Doc->Sections, "Referrer-ID", parent);
  UdmURLActionNoLock(Indexer, Doc, UDM_URL_ACTION_ADD);
  if (*lang != '\0') UdmVarListAddStr(&Doc->RequestHeaders, "Accept-Language", lang);
  
ret:
  UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);
  UDM_RELEASELOCK(Indexer, UDM_LOCK_THREAD);
  return;
}


/*
  Remove character set part,
  and other extended parts.
  Return pointer to new value.
*/
static const char*
UdmDocNormalizeContentType(UDM_DOCUMENT *Doc)
{
  UDM_VAR *Var= UdmVarListFind(&Doc->Sections, "Content-Type");
  char *p;
  
  if (!Var || !Var->val)
    return NULL;
  
  if ((p = strstr(Var->val, "charset=")) != NULL)
  {
    const char *cs = UdmCharsetCanonicalName(p + 8);
    *p= '\0';
    UdmRTrim((char*) Var->val, "; ");
    UdmVarListReplaceStr(&Doc->Sections, "Server-Charset", cs ? cs : (p + 8));
  }
  else
  {
    if ((p= strchr(Var->val, ';')))
      *p= '\0';
  }
  return Var->val;
}


static int
UdmDocProcessVaryLang(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc)
{
  const char *vary= UdmVarListFindStr(&Doc->Sections, "Vary", NULL);
  const char *VaryLang;
  
  if (vary && strcasestr(vary, "accept-language") &&
      (VaryLang= UdmVarListFindStr(&Doc->Sections, "VaryLang", "")) &&
      VaryLang[0])
  {  
    UDM_URL newURL;
    const char *ourl;
    
    UdmURLInit(&newURL);
    UdmVarListReplaceStr(&Doc->Sections, "Status", "300");
    UdmURLParse(&newURL, ourl= UdmVarListFindStr(&Doc->Sections, "URL", ""));
    
    if (strcmp(UDM_NULL2EMPTY(newURL.filename), "robots.txt"))
    {
      const char *CL= UdmVarListFindStr(&Doc->Sections, "Content-Location", UDM_NULL2EMPTY(newURL.filename));
      size_t urlen= 128 + strlen(UDM_NULL2EMPTY(newURL.hostinfo)) + strlen(UDM_NULL2EMPTY(newURL.path)) + strlen(CL);
      char *url;
      
      if ((url= (char*)UdmMalloc(urlen)) != NULL)
      {
        char *tok, *lt;
        int parent= UdmVarListFindInt(&Doc->Sections, "Referrer-ID", 0);
        int hops= UdmVarListFindInt(&Doc->Sections, "Hops", 0);
        
        char tmp[80];
        udm_snprintf(tmp, sizeof(tmp), "%s", VaryLang);
        snprintf(url, urlen, "%s://%s%s%s",
                 UDM_NULL2EMPTY(newURL.schema),
                 UDM_NULL2EMPTY(newURL.hostinfo), 
                 UDM_NULL2EMPTY(newURL.path), CL);
        UdmAppendTarget(Indexer,  url, "", hops, parent);
        
        for (tok= udm_strtok_r(tmp, " ,\t", &lt) ;
             tok != NULL ;
             tok= udm_strtok_r(NULL, " ,\t", &lt))
        {
          UdmAppendTarget(Indexer, ourl, tok, hops, parent);
        }
        UdmFree(url);
      }
    }
    UdmURLFree(&newURL);
  }
  return UDM_OK;
}


int
UdmDocProcessContentResponseHeaders(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc)
{
  UDM_VAR *var;
  const char *content_type= UdmDocNormalizeContentType(Doc);

  if(!UdmVarListFindBool(&Indexer->Conf->Vars, "UseRemoteContentType", 1) ||
     content_type == NULL)
  {
    UDM_MATCH  *M;
    UDM_MATCH_PART  P[10];
    const char *fn= (Doc->CurURL.filename && Doc->CurURL.filename[0]) ? Doc->CurURL.filename : "index.html";
    
    if((M=UdmMatchListFind(&Indexer->Conf->MimeTypes,fn,10,P)))
    {
      UdmVarListReplaceStr(&Doc->Sections,"Content-Type",M->arg);
      content_type= UdmDocNormalizeContentType(Doc); 
    }
    
    if ((M= UdmMatchListFind(&Indexer->Conf->Encodings, fn, 10, P)))
      UdmVarListReplaceStr(&Doc->Sections, "Content-Encoding", M->arg);
  }

  if ((var=UdmVarListFind(&Doc->Sections,"Server")))
  {
    if(!strcasecmp("yes",UdmVarListFindStr(&Indexer->Conf->Vars,"ForceIISCharset1251","no")))
    {
      if (!UdmWildCaseCmp(var->val,"*Microsoft*") ||
          !UdmWildCaseCmp(var->val,"*IIS*"))
      {
        const char *cs;
        if((cs=UdmCharsetCanonicalName("windows-1251")))
          UdmVarListReplaceStr(&Doc->Sections, "Server-Charset", cs);
      }
    }
  }
  return UDM_OK;
}


static int
UdmDocProcessResponseHeaders(UDM_AGENT *Indexer,UDM_DOCUMENT *Doc)
{
  UDM_VAR    *var;
  const int content_length= UdmVarListFindInt(&Doc->Sections, "Content-Length", 0);

  UdmDocProcessVaryLang(Indexer, Doc);

  if ((size_t)content_length > Doc->Buf.maxsize)
    UdmVarListReplaceInt(&Doc->Sections, "Status", UDM_HTTP_STATUS_PARTIAL_OK);


  UDM_GETLOCK(Indexer, UDM_LOCK_CONF);

  UdmDocProcessContentResponseHeaders(Indexer, Doc);
  
  if (UdmVarListFindBool(&Indexer->Conf->Vars, "UseCookie", 0))
  {
    size_t i;
    for (i= 0; i < Doc->Sections.nvars; i++)
    {
      var= &Doc->Sections.Var[i];
      if (!strncmp(var->name, "Set-Cookie.", 11))
        UdmVarListReplaceStr(&Indexer->Conf->Cookies, var->name + 11, var->val);
    }
  }
  UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);

  if(!UdmVarListFind(&Doc->Sections,"Content-Type"))
    UdmVarListAddStr(&Doc->Sections,"Content-Type","application/octet-stream");

  if ((var= UdmVarListFind(&Doc->Sections, "Location")))
  {
    int err;
    UDM_URL newURL;
    UdmURLInit(&newURL);
    if (UDM_URL_OK == (err= UdmURLParse(&newURL, var->val)) &&
        UDM_URL_OK == (err= newURL.schema ? UDM_URL_OK : UDM_URL_BAD))
    {
      UDM_HREF Href;
      UdmHrefInit(&Href);
      Href.url= var->val;
      Href.hops= UdmVarListFindInt(&Doc->Sections,"Hops",0)+1;
      Href.referrer= UdmVarListFindInt(&Doc->Sections,"Referrer-ID",0);
      Href.method= UDM_METHOD_GET;
      Href.site_id= UdmVarListFindInt(&Doc->Sections, "Site_id", 0);
      Href.server_id= UdmVarListFindInt(&Doc->Sections,"Server_id", 0);
      Href.collect_links= Doc->Spider.collect_links;
      UdmHrefListAdd(&Doc->Hrefs,&Href);
    }
    else
    {
      UdmLog(Indexer, UDM_LOG_ERROR, "Redirect: %s: '%s'",
                                     UdmURLErrorStr(err), var->val);
    }
    UdmURLFree(&newURL);
  }
  return UDM_OK;
}


#ifdef HAVE_ZLIB
#include <zlib.h>

static int UdmPutCachedCopy(UDM_AGENT *Indexer, UDM_DOCUMENT * Doc)
{
  int rc= UDM_OK;
  z_stream zstream;
  char *Zbuf = NULL;
  urlid_t rec_id = UdmVarListFindInt(&Doc->Sections, "ID", 0);
  size_t content_size=Doc->Buf.size - (Doc->Buf.content-Doc->Buf.buf);
  size_t Zlen;
  size_t Blen;
  UDM_VAR *Sec; 
  
  if (!content_size)
    return rc;

  if (!(Sec=UdmVarListFind(&Doc->Sections, "CachedCopy")))
    return rc;
  
  zstream.zalloc = Z_NULL;
  zstream.zfree = Z_NULL;
  zstream.opaque = Z_NULL;
  zstream.next_in = (Byte*)Doc->Buf.content;
  
  if (deflateInit2(&zstream, 9, Z_DEFLATED, 15, 9, Z_DEFAULT_STRATEGY) != Z_OK)
  {
    rc= UDM_ERROR;
    goto ex;
  }
  
  zstream.avail_in = content_size;
  zstream.avail_out = 2 * content_size + 256;
  if (!(Zbuf=  UdmMalloc(zstream.avail_out)))
  {
    rc= UDM_ERROR;
    goto ex;
  }
  zstream.next_out= (Byte*) Zbuf;
  deflate(&zstream, Z_FINISH);
  deflateEnd(&zstream);
  Zlen= (size_t)zstream.total_out;
  Blen= BASE64_LEN(Zlen);
  if (Blen > Sec->maxlen)
    goto ex;
  
  UdmLog(Indexer, UDM_LOG_DEBUG, "Stored rec_id: %d Size: %d Ratio: %5.2f%%",
          rec_id, (int) content_size,
          (double)100.0 * zstream.total_out / content_size);
  
  if (!Sec->val &&
      !(Sec->val= UdmRealloc(Sec->val, Sec->maxlen + 1)))
  {
    rc= UDM_ERROR;
    goto ex;
  }
  Sec->curlen= udm_base64_encode(Zbuf,Sec->val,Zlen);
  
ex:
  UDM_FREE(Zbuf);
  
  return rc;
}
#endif


static void
UdmDSTRAppendWild(UDM_DSTR *d, UDM_VARLIST *L, const char *pattern)
{
  size_t i;
  for (i= 0; i < L->nvars; i++)
  {
    UDM_VAR *V= &L->Var[i];
    if (V->name && V->val && !UdmWildCmp(V->name, pattern))
      UdmDSTRAppendSTR(d, V->val);
  }
}

size_t UdmDSTRParse(UDM_DSTR *d, const char *fmt, UDM_VARLIST *L)
{
  if (d->data)
    d->data[0]= '\0';
  
  for ( ; fmt[0]; fmt++)
  {
    const char *end;
    if (fmt[0] == '$' && fmt[1] == '{' && (end= strchr(fmt,'}')))
    {
      char name[128];
      size_t namelen;
      fmt+=2;
      namelen= end - fmt;
      if (namelen < sizeof(name) - 1)
      {
        const char *val;
        memcpy(name, fmt, namelen);
        name[namelen]= '\0';
        if (strchr(name, '*'))
        {
          /* Append all variables whose name prefix matchs */
          UdmDSTRAppendWild(d, L, name);
        }
        else if ((val= UdmVarListFindStr(L, name, NULL)))
          UdmDSTRAppendSTR(d, val);
      }
      fmt= end;
    }
    else
      UdmDSTRAppend(d, fmt, 1);
  }
  return d->size_data;
}


static int
UdmParseSections(UDM_AGENT *Indexer,
                 UDM_MATCHLIST *SectionMatch,
                 UDM_DOCUMENT *Doc, UDM_CHARSET *doccs)
{
  size_t i;
  UDM_DSTR tbuf, sbuf;

  if (!SectionMatch->nmatches)
    return UDM_OK;
  
  UdmDSTRInit(&tbuf, 1024);
  UdmDSTRInit(&sbuf, 1024);
  
  for (i = 0; i < SectionMatch->nmatches; i++)
  {
    UDM_VAR *Sec;
    UDM_HTMLTOK tag;
    const char *htok, *last, *src;
    UDM_MATCH_PART  Parts[10];
    size_t nparts= 10;
    UDM_TEXTITEM Item;
    UDM_MATCH *Alias= &SectionMatch->Match[i];
    size_t buflen, srclen;

    if (Alias->arg1)
    {
      UdmDSTRReset(&tbuf);
      UdmDSTRParse(&tbuf, Alias->arg1, &Doc->Sections);
      src= tbuf.data;
      srclen= tbuf.size_data;
    }
    else
    {
      src= Doc->Buf.content;
      srclen= strlen(Doc->Buf.content);
    }

    if (!(Sec= UdmVarListFind(&Doc->Sections, Alias->section)))
      continue;
    
    if (UdmMatchExec(Alias, src, srclen, src, nparts, Parts))
      continue;

    /* Calculate space required for UdmMatchApply, and allocate space */
    buflen= UdmMatchApply(NULL, 1, src, Alias->arg, Alias, nparts, Parts);
    UdmDSTRReset(&sbuf);
    UdmDSTRRealloc(&sbuf, buflen);

    if (!(sbuf.size_data= UdmMatchApply(sbuf.data, buflen, src,
                                        Alias->arg, Alias, nparts, Parts)))
      continue;

    UDM_ASSERT(sbuf.size_data + 1 == buflen);
    
    Item.href=NULL;
    Item.section=Sec->section;
    Item.section_name=Sec->name;
    Item.flags= 0;

    UdmHTMLTOKInit(&tag);
    for (htok= UdmHTMLToken(sbuf.data, &last, &tag); htok; 
         htok = UdmHTMLToken(NULL, &last, &tag))
    {
      if (tag.type == UDM_HTML_TXT && ! tag.script && ! tag.comment && ! tag.style)
      {
        UdmDSTRReset(&tbuf);
        UdmDSTRAppend(&tbuf, htok, last - htok);
        Item.str = tbuf.data;
        if (&Indexer->Conf->SectionHdrMatch == SectionMatch)
        {
          UdmVarListReplaceStr(&Doc->Sections, Item.section_name, Item.str);
        }
        else if (&Indexer->Conf->SectionGsrMatch == SectionMatch && doccs)
        {
          char *trmsrc= UdmTrim(Item.str, " \t\r\n");
          size_t trmlen= strlen(trmsrc);
          size_t cnvlen= trmlen * 12 + 1;
          char *cnvsrc= (char*) UdmMalloc(cnvlen);
          UdmVarListReplaceStr(&Doc->Sections, Item.section_name, Item.str);
          if (trmsrc[0] && cnvsrc)
          {
            UDM_CONV lcs_dcs;
            UdmConvInit(&lcs_dcs, Doc->lcs, doccs, UDM_RECODE_HTML);
            UdmConv(&lcs_dcs, cnvsrc, cnvlen, trmsrc, trmlen + 1);
            Item.str= cnvsrc;
            Item.flags= UDM_TEXTLIST_FLAG_SKIP_ADD_SECTION;
            UdmTextListAdd(&Doc->TextList, &Item);
            UdmFree(cnvsrc);
          }
        }
        else
        {
          UdmTextListAdd(&Doc->TextList, &Item);
        }
      }
    }
  }

  UdmDSTRFree(&sbuf);
  UdmDSTRFree(&tbuf);
  return UDM_OK;
}



/*
  Create HTTP.Content variable if requested
*/

static int
UdmVarListAddContentStr(UDM_VARLIST *Sections, const char *name,
                        UDM_DOCUMENT *Doc)
{
  UDM_VAR *Sec;
  if (Doc->Buf.content && (Sec= UdmVarListFind(Sections, name)))
  {
    size_t content_size=Doc->Buf.size - (Doc->Buf.content-Doc->Buf.buf);
    if (content_size)
    {
      if (!(Sec->val= (char*) UdmRealloc(Sec->val, content_size + 1)))
        return UDM_ERROR;
      memcpy(Sec->val, Doc->Buf.content, content_size);
      Sec->val[content_size]= '\0';
      Sec->curlen= content_size;
    }
  }
  return UDM_OK;
}


/*
  Create "HTTP.LocalCharsetContent" variable if requested
  Must be executed after character set guesser;
*/
static int
UdmVarListAddLocalCharsetContentStr(const char *name,
                                    UDM_DOCUMENT *Doc, UDM_CHARSET *doccs)
{
  UDM_VAR *Sec;
  size_t srclen;
  
  if (Doc->Buf.content &&
      (Sec= UdmVarListFind(&Doc->Sections, "HTTP.LocalCharsetContent")) &&
      (srclen= Doc->Buf.size - (Doc->Buf.content-Doc->Buf.buf)))
  {
    const char *src= Doc->Buf.content;
    if (!doccs)
      doccs= UdmGetCharSet("iso-8859-1");
    if (Doc->lcs == doccs)
    {
      Sec->val= (char*) UdmRealloc(Sec->val, srclen + 1);
      memcpy(Sec->val, src, srclen);
      Sec->val[srclen]= '\0';
      Sec->curlen= srclen;
    }
    else
    {
      UDM_CONV dcs_lcs;
      Sec->val= (char*) UdmRealloc(Sec->val, srclen * 8 + 1);
      UdmConvInit(&dcs_lcs, doccs, Doc->lcs, UDM_RECODE_HTML_NONASCII);
      srclen= UdmConv(&dcs_lcs, Sec->val, srclen * 8 + 1, src, srclen);
      Sec->val[srclen]= '\0';
      Sec->curlen= srclen;
    }
  }
  return UDM_OK;
}


static int
UdmVarListLog(UDM_AGENT *A,UDM_VARLIST *V,int l,const char *pre){
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


static int
UdmParseHTTPResponseAndHeaders(UDM_AGENT *A, UDM_DOCUMENT *Doc)
{
  int status;
  UdmParseHTTPResponse(A, Doc);
  UdmDocProcessResponseHeaders(A, Doc);
  UdmVarListAddContentStr(&Doc->Sections, "HTTP.Content", Doc);
  UdmParseSections(A, &A->Conf->SectionHdrMatch, Doc, NULL);
  UdmVarListLog(A, &Doc->Sections, UDM_LOG_DEBUG, "Response");
  status= UdmVarListFindInt(&Doc->Sections,"Status",0);
  UdmLog(A, UDM_LOG_EXTRA, "Status: %d %s", status, UdmHTTPErrMsg(status));
  return status;
}


static int
UdmDocContentDecode(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc)
{
  const char  *url=UdmVarListFindStr(&Doc->Sections,"URL","");
  const char  *ce=UdmVarListFindStr(&Doc->Sections,"Content-Encoding","");
  int code= 0;

#ifdef HAVE_ZLIB
  if(!strcasecmp(ce,"gzip") || !strcasecmp(ce,"x-gzip"))
  {
    UDM_THREADINFO(Indexer,"UnGzip",url);
    code= UdmDocUnGzip(Doc);
    UdmVarListReplaceInt(&Doc->Sections, "Content-Length", Doc->Buf.buf - Doc->Buf.content + (int)Doc->Buf.size);
  }
  else if(!strcasecmp(ce,"deflate"))
  {
    UDM_THREADINFO(Indexer,"Inflate",url);
    code= UdmDocInflate(Doc);
    UdmVarListReplaceInt(&Doc->Sections, "Content-Length", Doc->Buf.buf - Doc->Buf.content + (int)Doc->Buf.size);
  }
  else if(!strcasecmp(ce,"compress") || !strcasecmp(ce,"x-compress"))
  {
    UDM_THREADINFO(Indexer,"Uncompress",url);
    code= UdmDocUncompress(Doc);
    UdmVarListReplaceInt(&Doc->Sections, "Content-Length", Doc->Buf.buf - Doc->Buf.content + (int)Doc->Buf.size);
  }
  else
#endif  
  if(!strcasecmp(ce,"identity") || !strcasecmp(ce,""))
  {
    /* Nothing to do*/
  }
  else
  {
    UdmLog(Indexer,UDM_LOG_WARN,"Unsupported Content-Encoding");
    UdmVarListReplaceInt(&Doc->Sections,"Status",UDM_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE);
  }
  
  if (code == -1)
  {
    const char  *te= UdmVarListFindStr(&Doc->Sections,"Transfer-Encoding", NULL);
    if (te)
      UdmLog(Indexer, UDM_LOG_ERROR, "Transfer-Encoding '%s' is not supported", te);
    UdmLog(Indexer, UDM_LOG_ERROR, "Content-Encoding processing failed");
    Doc->Buf.content[0]= '\0';
    Doc->Buf.size= Doc->Buf.content - Doc->Buf.buf;
  }
  return UDM_OK;
}


int UdmDocParseContent(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc);

int UdmDocParseContent(UDM_AGENT * Indexer, UDM_DOCUMENT * Doc){
  
#ifdef USE_PARSER
  UDM_PARSER  *Parser;
  const int   status = UdmVarListFindInt(&Doc->Sections, "Status", 0);
#endif
  const char  *real_content_type=NULL;
  const char  *ct=UdmVarListFindStr(&Doc->Sections,"Content-Type","");
  int    result=UDM_OK;

  if(!strcmp(UDM_NULL2EMPTY(Doc->CurURL.filename), "robots.txt")) return UDM_OK;
  
  UdmDocContentDecode(Indexer, Doc);
  
#ifdef USE_PARSER
  /* Let's try to start external parser for this Content-Type */

  if((Parser = UdmParserFind(&Indexer->Conf->Parsers, ct))) {
    UdmLog(Indexer,UDM_LOG_DEBUG,"Found external parser '%s' -> '%s'",
      Parser->from_mime?Parser->from_mime:"NULL",
      Parser->to_mime?Parser->to_mime:"NULL");
  }
  if(Parser && UdmParserExec(Indexer, Parser, Doc)) {
    if (status == UDM_HTTP_STATUS_OK) {
    char *to_charset;
    real_content_type=Parser->to_mime?Parser->to_mime:"unknown";
    UdmLog(Indexer,UDM_LOG_DEBUG,"Parser.Content-Type: %s",real_content_type);
    if((to_charset=strstr(real_content_type,"charset="))){
            const char *cs = UdmCharsetCanonicalName(UdmTrim(to_charset + 8, " \t"));
      UdmVarListReplaceStr(&Doc->Sections, "Server-Charset", cs);
      UdmVarListReplaceStr(&Doc->Sections, "RemoteCharset", cs);
      UdmLog(Indexer,UDM_LOG_DEBUG, "Parser.Charset: %s", cs);
      UdmVarListAddStr(&Doc->Sections,"Parser.Charset",cs);
    }
#ifdef DEBUG_PARSER
    fprintf(stderr,"content='%s'\n",Doc->content);
#endif
    } else {
      UdmLog(Indexer, UDM_LOG_WARN, "Parser not executed, document status: %d", status);
    }
  }
#endif
  
  if(!real_content_type)real_content_type=ct;
  UdmVarListAddStr(&Doc->Sections,"Parser.Content-Type",real_content_type);
  
  if (!strcasecmp(real_content_type, "application/http") ||
      !strcasecmp(real_content_type, "message/http"))
  {
    size_t old_hdr_len= Doc->Buf.content - Doc->Buf.buf;
    size_t new_response_len= Doc->Buf.size - old_hdr_len;
    UdmLog(Indexer, UDM_LOG_DEBUG, "Re-parsing headers");
    memmove(Doc->Buf.buf, Doc->Buf.content, new_response_len);
    Doc->Buf.content= Doc->Buf.buf;
    Doc->Buf.content_length= 0;
    Doc->Buf.size= new_response_len;
    Doc->Buf.buf[new_response_len]= '\0';
    UdmParseHTTPResponseAndHeaders(Indexer, Doc);
    real_content_type= UdmVarListFindStr(&Doc->Sections,
                                         "Content-Type", real_content_type);
  }

#ifdef HAVE_ZLIB
  if (Doc->method == UDM_METHOD_GET &&
      UDM_OK!=(result = UdmPutCachedCopy(Indexer, Doc)))
  {
    UdmLog(Indexer,UDM_LOG_ERROR,"Failed to create cached copy");
    return result;
  }
#endif
  
  /* CRC32 without headers */
  {
    size_t crclen=Doc->Buf.size - (Doc->Buf.content-Doc->Buf.buf);
    UdmVarListReplaceInt(&Doc->Sections, "crc32", (int)UdmCRC32(Doc->Buf.content,crclen));
  }

  if(Doc->method == UDM_METHOD_HEAD)
    return result;
  
  switch (UdmContentTypeByName(real_content_type))
  {
    case UDM_CONTENT_TYPE_TEXT_PLAIN:
      UdmParseSections(Indexer, &Indexer->Conf->SectionMatch, Doc, NULL);
      UdmParseText(Indexer,Doc);
      break;

    case UDM_CONTENT_TYPE_TEXT_HTML:
      UdmHTMLParse(Indexer,Doc);
      UdmParseSections(Indexer, &Indexer->Conf->SectionMatch, Doc, NULL);
      break;

    case UDM_CONTENT_TYPE_TEXT_XML:
    {
      int rc;
      UdmVarListReplaceStr(&Indexer->Conf->Vars, "XMLDefaultSection", "body");
      rc= UdmXMLParse(Indexer, Doc);
      if (rc != UDM_OK)
      {
        UdmLog(Indexer, UDM_LOG_ERROR, "Error: %s",
               UdmVarListFindStr(&Doc->Sections, "X-Reason", ""));
      }
      break;
    }

    case UDM_CONTENT_TYPE_MESSAGE_RFC822:
      UdmMessageRFC822Parse(Indexer, Doc);
      break;

    case UDM_CONTENT_TYPE_AUDIO_MPEG:
      UdmMP3Parse(Indexer,Doc);
      break;

    case UDM_CONTENT_TYPE_HTDB:
      /* Do nothing */
      break;

    case UDM_CONTENT_TYPE_DOCX:
      UdmDOCXParse(Indexer, Doc);
      break;

    case UDM_CONTENT_TYPE_TEXT_RTF:
      UdmRTFParse(Indexer, Doc);
      break;

    default:
    {
      /* Unknown Content-Type  */
      UdmLog(Indexer,UDM_LOG_WARN, "Unsupported Content-Type '%s'", real_content_type);
      UdmVarListReplaceInt(&Doc->Sections, "Status", UDM_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE);
    }
  }
  return result;
}


static int
UdmDocAddDocExtraHeaders(UDM_DOCUMENT *Doc){
  /* Host Name for virtual hosts */
  if(Doc->CurURL.hostname != NULL) {
    char    arg[128]="";
    if(Doc->CurURL.port){
      sprintf(arg,"%s:%d",Doc->CurURL.hostname,Doc->CurURL.port);
      UdmVarListReplaceStr(&Doc->RequestHeaders,"Host",arg);
    }else{
      UdmVarListReplaceStr(&Doc->RequestHeaders,"Host",Doc->CurURL.hostname);
    }
  }
  return UDM_OK;
}


static int
UdmDocAddCookieHeaders(UDM_ENV *Conf, UDM_DOCUMENT *Doc)
{
  size_t    i;
  UDM_DSTR cookie;

  if (!Doc->CurURL.hostname)
    return UDM_OK;

  UdmDSTRInit(&cookie, 1024);
  for (i= 0; i < Conf->Cookies.nvars; i++)
  {
    UDM_VAR *v= &Conf->Cookies.Var[i];
    const char *domain, *path;
    size_t short_dlen, long_dlen;
    if (!(domain= strchr(v->name, '@')))
      continue;
    domain++;
    if (!(path= strchr(domain, '/')))
    {
      continue;
    }
    else
    {
      size_t plen= strlen(path);
      size_t path_length= strlen(Doc->CurURL.path);
      if (memcmp(Doc->CurURL.path, path, udm_min(plen, path_length)))
        continue;
      if (plen > path_length && (!Doc->CurURL.filename ||
                                  memcmp(Doc->CurURL.filename,
                                         path + path_length,
                                         plen - path_length)))
        continue;
    }
    short_dlen= path-domain;
    long_dlen= strlen(Doc->CurURL.hostname);
    if (short_dlen > long_dlen)
      continue;
    if (strncasecmp(domain,
                    Doc->CurURL.hostname + long_dlen - short_dlen, short_dlen))
      continue;
    if (cookie.size_data)
      UdmDSTRAppend(&cookie, "; ", 2);
    UdmDSTRAppend(&cookie, v->name, domain - v->name - 1);
    UdmDSTRAppend(&cookie, "=", 1);
    UdmDSTRAppendSTR(&cookie, v->val);
  }
  if (cookie.size_data)
  {
    UdmVarListReplaceStr(&Doc->RequestHeaders, "Cookie", cookie.data);
  }
  UdmDSTRFree(&cookie);
  return UDM_OK;
}


static int UdmDocAddConfExtraHeaders(UDM_ENV *Conf,UDM_DOCUMENT *Doc){
  char    arg[128]="";
  const char  *lc;
  size_t    i;
  
  /* If LocalCharset specified, add Accept-Charset header */
  if((lc=UdmVarListFindStr(&Conf->Vars,"LocalCharset",NULL))){
    snprintf(arg,sizeof(arg)-1,"%s;q=1.0, *;q=0.9, utf-8;q=0.8",lc);
    arg[sizeof(arg)-1]='\0';
    UdmVarListAddStr(&Doc->RequestHeaders,"Accept-Charset",arg);
  }
  
  for (i=0;i<Conf->Vars.nvars;i++){
    UDM_VAR *v=&Conf->Vars.Var[i];
    if(!strncmp(v->name,"Request.",8))
      UdmVarListInsStr(&Doc->RequestHeaders,v->name+8,v->val);
  }
  
  if (UdmVarListFindBool(&Conf->Vars, "UseCookie", 0))
    UdmDocAddCookieHeaders(Conf, Doc);
  
#ifdef HAVE_ZLIB
  UdmVarListInsStr(&Doc->RequestHeaders,"Accept-Encoding","gzip,deflate,compress");
#endif
  return UDM_OK;
}

static int UdmDocAddServExtraHeaders(UDM_SERVER *Server,UDM_DOCUMENT *Doc){
  char  arg[128]="";
  size_t  i;
  
  for( i=0 ; i<Server->Vars.nvars ; i++){
    UDM_VAR *Hdr=&Server->Vars.Var[i];
    
    if(!strcasecmp(Hdr->name,"AuthBasic")){
      /* HTTP and FTP specific stuff */
      if((!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "http")) ||
        (!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "https")) ||
        (!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "ftp")) ||
        (!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "https"))) {
        
        snprintf(arg,sizeof(arg)-1, "Basic %s", Hdr->val);
        arg[sizeof(arg)-1]='\0';
        UdmVarListReplaceStr(&Doc->RequestHeaders,"Authorization",arg);
      }
      
      if(!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "nntp") || 
         !strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "news")) {
        /* Auth if required                      */
        /* NNTPGet will parse this header        */
        /* We'll pass authinfo still in base64   */
        /* form to avoid plain user name in core */
        /* file on crashes if any                */
        
        if(Hdr->val && Hdr->val[0]){
          UdmVarListReplaceStr(&Doc->RequestHeaders,"Authorization",Hdr->val);
        }
      }
    }else
    if(!strcasecmp(Hdr->name,"ProxyAuthBasic")){
      if(Hdr->val && Hdr->val[0]){
        snprintf(arg,sizeof(arg)-1,"Basic %s", Hdr->val);
        arg[sizeof(arg)-1]='\0';
        UdmVarListReplaceStr(&Doc->RequestHeaders,"Proxy-Authorization",arg);
      }
    }else
    if(!strcasecmp(Hdr->name, "Proxy")){
      if(Hdr->val && Hdr->val[0]){
        UdmVarListReplaceStr(&Doc->RequestHeaders, Hdr->name, Hdr->val);
      }
    }else{
      if(!strncmp(Hdr->name,"Request.",8))
        UdmVarListReplaceStr(&Doc->RequestHeaders,Hdr->name+8,Hdr->val);
    }
    
  }
  return UDM_OK;
}


static int UdmNextTarget(UDM_AGENT * Indexer,UDM_DOCUMENT *Result){
  int  rc= UDM_NOTARGET;

  UDM_GETLOCK(Indexer, UDM_LOCK_THREAD);
  UDM_GETLOCK(Indexer, UDM_LOCK_CONF);

  if (Indexer->Conf->url_number <= 0)
    goto unlock; /* UDM_NOTARGET */

  /* Load targets into memory cache */
  if (Indexer->Conf->Targets.cur_row >= Indexer->Conf->Targets.num_rows)
  {
    if(UDM_OK != (rc= UdmTargets(Indexer)))
      goto unlock;
  }

  /* Choose next target */
  if(Indexer->Conf->Targets.num_rows &&
     Indexer->Conf->Targets.cur_row < Indexer->Conf->Targets.num_rows)
  {
    UDM_DOCUMENT *Doc=&Indexer->Conf->Targets.Doc[Indexer->Conf->Targets.cur_row];

    UdmVarListReplaceLst(&Result->Sections,&Doc->Sections,NULL,"*");
    UdmVarListReplaceLst(&Result->Sections,&Indexer->Conf->Sections,NULL,"*");
    UdmVarListReplaceLst(&Result->RequestHeaders, &Doc->RequestHeaders, NULL, "*");

    Indexer->Conf->Targets.cur_row++;
    Indexer->Conf->url_number--;
    rc= UDM_OK;
  }
  else
    rc= UDM_NOTARGET;

unlock:
  UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);
  UDM_RELEASELOCK(Indexer, UDM_LOCK_THREAD);
  return rc;
}




static int UdmVarList2Spider(UDM_SPIDERPARAM *S,UDM_VARLIST *V){
  S->period    =UdmVarListFindInt(V, "Period",    UDM_DEFAULT_REINDEX_TIME);
  S->maxhops    =UdmVarListFindInt(V, "MaxHops",  UDM_DEFAULT_MAX_HOPS);
  S->doc_per_site= UdmVarListFindInt(V, "MaxDocPerSite",  0);
  S->max_net_errors  =UdmVarListFindInt(V, "MaxNetErrors",  UDM_MAXNETERRORS);
  S->net_error_delay_time  =UdmVarListFindInt(V, "NetErrorDelayTime",UDM_DEFAULT_NET_ERROR_DELAY_TIME);
  S->read_timeout    =UdmVarListFindInt(V, "ReadTimeOut",  UDM_READ_TIMEOUT);
  S->doc_timeout    =UdmVarListFindInt(V, "DocTimeOut",  UDM_DOC_TIMEOUT);
  S->index    =UdmVarListFindInt(V, "Index",    1);
  S->use_robots    =UdmVarListFindInt(V, "Robots",    1);
  S->use_clones    =UdmVarListFindInt(V, "DetectClones",  0);
  S->collect_links= UdmVarListFindInt(V, "CollectLinks", 0);
  S->crawl_delay= UdmVarListFindInt(V, "CrawlDelay", 0);
  return UDM_OK;
}


static int
UdmDocImportSections(UDM_AGENT *A, UDM_DOCUMENT *D, UDM_CHARSET *doccs)
{
  int rc= UDM_OK;
  UDM_LOCK_CHECK_OWNER(A, UDM_LOCK_CONF);
  if (doccs && doccs != D->lcs)
  {
    UDM_DOCUMENT IDoc;
    UdmDocInit(&IDoc);
    UdmVarListReplaceLst(&IDoc.Sections, &D->Sections, NULL, "*");
    if (UDM_OK != (rc= UdmURLActionNoLock(A, &IDoc, UDM_URL_ACTION_SQLIMPORTSEC)))
      goto ex;
    if (IDoc.TextList.nitems)
    {
      size_t i;
      UDM_CONV conv;
      /*
         Convert imported sections from local
         charset to document charset
      */
      UdmConvInit(&conv, D->lcs, doccs, UDM_RECODE_HTML);

      for (i= 0; i < IDoc.TextList.nitems; i++)
      {
        UDM_TEXTITEM *I= &IDoc.TextList.Item[i];
        size_t slen= strlen(I->str);
        char *newval= (char*) UdmMalloc(slen * 12 + 1);
        UdmConv(&conv, newval, slen * 12 + 1, I->str, slen + 1);
        UDM_FREE(I->str);
        I->str= newval;
        I->flags= 0;
        UdmTextListAdd(&D->TextList, I);
      }
    }
    UdmDocFree(&IDoc);
  }
  else
  {
    if (UDM_OK != (rc= UdmURLActionNoLock(A, D, UDM_URL_ACTION_SQLIMPORTSEC)))
      goto ex;
  }
ex:
  return rc;
}



static void
UdmWordListMarkBadWords(UDM_ENV *Conf, UDM_WORDLIST *Words, const char *lang)
{
  size_t wordnum;

  for(wordnum= 0; wordnum < Words->nwords; wordnum++)
  {
    const char  *w= Words->Word[wordnum].word;
    size_t    wlen=strlen(w);
    
    if(wlen > Conf->WordParam.max_word_len ||
       wlen < Conf->WordParam.min_word_len ||
       UdmStopListListFind(&Conf->StopWord, w, lang ) != NULL)
    {
      Words->Word[wordnum].secno= 0;
    }  
  }
}


static void
UdmCrosswordListMarkBadWords(UDM_ENV *Conf,
                             UDM_CROSSLIST *Words,
                             const char *lang)
{
  size_t wordnum;
  
  for(wordnum=0; wordnum < Words->ncrosswords; wordnum++)
  {
    const char  *w= Words->CrossWord[wordnum].word;
    size_t    wlen=strlen(w);
    if (wlen > Conf->WordParam.max_word_len ||
        wlen < Conf->WordParam.min_word_len ||
        UdmStopListListFind(&Conf->StopWord, w, lang) != NULL)
    {
      Words->CrossWord[wordnum].weight= 0;
    }  
  }
}


static void
UdmCustomLog(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc)
{
  const char *logfmt;

  UDM_LOCK_CHECK_OWNER(Indexer, UDM_LOCK_CONF);
  
  if ((logfmt= UdmVarListFindStr(&Indexer->Conf->Vars, "CustomLog", NULL)))
  {
    UDM_DSTR line;
    char buf[256];
    const char *datefmt;
    struct tm *tim;
    time_t t;
    UdmDSTRInit(&line, 1024);
    t= time(NULL);
    tim= localtime(&t);
    datefmt= UdmVarListFindStr(&Indexer->Conf->Vars, "DateFormat", "%a, %d %b %Y, %X");
    strftime(buf, sizeof(buf), datefmt , tim);
    UdmVarListReplaceStr(&Doc->Sections, "CurrentTime", buf);
    UdmVarListReplaceInt(&Doc->Sections, "PID", (int) getpid());
    UdmDSTRParse(&line, logfmt, &Doc->Sections);
    fprintf(stdout, "%s\n", line.data);
    UdmDSTRFree(&line);
  }
}


#define UDM_ROBOT_FILE_TYPE_ROBOTS_TXT 0
#define UDM_ROBOT_FILE_TYPE_SITEMAP    1

static int
UdmGetAndParseRobotFile(UDM_AGENT *Indexer,
                        UDM_SERVER *Server,
                        UDM_HREFLIST *Sitemap,
                        UDM_DOCUMENT *Doc,
                        const char *UseRobotsTxtURL,
                        int filetype)
{
  UDM_SERVER  *rServer;
  char    *rurl;
  UDM_DOCUMENT  rDoc;
  size_t          rurlen;
  int rc, status;

  UdmDocInit(&rDoc);
  rDoc.Buf.maxsize= Doc->Buf.maxsize;
  rDoc.Spider= Doc->Spider;
  rDoc.Buf.buf= (char*)UdmMalloc(Doc->Buf.maxsize + 1);
  rDoc.Buf.buf[0]= '\0';

  if (UseRobotsTxtURL)
  {
    rurl = UdmStrdup(UseRobotsTxtURL);
  }
  else
  {
    rurlen= 32 + strlen(UDM_NULL2EMPTY(Doc->CurURL.schema)) + 
                 strlen(UDM_NULL2EMPTY(Doc->CurURL.hostinfo));
    rurl = (char*)UdmMalloc(rurlen);
    snprintf(rurl, rurlen, "%s://%s/robots.txt", 
             UDM_NULL2EMPTY(Doc->CurURL.schema),
             UDM_NULL2EMPTY(Doc->CurURL.hostinfo));
  }

  UdmVarListAddStr(&rDoc.Sections,"URL",rurl);
  UdmVarListReplaceInt(&rDoc.Sections, "URL_ID", UdmStrHash32(rurl));
  UdmURLParse(&rDoc.CurURL,rurl);
  UdmLog(Indexer,UDM_LOG_INFO,"ROBOTS: %s",rurl);

  UDM_GETLOCK(Indexer,UDM_LOCK_CONF);
  rServer = UdmServerFind(Indexer->Conf, &Indexer->Conf->Servers, rurl, NULL);
  UdmDocAddDocExtraHeaders(&rDoc);
  UdmDocAddConfExtraHeaders(Indexer->Conf,&rDoc);
  UDM_RELEASELOCK(Indexer,UDM_LOCK_CONF);

  if (rServer == NULL)
    rServer = Server;
  UdmVarListReplaceLst(&rDoc.Sections, &rServer->Vars, NULL, "*");
  UdmDocAddServExtraHeaders(rServer, &rDoc);
  UdmVarListLog(Indexer,&rDoc.RequestHeaders,UDM_LOG_DEBUG,"Request");
   
  UdmAgentSetTask(Indexer,  "Resolving");
  UDM_THREADINFO(Indexer,"Resolving",rurl);
  UdmDocLookupConn(Indexer,&rDoc);
  UdmAgentSetTask(Indexer,  "Downloading");
  UDM_THREADINFO(Indexer,"Getting",rurl);
  rc= UdmGetURL(Indexer,&rDoc);
  UdmParseHTTPResponse(Indexer,&rDoc);
  UdmDocProcessResponseHeaders(Indexer,&rDoc);
  UdmDocContentDecode(Indexer,&rDoc);
  UdmVarListLog(Indexer,&rDoc.Sections,UDM_LOG_DEBUG,"Response");
  UDM_FREE(rurl);

  status= UdmVarListFindInt(&rDoc.Sections, "Status", 0);
  
  if (filetype == UDM_ROBOT_FILE_TYPE_ROBOTS_TXT)
  {
    if (status >= 500)
    {
      /*
        Can't access to robots.txt file.
        Set status of the original URL to VISITLATER.
      */
      Doc->method = UDM_METHOD_VISITLATER;
    }
    else if (status == UDM_HTTP_STATUS_OK)
    {
      UDM_GETLOCK(Indexer, UDM_LOCK_CONF);
      rc= UdmRobotParse(Indexer, rServer, Sitemap, rDoc.Buf.content, 
                        UDM_NULL2EMPTY(rDoc.CurURL.hostinfo));
      UDM_RELEASELOCK(Indexer,UDM_LOCK_CONF);
    }
    else
    {
      UDM_GETLOCK(Indexer, UDM_LOCK_CONF);
      UdmRobotAddEmpty(&Indexer->Conf->Robots, UDM_NULL2EMPTY(Doc->CurURL.hostinfo));
      UDM_RELEASELOCK(Indexer,UDM_LOCK_CONF);
    }
  }
  else if (filetype == UDM_ROBOT_FILE_TYPE_SITEMAP)
  {
    if (status == UDM_HTTP_STATUS_OK)
    {
      size_t buflen= rDoc.Buf.buf - rDoc.Buf.content + rDoc.Buf.size;
      UdmSitemapParse(Indexer, Sitemap, UseRobotsTxtURL,
                      rDoc.Buf.content, buflen,
                      UDM_NULL2EMPTY(rDoc.CurURL.hostinfo));
    }
  }
  
  UdmDocFree(&rDoc);
  return rc;
}


/*
  Process robots.txt
  Download if has not been downloaded yet.
  Then check the current document.
*/

static int
UdmRobotsCheck(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc, UDM_SERVER *Server)
{
  int result= UDM_OK;
  const char *UseRobotsTxtURL= UdmVarListFindStr(&Server->Vars, "UseRobotsTxtURL", NULL);
  UDM_ROBOT_RULE  *rule;
  int    take_robots;
  size_t mutex_num;

  /* If neither "http" nor "https" nor UseRobotsTxtURL - nothing to do */
  if (strncasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "http", 4) &&
      !UseRobotsTxtURL)
    return UDM_OK;
  
  if (!Doc->Spider.use_robots)
  {
    UdmLog(Indexer,UDM_LOG_WARN, "robots.txt support is disallowed for '%s'", 
           UDM_NULL2EMPTY(Doc->CurURL.hostinfo));
    UDM_GETLOCK(Indexer,UDM_LOCK_CONF);
    result= UdmRobotParse(Indexer, NULL, NULL, NULL, UDM_NULL2EMPTY(Doc->CurURL.hostinfo));
    UDM_RELEASELOCK(Indexer,UDM_LOCK_CONF);
    return result;
  }
  
  /*
    hostinfo can be NULL when UseRobotsTxtURL is given
    and the current schema is file:///
  */
  mutex_num= Doc->CurURL.hostinfo ? UdmStrCRC32(Doc->CurURL.hostinfo) : 0;
  mutex_num= UDM_LOCK_ROBOT_FIRST + (mutex_num % UDM_LOCK_ROBOT_COUNT);
  UdmAgentSetTask(Indexer,  "Robots");

  /*
    Get robots.txt
    Note: Don't use "goto" below until mutex_num is unlocked
  */
  UDM_GETLOCK(Indexer, mutex_num);
  UDM_GETLOCK(Indexer,UDM_LOCK_CONF);
  take_robots= !UdmRobotFind(&Indexer->Conf->Robots, UDM_NULL2EMPTY(Doc->CurURL.hostinfo));
  UDM_RELEASELOCK(Indexer,UDM_LOCK_CONF);
  if(take_robots)
  {
    size_t i;
    int UseSitemap= UdmVarListFindBool(&Server->Vars, "UseSitemap", 1);
    UDM_HREFLIST Sitemap;
    UdmHrefListInit(&Sitemap);
    result= UdmGetAndParseRobotFile(Indexer, Server, &Sitemap, Doc,
                                    UseRobotsTxtURL, UDM_ROBOT_FILE_TYPE_ROBOTS_TXT);
    for (i= 0; UseSitemap && i < Sitemap.nhrefs; i++)
    {
      char *alstr= NULL;
      UDM_SERVER *SitemapServer= UdmServerFind(Indexer->Conf, &Indexer->Conf->Servers, Sitemap.Href[i].url, &alstr);
      if (SitemapServer)
      {
        UdmGetAndParseRobotFile(Indexer, Server, &Doc->Hrefs, Doc,
                                alstr ? alstr : Sitemap.Href[i].url,
                                UDM_ROBOT_FILE_TYPE_SITEMAP);
      }
      UDM_FREE(alstr);
    }
    UdmHrefListFree(&Sitemap);
  }
  UDM_RELEASELOCK(Indexer, mutex_num);
        
  if (result != UDM_OK)
    return result;

  /* Check whether URL is disallowed by robots.txt */
  UDM_GETLOCK(Indexer,UDM_LOCK_CONF);
  if ((rule= UdmRobotRuleFind(&Indexer->Conf->Robots, &Doc->CurURL)))
  {
    UdmLog(Indexer,UDM_LOG_WARN,"robots.txt: '%s %s'",(rule->cmd==UDM_METHOD_DISALLOW)?"Disallow":"Allow",rule->path);
    if (rule->cmd == UDM_METHOD_DISALLOW)
      Doc->method= rule->cmd;
  }
  UDM_RELEASELOCK(Indexer,UDM_LOCK_CONF);

  return result;
}


static int
UdmDocGetRemote(UDM_AGENT *A, UDM_DOCUMENT *Doc,
                const char *origurl, const char *aliasurl, const char *alias)
{
  int rc= UDM_OK, mp3type= UDM_MP3_UNKNOWN;
  int state, start= Doc->method == UDM_METHOD_CHECKMP3 ||
                    Doc->method == UDM_METHOD_CHECKMP3ONLY ? 1 : 0;
  
  if (Doc->method == UDM_METHOD_IMPORTONLY)
  {
    Doc->Buf.size= udm_snprintf(Doc->Buf.buf, Doc->Buf.maxsize,
                               "HTTP/1.0 200 OK\r\n"
                               "Content-Type: text/plain; charset=%s\r\n"
                               "X-Method: ImportOnly\r\n\r\n"
                               " ", Doc->lcs->name);
    if(origurl != NULL)
    {
      UdmVarListReplaceStr(&Doc->Sections,"URL",origurl);
      UdmVarListReplaceInt(&Doc->Sections, "URL_ID", UdmStrHash32(origurl));
      UdmURLParse(&Doc->CurURL,origurl);
    }
    
    UdmParseHTTPResponseAndHeaders(A, Doc);
    return UDM_OK;
  }
  
  for(state= start; state >= 0; state--)
  {
    int status;
    const char *hdr= NULL;
    
    if(state==1)hdr="bytes=0-256";
    if(mp3type==UDM_MP3_TAG)hdr="bytes=-128";
    
    UdmVarListReplaceInt(&Doc->Sections, "Status", UDM_HTTP_STATUS_UNKNOWN);
    
    if(aliasurl != NULL)
    {
      UdmVarListReplaceStr(&Doc->Sections,"URL",alias);
      UdmVarListReplaceInt(&Doc->Sections, "URL_ID", UdmStrHash32(alias));
      UdmURLParse(&Doc->CurURL,alias);
    }
    
    UdmVarListLog(A, &Doc->RequestHeaders, UDM_LOG_DEBUG, "Request");
      
    if(hdr)
    {
      UdmVarListAddStr(&Doc->RequestHeaders, "Range", hdr);
      UdmLog(A, UDM_LOG_INFO, "Range: [%s]", hdr);
    }
        
    rc= UdmGetURL(A, Doc);
      
    if(hdr)
    {
      UdmVarListDel(&Doc->RequestHeaders, "Range");
    }
    
    if(origurl != NULL)
    {
      UdmVarListReplaceStr(&Doc->Sections,"URL",origurl);
      UdmVarListReplaceInt(&Doc->Sections, "URL_ID", UdmStrHash32(origurl));
      UdmURLParse(&Doc->CurURL,origurl);
    }
    
    if(rc != UDM_OK)
      return rc;
    
    status= UdmParseHTTPResponseAndHeaders(A, Doc);
    if (status !=206 && status != 200)
      break;
    
    if(state == 1)  /* Needs guessing */
    {
      if(UDM_MP3_UNKNOWN != (mp3type= UdmMP3Type(Doc)))
      {
        UdmVarListReplaceStr(&Doc->Sections,"Content-Type","audio/mpeg");
        if(Doc->method == UDM_METHOD_CHECKMP3ONLY &&
           mp3type != UDM_MP3_TAG)
          break;
      }
      if(Doc->method == UDM_METHOD_CHECKMP3ONLY)
        break;
    }
  }
  return rc;
}


__C_LINK int __UDMCALL
UdmIndexNextURL(UDM_AGENT *Indexer)
{
  int rc= UDM_OK, status= 0, parse_res;
  UDM_DOCUMENT  Doc;
  UDM_CHARSET *doccs= NULL;
  const char  *url, *alias;
  char  *origurl= NULL, *aliasurl= NULL;
  UDM_SERVER  *Server = NULL;
  char reason[1024]= "";
  char state_param[128];
  char *alstr= NULL;
  const char *lang= NULL;
  UdmDebugEnter();
  
  UdmDocInit(&Doc);
  
  UdmAgentSetTask(Indexer, "Selecting");
  UDM_THREADINFO(Indexer, "Selecting", "");

  if (UDM_OK != (rc= UdmStoreHrefs(Indexer)) ||
      UDM_OK != (rc= Indexer->action) ||
      UDM_OK != (rc= UdmNextTarget(Indexer, &Doc)))
    goto ret;
  
  
  UdmAgentSetTask(Indexer, "Preparing");
  url=UdmVarListFindStr(&Doc.Sections,"URL","");
  UdmVarListReplaceInt(&Doc.Sections,"crc32old",UdmVarListFindInt(&Doc.Sections,"crc32",0));
  UdmLog(Indexer,UDM_LOG_INFO,"URL: %s",url);
  udm_snprintf(state_param, sizeof(state_param) - 1, "%s", url);
  Indexer->State.param= state_param;
  
#ifdef HAVE_SETPROCTITLE
  /* To see the URL being indexed in "ps" output on FreeBSD */
  /* Do it if single thread version */
  if(!(Indexer->handle)) setproctitle("%s",url);
#endif
  
  
  /* Collect information from Conf */
  if(!Doc.Buf.buf)
  {
    UdmAgentSetTask(Indexer, "Allocating buffer");
    /* Alloc buffer for document */
    UDM_GETLOCK(Indexer,UDM_LOCK_CONF);
    Doc.Buf.maxsize=(size_t)UdmVarListFindInt(&Indexer->Conf->Vars,"MaxDocSize",UDM_MAXDOCSIZE);
    UDM_RELEASELOCK(Indexer,UDM_LOCK_CONF);
    if ((Doc.Buf.buf=(char*)UdmMalloc(Doc.Buf.maxsize + 1)) == NULL)
    {
      UdmLog(Indexer, UDM_LOG_ERROR, "Out of memory.");
      rc= UDM_ERROR;
      goto ret;
    }
    Doc.Buf.buf[0]='\0';
  }
  
  /* Check that URL has valid syntax */
  if (UDM_URL_OK != (parse_res= UdmURLParse(&Doc.CurURL,url)))
  {
    UdmLog(Indexer, UDM_LOG_WARN, "%s: %s", UdmURLErrorStr(parse_res), url);
    Doc.method = UDM_METHOD_DISALLOW;
    goto flush;
  }
  
  
  /* Don't index robots.txt */
  if ((Doc.CurURL.filename != NULL) &&
           (!strcmp(Doc.CurURL.filename, "robots.txt")))
  {
    Doc.method= UDM_METHOD_DISALLOW;
    goto flush;
  }
  
  
  /* Find correspondent Server */
  UdmAgentSetTask(Indexer, "Checking Server");
  UDM_GETLOCK(Indexer,UDM_LOCK_CONF);
  Server= UdmServerFind(Indexer->Conf, &Indexer->Conf->Servers, url, &alstr);
  UDM_RELEASELOCK(Indexer,UDM_LOCK_CONF);
  if (!Server)
  {
    UdmLog(Indexer,UDM_LOG_WARN,"No 'Server' command for url");
    Doc.method= UDM_METHOD_DISALLOW;
    goto flush;
  }


  /* Prepare for downloading */
  UDM_GETLOCK(Indexer,UDM_LOCK_CONF);
  Doc.lcs= Indexer->Conf->lcs;
  UdmVarList2Spider(&Doc.Spider,&Server->Vars);
  Doc.Spider.follow= Server->follow;
  UdmDocAddConfExtraHeaders(Indexer->Conf,&Doc);
  UdmDocAddServExtraHeaders(Server,&Doc);
  UDM_RELEASELOCK(Indexer,UDM_LOCK_CONF);

  UdmVarListReplaceLst(&Doc.Sections, &Server->Vars,NULL,"*");
  UdmVarListReplaceInt(&Doc.Sections, "Site_id", UdmServerGetSiteId(Indexer, Server, &Doc.CurURL));
  UdmVarListReplaceInt(&Doc.Sections, "Server_id", Server->site_id); /* TODO: not re-entrant */
  UdmVarListReplaceInt(&Doc.Sections, "MaxHops", Doc.Spider.maxhops);
  UdmVarListReplaceInt(&Doc.Sections, "MaxDocPerSite", Doc.Spider.doc_per_site);
  
  
  /* Apply aliases */
  if (alstr != NULL)
  {
    /* Server Primary alias found */
    UdmVarListReplaceStr(&Doc.Sections,"Alias",alstr);
    UdmFree(alstr);
  }
  else
  {
    /* Apply non-primary alias */
    if (UDM_OK != (rc= UdmDocAlias(Indexer,&Doc)))
      goto flush;
  }


  /* Check hops, filters */
  UdmAgentSetTask(Indexer, "Checking filters");
  if (UDM_OK != (rc= UdmDocCheck(Indexer,Server,&Doc)))
    goto ret;

  if (Doc.method == UDM_METHOD_VISITLATER)
    goto flush;


  /* Replace URL with alias, if exists */
  if((alias = UdmVarListFindStr(&Doc.Sections, "Alias", NULL)))
  {
    const char *u = UdmVarListFindStr(&Doc.Sections, "URL", NULL);
    origurl= (char*)UdmStrdup(u);
    aliasurl= (char*)UdmStrdup(alias);
    UdmLog(Indexer,UDM_LOG_EXTRA,"Alias: '%s'", alias);
    UdmVarListReplaceStr(&Doc.Sections, "URL", alias);
    UdmVarListReplaceInt(&Doc.Sections, "URL_ID", UdmStrHash32(alias));
    UdmURLParse(&Doc.CurURL, alias);
  }


  /* Resolve */
  UdmAgentSetTask(Indexer,  "Resolving");
  UdmDocLookupConn(Indexer, &Doc);


  /* Check for too many errors on this server */
  if((Doc.connp.net_errors >= Doc.Spider.max_net_errors)&&
     (Doc.Spider.max_net_errors))
  {
    size_t next_index_time= time(NULL) + Doc.Spider.net_error_delay_time;
    char  buf[UDM_MAXTIMESTRLEN];
    UdmLog(Indexer,UDM_LOG_WARN,"Too many network errors (%d) for this server", Doc.connp.net_errors);
    UdmVarListReplaceInt(&Doc.Sections,"Status",UDM_HTTP_STATUS_SERVICE_UNAVAILABLE);
    UdmTime_t2HttpStr((int)next_index_time, buf, sizeof(buf));
    UdmVarListReplaceStr(&Doc.Sections,"Next-Index-Time",buf);
    Doc.method= UDM_METHOD_VISITLATER;
    goto flush;
  }
  
  
  /* Process CrawlDelay */
  {
    time_t now, next;
    if (Doc.Spider.crawl_delay &&
        (now= time(NULL)) <
         (next= Doc.connp.host_last_used + Doc.Spider.crawl_delay))
    {
      time_t diff= next - now;
      UdmLog(Indexer, UDM_LOG_DEBUG, "CrawlDelay %d/%d",
             (int) Doc.Spider.crawl_delay, (int) diff);
      UdmAgentSetTask(Indexer,  "CrawlDelay");
      UDMSLEEP(diff);
    }
  }
  
  
  if (Doc.method == UDM_METHOD_DISALLOW ||
      Doc.method == UDM_METHOD_VISITLATER)
    goto flush;
  
  
  if (Doc.method == UDM_METHOD_IMPORTONLY)
    goto download;

  /* Add "Hostname:" for virtual hosts */
  UdmDocAddDocExtraHeaders(&Doc);


  /*
    Download robots.txt it has not been downloaded before.
    Then check the current URL againts host robot rules.
  */
  if (UDM_OK != (rc= UdmRobotsCheck(Indexer, &Doc, Server)))
    goto ret;


  /* Restore original URL */
  if(origurl != NULL)
  {
    UdmVarListReplaceStr(&Doc.Sections, "URL", origurl);
    UdmVarListReplaceInt(&Doc.Sections, "URL_ID", UdmStrHash32(origurl));
    UdmURLParse(&Doc.CurURL, origurl);
  }


download:

  if (Doc.method == UDM_METHOD_DISALLOW ||
      Doc.method == UDM_METHOD_VISITLATER)
    goto flush;


  if (!(Indexer->flags & UDM_FLAG_REINDEX))
  {
    const char *l= UdmVarListFindStr(&Doc.Sections,"Last-Modified",NULL);
    if (l) UdmVarListReplaceStr(&Doc.RequestHeaders, "If-Modified-Since", l);
  }

  UdmAgentSetTask(Indexer, "Downloading");
  UDM_THREADINFO(Indexer,"Getting",url);
    
  if (UDM_OK != (rc= UdmDocGetRemote(Indexer, &Doc,
                                         origurl, aliasurl, alias)))
    goto flush;
  
  status= UdmVarListFindInt(&Doc.Sections, "Status", 0);

  /*
    Add URL from "Location:" header.
    This is to give a chance for a concurent thread to take it.
  */
  if (UDM_OK != (rc= UdmDocStoreHrefs(Indexer,&Doc)))
    goto ret;
    
  /* Increment indexer's download statistics */
  Indexer->nbytes+=Doc.Buf.size + Doc.Buf.content_length;
  Indexer->ndocs++;
  
  if((!Doc.Buf.content) && (status < 500))
  {
    UdmLog(Indexer, UDM_LOG_ERROR, "No data received");
    status= UDM_HTTP_STATUS_SERVICE_UNAVAILABLE;
    UdmVarListReplaceInt(&Doc.Sections,"Status",status);
  }
  
  if (status != UDM_HTTP_STATUS_OK &&
      status != UDM_HTTP_STATUS_PARTIAL_OK)
    goto flush;
  
  
  /* Parse document */
  UdmAgentSetTask(Indexer, "Parsing");
  UDM_THREADINFO(Indexer,"Parsing",url);
  if (UDM_OK != (rc= UdmDocParseContent(Indexer, &Doc)))
    goto ret;
    
    
  /* Build langmap for guesser */
  UDM_GETLOCK(Indexer,UDM_LOCK_CONF);
  if (Indexer->Conf->LangMaps.nmaps && Doc.method != UDM_METHOD_DISALLOW)
  {
    register size_t t;
    int flag= !strcasecmp(UdmVarListFindStr(&Indexer->Conf->Vars, "LangMapUpdate", "no"), "yes");

    bzero((void*)Indexer->LangMap, sizeof(UDM_LANGMAP));
    for (t= 0; t <= UDM_LM_HASHMASK; t++)
      Indexer->LangMap->memb[t].index= t;
    for(t= 0; t < Doc.TextList.nitems; t++)
    {
      UDM_TEXTITEM *Item=&Doc.TextList.Item[t];
      UdmBuildLangMap(Indexer->LangMap, Item->str, strlen(Item->str), flag);
    }
  }
  UDM_RELEASELOCK(Indexer,UDM_LOCK_CONF);


  {
    const char *csname;

    /* Guess charset */
    UdmGuessCharSet(Indexer, &Doc, &Indexer->Conf->LangMaps, Indexer->LangMap);
    UdmLog(Indexer, UDM_LOG_EXTRA, "Guesser: Lang: %s, Charset: %s",
           lang= UdmVarListFindStr(&Doc.Sections,"Content-Language",""),
           csname= UdmVarListFindStr(&Doc.Sections,"Charset",""));


    /*
      Convert Href variables into LocalCharset.
      Typically, for RSS indexing.
    */
    if ((doccs= UdmGetCharSet(csname)) && doccs != Doc.lcs)
    {
      size_t h;
      UDM_CONV conv;
      UdmConvInit(&conv, doccs, Doc.lcs, UDM_RECODE_HTML);
      for (h= 0; h < Doc.Hrefs.nhrefs; h++)
      {
        UdmVarListConvert(&Doc.Hrefs.Href[h].Vars, &conv);
      }
    }
  }


  /* Parse additional content */
  if (Doc.Spider.index)
  {
    /*
      Doc.Spider.index can be unset e.g. in HTML parser by:
      <META NAME="ROBOTS" CONTENT="NOINDEX">
      Don't parse URL and headers in this case.
    */
    UdmParseURLText(Indexer,&Doc);
    UdmParseHeaders(Indexer,&Doc);
  }


  /* Process "SQLImportSection" */
  UDM_GETLOCK(Indexer, UDM_LOCK_CONF);
  rc= UdmDocImportSections(Indexer, &Doc, doccs);
  UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);
  if (UDM_OK != rc)
    goto flush;


  /* Create "HTTP.LocalCharsetContent" variable if requested */
  if (UDM_OK != (rc= UdmVarListAddLocalCharsetContentStr("HTTP.LocalCharsetContent", &Doc, doccs)))
    goto flush;


  /* Process "afterguesser" user-defined sections */
  UDM_GETLOCK(Indexer,UDM_LOCK_CONF);
  UdmParseSections(Indexer, &Indexer->Conf->SectionGsrMatch, &Doc, doccs);
  UDM_RELEASELOCK(Indexer,UDM_LOCK_CONF);


  if (Doc.method == UDM_METHOD_HREFONLY)
    goto flush;


  /* Process IndexIf, SkipIf */
  {
    size_t nfilters;
  
    UDM_GETLOCK(Indexer,UDM_LOCK_CONF);
    nfilters= Indexer->Conf->SectionFilters.nmatches;
    UDM_RELEASELOCK(Indexer,UDM_LOCK_CONF);

    if (nfilters)
    {
      UDM_VARLIST TmpList;
      int sec_method;
      
      /* Save original sections into TmpList */
      UdmVarListInit(&TmpList);
      UdmVarListReplaceLst(&TmpList, &Doc.Sections, NULL, "*");
      
      /* Now parse TextList into words and create sections */
      UdmPrepareWords(Indexer,&Doc);
      
      /* Now check newly created sections agains IndexIf/SkipIf*/
      UDM_GETLOCK(Indexer,UDM_LOCK_CONF);
      sec_method= UdmSectionFilterFind(&Indexer->Conf->SectionFilters, &Doc, reason);
      UDM_RELEASELOCK(Indexer,UDM_LOCK_CONF);
      
      if (sec_method != UDM_METHOD_INDEX)
      {
        /* Restore saved sections */
        UdmVarListFree(&Doc.Sections);
        UdmVarListReplaceLst(&Doc.Sections, &TmpList, NULL, "*");
        
        /* Remove unnesessary data */
        UdmVarListDel(&Doc.Sections, "CachedCopy");
        UdmWordListFree(&Doc.Words);
        UdmCrossListFree(&Doc.CrossWords);
        if (sec_method == UDM_METHOD_VISITLATER)
        {
          UdmHrefListFree(&Doc.Hrefs);
          Doc.method= sec_method;
        }
      }
      UdmVarListFree(&TmpList);
      UdmLog(Indexer,UDM_LOG_DEBUG,"SectionFilter: %s",reason);
    }
    else
    {
      /* Parse TextList into words and create sections */
      UdmPrepareWords(Indexer,&Doc);
    }
  }


flush:

  /*
    Delete old links from the database.
    Must be done *before* StoreHrefs for RSS indexing.
  */
  UDM_GETLOCK(Indexer, UDM_LOCK_CONF);
  if (Doc.Spider.collect_links)
    rc= UdmURLActionNoLock(Indexer, &Doc, UDM_URL_ACTION_LINKS_DELETE);
  UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);
  if (UDM_OK != rc)
    goto ret;


  /* Store links to the link cache, then to the database */
  if ((UDM_OK != (rc= UdmDocStoreHrefs(Indexer,&Doc))) ||
      (UDM_OK != (rc= UdmStoreHrefs(Indexer))))
    goto ret;


  /* Lock mutex: localtime and printf are not reentrant */
  UDM_GETLOCK(Indexer,UDM_LOCK_CONF);
  UdmCustomLog(Indexer, &Doc);
  UDM_RELEASELOCK(Indexer,UDM_LOCK_CONF);
  
  
  /* Free unnecessary information */
  UdmHrefListFree(&Doc.Hrefs);
  UdmVarListFree(&Doc.RequestHeaders);
  UdmTextListFree(&Doc.TextList);
  UDM_FREE(Doc.Buf.buf);
  Doc.Buf.maxsize=0;

  /* Remove StopWords */
  UDM_GETLOCK(Indexer,UDM_LOCK_CONF);
  UdmWordListMarkBadWords(Indexer->Conf, &Doc.Words, lang);
  UdmCrosswordListMarkBadWords(Indexer->Conf, &Doc.CrossWords, lang);
  UDM_RELEASELOCK(Indexer,UDM_LOCK_CONF);

  /* Update document in the database */
  UDM_GETLOCK(Indexer, UDM_LOCK_CONF);
  rc= UdmURLActionNoLock(Indexer, &Doc, UDM_URL_ACTION_FLUSH);
  UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);

  /*
    Check action.
    The main thread may have requested to return something else than UDM_OK.
  */
  UDM_GETLOCK(Indexer, UDM_LOCK_THREAD);
  if (rc == UDM_OK)
    rc= Indexer->action;
  UDM_RELEASELOCK(Indexer, UDM_LOCK_THREAD);


ret:
  /* Free all allocated memory */
  UDM_FREE(origurl);
  UDM_FREE(aliasurl);
  UdmDocFree(&Doc);
  Indexer->State.param= NULL;
  UdmAgentSetTask(Indexer, "Unknown");
  UdmDebugReturn(rc);
}


#include "udm_signals.h"
#include "udm_env.h"

unsigned int seconds= 0; /* To sleep between documents    */
int flags=            0; /* For indexer            */
int total_threads=    0; /* Total threads number         */
int sleep_threads=    0; /* Number of sleepping threads      */
int max_index_time=  -1;
int cur_url_number=   0;
int maxthreads=       1;
UDM_AGENT *ThreadIndexers= NULL;
int thd_errors= 0;
udm_thread_t *threads= NULL;

#define UDM_NOTARGETS_SLEEP 10

#ifdef  WIN32
unsigned int __stdcall thread_main(void *arg)
{
  char *str_buf;
#else
void * thread_main(void *arg)
{
#endif
  UDM_AGENT * Indexer = (UDM_AGENT *)arg;
  int res= UDM_OK;
  int done= 0;
  int i_sleep= 0;

  while (!done)
  {
    if (max_index_time>=0)
    {
      time_t now;
      time(&now);
      if ((now-Indexer->start_time)>max_index_time)
        break;
    }

    UDM_GETLOCK(Indexer, UDM_LOCK_THREAD);
    if (have_sighup)
    {
      /* UdmReloadEnv(Indexer); */
      have_sighup=0;
    }

    if (have_sigint || have_sigterm)
    {
      int z;
      UdmLog(Indexer, UDM_LOG_ERROR, "%s received. Terminating. Please wait...", (have_sigint) ? "SIGINT" : "SIGTERM");
      for (z = 0 ; z < maxthreads; z++)
      {
        if (ThreadIndexers[z].handle)
          UdmAgentSetAction(&ThreadIndexers[z], UDM_TERMINATED);
      }
      UdmAgentSetAction(Indexer, UDM_TERMINATED);
      have_sigint = have_sigterm = 0;
    }

    if (have_sigusr1)
    {
      UdmIncLogLevel(Indexer);
      have_sigusr1 = 0;
    }

    if (have_sigusr2)
    {
      UdmDecLogLevel(Indexer);
      have_sigusr2 = 0;
    }
    UDM_RELEASELOCK(Indexer, UDM_LOCK_THREAD);

    if (done)break;

    if (res == UDM_OK || res == UDM_NOTARGET)
    { /* Possible after bad startup */
      res= UdmIndexNextURL(Indexer);
    }

    UDM_GETLOCK(Indexer, UDM_LOCK_THREAD);
    cur_url_number++;
    UDM_RELEASELOCK(Indexer, UDM_LOCK_THREAD);

    switch(res)
    {
      case UDM_OK:
        if (i_sleep)
        {
          UDM_GETLOCK(Indexer, UDM_LOCK_THREAD);
          sleep_threads--;
          UDM_RELEASELOCK(Indexer, UDM_LOCK_THREAD);
          i_sleep= 0;
        }
        break;


      case UDM_NOTARGET:
#ifdef HAVE_PTHREAD
        /*
         in multi-threaded environment we
         should wait for a moment when every thread
         has nothing to do
        */
        UdmURLAction(Indexer, NULL, UDM_URL_ACTION_FLUSH); /* flush DocCache */

        if (!i_sleep)
        {
          UDM_GETLOCK(Indexer, UDM_LOCK_THREAD);
          sleep_threads++;
          UDM_RELEASELOCK(Indexer, UDM_LOCK_THREAD);
          i_sleep= 1;
        }

        UDM_GETLOCK(Indexer, UDM_LOCK_THREAD);
        done= (sleep_threads >= total_threads);
        UDM_RELEASELOCK(Indexer, UDM_LOCK_THREAD);
        break;
#else
        done= 1;
        break;
#endif
      case UDM_ERROR:
        thd_errors++;
#ifdef WIN32
        str_buf= (char *)UdmMalloc(1024);
        udm_snprintf(str_buf, 1024, "Error: %s",  UdmEnvErrMsg(Indexer->Conf));
        UdmShowThreadInfo(0, NULL, str_buf);
        UDM_FREE(str_buf);
        UdmShowThreadInfo(Indexer, "Error", UdmEnvErrMsg(Indexer->Conf));
#endif
      case UDM_TERMINATED:
#ifdef WIN32
        UdmShowThreadInfo(Indexer, "Aborted", "");
#endif
      default:
#ifdef HAVE_PTHREAD
        /*
         in multi-threaded environment we
         should wait for a moment when every thread
         has nothing to do
        */
        if (!i_sleep)
        {
          if (res == UDM_ERROR)
            UdmLog(Indexer,UDM_LOG_ERROR,"Error: '%s'",UdmEnvErrMsg(Indexer->Conf));
          UDM_GETLOCK(Indexer, UDM_LOCK_THREAD);
          sleep_threads++;
          UDM_RELEASELOCK(Indexer, UDM_LOCK_THREAD);
          i_sleep=1;
        }
        UDM_GETLOCK(Indexer, UDM_LOCK_THREAD);
        done= (sleep_threads>=total_threads);
        UDM_RELEASELOCK(Indexer, UDM_LOCK_THREAD);
        break;
#else
        if (res == UDM_ERROR)
          UdmLog(Indexer,UDM_LOG_ERROR,"Error: '%s'",UdmEnvErrMsg(Indexer->Conf));
        done=1;
#endif
        break;
    }

    if ((seconds)&&(!done))
    {
      UdmAgentSetTask(Indexer, "Sleeping");
      UdmLog(Indexer,UDM_LOG_DEBUG,"Sleeping %d second(s)",seconds);
#ifndef WIN32
      Indexer->nsleepsecs+= seconds - UDMSLEEP(seconds);
#else
      Indexer->nsleepsecs+= seconds;
      UDMSLEEP(seconds);
#endif
    }
  
    if ((i_sleep) && (!done))
    {
      UdmLog(Indexer, UDM_LOG_ERROR, "%s, sleeping %d seconds (%d threads, %d sleeping)",
             (res == UDM_NOTARGET) ? "No targets" : 
             ((res == UDM_TERMINATED) ? "Terminating" : "An error occured"),
             UDM_NOTARGETS_SLEEP, total_threads, sleep_threads);
#ifndef WIN32
      Indexer->nsleepsecs+= UDM_NOTARGETS_SLEEP - UDMSLEEP(UDM_NOTARGETS_SLEEP);
#else
      Indexer->nsleepsecs+= UDM_NOTARGETS_SLEEP;
      UDMSLEEP(UDM_NOTARGETS_SLEEP);
#endif
    }
    UdmAgentSetTask(Indexer, "Unknown");
  }

  if (res!=UDM_ERROR)
  {
    time_t now, sec;
    float M = 0.0, K = 0.0;

    UdmURLAction(Indexer, NULL, UDM_URL_ACTION_FLUSH); /* flush DocCache */
    UdmWordCacheFlush(Indexer);

    time(&now);
    sec= now - Indexer->start_time - Indexer->nsleepsecs;
    if (sec > 0)
    {
      /* Convert to int64 - conversion from uint64 to double doesn't work on windows */
      M= ((udm_timer_t)Indexer->nbytes) / 1048576.0 / sec;
      if (M < 1.0) K = ((udm_timer_t)Indexer->nbytes) / 1024.0 / sec;
    }
    UdmLog(Indexer,UDM_LOG_ERROR,"Done (%d seconds, %u documents, %llu bytes, %5.2f %cbytes/sec.)",
           (int) sec, (int) Indexer->ndocs, (unsigned long long) Indexer->nbytes,
           (M < 1.0) ? K : M, (M < 1.0) ? 'K' : 'M' );
#if !defined(WIN32) && defined(HAVE_PTHREAD)
    {
      int z;
      for (z= 0 ; z < maxthreads; z++)
      if (ThreadIndexers[z].handle)
        pthread_kill(threads[z], SIGALRM); /* wake-up sleeping threads */
    }
#endif
  }
  UDM_GETLOCK(Indexer, UDM_LOCK_THREAD);
  total_threads--;
  UDM_RELEASELOCK(Indexer, UDM_LOCK_THREAD);

  UdmAgentFree(Indexer);

#ifdef WIN32
  return(0);
#else
  return(NULL);
#endif
}
