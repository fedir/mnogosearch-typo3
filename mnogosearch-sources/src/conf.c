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

#include "udm_common.h"
#include "udm_spell.h"
#include "udm_proto.h"
#include "udm_url.h"
#include "udm_parser.h"
#include "udm_conf.h"
#include "udm_log.h"
#include "udm_hrefs.h"
#include "udm_robots.h"
#include "udm_utils.h"
#include "udm_host.h"
#include "udm_server.h"
#include "udm_alias.h"
#include "udm_search_tl.h"
#include "udm_env.h"
#include "udm_match.h"
#include "udm_stopwords.h"
#include "udm_guesser.h"
#include "udm_unicode.h"
#include "udm_synonym.h"
#include "udm_vars.h"
#include "udm_db.h"
#include "udm_agent.h"
#include "udm_db_int.h"
#include "udm_chinese.h"

static int EnvLoad(UDM_CFG *Cfg,const char *cname);

/****************************  Load Configuration **********************/

int UdmSearchMode(const char * mode)
{
  if(!mode)return(UDM_MODE_ALL);
  if(!strcmp(mode,"all-minus"))return(UDM_MODE_ALL_MINUS);
  if(!strcmp(mode,"all-minus-half"))return(UDM_MODE_ALL_MINUS_HALF);
  if(!strcmp(mode,"all"))return(UDM_MODE_ALL);
  if(!strcmp(mode,"any"))return(UDM_MODE_ANY);
  if(!strcmp(mode,"bool"))return(UDM_MODE_BOOL);
  if(!strcmp(mode,"phrase"))return(UDM_MODE_PHRASE);
  return(UDM_MODE_ALL);
}


int UdmMatchMode(const char * mode)
{
  if(!mode)return(UDM_MATCH_FULL);
  if(!strcmp(mode,"wrd"))return(UDM_MATCH_FULL);
  if(!strcmp(mode,"full"))return(UDM_MATCH_FULL);
  if(!strcmp(mode,"beg"))return(UDM_MATCH_BEGIN);
  if(!strcmp(mode,"end"))return(UDM_MATCH_END);
  if(!strcmp(mode,"sub"))return(UDM_MATCH_SUBSTR);
  return(UDM_MATCH_FULL);
}

__C_LINK const char * __UDMCALL UdmFollowStr(int method)
{
  switch(method)
  {
    case UDM_FOLLOW_NO:    return "Page";
    case UDM_FOLLOW_PATH:  return "Path";
    case UDM_FOLLOW_SITE:  return "Site";
    case UDM_FOLLOW_WORLD: return "World";
    case UDM_FOLLOW_URLLIST: return "URLList";
  }
  return "<Unknown follow type>";
}


int UdmFollowType(const char * follow)
{
  if(!follow)return UDM_FOLLOW_UNKNOWN;
  if(!strcasecmp(follow,"no"))return(UDM_FOLLOW_NO);
  if(!strcasecmp(follow,"page"))return(UDM_FOLLOW_NO);
  if(!strcasecmp(follow,"yes"))return(UDM_FOLLOW_DEFAULT);
  if(!strcasecmp(follow,"path"))return(UDM_FOLLOW_PATH);
  if(!strcasecmp(follow,"site"))return(UDM_FOLLOW_SITE);
  if(!strcasecmp(follow,"world"))return(UDM_FOLLOW_WORLD);
  if(!strcasecmp(follow,"urllist"))return(UDM_FOLLOW_URLLIST);
  return(UDM_FOLLOW_UNKNOWN);
}

const char *UdmMethodStr(int method)
{
  switch(method)
  {
    case UDM_METHOD_DISALLOW:  return "Disallow";
    case UDM_METHOD_GET:    return "Allow";
    case UDM_METHOD_CHECKMP3ONLY:  return "CheckMP3Only";
    case UDM_METHOD_CHECKMP3:  return "CheckMP3";
    case UDM_METHOD_HEAD:    return "CheckOnly";
    case UDM_METHOD_HREFONLY:  return "HrefOnly";
    case UDM_METHOD_VISITLATER:  return "Skip";
    case UDM_METHOD_INDEX:    return "IndexIf";
    case UDM_METHOD_NOINDEX:  return "NoIndexIf";
    case UDM_METHOD_IMPORTONLY:  return "ImportOnly";
  }
  return "<Unknown method>";
}

int UdmMethod(const char *s)
{
  if (s == NULL) return UDM_METHOD_UNKNOWN;
  if(!strcasecmp(s,"Disallow"))    return UDM_METHOD_DISALLOW;
  if(!strcasecmp(s,"Allow"))    return UDM_METHOD_GET;
  if(!strcasecmp(s,"CheckMP3Only"))  return UDM_METHOD_CHECKMP3ONLY;
  if(!strcasecmp(s,"CheckMP3"))    return UDM_METHOD_CHECKMP3;
  if(!strcasecmp(s,"CheckOnly"))    return UDM_METHOD_HEAD;
  if(!strcasecmp(s,"HrefOnly"))    return UDM_METHOD_HREFONLY;
  if(!strcasecmp(s,"Skip"))    return UDM_METHOD_VISITLATER;
  if(!strcasecmp(s,"SkipIf"))    return UDM_METHOD_VISITLATER;
  if(!strcasecmp(s,"IndexIf"))    return UDM_METHOD_INDEX;
  if(!strcasecmp(s,"NoIndexIf"))    return UDM_METHOD_NOINDEX;
  if(!strcasecmp(s,"ImportOnly"))   return UDM_METHOD_IMPORTONLY;
  return UDM_METHOD_UNKNOWN;
}



void UdmWeightFactorsInit(char *res, const char *wf, size_t num)
{
  size_t len;
  int sn;
  
  for(sn=0;sn<256;sn++)
    res[sn]=1;
  
  len=strlen(wf);
  if((len>0)&&(len<256))
  {
    const char *sec;
    int secno;
    
    for(sec= wf + len - 1, secno= 1; sec >= wf; sec--)
    {
      if (*sec != '-' && *sec != '.')
      {
        res[secno]= UdmHex2Int(*sec);
        secno++;
      }
    }
  }
  
  for (sn= num + 1 ; sn < 256; sn++)
    res[sn]= 0;
}


size_t UdmWeightFactorsInit2(char *res,
                             UDM_VARLIST *V1,
                             UDM_VARLIST *V2,
                             const char *name)
{
  size_t numsections= UdmVarListFindInt(V1, "NumSections", 256);
  const char *wf1= UdmVarListFindStr(V1, name, "");
  const char *wf2= UdmVarListFindStr(V2, name, "");
  const char *wf3= wf2[0] ? wf2 : wf1;
  UdmWeightFactorsInit(res, wf3, numsections);
  return wf3[0] ? numsections : 0;
}

/* hold the path of the current config file */
static char current[1024] = "";

static void update_current(const char *name)
{
  char *slash;
  strcpy(current, name);
  if ((slash= strrchr(current, '/')))
  {
   *slash= 0;
  }
  else
  {
    *current= 0;
  }
}

static size_t rel_cur_name(char *res, size_t maxlen, const char *name)
{
  size_t n = 0;
  if (*current)
  {
#ifdef WIN32
    n= udm_snprintf(res, maxlen, "%s", name);
#else
    n= udm_snprintf(res, maxlen, "%s%s%s", current, UDMSLASHSTR, name);
#endif
  }
  else
  {
    strncpy(res, name, maxlen);
  }
  res[maxlen]= 0;
  return n;
}


static size_t
rel_name(UDM_ENV *Env, char *res, size_t maxlen,
         const char *varname, const char *dirname, const char *name)
{
  size_t    n;
  const char  *dir= UdmVarListFindStr(&Env->Vars, varname, dirname);
  if(name[0]=='/' || (name[0] && name[1] == ':'))
    n= udm_snprintf(res, maxlen, "%s", name);
  else
    n= udm_snprintf(res,maxlen,"%s%s%s",dir,UDMSLASHSTR,name);
  res[maxlen]='\0';
  return n;
}


/* Relative name for included .conf files */
static size_t
rel_etc_name(UDM_ENV *Env, char *res, size_t maxlen, const char *fname)
{
  return rel_name(Env, res, maxlen, "ConfDir", UDM_CONF_DIR, fname);
}


/*
  Relative name for langmap, stopwords, synonym files.
  Their position depends on --enable-fhs-layot.
*/
static size_t
rel_etc2_name(UDM_ENV *Env, char *res, size_t maxlen, const char *fname)
{
#ifdef HAVE_FHS_LAYOUT
  /* Better FHS layout */
  return rel_name(Env, res, maxlen, "ShareDir", UDM_SHARE_DIR, fname);
#else
  /* Traditional mnoGoSearch layout */
  return rel_name(Env, res, maxlen, "ConfDir", UDM_CONF_DIR, fname);
#endif
}


static size_t
rel_langmap_name(UDM_ENV *Env, char *res, size_t maxlen, const char *fname)
{
  return rel_etc2_name(Env, res, maxlen, fname);
}


static size_t
rel_stopwords_name(UDM_ENV *Env, char *res, size_t maxlen, const char *fname)
{
  return rel_etc2_name(Env, res, maxlen, fname);
}


static size_t
rel_synonym_name(UDM_ENV *Env, char *res, size_t maxlen, const char *fname)
{
  return rel_etc2_name(Env, res, maxlen, fname);
}


/*
  *.freq files is a special case.
  It's installed to /share/freq/xxx.freq with --enable-fhs-layout,
  and to /etc/xxx.freq otherwise.
*/
static size_t
rel_freq_name(UDM_ENV *Env, char *res, size_t maxlen, const char *fname)
{
#ifdef HAVE_FHS_LAYOUT
  char freqname[128];
  if (fname[0] == '/')
    udm_snprintf(freqname, sizeof(freqname), "%s", fname);
  else
    udm_snprintf(freqname, sizeof(freqname), "freq/%s", fname);
  return rel_name(Env, res, maxlen, "ShareDir", UDM_SHARE_DIR, freqname);
#else
  return rel_name(Env, res, maxlen, "ConfDir", UDM_CONF_DIR, fname);
#endif
}


