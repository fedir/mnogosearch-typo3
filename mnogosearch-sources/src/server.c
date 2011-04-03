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
#include <ctype.h>

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_proto.h"
#include "udm_url.h"
#include "udm_hrefs.h"
#include "udm_server.h"
#include "udm_xmalloc.h"
#include "udm_host.h"
#include "udm_vars.h"
#include "udm_wild.h"
#include "udm_match.h"
#include "udm_db.h"
#include "udm_hash.h"

#define DEFAULT_PROXY_PORT  3128
#define ERRSTRSIZ 1000


static int
UdmServerCopy(UDM_ENV *Conf, UDM_SERVER *dst, UDM_SERVER *src,
              const char *urlstr)
{
  char regerrstr[ERRSTRSIZ]= "";
  int err;

  dst->command= src->command;
  dst->ordre= src->ordre;
  dst->weight= src->weight;
  dst->MaxHops= src->MaxHops;
  dst->follow= src->follow;
  dst->method= src->method;
  
  dst->Match.pattern= (char*)UdmStrdup(urlstr);
  dst->Match.nomatch= src->Match.nomatch;
  dst->Match.case_sense= src->Match.case_sense;
  dst->Match.match_type= src->Match.match_type;
  dst->Match.reg= src->Match.reg;
  dst->Match.arg= src->Match.arg;

  src->Match.reg= NULL;
  src->Match.arg= NULL;

  if (UDM_OK != (err= UdmMatchComp(&dst->Match, regerrstr, sizeof(regerrstr)-1)))
  {
    udm_snprintf(Conf->errstr,sizeof(Conf->errstr),"'%s:' %s",
                 dst->Match.pattern, regerrstr);
    return UDM_ERROR;
  }

  UdmVarListReplaceLst(&dst->Vars, &src->Vars, NULL, "*");

  return UDM_OK;
}

/* return values: 0 on success, non-zero on error */

__C_LINK int __UDMCALL UdmServerAdd(UDM_AGENT *A, UDM_SERVER *src, int flags)
{
  int            add= 1, rc= UDM_OK;
  char           *urlstr= NULL;
  size_t         i;
  UDM_SERVER     *dst= NULL;
  UDM_SERVERLIST S;
  UDM_ENV        *Conf = A->Conf;

  if (src->Match.match_type == UDM_MATCH_BEGIN)
  {
    UDM_URL from;
    size_t len;
    
    /* Copy URL to temp string    */
    /* to keep srv->url unchanged */
    len= 3*strlen(src->Match.pattern) + 4;
    if ((urlstr= (char*)UdmMalloc(len)) == NULL) return UDM_ERROR;
    UdmURLCanonize(src->Match.pattern, urlstr, len);
    
    UdmURLInit(&from);
    
    /* Check whether valid URL is passed */
    if((rc= UdmURLParse(&from,urlstr)))
    {
      switch (rc)
      {
        case UDM_URL_LONG:
          sprintf(Conf->errstr,"URL too long");
          break;
        case UDM_URL_BAD:
        default:
          sprintf(Conf->errstr,"Badly formed URL");
          break;
      }
      UDM_FREE(urlstr);
      UdmURLFree(&from);
      return UDM_ERROR;
    }
    if((from.hostinfo) && (from.filename == NULL))
    {
      /* Add trailing slash                    */
      /* http://localhost -> http://localhost/ */
      udm_snprintf(urlstr, len, "%s://%s%s", from.schema, from.hostinfo, UDM_NULL2EMPTY(from.path));
    }
    
    switch(src->follow)
    {
      char * s, * anchor;
      case UDM_FOLLOW_PATH:
        /* Cut before '?' and after last '/' */
        if((anchor=strchr(urlstr,'?')))
          *anchor='\0';
        if((s=strrchr(urlstr,'/')))
          *(s+1)='\0';
        break;

      case UDM_FOLLOW_SITE:
        if (from.hostinfo != NULL)
        {
          /* Cut after hostinfo */
          udm_snprintf(urlstr, len, "%s://%s/", UDM_NULL2EMPTY(from.schema), from.hostinfo);
        }
        else
        {
          /* Cut after first '/' */
          if((s=strchr(urlstr,'/')))
            *(s+1)='\0';
        }
        break;
      
      case UDM_FOLLOW_NO: 
      case UDM_FOLLOW_WORLD:
      case UDM_FOLLOW_URLLIST:
      default:
        break;
    }
    if (!strcmp(UDM_NULL2EMPTY(from.schema), "news"))
    {
      char *c, *cc;
      /* Cat server name to remove group names */
      /* This is because group names do not    */
      /* present in message URL                */
      c=urlstr+7;
      cc=strchr(c,'/');
      if(cc)*(cc+1)='\0';
    }
    UdmURLFree(&from);
  }
  else if(src->Match.match_type == UDM_MATCH_REGEX)
  {
    urlstr= UdmStrdup(src->Match.pattern);
  }
  else
  {
    urlstr= UdmStrdup(src->Match.pattern);
  }
  
  if (!urlstr)return UDM_ERROR; 
  
  for (i = 0; i < Conf->Servers.nservers; i++)
  {
    if (!strcmp(Conf->Servers.Server[i].Match.pattern, urlstr))
    {
      add= 0;
      dst= &Conf->Servers.Server[i];
      UDM_FREE(dst->Match.pattern); /* TODO: should be UdmMatchFree() ? */
      break;
    }
  }
  
  if (add)
  {
    if(Conf->Servers.nservers>=Conf->Servers.mservers)
    {
      Conf->Servers.mservers+=16;
      Conf->Servers.Server=(UDM_SERVER *)UdmXrealloc(Conf->Servers.Server,Conf->Servers.mservers*sizeof(UDM_SERVER));
    }
    dst= &Conf->Servers.Server[Conf->Servers.nservers];
    Conf->Servers.nservers++;
    UdmServerInit(dst);
  }

  
  UdmServerCopy(Conf, dst, src, urlstr);
  
  S.Server= dst;
  if (!(flags & UDM_FLAG_DONT_ADD_TO_DB))
    rc= UdmSrvAction(A, &S, UDM_SRV_ACTION_ADD);
  src->site_id = dst->site_id;
  
  UDM_FREE(urlstr);
  return rc;
}