/* Relative name for /var files */
static size_t rel_var_name(UDM_ENV *Env,char *res,size_t maxlen,
                           const char *name)
{
  size_t    n;
  const char  *dir=UdmVarListFindStr(&Env->Vars,"VarDir",UDM_VAR_DIR);
  if(name[0]=='/')n = udm_snprintf(res, maxlen, "%s", name);
  else    n = udm_snprintf(res,maxlen,"%s%s%s",dir,UDMSLASHSTR,name);
  res[maxlen]='\0';
  return n;
}

size_t UdmGetArgs(char *str, char **av, size_t max)
{
  size_t  ac=0;
  char  *lt;
  char  *tok;
  
  bzero((void*)av, max * sizeof(*av));
  tok=UdmGetStrToken(str,&lt);
  
  while (tok && (ac<max))
  {
    av[ac]=tok;
    ac++;
    tok=UdmGetStrToken(NULL,&lt);
  }
  return ac;
}


static int add_srv(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  UDM_AGENT *Indexer = C->Indexer;
  size_t  i;
  int  has_alias=0;
  
  if(!(C->flags & UDM_FLAG_ADD_SERV))
    return UDM_OK;
  
  C->Srv->command = 'S';
  C->Srv->follow= UDM_FOLLOW_DEFAULT;
  C->Srv->ordre = ++C->ordre;
  C->Srv->Match.nomatch=0;
  C->Srv->Match.case_sense = UDM_CASE_INSENSITIVE;
  C->Srv->Match.flags= (C->flags & UDM_FLAG_DONT_ADD_TO_DB) ?
                       UDM_MATCH_FLAG_SKIP_OPTIMIZATION : 0;
  C->Srv->method= UDM_METHOD_DEFAULT;

  if(!strcasecmp(av[0],"Server"))
  {
    C->Srv->Match.match_type=UDM_MATCH_BEGIN;
  }else if(!strcasecmp(av[0],"Subnet"))
  {
    C->Srv->Match.match_type=UDM_MATCH_SUBNET;
    Conf->Servers.have_subnets=1;
  }
  else
  {
    C->Srv->Match.match_type=UDM_MATCH_WILD;
  }
  
  
  for(i=1; i<ac; i++)
  {
    int  o;
    
    if (UDM_FOLLOW_UNKNOWN!= (o= UdmFollowType(av[i]))) C->Srv->follow= o;
    else
    if (UDM_METHOD_UNKNOWN!= (o= UdmMethod(av[i]))) C->Srv->method= o;
    else
    if(!strcasecmp(av[i],"nocase"))C->Srv->Match.case_sense= UDM_CASE_SENSITIVE;
    else
    if(!strcasecmp(av[i],"case"))C->Srv->Match.case_sense= UDM_CASE_INSENSITIVE;
    else
    if(!strcasecmp(av[i],"match"))C->Srv->Match.nomatch=0;
    else
    if(!strcasecmp(av[i],"nomatch"))C->Srv->Match.nomatch=1;
    else
    if(!strcasecmp(av[i],"string"))C->Srv->Match.match_type=UDM_MATCH_WILD;
    else
    if(!strcasecmp(av[i],"regex"))C->Srv->Match.match_type=UDM_MATCH_REGEX;
    else
    if(!strcasecmp(av[i],"regexp"))C->Srv->Match.match_type=UDM_MATCH_REGEX;
    else
    {
      if(!C->Srv->Match.pattern)
        C->Srv->Match.pattern = (char*)UdmStrdup(av[i]);
      else
      if(!has_alias){
        has_alias=1;
        UdmVarListReplaceStr(&C->Srv->Vars,"Alias",av[i]);
      }else{
        sprintf(Conf->errstr,"too many argiments: '%s'",av[i]);
        return UDM_ERROR;
      }
    }
  }
  if (!C->Srv->Match.pattern)
  {
    sprintf(Conf->errstr,"too few argiments in '%s' command", av[0]);
    return UDM_ERROR;
  }
  if(UDM_OK != UdmServerAdd(Indexer, C->Srv, C->flags))
  {
    char * s_err;
    s_err = (char*)UdmStrdup(Conf->errstr);
    sprintf(Conf->errstr,"%s",s_err);
    UDM_FREE(s_err);
    UDM_FREE(C->Srv->Match.pattern);
    return UDM_ERROR;
  }
  if((C->Srv->Match.match_type==UDM_MATCH_BEGIN)&&
     (C->Srv->Match.pattern[0])&&
     (C->flags&UDM_FLAG_ADD_SERVURL))
  {
    UDM_HREF Href;
    
    UdmHrefInit(&Href);
    Href.url=C->Srv->Match.pattern;
    Href.method=UDM_METHOD_GET;
    Href.site_id = C->Srv->site_id;
    Href.server_id = C->Srv->site_id;
    Href.hops= (uint4) UdmVarListFindInt(&C->Srv->Vars, "StartHops", 0);
    UdmHrefListAdd(&Conf->Hrefs, &Href);
  }
  UDM_FREE(C->Srv->Match.pattern);
  UdmVarListDel(&C->Srv->Vars,"AuthBasic");
  UdmVarListDel(&C->Srv->Vars,"Alias");
  return UDM_OK;
}

static int UdmMatchListAddWithServer(UDM_AGENT *A,
                                     UDM_MATCHLIST *L, UDM_MATCH *M,
                                     char *err, size_t errsize, int ordre)
{
  if (UDM_OK != UdmMatchListAdd(A, L, M, err, errsize, ordre))
    return UDM_ERROR;
  
  if (A != NULL)
  {
    UDM_SERVERLIST S;
    UDM_SERVER n;
    int rc;
    
    bzero((void*)&n, sizeof(n));
    S.Server= &n;
    n.command= 'F';
    n.Match.pattern= M->pattern;
    n.Match.match_type= M->match_type;
    n.Match.case_sense= M->case_sense;
    n.Match.nomatch= M->nomatch;
    n.Match.arg= M->arg;
    n.ordre= ordre;
    
    rc= UdmSrvAction(A, &S, UDM_SRV_ACTION_ADD);
    UdmVarListFree(&n.Vars);
    
    if (rc != UDM_OK) return rc;
  }
  return UDM_OK;
}


static int add_alias(void *Cfg, size_t ac,char **av)
{
  UDM_CFG    *C=(UDM_CFG*)Cfg;
  UDM_ENV    *Conf = C->Indexer->Conf;
  UDM_MATCH  Alias;
  size_t    i;
  
  UdmMatchInit(&Alias);
  Alias.match_type=UDM_MATCH_BEGIN;
  Alias.case_sense= UDM_CASE_INSENSITIVE;
  Alias.flags= (C->flags & UDM_FLAG_DONT_ADD_TO_DB) ?
                UDM_MATCH_FLAG_SKIP_OPTIMIZATION : 0;

  for(i=1; i<ac; i++)
  {
    if(!strcasecmp(av[i],"regex"))
      Alias.match_type=UDM_MATCH_REGEX;
    else if(!strcasecmp(av[i],"regexp"))
      Alias.match_type=UDM_MATCH_REGEX;
    else if(!strcasecmp(av[i],"prefix"))
      Alias.match_type=UDM_MATCH_BEGIN;
    else if(!strcasecmp(av[i],"case"))
      Alias.case_sense= UDM_CASE_INSENSITIVE;
    else if(!strcasecmp(av[i],"nocase"))
      Alias.case_sense= UDM_CASE_SENSITIVE;
    else if(!Alias.pattern)
    {
      Alias.pattern=av[i];
    }
    else
    {
      char    err[120]="";
      UDM_MATCHLIST  *L=NULL;
      
      Alias.arg=av[i];
      
      if(!strcasecmp(av[0],"Alias"))L=&Conf->Aliases;
      if(!strcasecmp(av[0],"ReverseAlias"))L=&Conf->ReverseAliases;
      
      if(UDM_OK != UdmMatchListAdd(NULL, L, &Alias, err, sizeof(err), 0))
      {
        udm_snprintf(Conf->errstr,sizeof(Conf->errstr)-1,"%s",err);
        return UDM_ERROR;
      }
    }
  }
  if(!Alias.arg)
  {
    udm_snprintf(Conf->errstr,sizeof(Conf->errstr)-1,"too few arguments");
    return UDM_ERROR;
  }
  return UDM_OK;
}


static int add_filter(void *Cfg, size_t ac,char **av)
{
  UDM_CFG    *C=(UDM_CFG*)Cfg;
  UDM_ENV    *Conf=C->Indexer->Conf;
  UDM_MATCH  M;
  size_t    i;
  
  if(!(C->flags & UDM_FLAG_ADD_SERV))
    return UDM_OK;
  
  UdmMatchInit(&M);
  M.match_type=UDM_MATCH_WILD;
  M.case_sense= UDM_CASE_INSENSITIVE;
  M.flags= (C->flags & UDM_FLAG_DONT_ADD_TO_DB) ?
            UDM_MATCH_FLAG_SKIP_OPTIMIZATION : 0;

  C->ordre++;
  for(i=1; i<ac ; i++)
  {
    if(!strcasecmp(av[i],"case"))M.case_sense= UDM_CASE_INSENSITIVE;
    else
    if(!strcasecmp(av[i],"nocase"))M.case_sense= UDM_CASE_SENSITIVE;
    else
    if(!strcasecmp(av[i],"regex"))M.match_type=UDM_MATCH_REGEX;
    else
    if(!strcasecmp(av[i],"regexp"))M.match_type=UDM_MATCH_REGEX;
    else
    if(!strcasecmp(av[i],"string"))M.match_type=UDM_MATCH_WILD;
    else
    if(!strcasecmp(av[i],"nomatch"))M.nomatch=1;
    else
    if(!strcasecmp(av[i],"match"))M.nomatch=0;
    else
    {
      char    err[120]="";
      
      M.arg = av[0];
      M.pattern = av[i];
      
      if(UDM_OK != UdmMatchListAddWithServer(NULL, &Conf->Filters, &M, 
                                             err, sizeof(err), ++C->ordre))
      {
        udm_snprintf(Conf->errstr,sizeof(Conf->errstr)-1,"%s",err);
        return UDM_ERROR;
      }
    }
  }
  return UDM_OK;
}