void UdmServerFree(UDM_SERVER *Server)
{
  UdmMatchFree(&Server->Match);
  UdmVarListFree(&Server->Vars);
}

void UdmServerListFree(UDM_SERVERLIST *List)
{
  size_t i;
  
  for(i=0;i<List->nservers;i++)
    UdmServerFree(&List->Server[i]);
  
  List->nservers=List->mservers=0;
  UDM_FREE(List->Server);
}

/* This fuction finds Server entry for given URL         */
/* and return Alias in "aliastr" if it is not NULL       */
/* "aliastr" must be big enough to store result          */
/* not more than UDM_URLSTR bytes are written to aliastr */

UDM_SERVER * UdmServerFind(UDM_ENV *Conf, UDM_SERVERLIST *List, const char *url, char **aliastr)
{
#define NS 10
  size_t      i;
  char        *robots= NULL;
  UDM_SERVER  *Res= NULL;
  char        net[32];
  size_t      urllen= strlen(url);
  
  /* If it's a robot.txt, cut to hostinfo and find result */
  if((robots=strstr(url,"/robots.txt")))
  {
    if(!strcmp(robots,"/robots.txt"))
    {
      robots = (char*)UdmStrdup(url);
      robots[urllen-10]='\0';
    }
    else
    {
      robots=NULL;
    }
  }
  
  net[0] = '\0';
  for(i=0;i<List->nservers;i++)
  {
    UDM_SERVER  *srv=&List->Server[i];
    UDM_MATCH_PART  P[10];
    const char  *alias=UdmVarListFindStr(&srv->Vars,"Alias",NULL);
    size_t aliastrlen;

    if (srv->Match.match_type == UDM_MATCH_SUBNET && *net == '\0')
    {
      UDM_CONN conn;
      UDM_URL  URL;
      
      UdmURLInit(&URL);
      if(UdmURLParse(&URL, url))
      {
        UdmURLFree(&URL);
        continue;
      }
      conn.hostname = URL.hostname;
      conn.port=80;
      if (UdmHostLookup(&Conf->Hosts, &conn) != -1)
      {
        unsigned char * h;
        h=(unsigned char*)(&conn.sin.sin_addr);
        snprintf(net, sizeof(net) - 1, "%d.%d.%d.%d", h[0], h[1], h[2], h[3]);
      }
      UdmURLFree(&URL);
    }
     
    if(srv->follow == UDM_FOLLOW_WORLD ||
       !UdmMatchExec(&srv->Match, url, urllen, net, 10, P))
    {
      Res=srv;
      if((aliastr != NULL) && (alias != NULL))
      {
        aliastrlen = 128 + urllen + strlen(alias) + strlen(srv->Match.pattern);
        *aliastr = (char*)UdmMalloc(aliastrlen);
        if (*aliastr != NULL)
          UdmMatchApply(*aliastr, aliastrlen, url, alias, &srv->Match, 10, P);
      }
      break;
    }
  }
  UDM_FREE(robots);
  return(Res);
}