static int add_section_filter(void *Cfg, size_t ac,char **av)
{
  UDM_CFG    *C=(UDM_CFG*)Cfg;
  UDM_ENV    *Conf=C->Indexer->Conf;
  UDM_MATCH  M;
  size_t    i;
  char    ss = 0;
  
  if(!(C->flags & UDM_FLAG_ADD_SERV))
    return UDM_OK;
  
  UdmMatchInit(&M);
  M.match_type=UDM_MATCH_WILD;
  M.case_sense= UDM_CASE_INSENSITIVE;
  M.flags= (C->flags & UDM_FLAG_DONT_ADD_TO_DB) ?
            UDM_MATCH_FLAG_SKIP_OPTIMIZATION : 0;

  C->ordre++;
  for(i=1; i<ac ; i++)
  {
    if(!strcasecmp(av[i],"case"))M.case_sense= UDM_CASE_INSENSITIVE;
    else
    if(!strcasecmp(av[i],"nocase"))M.case_sense= UDM_CASE_SENSITIVE;
    else
    if(!strcasecmp(av[i],"regex"))M.match_type=UDM_MATCH_REGEX;
    else
    if(!strcasecmp(av[i],"regexp"))M.match_type=UDM_MATCH_REGEX;
    else
    if(!strcasecmp(av[i],"string"))M.match_type=UDM_MATCH_WILD;
    else
    if(!strcasecmp(av[i],"nomatch"))M.nomatch=1;
    else
    if(!strcasecmp(av[i],"match"))M.nomatch=0;
    else
    if (! ss) {
/*
      UDM_VAR *Sec = UdmVarListFind(&Conf->Sections, av[i]);
      if (! Sec) {
        udm_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "Section '%s' not found for %s", av[i], av[0]);
        return(UDM_ERROR);
      }
      ss = 1;
      M.section = Sec->name;
*/
      ss = 1;
      M.section = av[i];
    }
    else
    {
      char    err[120]="";

      M.arg = av[0];
      M.pattern = av[i];

      if(UDM_OK != UdmMatchListAdd(C->Indexer, &Conf->SectionFilters, &M,
                                   err, sizeof(err), ++C->ordre))
      {
        udm_snprintf(Conf->errstr,sizeof(Conf->errstr)-1,"%s",err);
        return UDM_ERROR;
      }
    }
  }

  if (!ss)
  {
    udm_snprintf(Conf->errstr, sizeof(Conf->errstr) - 1, "No section given for %s", av[0]);
    return(UDM_ERROR);
  }
  return UDM_OK;
}


static int
add_type_internal(void *Cfg, size_t ac,char **av, UDM_MATCHLIST *Lst)
{
  UDM_CFG    *C=(UDM_CFG*)Cfg;
  UDM_ENV    *Conf=C->Indexer->Conf;
  UDM_MATCH  M;
  size_t    i;
  int    rc=UDM_OK;
  char err[128];
  
  UdmMatchInit(&M);
  M.match_type=UDM_MATCH_WILD;
  M.case_sense= UDM_CASE_INSENSITIVE;
  M.flags= (C->flags & UDM_FLAG_DONT_ADD_TO_DB) ?
            UDM_MATCH_FLAG_SKIP_OPTIMIZATION : 0;

  for (i=1; i<ac; i++)
  {
    if(!strcasecmp(av[i],"regex"))M.match_type=UDM_MATCH_REGEX;
    else
    if(!strcasecmp(av[i],"regexp"))M.match_type=UDM_MATCH_REGEX;
    else
    if(!strcasecmp(av[i],"string"))M.match_type=UDM_MATCH_WILD;
    else
    if(!strcasecmp(av[i],"match"))M.nomatch= 0;
    else
    if(!strcasecmp(av[i],"nomatch"))M.nomatch= 1;
    else
    if(!strcasecmp(av[i],"case"))M.case_sense= UDM_CASE_INSENSITIVE;
    else
    if(!strcasecmp(av[i],"nocase"))M.case_sense= UDM_CASE_SENSITIVE;
    else
    if(!M.arg)
      M.arg=av[i];
    else
    {
      M.pattern=av[i];
      if(UDM_OK != (rc = UdmMatchListAdd(NULL, Lst ,&M,
                                         err,sizeof(err), 0)))
      {
        udm_snprintf(Conf->errstr,sizeof(Conf->errstr)-1,"%s",err);
        return rc;
      }
    }
  }
  return rc;
}


static int
add_type(void *Cfg, size_t ac,char **av)
{
  UDM_CFG    *C=(UDM_CFG*)Cfg;
  UDM_ENV    *Conf=C->Indexer->Conf;
  return add_type_internal(Cfg, ac, av, &Conf->MimeTypes);
}


static int
add_encoding(void *Cfg, size_t ac,char **av)
{
  UDM_CFG    *C=(UDM_CFG*)Cfg;
  UDM_ENV    *Conf=C->Indexer->Conf;
  return add_type_internal(Cfg, ac, av, &Conf->Encodings);
}


static int add_parser(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  UDM_PARSER P;
  char empty[]="";
  P.from_mime=av[1];
  P.to_mime=av[2];
  P.cmd = av[3] ? av[3] : empty;
  P.src= av[4];
  UdmParserAdd(&Conf->Parsers,&P);
  return UDM_OK;
}


static int add_separator(UDM_VARLIST *Vars, const char *name, const char *val)
{
  UDM_DSTR buf;
  UdmDSTRInit(&buf, 128);
  UdmDSTRReset(&buf);
  UdmDSTRAppendf(&buf, "separator.%s", name);
  UdmVarListAddStr(Vars, buf.data, val);
  UdmDSTRFree(&buf);
  return UDM_OK;
}


static int add_section(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  UDM_VAR  S;
  int      cdon, noindex= 0;
  UDM_MATCHLIST *SectionMatch= &Conf->SectionMatch;

  bzero((void*)&S, sizeof(S));
  S.name= av[1];
  /*
    Do not use "url.*" sections in
    clone detection by default
  */
  cdon= strncasecmp(av[1], "url", 3) ? 1 : 0;
  if (!(S.section= atoi(av[2])) && av[2][0] != '0')
  {
    sprintf(Conf->errstr,"Section ID is not a number: %s",av[2]);
    return UDM_ERROR;
  }
  if (!(S.maxlen= atoi(av[3])) && av[3][0] != '0')
  {
    sprintf(Conf->errstr,"Section length is not a number: %s",av[3]);
    return UDM_ERROR;
  }
  
  av+= 4;
  ac-= 4;
  
  for ( ; ac ; ac--, av++)
  {
    if (!strcasecmp(av[0], "cdon") || !strcasecmp(av[0], "DetectClones"))
      cdon= 1;
    else if (!strcasecmp(av[0], "cdoff") || !strcasecmp(av[0], "NoDetectClones"))
      cdon= 0;
    else if (!strcasecmp(av[0], "html"))
      S.flags|= UDM_VARFLAG_HTMLSOURCE;
    else if (!strcasecmp(av[0], "decimal"))
      S.flags|= UDM_VARFLAG_DECIMAL;
    else if (!strcasecmp(av[0], "wiki"))
      S.flags|= UDM_VARFLAG_HTMLSOURCE | UDM_VARFLAG_WIKI;
    else if (!strcasecmp(av[0], "noindex"))
      noindex= 1;
    else if (!strcasecmp(av[0], "index"))
      noindex= 0;
    else if (!strcasecmp(av[0], "text"))
      /* do nothing */;
    else if (!strcasecmp(av[0], "afterheaders"))
      SectionMatch= &Conf->SectionHdrMatch;
    else if (!strcasecmp(av[0], "afterguesser"))
      SectionMatch= &Conf->SectionGsrMatch;
    else if (!strcasecmp(av[0], "afterparser"))
      SectionMatch= &Conf->SectionMatch;
    else
      break;
  }
  S.flags|= cdon ? 0 : UDM_VARFLAG_NOCLONE;
  S.flags|= noindex ? UDM_VARFLAG_NOINDEX : 0;

  if (ac == 0)
  {
    /* no optional arguments */
  }
  else if (ac == 1)
  {
    /* <sep> */
    add_separator(&Conf->Vars, S.name, av[0]);
  }
  else if (ac >= 2 && ac <= 4)
  {
    /* 
       <expr> <repl>
       <sep> <expr> <repl>
       <sep> <src> <expr> <repl>
    */

    UDM_MATCH M;
    char err[120]= "";

    UdmMatchInit(&M);
    M.match_type= UDM_MATCH_REGEX;
    M.case_sense= UDM_CASE_INSENSITIVE;
    M.section= S.name;
    M.flags= (C->flags & UDM_FLAG_DONT_ADD_TO_DB) ?
              UDM_MATCH_FLAG_SKIP_OPTIMIZATION : 0;

    switch (ac)
    {
      case 2:
        M.pattern= av[0];
        M.arg= av[1];
        break;

      case 3:
        add_separator(&Conf->Vars, S.name, av[0]);
        M.pattern= av[1];
        M.arg= av[2];
        break;

      case 4:
        add_separator(&Conf->Vars, S.name, av[0]);
        M.arg1= av[1];
        M.pattern= av[2];
        M.arg= av[3];
        break;
    }

    if(UDM_OK != UdmMatchListAdd(C->Indexer, SectionMatch, &M,
                                 err, sizeof(err), ++C->ordre))
    {
      udm_snprintf(Conf->errstr,sizeof(Conf->errstr)-1,"%s",err);
      return UDM_ERROR;
    }
    S.flags+= UDM_VARFLAG_USERDEF;
  }
  else
  {
    sprintf(Conf->errstr,"too many argiments: '%s'", av[0]);
    return UDM_ERROR;
  }

  UdmVarListReplace(&Conf->Sections,&S);
  return UDM_OK;
}


static int do_include(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  FILE *test;
  char save[1024];
  if(C->level<5)
  {
    int  rc;
    char  fname[1024];
    rel_cur_name(fname, sizeof(fname)-1, av[1]);
    if ((test= fopen(fname, "r")))
      fclose(test);
    else
      rel_etc_name(C->Indexer->Conf, fname, sizeof(fname)-1, av[1]);
    strcpy(save, current);
    C->level++;
    rc=EnvLoad(C,fname);
    strcpy(current, save);
    C->level--;
    return rc;
  }
  else
  {
    sprintf(C->Indexer->Conf->errstr,"too big (%d) level in included files",C->level);
    return UDM_ERROR;
  }
  return UDM_OK;
}

static int add_affix(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  
  if(C->flags&UDM_FLAG_SPELL)
  {
    char  fname[1024];
    rel_etc_name(Conf, fname, sizeof(fname) - 1, av[3]);
    if(UdmAffixListListAdd(&Conf->Affixes,av[1],av[2],fname))
    {
      sprintf(Conf->errstr,"Can't add affix :%s",fname);
      return UDM_ERROR;
    }
  }
  return UDM_OK;
}

static int add_spell(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  
  if(C->flags&UDM_FLAG_SPELL)
  {
    char  fname[1024];
    rel_etc_name(Conf, fname, sizeof(fname) - 1, av[3]);
    if(UdmSpellListListAdd(&Conf->Spells,av[1],av[2],fname))
    {
     sprintf(Conf->errstr,"Can't load dictionary :%s",fname);
      return UDM_ERROR;
    }
  }
  return UDM_OK;
}

static int add_stoplist(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  int  res;
  char  fname[1024];
  rel_stopwords_name(Conf, fname, sizeof(fname) - 1, av[1]);
  res=UdmStopListLoad(Conf,fname);
  return res;
}

static int add_langmap(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  int  res=UDM_OK;
  if(C->flags&UDM_FLAG_LOAD_LANGMAP){
    char  fname[1024];
    rel_langmap_name(Conf, fname, sizeof(fname) - 1, av[1]);
    res = UdmLoadLangMapFile(&Conf->LangMaps, fname);
  }
  return res;
}

static int add_synonym(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  int  res=UDM_OK;
  if(C->flags&UDM_FLAG_SPELL)
  {
    char  fname[1024];
    rel_synonym_name(Conf, fname, sizeof(fname) - 1, av[1]);
    res=UdmSynonymListLoad(Conf,fname);
  }
  return res;
}

static int add_chinese(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  
  /*
    This line was wrong: ChinesList was not really loaded
    from search.cgi
  */
  /* if(C->flags & UDM_FLAG_ADD_SERV)*/
  
  {
    char fname[1024];
    rel_freq_name(Conf, fname, sizeof(fname)-1, av[2] ? av[2] : "mandarin.freq");
    return UdmChineseListLoad(C->Indexer, &Conf->Chi, 
                              av[1] ? av[1] : "GB2312", fname);
  }
  return UDM_OK;
}

static int add_thai(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  if(C->flags & UDM_FLAG_ADD_SERV)
  {
    char fname[1024];
    rel_freq_name(Conf, fname, sizeof(fname)-1, av[2] ? av[2] : "thai.freq");
    return UdmChineseListLoad(C->Indexer, &Conf->Thai, 
                              av[1] ? av[1] : "tis-620", fname);
  }
  return UDM_OK;
}
        

static int add_url(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  
  if(C->flags&UDM_FLAG_ADD_SERV)
  {
    char    *al = NULL;
    UDM_SERVER  *Srv;
    UDM_HREF  Href;
    if((Srv = UdmServerFind(Conf, &Conf->Servers, av[1], &al)))
    {
      UdmHrefInit(&Href);
      Href.url=av[1];
      Href.method=UDM_METHOD_GET;
      UdmHrefListAdd(&Conf->Hrefs, &Href);
    }
    UDM_FREE(al);
  }
  return UDM_OK;
}


static int add_srv_table(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  int    res = UDM_OK;
  UDM_DBLIST      dbl;
  UDM_DB    *db;
  
  /*
    Skip ServerTable when loading for search, not for indexing.
    Useful when the ServerTable options are written in a shared
    include.conf file together with DBAddr options, and this file
    is included from both indexer.conf and search.htm
  */
  if(!(C->flags & UDM_FLAG_ADD_SERV))
    return UDM_OK;

  UdmDBListInit(&dbl);
  if (UDM_OK != (res= UdmEnvDBListAdd(Conf, av[1], UDM_OPEN_MODE_READ)))
    goto ex;
  db = &dbl.db[0];

#ifdef HAVE_SQL
  res = UdmSrvActionSQL(C->Indexer, &Conf->Servers, UDM_SRV_ACTION_TABLE, db);
#endif
  if(res != UDM_OK)
    udm_snprintf(Conf->errstr, sizeof(Conf->errstr), "%s", db->errstr);
ex:
  UdmDBListFree(&dbl);
  return res;
}


static int add_limit(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  char * sc;
  char * nm;

  if (ac == 2)
  {
    if((sc = strchr(av[1],':')))
    {
      *sc++='\0';
      nm=(char*)UdmMalloc(strlen(av[1])+8);
      sprintf(nm,"Limit-%s",av[1]);
      UdmVarListReplaceStr(&Conf->Vars, nm, sc);
      UDM_FREE(nm);
    }
  }
  else if (ac == 3)
  {
    char name[128];
    udm_snprintf(name, sizeof(name), "Limit.%s", av[1]);
    UdmVarListReplaceStr(&Conf->Vars, name, av[2]);
  }
  return UDM_OK;
}


static int add_user_score(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  char name[128];
  UDM_ASSERT(ac == 3);
  udm_snprintf(name, sizeof(name), "Score.%s", av[1]);
  UdmVarListReplaceStr(&Conf->Vars, name, av[2]);
  return UDM_OK;
}


static int
add_user_site_score(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  char name[128];
  udm_snprintf(name, sizeof(name), "SiteScore.%s", av[1]);
  UdmVarListReplaceStr(&Conf->Vars, name, av[2]);
  return UDM_OK;
}


static int add_user_order(void *Cfg, size_t ac,char **av)
{
  UDM_CFG *C=(UDM_CFG*) Cfg;
  UDM_ENV *Conf= C->Indexer->Conf;
  char name[128];
  udm_snprintf(name, sizeof(name), "Order.%s", av[1]);
  UdmVarListReplaceStr(&Conf->Vars, name, av[2]);
  return UDM_OK;
}


static int flush_srv_table(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  int  res=UDM_OK;
  if(C->flags&UDM_FLAG_ADD_SERV)
  {
    UDM_AGENT A;
    A.Conf = Conf;
    res=UdmSrvAction(&A, &Conf->Servers, UDM_SRV_ACTION_FLUSH);
  }
  return res;
}

static int dblist_free(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  UdmDBListFree(&Conf->dbl);
  return UDM_OK;
}


static int
env_rpl_casefolding(void *Cfg, size_t ac, char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf = C->Indexer->Conf;
  UDM_UNIDATA  *unidata;
  if(!(unidata= UdmUnidataGetByName(av[1])))
  {
    sprintf(Conf->errstr,"CaseFolding '%s' is not supported", av[1]);
    return UDM_ERROR;
  }
  Conf->unidata= unidata;
  return UDM_OK;
}


static int env_rpl_charset(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf = C->Indexer->Conf;
  UDM_CHARSET  *cs;
  if(!(cs=UdmGetCharSet(av[1])))
  {
    sprintf(Conf->errstr,"charset '%s' is not supported",av[1]);
    return UDM_ERROR;
  }
  if(!strcasecmp(av[0],"LocalCharset"))
  {
    Conf->lcs=cs;
    UdmVarListReplaceStr(&Conf->Vars,av[0],av[1]);
  }
  else if(!strcasecmp(av[0],"BrowserCharset")){
    Conf->bcs=cs;
    UdmVarListReplaceStr(&Conf->Vars,av[0],av[1]);
  }
  return UDM_OK;
}


static int srv_rpl_charset(void *Cfg, size_t ac,char **av){
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf = C->Indexer->Conf;
  UDM_CHARSET  *cs;
  if(!(cs=UdmGetCharSet(av[1])))
  {
    sprintf(Conf->errstr,"charset '%s' is not supported",av[1]);
    return UDM_ERROR;
  }
  UdmVarListReplaceStr(&C->Srv->Vars,av[0],av[1]);
  return UDM_OK;
}

static int srv_rpl_mirror(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  if(!strcasecmp(av[0],"MirrorRoot") || !strcasecmp(av[0],"MirrorHeadersRoot"))
  {
    char  fname[1024];
    rel_var_name(C->Indexer->Conf, fname, sizeof(fname)-1, av[1]);
    UdmVarListReplaceStr(&C->Srv->Vars,av[0],fname);
  }else if(!strcasecmp(av[0],"MirrorPeriod"))
  {
    int  tm=Udm_dp2time_t(av[1]);
    UdmVarListReplaceInt(&C->Srv->Vars,"MirrorPeriod",tm);
  }
  return UDM_OK;
}
    