#if 0
static int cmpserver(const void *s1,const void *s2)
{
  int res;
  
  if(!(res=strlen(((const UDM_SERVER*)s2)->url)-strlen(((const UDM_SERVER*)s1)->url)))
    res=(((const UDM_SERVER*)s2)->rec_id)-(((const UDM_SERVER*)s1)->rec_id);
  return(res);
}
void UdmServerListSort(UDM_SERVERLIST *List)
{
  /*  Long name should be found first    */
  /*  to allow different options         */
  /*  for server and it's subdirectories */
  UdmSort(List->Server,List->nservers,sizeof(UDM_SERVER),cmpserver);
}
#endif


int UdmSpiderParamInit(UDM_SPIDERPARAM *Spider)
{
  Spider->period=UDM_DEFAULT_REINDEX_TIME;
  Spider->max_net_errors=UDM_MAXNETERRORS;
  Spider->read_timeout=UDM_READ_TIMEOUT;
  Spider->doc_timeout=UDM_DOC_TIMEOUT;
  Spider->maxhops=UDM_DEFAULT_MAX_HOPS;
  Spider->index=1;
  Spider->follow=UDM_FOLLOW_DEFAULT;
  Spider->use_robots=1;
  Spider->use_clones=1;
  Spider->net_error_delay_time=UDM_DEFAULT_NET_ERROR_DELAY_TIME;
  Spider->crawl_delay= 0;
  return UDM_OK;
}


__C_LINK int __UDMCALL UdmServerInit(UDM_SERVER * srv)
{
  bzero((void*)srv, sizeof(*srv));
  srv->Match.match_type=UDM_MATCH_BEGIN;
  srv->weight = 1; /* default ServerWeight */
  srv->MaxHops = 255; /* default MaxHops value */
  srv->follow= UDM_FOLLOW_DEFAULT;
  srv->method= UDM_METHOD_DEFAULT;
  srv->enabled= 1;
  return(0);
}


urlid_t UdmServerGetSiteId(UDM_AGENT *Indexer, UDM_SERVER *srv, UDM_URL *url)
{
  char *urlstr;
  UDM_SERVERLIST sl;
  UDM_SERVER S;
  int rc;
  int crc32id= UdmVarListFindBool(&Indexer->Conf->Vars, "UseCRC32SiteId", 0);
  if (!crc32id && (srv->Match.match_type == UDM_MATCH_BEGIN) &&
      (srv->Match.nomatch == 0) && (srv->follow == UDM_FOLLOW_SITE))
  {
    return srv->site_id;
  }
  if((urlstr = (char*)UdmMalloc(strlen(UDM_NULL2EMPTY(url->schema)) + strlen(UDM_NULL2EMPTY(url->hostname)) + 10)) == NULL)
  {
    return 0;
  }
  sprintf(urlstr, "%s://%s/", UDM_NULL2EMPTY(url->schema), UDM_NULL2EMPTY(url->hostname));
  {
    register size_t i;
    for (i = 0; i < strlen(urlstr); i++) urlstr[i] = tolower(urlstr[i]);
  }
  if (crc32id)
  {
    urlid_t res= UdmStrHash32(urlstr);
    UdmFree(urlstr);
    return res;
  }
  bzero((void*)&S, sizeof(S));
  sl.Server = &S;
  S.Match.pattern     = urlstr;
  S.Match.match_type  = UDM_MATCH_BEGIN;
  S.Match.nomatch     = 0;
  S.command = 'S';
  S.ordre = srv->ordre;
  S.parent = srv->site_id;
  S.weight = srv->weight;
  rc = UdmSrvAction(Indexer, &sl, UDM_SRV_ACTION_ID);
  UDM_FREE(urlstr);
  return (rc == UDM_OK) ? S.site_id : 0;
}