static int srv_rpl_auth(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  char  name[128];
  udm_snprintf(name,sizeof(name)-1,"%s",av[0]);
  name[sizeof(name)-1]='\0';
  if(av[1])
  {
    size_t  len=strlen(av[1]);
    char  *auth=(char*)UdmMalloc(BASE64_LEN(strlen(av[1])));
    udm_base64_encode(av[1],auth,len);
    UdmVarListReplaceStr(&C->Srv->Vars,name,auth);
    UDM_FREE(auth);
  }
  else
  {
    UdmVarListReplaceStr(&C->Srv->Vars,name,"");
  }
  return UDM_OK;
}

char *UdmParseEnvVar (UDM_ENV *Conf, const char *str)
{
  const char *p1= str, *p2= str;
  UDM_DSTR rc;
  UdmDSTRInit(&rc, 256);
  while ((p1= strstr(p1, "$(")))
  {
    char *p3;
    UdmDSTRAppend(&rc, p2, p1 - p2);
    if ((p3= strchr(p1 + 2, ')')))
    {
      const char *s;
      *p3= 0;
      if ((s= UdmVarListFindStr(&Conf->Vars, p1 + 2, NULL)))
        UdmDSTRAppendSTR(&rc, s);
      *p3= ')';
      p1= p2= p3 + 1;
    }
    else
    {
      UdmDSTRFree(&rc);
      return(NULL);
    }
  }
  UdmDSTRAppendSTR(&rc, p2);
  return(rc.data);
}

static int env_rpl_env_var (void *Cfg, size_t ac, char **av)
{
  UDM_ENV *Conf = ((UDM_CFG *)Cfg)->Indexer->Conf;
  char *p = getenv(av[1]);
  if (!p)
  {
    sprintf(Conf->errstr, "ImportEnv '%s': no such variable.", av[1]);
    return UDM_ERROR;
  }
  UdmVarListReplaceStr(&Conf->Vars, av[1], p);
  return UDM_OK;
}


static int env_rpl_var(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  if(!strcasecmp(av[0],"DBAddr"))
  {
    int rc;
    if(UDM_OK != (rc= UdmEnvDBListAdd(Conf, av[1] ? av[1] : "",
                                      UDM_OPEN_MODE_WRITE)))
      return rc;
  }
  if (!strcasecmp(av[0], "Segmenter"))
  {
    int seg= 0;
#ifdef CHASEN
    if (!strcasecmp(av[1], "Chasen"))
      seg= 1;
#endif
#ifdef MECAB
    if (!strcasecmp(av[1], "Mecab"))
      seg= 1;
#endif
    if (!strcasecmp(av[1], "Freq"))
      seg= 1;
    if (!strcasecmp(av[1], "CJK"))
      seg= 1;
    if (!seg)
    {
      sprintf(Conf->errstr, "Unsupported segmenter method: '%s'", av[1]);
      return UDM_ERROR;
    }
  }
  UdmVarListReplaceStr(&Conf->Vars,av[0],av[1]);
  return UDM_OK;
}


static int env_rpl_named_var(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  UdmVarListReplaceStr(&Conf->Vars,av[1],av[2]);
  return UDM_OK;
}


static int rpl_xml_hook(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  UDM_VARLIST *Vars= !strcasecmp(av[0], "XMLEnterHook") ?
                     &Conf->XMLEnterHooks : 
                     !strcasecmp(av[0], "XMLDataHook") ? 
                     &Conf->XMLDataHooks : &Conf->XMLLeaveHooks;
  UdmVarListReplaceStr(Vars,av[1],av[2]);
  return UDM_OK;
}

static int srv_rpl_var(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UdmVarListReplaceStr(&C->Srv->Vars,av[0],av[1]);
  return UDM_OK;
}

static int srv_rpl_hdr(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  char  *nam=NULL;
  char  *val=NULL;
  char  name[128];
  
  switch(ac){
    case 3:  nam=av[1];val=av[2];break;
    case 2:
      if((val=strchr(av[1],':'))){
        *val++='\0';
        val=UdmTrim(val," \t");
        nam=av[1];
      }
      break;
  }
  if(nam)
  {
    udm_snprintf(name,sizeof(name),"Request.%s",nam);
    name[sizeof(name)-1]='\0';
    UdmVarListReplaceStr(&C->Srv->Vars,name,val);
  }
  return UDM_OK;
}

static int env_rpl_bool_var(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  int res= !strcasecmp(av[1],"yes") || atoi(av[1]) == 1;
  if(!strcasecmp(av[0], "CVSIgnore")) Conf->CVS_ignore = res;
  UdmVarListReplaceInt(&Conf->Vars,av[0],res);
  return UDM_OK;
}

static int srv_rpl_bool_var(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  int res= !strcasecmp(av[1],"yes") || atoi(av[1]) == 1;
  UdmVarListReplaceInt(&C->Srv->Vars,av[0],res);
  return UDM_OK;
}

static int env_rpl_num_var(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  int  res=atoi(av[1]);
  if(!strcasecmp(av[0], "DocSizeWeight"))
  {
    UdmVarListReplaceInt(&Conf->Vars, "MaxCoordFactor" ,res);
    return UDM_OK;
  }
  if(!strcasecmp(av[0],"MinWordLength"))Conf->WordParam.min_word_len=res;
  if(!strcasecmp(av[0],"MaxWordLength"))Conf->WordParam.max_word_len=res;
  UdmVarListReplaceInt(&Conf->Vars,av[0],res);
  return UDM_OK;
}    

static int srv_rpl_num_var(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  int  res=atoi(av[1]);
  UdmVarListReplaceInt(&C->Srv->Vars,av[0],res);
  if (strcasecmp(av[0], "MaxHops") == 0) C->Srv->MaxHops = (uint4) res;
  if (strcasecmp(av[0], "ServerWeight") == 0) C->Srv->weight = atof(av[1]);
  return UDM_OK;
}

static int env_rpl_rand(void *Cfg, size_t ac, char **av)
{
  UDM_CFG *C=(UDM_CFG*)Cfg;
  float r;
  int ir;

  r = (float)UDM_ATOF(av[1]);
  srand((unsigned)time(0));
  r = r * rand() / RAND_MAX; ir = (int)r;
  UdmVarListReplaceInt(&C->Indexer->Conf->Vars,av[0],ir);
  return UDM_OK;
}

static int srv_rpl_time_var(void *Cfg, size_t ac,char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  int  res=Udm_dp2time_t(av[1]);
  if(res==-1)
  {
    sprintf(Conf->errstr,"bad time interval: %s",av[1]);
    res=UDM_ERROR;
  }
  UdmVarListReplaceInt(&C->Srv->Vars,av[0],res);
  return UDM_OK;
}

static int srv_rpl_category(void *Cfg, size_t ac, char **av)
{
  UDM_CFG  *C=(UDM_CFG*)Cfg;
  UDM_ENV  *Conf=C->Indexer->Conf;
  unsigned int cid = UdmGetCategoryId(Conf, av[1]);
  char buf[64];
  udm_snprintf(buf, 64, "%u", cid);
  UdmVarListReplaceStr(&C->Srv->Vars, av[0], buf);
  return UDM_OK;
}

typedef struct conf_cmd_st
{
  const char  *name;
  size_t    argmin;
  size_t    argmax;
  int    (*action)(void *a,size_t n,char **av);
} UDM_CONFCMD;


static UDM_CONFCMD commands[] = 
{
  {"Include",              1,1,   do_include},      /* Documented */

  {"ImportEnv",            1,1,   env_rpl_env_var}, /* Documented */
  {"DBAddr",               1,1,   env_rpl_var},     /* Documented */
  {"DefaultContentType",   1,1,   env_rpl_var},     /* Documented */
  {"ResultContentType",    1,1,   env_rpl_var},     /* Documented */
  {"Listen",               1,1,   env_rpl_var},     /* TODO       */
  {"UseRemoteContentType", 1,1,   env_rpl_bool_var},/* Documented */
  {"UseCRC32URLId",        1,1,   env_rpl_var},     /* Documented */
  {"UseCRC32SiteId",       1,1,   env_rpl_var},     /* TODO       */
  {"NewsExtensions",       1,1,   env_rpl_var},     /* Documented */
  {"CrossWords",           1,1,   env_rpl_bool_var},/* Documented */
  {"SyslogFacility",       1,1,   env_rpl_var},     /* Documented */
  {"AliasProg",            1,1,   env_rpl_var},     /* Documented */
  {"ForceIISCharset1251",  1,1,   env_rpl_var},     /* Documented */
  {"GroupBySite",          1,1,   env_rpl_bool_var},/* Documented */
  {"Cache",                1,1,   env_rpl_var},     /* Documented */
  {"wf",                   1,1,   env_rpl_var},     /* Documented */
  {"StrictModeThreshold",  1,1,   env_rpl_var},     /* Documented */
  {"PopRankSkipSameSite",  1,1,   env_rpl_var},     /* Documented */
  {"PopRankFeedBack",      1,1,   env_rpl_var},     /* Documented */
  {"PopRankUseTracking",   1,1,   env_rpl_var},     /* Documented */
  {"PopRankUseShowCnt",    1,1,   env_rpl_var},     /* Documented */
  {"PopRankShowCntRatio",  1,1,   env_rpl_var},     /* Documented */
  {"PopRankShowCntWeight", 1,1,   env_rpl_var},     /* Documented */
  {"VarDir",               1,1,   env_rpl_var},     /* Documented */
  {"DocMemCacheSize",      1,1,   env_rpl_var},     /* Documented */
  {"IspellUsePrefixes",    1,1,   env_rpl_var},     /* Documented */
  {"URLSelectCacheSize",   1,1,   env_rpl_var},     /* Documented */
  {"URLSelectSkipLock",    1,1,   env_rpl_bool_var},/* Documented */
  {"MaxDocSize",           1,1,   env_rpl_var},     /* Documented */
  {"ParserTimeOut",        1,1,   env_rpl_var},     /* Documented */
  {"NumSections",          1,1,   env_rpl_var},     /* Documented */
  {"DateFormat",           1,1,   env_rpl_var},     /* Documented */
  {"GuesserUseMeta",       1,1,   env_rpl_var},     /* Documented */
  {"ResultsLimit",         1,1,   env_rpl_var},     /* Documented */
  {"Segmenter",            1,1,   env_rpl_var},     /* TODO       */
  {"HlBeg",                1,1,   env_rpl_var},     /* Documented */
  {"HlEnd",                1,1,   env_rpl_var},     /* Documented */
  {"Log2stderr",           1,1,   env_rpl_var},     /* Documented */
  {"LogFlags",             1,1,   env_rpl_num_var}, /* TODO     */
  {"PagesPerScreen",       1,1,   env_rpl_var},     /* Documented */
  {"SQLClearDBHook",       1,1,   env_rpl_var},     /* TODO       */
  {"UserCacheQuery",       1,1,   env_rpl_var},     /* Documented */
#ifdef HAVE_SETVBUF
  {"StdoutBufferSize",     1,1,   env_rpl_var},     /* TODO       */
#endif
  {"AlwaysFoundWord",      1,1,   env_rpl_var},     /* Documented */
  {"CustomLog",            1,1,   env_rpl_var},     /* Documented */
  {"CurrentTime",          1,1,   env_rpl_var},     /* TODO       */
  {"Locale",               1,1,   env_rpl_var},     /* Documented */
  {"WordDistanceWeight",   1,1,   env_rpl_num_var}, /* Documented */
  {"MaxCoordFactor",       1,1,   env_rpl_num_var}, /* TODO       */
  {"IDFFactor",            1,1,   env_rpl_num_var}, /* Documented */
  {"MinCoordFactor",       1,1,   env_rpl_num_var}, /* Documented */
  {"NumWordFactor",        1,1,   env_rpl_var},     /* Documented */
  {"NumDistinctWordFactor",1,1,   env_rpl_num_var}, /* Documented */
  {"UserScoreFactor",      1,1,   env_rpl_num_var}, /* Documented */
  {"WordDensityFactor"    ,1,1,   env_rpl_num_var}, /* Documented */
  {"WordFormFactor"       ,1,1,   env_rpl_num_var}, /* Documented */
  {"URLDataThreshold",     1,1,   env_rpl_num_var}, /* Documented */
  {"DocSizeWeight",        1,1,   env_rpl_num_var}, /* Documented */
  {"RelevancyFactor",      1,1,   env_rpl_num_var}, /* TODO       */
  {"DateFactor",           1,1,   env_rpl_num_var}, /* Documented */
  {"MinWordLength",        1,1,   env_rpl_num_var}, /* Documented */
  {"MaxWordLength",        1,1,   env_rpl_num_var}, /* Documented */
  {"SubstringMatchMinWordLength", 1,1, env_rpl_num_var}, /* Documented */
  {"ExcerptSize",          1,1,   env_rpl_num_var}, /* Documented */
  {"ExcerptPadding",       1,1,   env_rpl_num_var}, /* Documented */
  {"LogLevel",             1,1,   env_rpl_num_var}, /* Documented */
  {"CrawlerThreads",       1,1,   env_rpl_num_var}, /* Documented */
  {"WordCacheSize",        1,1,   env_rpl_num_var}, /* Documented */
  {"CVSIgnore",            1,1,   env_rpl_bool_var},/* Documented */
  {"UseHTDBURLId",         1,1,   env_rpl_bool_var},/* TODO       */
  {"Suggest",              1,1,   env_rpl_bool_var},/* Documented */
  {"IndexTime",            1,1,   env_rpl_bool_var},/* Documented */
  {"ExcerptStopword",      1,1,   env_rpl_bool_var},/* Documented */
  {"UseCookie",            1,1,   env_rpl_bool_var},/* Documented */
  {"UseSitemap",           1,1,   env_rpl_bool_var},/* Documented */
  {"UseNumericOperators",  1,1,   env_rpl_bool_var},/* Documented */
  {"UseRangeOperators",    1,1,   env_rpl_bool_var},/* TODO */
  {"SaveSectionSize",      1,1,   env_rpl_bool_var},/* Documented */
  {"Dehyphenate",          1,1,   env_rpl_bool_var},/* Documented */
  {"HyphenateNumbers",     1,1,   env_rpl_bool_var},/* TODO       */
  {"StripAccents",         1,1,   env_rpl_bool_var},/* Documented */
  {"LoadURLInfo",          1,1,   env_rpl_bool_var},/* Documented */
  {"LoadURLBasicInfo",     1,1,   env_rpl_bool_var},/* Documented */
  {"LoadTagInfo",          1,1,   env_rpl_bool_var},/* Documented */
  {"ComplexSynonyms",      1,1,   env_rpl_bool_var},/* Documented */

  {"ReplaceVar",           2,2,   env_rpl_named_var},/* Documented */

  {"LocalCharset",         1,1,   env_rpl_charset},  /* Documented */
  {"BrowserCharset",       1,1,   env_rpl_charset},  /* Documented */
  {"CaseFolding",          1,1,   env_rpl_casefolding},/* Documented */

  {"XMLEnterHook",         2,2,   rpl_xml_hook},     /* TODO       */
  {"XMLLeaveHook",         2,2,   rpl_xml_hook},     /* TODO       */
  {"XMLDataHook",          2,2,   rpl_xml_hook},     /* TODO       */
  
  {"R0",                   1,1,   env_rpl_rand},     /* Documented */
  {"R1",                   1,1,   env_rpl_rand},     /* Documented */
  {"R2",                   1,1,   env_rpl_rand},     /* Documented */
  {"R3",                   1,1,   env_rpl_rand},     /* Documented */
  {"R4",                   1,1,   env_rpl_rand},     /* Documented */
  {"R5",                   1,1,   env_rpl_rand},     /* Documented */
  {"R6",                   1,1,   env_rpl_rand},     /* Documented */
  {"R7",                   1,1,   env_rpl_rand},     /* Documented */
  {"R8",                   1,1,   env_rpl_rand},     /* Documented */
  {"R9",                   1,1,   env_rpl_rand},     /* Documented */
  
  {"HTDBAddr",             1,1,   srv_rpl_var},      /* Documented */
  {"HTDBList",             1,1,   srv_rpl_var},      /* Documented */
  {"HTDBDoc",              1,1,   srv_rpl_var},      /* Documented */
  {"HTDBLimit",            1,1,   srv_rpl_var},      /* Documented */
  {"SQLImportSection",     1,1,   srv_rpl_var},      /* TODO       */
  {"SQLExportHref",        1,1,   srv_rpl_var},      /* TODO       */
  {"SQLWordForms",         1,1,   env_rpl_var},      /* Documented */
  {"DefaultLang",          1,1,   srv_rpl_var},      /* Documented */
  {"Category",             1,1,   srv_rpl_category}, /* Documented */
  {"Tag",                  1,1,   srv_rpl_var},      /* Documented */
  {"Proxy",                0,1,   srv_rpl_var},      /* Documented */
  {"VaryLang",             1,1,   srv_rpl_var},      /* Documented */
  {"UseRobotsTxtURL",      1,1,   srv_rpl_var},      /* TODO       */
  {"MaxNetErrors",         1,1,   srv_rpl_num_var},  /* Documented */
  {"CrawlDelay",           1,1,   srv_rpl_num_var},  /* Documented */
  {"MaxHops",              1,1,   srv_rpl_num_var},  /* Documented */
  {"StartHops",            1,1,   srv_rpl_num_var},  /* Documented */
  {"MaxDocPerSite",        1,1,   srv_rpl_num_var},  /* Documented */
  {"ServerWeight",         1,1,   srv_rpl_num_var},  /* Documented */
  {"Robots",               1,1,   srv_rpl_bool_var}, /* Documented */
  {"DetectClones",         1,1,   srv_rpl_bool_var}, /* Documented */
  {"CollectLinks",         1,1,   srv_rpl_bool_var}, /* Documented */
  {"Index",                1,1,   srv_rpl_bool_var}, /* Documented */
  {"FollowSymLinks",       1,1,   srv_rpl_bool_var}, /* Documented */
  {"NetErrorDelayTime",    1,1,   srv_rpl_time_var}, /* Documented */
  {"ReadTimeOut",          1,1,   srv_rpl_time_var}, /* Documented */
  {"DocTimeOut",           1,1,   srv_rpl_time_var}, /* Documented */
  {"Period",               1,1,   srv_rpl_time_var}, /* Documented */
  {"HoldBadHrefs",         1,1,   srv_rpl_time_var}, /* Documented */
  {"HTTPHeader",           1,2,   srv_rpl_hdr},      /* Documented */
  {"ProxyAuthBasic",       1,1,   srv_rpl_auth},     /* Documented */
  {"AuthBasic",            1,1,   srv_rpl_auth},     /* Documented */
  {"MirrorRoot",           1,1,   srv_rpl_mirror},   /* Documented */
  {"MirrorHeadersRoot",    1,1,   srv_rpl_mirror},   /* Documented */
  {"MirrorPeriod",         1,1,   srv_rpl_mirror},   /* Documented */
  {"RemoteCharset",        1,1,   srv_rpl_charset},  /* Documented */
  {"RemoteFileNameCharset",1,1,   srv_rpl_charset},  /* Documented */
  
  {"Disallow",             1,100, add_filter},       /* Documented */
  {"Allow",                1,100, add_filter},       /* Documented */
  {"CheckMP3Only",         1,100, add_filter},       /* Documented */
  {"CheckMP3",             1,100, add_filter},       /* Documented */
  {"CheckOnly",            1,100, add_filter},       /* Documented */
  {"HrefOnly",             1,100, add_filter},       /* Documented */
  {"ImportOnly",           1,100, add_filter},       /* TODO       */
  {"Skip",                 1,100, add_filter},       /* Documented */

  {"IndexIf",              1,100, add_section_filter},/* Documented */
  {"NoIndexIf",            1,100, add_section_filter},/* Documented */
  {"SkipIf",               1,100, add_section_filter},/* Documented */
  
  {"Server",               1,100, add_srv},          /* Documented */
  {"Realm",                1,100, add_srv},          /* Documented */
  {"Subnet",               1,100, add_srv},          /* Documented */
  {"URL",                  1,1,   add_url},          /* Documented */
  
  {"Alias",                1,100, add_alias},        /* Documented */
  {"ReverseAlias",         1,100, add_alias},        /* Documented */
  
  {"AddType",              1,100, add_type},         /* Documented */
  {"AddEncoding",          1,100, add_encoding},     /* Documented */
  {"Mime",                 2,4,   add_parser},       /* Documented */
  {"Section",              3,9,   add_section},      /* Documented */ /* TODO: index/noindex */
  {"Affix",                3,3,   add_affix},        /* Documented */
  {"Spell",                3,3,   add_spell},        /* Documented */
  {"StopwordFile",         1,1,   add_stoplist},     /* Documented */
  {"LangMapFile",          1,1,   add_langmap},      /* Documented */
  {"LangMapUpdate",        1,1,   env_rpl_var},      /* Documented */
  {"Synonym",              1,1,   add_synonym},      /* Documented */
  {"LoadChineseList",      0,2,   add_chinese},      /* Documented */
  {"LoadThaiList",         0,2,   add_thai},         /* Documented */
  {"Limit",                1,2,   add_limit},        /* Documented */
  {"UserScore",            2,2,   add_user_score},   /* Documented */
  {"UserSiteScore",        2,2,   add_user_site_score},/* Documented */
  {"UserOrder",            2,2,   add_user_order},   /* Documented */
  {"ServerTable",          1,1,   add_srv_table},    /* Documented */
  {"FlushServerTable",     0,0,   flush_srv_table},  /* Documented */
  {"DBListFree",           0,0,   dblist_free},      /* TODO       */
  
  {NULL,0,0,0}
};

__C_LINK int __UDMCALL UdmEnvAddLine(UDM_CFG *C,char *str)
{
  UDM_ENV    *Conf=C->Indexer->Conf;
  UDM_CONFCMD  *Cmd;
  char    *av[255];
  size_t    ac;
  int             res = UDM_OK;
  
  ac = UdmGetArgs(str, av, 255);
  
  for (Cmd=commands ; Cmd->name ; Cmd++)
  {
    if(!strcasecmp(Cmd->name,av[0]))
    {
      int argc=ac;
      size_t i;
      char *p;
      
      argc--;
      if(ac<Cmd->argmin+1)
      {
        sprintf(Conf->errstr,"too few (%d) arguments for command '%s'",
                argc,Cmd->name);
        return UDM_ERROR;
      }
      
      if(ac>Cmd->argmax+1)
      {
        sprintf(Conf->errstr,"too many (%d) arguments for command '%s'",
                argc,Cmd->name);
        return UDM_ERROR;
      }
      
      for (i = 1; i < ac; i++)
      {
        if (! av[i]) continue;
        p = UdmParseEnvVar(Conf, av[i]);
        if (! p)
        {
          sprintf(Conf->errstr, "An error occured while parsing '%s'", av[i]);
          return UDM_ERROR;
        }
        av[i] = p;
      }

      if(Cmd->action)
        res = Cmd->action((void*)C,ac,av);
      
      for (i = 1; i < ac; i++) UDM_FREE(av[i]);

      if(Cmd->action)
        return res;
    }
  }
  sprintf(Conf->errstr,"Unknown command: %s",av[0]);
  return UDM_ERROR;
}


__C_LINK int __UDMCALL UdmAgentAddLine(UDM_AGENT *Agent, const char *line)
{
  UDM_CFG Cfg;
  char str[1024];
  bzero((void*) &Cfg, sizeof(Cfg));
  Cfg.Indexer= Agent;
  udm_snprintf(str, sizeof(str) - 1, "%s", line);
  return UdmEnvAddLine(&Cfg, str);
}

static int EnvLoad(UDM_CFG *Cfg,const char *cname)
{
  char  *str0 = NULL;  /* Unsafe copy - will be used in strtok  */
  char  str1[1024]="";  /* To concatenate lines      */
  FILE  *config;  /* File struct */
  int  rc=UDM_OK;
  size_t  line = 0, str0len = 0, str1len, str0size = 4096;
  
  if ((str0 = (char*)UdmMalloc(str0size)) == NULL)
  {
    sprintf(Cfg->Indexer->Conf->errstr,
            "Can't alloc %d bytes at '%s': %d",
            (int) str0size, __FILE__, __LINE__);
    return UDM_ERROR;
  }
  str0[0]=0;
  
  /* Open config file */
  if(!(config=fopen(cname,"r")))
  {
    sprintf(Cfg->Indexer->Conf->errstr,
            "Can't open config file '%s': %s", cname, strerror(errno));
    UDM_FREE(str0);
    return UDM_ERROR;
  }

  update_current(cname);
  
  /*  Read lines and parse */
  while(fgets(str1,sizeof(str1),config))
  {
    char  *end;
    
    line++;
    
    if(str1[0]=='#')continue;
    for (end = str1 + (str1len = strlen(str1)) - 1 ;
         (end>=str1) && (*end=='\r'||*end=='\n'||*end==' '||*end=='\t') ;
         *end--='\0');
    if(!str1[0])continue;
    
    if(*end=='\\')
    {
      *end=0;
      if (str0len + str1len >= str0size)
      {
        str0size += 4096 + str1len;
        if ((str0 = (char*)UdmRealloc(str0, str0size)) == NULL)
        {
          sprintf(Cfg->Indexer->Conf->errstr,
                  "Can't realloc %d bytes at '%s': %d",
                  (int) str0size, __FILE__, __LINE__);
          return UDM_ERROR;
        }
      }
      strcat(str0,str1);
      str0len += str1len;
      continue;
    }
    strcat(str0,str1);
    str0len += str1len;
    
    if(UDM_OK != (rc=UdmEnvAddLine(Cfg,str0)))
    {
      char  err[2048];
      strcpy(err,Cfg->Indexer->Conf->errstr);
      sprintf(Cfg->Indexer->Conf->errstr,"%s:%d: %s", cname, (int) line, err);
      break;
    }
    
    str0[0]=0;
    str0len = 0;
  }
  UDM_FREE(str0);
  fclose(config);
  return rc;
}


__C_LINK  int __UDMCALL UdmEnvLoad(UDM_AGENT *Indexer,
                                   const char *cname, int lflags)
{
  UDM_CFG    Cfg;
  UDM_SERVER  Srv;
  int    rc=UDM_OK;
  const char  *dbaddr=NULL;
  
  UdmServerInit(&Srv);
  bzero((void*)&Cfg, sizeof(Cfg));
  Cfg.Indexer = Indexer;
  Indexer->Conf->Cfg_Srv = Cfg.Srv = &Srv;
  Cfg.flags=lflags;
  Cfg.level=0;
  
  /* Set DBAddr if for example passed from environment */
  if((dbaddr=UdmVarListFindStr(&Indexer->Conf->Vars,"DBAddr",NULL)))
  {
    if(UDM_OK != (rc= UdmEnvDBListAdd(Indexer->Conf, dbaddr, UDM_OPEN_MODE_WRITE)))
      goto freeex;
  }
  
  if(UDM_OK == (rc=EnvLoad(&Cfg,cname)))
  {
    UDM_ENV *Env= Indexer->Conf;
    
    if (UDM_OK != (rc= UdmEnvPrepare(Env)))
      goto freeex;

    UdmVarListInsStr(&Env->Vars, "Request.User-Agent", UDM_USER_AGENT);
  }

freeex:
  UdmServerFree(&Srv);
  return rc;
}



static size_t
UdmMatchToStr(char *str, size_t size, const UDM_MATCH *M, const char *cmd)
{
  if (cmd)
    return udm_snprintf(str, size, "%s %s%s%s \"%s\" \"%s\"",
                        cmd,
                        M->match_type == UDM_MATCH_REGEX ? " regex" : "",
                        M->nomatch ? " nomatch" : "",
                        M->case_sense ? "" : " NoCase",
                        M->arg, M->pattern);
  else
    return udm_snprintf(str, size, "%s %s%s%s \"%s\"",
                        M->arg,
                        M->match_type == UDM_MATCH_REGEX ? " regex" : "",
                        M->nomatch ? " nomatch" : "",
                        M->case_sense ? "" : " NoCase",
                        M->pattern);

}


static int
UdmMatchListPrint(FILE *f, UDM_MATCHLIST *L, const char *cmd)
{
  size_t i;
  char str[128];
  for (i= 0; i < L->nmatches; i++)
  {
    UDM_MATCH *M= &L->Match[i];
    UdmMatchToStr(str, sizeof(str), M, cmd);
    fprintf(f, "%s\n", str);
  }
  return UDM_OK;
}


static int
UdmSectionListPrint(FILE *f, UDM_VARLIST *L)
{
  size_t i;
  char str[128];
  for (i= 0; i < L->nvars; i++)
  {
    UDM_VAR *V= &L->Var[i];
    udm_snprintf(str, sizeof(str),
                 "Section %s %d %d",
                 V->name, V->section, (int) V->maxlen);
    fprintf(f, "%s\n", str);
  }
  return UDM_OK;
}


static int
UdmDBListPrint(FILE *f, UDM_DBLIST *L)
{
  size_t i;
  char str[128];
  for (i= 0; i < L->nitems; i++)
  {
    const char *dbaddr;
    dbaddr= UdmVarListFindStr(&L->db[i].Vars, "DBAddr", "<noaddr>");
    udm_snprintf(str, sizeof(str), "DBAddr %s", dbaddr);
    fprintf(f, "%s\n", str);
  }                                
  return UDM_OK;
}


static const char *
UdmMatchTypeToServerCommand(int match_type)
{
  switch (match_type)
  {
    case UDM_MATCH_WILD:   return "Realm";
    case UDM_MATCH_REGEX:  return "Realm regex";
    case UDM_MATCH_SUBNET: return "Subnet";
    case UDM_MATCH_BEGIN:  return "Server";
    default: return "<UnknownMatchType>";
  }
}


static int
UdmServerOptionsPrint(FILE *f, UDM_SERVER *Prev, UDM_SERVER *Curr)
{
  UDM_CONFCMD *cmd;
  for (cmd= commands; cmd->name; cmd++)
  {
    if (cmd->action == srv_rpl_var      ||
        cmd->action == srv_rpl_category ||
        cmd->action == srv_rpl_num_var  ||
        cmd->action == srv_rpl_bool_var ||
        cmd->action == srv_rpl_time_var || 
     /* cmd->action ==  srv_rpl_hdr     || */
        cmd->action ==  srv_rpl_auth    ||
     /* cmd->action ==  srv_rpl_mirror  || */
        cmd->action ==  srv_rpl_charset ||
        0)
    {
      const char *cval= UdmVarListFindStr(&Curr->Vars, cmd->name, "");
      if (cmd->action == srv_rpl_auth)
      {
        if (cval[0])
        {
          char encoded[128], decoded[128];
          udm_snprintf(encoded, sizeof(encoded), "%s", cval);
          udm_base64_decode(decoded, encoded, sizeof(decoded));
          fprintf(f, "%s '%s'\n", cmd->name, decoded);
        }
      }
      else
      {
        const char *pval= Prev ? UdmVarListFindStr(&Prev->Vars, cmd->name, "") : "";
        if (strcmp(pval, cval))
          fprintf(f, "%s '%s'\n", cmd->name, cval);
      }
    }
  }
  return UDM_OK;
}


static size_t
UdmServerToStr(char *str, size_t size, UDM_SERVER *S)
{
  const char *method= UdmMethodStr(S->method);
  int case_sense= UdmVarListFindBool(&S->Vars, "case_sense", UDM_CASE_INSENSITIVE);
  int nomatch= UdmVarListFindBool(&S->Vars, "nomatch", 0);
  const char *case_str= case_sense ? "" : "NoCase";
  const char *match_str= nomatch ? "NoMatch" : "";
  const char *follow_str= UdmFollowStr(S->follow);
  const char *command= UdmMatchTypeToServerCommand(S->Match.match_type);
  const char *alias= UdmVarListFindStr(&S->Vars, "Alias", "");
  
  /* TODO: Server site: cuts directory name */
  switch(S->Match.match_type)
  {
    case UDM_MATCH_WILD:
    case UDM_MATCH_REGEX:
    case UDM_MATCH_SUBNET:
      follow_str= "";
    default: break;
  }
  
  return udm_snprintf(str, size,
                      "%s %s %s %s %s '%s'%s%s",
                      command, follow_str, method,
                      case_str, match_str, S->Match.pattern,
                      alias[0] ? " " : "", alias);
}


static int
UdmServerListPrint(FILE *f, UDM_SERVERLIST *L)
{
  size_t i;
  char str[128];
  for (i= 0; i < L->nservers; i++)
  {
    UDM_SERVER *S= &L->Server[i];
    UDM_SERVER *P= i ? &L->Server[i-1] : NULL;
/*    UdmVarListPrint(f, &S->Vars);*/
    UdmServerOptionsPrint(f, P, S);
    UdmServerToStr(str, sizeof(str), S);
    fprintf(f, "%s\n", str);
  }
  return UDM_OK;
}


static int
UdmAliasListPrint(FILE *f, UDM_MATCHLIST *L, const char *cmd)
{
  size_t i;
  char str[128];
  for (i= 0; i < L->nmatches; i++)
  {
    UdmMatchToStr(str, sizeof(str), &L->Match[i], cmd);
    fprintf(f, "%s\n", str);
  }
  return UDM_OK;
}


static int
UdmParserListPrint(FILE *f, UDM_PARSERLIST *L)
{
  size_t i;
  char str[1024];
  for (i= 0; i < L->nparsers; i++)
  {
    UDM_PARSER *P= &L->Parser[i];
    udm_snprintf(str, sizeof(str),
                 "Mime \"%s\" \"%s\" '%s'%s%s%s",
                 P->from_mime, P->to_mime, P->cmd,
                 P->src ? " \"" : "",
                 P->src ? P->src : "",
                 P->src ? "\"" : "");
    fprintf(f, "%s\n", str);
  }
  return UDM_OK;
}


static int
UdmAffixListListPrint(FILE *f, UDM_AFFIXLISTLIST *L)
{
  size_t i;
  char str[256];
  for (i= 0; i < L->nitems; i++)
  {
    UDM_AFFIXLIST *A= &L->Item[i];
    udm_snprintf(str, sizeof(str),
                 "Affix %s %s '%s'", A->lang, A->cset, A->fname);
    fprintf(f, "%s\n", str);
  }
  return UDM_OK;
}


static int
UdmSpellListListPrint(FILE *f, UDM_SPELLLISTLIST *L)
{
  size_t i;
  char str[256];
  for (i= 0; i < L->nitems; i++)
  {
    UDM_SPELLLIST *S= &L->Item[i];
    udm_snprintf(str, sizeof(str),
                 "Spell %s %s '%s'", S->lang, S->cset, S->fname);
    fprintf(f, "%s\n", str);
  }
  return UDM_OK;
}


static int
UdmLangmapListPrint(FILE *f, UDM_LANGMAPLIST *L)
{
  size_t i;
  char str[256];
  for (i= 0; i < L->nmaps; i++)
  {
    UDM_LANGMAP *M= &L->Map[i];
    udm_snprintf(str, sizeof(str),
                 "LangmapFile '%s'", M->filename);
    fprintf(f, "%s\n", str);
  }
  return UDM_OK;
}


static int
UdmStopListListPrint(FILE *f, UDM_STOPLISTLIST *L)
{
  size_t i;
  char str[256];
  for (i= 0; i < L->nitems; i++)
  {
    UDM_STOPLIST *S= &L->Item[i];
    udm_snprintf(str, sizeof(str),
                 "StopwordFile '%s'", S->fname);
    fprintf(f, "%s\n", str);
  }
  return UDM_OK;
}


static int
UdmSynonymListListPrint(FILE *f, UDM_SYNONYMLISTLIST *L)
{
  size_t i;
  char str[256];
  for (i= 0; i < L->nitems; i++)
  {
    UDM_SYNONYMLIST *S= &L->Item[i];
    udm_snprintf(str, sizeof(str),
                 "Synonym '%s'", S->fname);
    fprintf(f, "%s\n", str);
  }
  return UDM_OK;
}

                    
__C_LINK  int __UDMCALL UdmEnvSave(UDM_AGENT *Indexer,
                                   const char *cname, int lflags)
{
  FILE *f;
  UDM_ENV *E= Indexer->Conf;

  if (!strcmp(cname, "-"))
    f= stdout;
  else if (!(f= fopen(cname,"w")))
  {
    sprintf(Indexer->Conf->errstr,
            "Can't open output file '%s': %s", cname, strerror(errno));
    return UDM_ERROR;
  }

  /* TODO: put interpreter line */
  
  UdmDBListPrint(f, &E->dbl);

  fprintf(f, "LocalCharset %s\n", E->lcs->name);
  fprintf(f, "BrowserCharset %s\n", E->bcs->name);

  UdmSectionListPrint(f, &E->Sections);
  UdmVarListPrint(&E->Vars, f);

  UdmMatchListPrint(f, &E->MimeTypes, "AddType");
  UdmParserListPrint(f, &E->Parsers);
  UdmMatchListPrint(f, &E->Filters, NULL);
  
  /*
  UDM_MATCHLIST	SectionFilters;
  UDM_MATCHLIST SectionHdrMatch;
  UDM_MATCHLIST SectionGsrMatch;
  UDM_MATCHLIST SectionMatch;
  */

  UdmStopListListPrint(f, &E->StopWord);
  UdmSynonymListListPrint(f, &E->Synonym);
  UdmAffixListListPrint(f, &E->Affixes);
  UdmSpellListListPrint(f, &E->Spells);
  UdmLangmapListPrint(f, &E->LangMaps);
  
  /*
  int		CVS_ignore;
  UDM_WORDPARAM	WordParam;
  UDM_CHINALIST   Chi;
  UDM_CHINALIST   Thai;
#ifdef MECAB
  mecab_t         *mecab;
#endif
  UDM_UNIDATA *unidata;
  */


  /*
  UDM_VARLIST	XMLEnterHooks;
  UDM_VARLIST	XMLLeaveHooks;
  UDM_VARLIST	XMLDataHooks;
  */

  UdmAliasListPrint(f, &E->Aliases, "Alias");
  UdmAliasListPrint(f, &E->ReverseAliases, "ReverseAlias");
  UdmServerListPrint(f, &E->Servers);

  if (f != stdout)
    fclose(f);
  return UDM_OK;
}
