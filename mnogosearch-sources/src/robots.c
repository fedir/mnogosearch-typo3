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
#include <sys/types.h>

#include "udm_common.h"
#include "udm_robots.h"
#include "udm_utils.h"
#include "udm_vars.h"
#include "udm_log.h"
#include "udm_mutex.h"
#include "udm_conf.h"
#include "udm_sgml.h"
#include "udm_hrefs.h"


UDM_ROBOT* UdmRobotFind(UDM_ROBOTS *Robots,const char *hostinfo){
  size_t i; 

  /* FIXMI: <hash> or <btree> or <bsearch with partial sorting> or <?> */
  for(i=0;i<Robots->nrobots;i++)
  {
    if(!strcasecmp(hostinfo, Robots->Robot[i].hostinfo))
      return &Robots->Robot[i];
  }
  return NULL;
}


static UDM_ROBOT* DeleteRobotRules(UDM_ROBOTS *Robots,const char *hostinfo)
{
  UDM_ROBOT *robot;
  size_t i;

  if((robot=UdmRobotFind(Robots, hostinfo))!=NULL)
  {
    for(i=0;i<robot->nrules;i++)
      UDM_FREE(robot->Rule[i].path);
    robot->nrules=0;
    UDM_FREE(robot->Rule);
    return robot;
  }
  return NULL;
}


UDM_ROBOT* UdmRobotAddEmpty(UDM_ROBOTS *Robots,const char *hostinfo)
{
  Robots->Robot= (UDM_ROBOT*)UdmRealloc(Robots->Robot,
                                        (Robots->nrobots + 1) * sizeof(UDM_ROBOT));
  if(Robots->Robot==NULL)
  {
    Robots->nrobots= 0;
    return NULL;
  }
  bzero((void*)&Robots->Robot[Robots->nrobots], sizeof(UDM_ROBOT));
  Robots->Robot[Robots->nrobots].hostinfo= (char*)UdmStrdup(hostinfo);
  Robots->nrobots++;
  return &Robots->Robot[Robots->nrobots-1];
}

static int AddRobotRule(UDM_ROBOT *robot,int cmd,const char *path)
{
  robot->Rule= (UDM_ROBOT_RULE*)UdmRealloc(robot->Rule,
                                           (robot->nrules + 1) * sizeof(UDM_ROBOT_RULE));
  if(robot->Rule==NULL)
  {
    robot->nrules= 0;
    return UDM_ERROR;
  }
  robot->Rule[robot->nrules].cmd= cmd;
  robot->Rule[robot->nrules].path= (char*)UdmStrdup(path);
  robot->nrules++;
  return UDM_OK;
}

int UdmRobotListFree(UDM_ROBOTS *Robots)
{
  size_t i,j; 
  
  if(!Robots->nrobots)
    return 0;
  for(i=0;i<Robots->nrobots;i++)
  {
    for(j=0;j<Robots->Robot[i].nrules;j++)
      UDM_FREE(Robots->Robot[i].Rule[j].path);
    UDM_FREE(Robots->Robot[i].hostinfo);
    UDM_FREE(Robots->Robot[i].Rule);
  }
  UDM_FREE(Robots->Robot);
  Robots->nrobots=0;
  return 0;
}

UDM_ROBOT_RULE* UdmRobotRuleFind(UDM_ROBOTS *Robots,UDM_URL *URL)
{
  UDM_ROBOT *robot;
  size_t j;
  
  if((robot=UdmRobotFind(Robots, UDM_NULL2EMPTY(URL->hostinfo)))!=NULL)
  {
    const char *specific;

    if (!URL->specific ||
        !URL->specific[0] ||
        !URL->specific[1] ||
        !(specific= strchr(URL->specific + 2, '/')))
      specific= "/";

    for(j=0;j<robot->nrules;j++)
    {
      /* FIXME: compare full URL */
      if(!strncmp(specific,robot->Rule[j].path,strlen(robot->Rule[j].path)))
      {
        if(robot->Rule[j].cmd!=UDM_METHOD_DISALLOW)
          return NULL;
        else
          return &robot->Rule[j];
      }
    }
  }
  return NULL;
}

int UdmRobotParse(UDM_AGENT *Indexer, UDM_SERVER *Srv,
                  UDM_HREFLIST *Sitemap,
                  char *content, const char *hostinfo)
{
  UDM_ENV *Conf= Indexer->Conf;
  UDM_ROBOTS *Robots= &Conf->Robots;
  UDM_ROBOT *robot;
  int rule= 0, common= 0, my= 0;
  char *s,*e,*lt;

  UDM_LOCK_CHECK_OWNER(Indexer, UDM_LOCK_CONF);
  /* Wipe out any existing (default) rules for this host */
  robot= DeleteRobotRules(Robots,hostinfo);
  if (robot == NULL) robot= UdmRobotAddEmpty(Robots, hostinfo);
  if(robot==NULL) return UDM_ERROR;
  if(content==NULL) return UDM_OK;

  s= udm_strtok_r(content, "\r\n", &lt);
  while(s)
  {
    if(*s=='#')
    {
    }
    else if(!(strncasecmp(s,"User-Agent:",11)))
    {
      char * agent;
      
      agent=UdmTrim(s+11," \t\r\n");
      
      /* The "*" User-Agent is important only */
      /* if no other rules apply */
      if(!strcmp(agent,"*") && (!robot->nrules))
      {
        if (!my)
        {
          rule= 1;
          common= 1;
        }
      }
      else if(!strncasecmp(agent, UdmVarListFindStr(&Srv->Vars, "Request.User-Agent", UDM_USER_AGENT), strlen(agent)))
      {
        rule= 1;
        my= 1;
        if (common)
        {
          robot= DeleteRobotRules(Robots, hostinfo);
          common= 0;
        }
      }
      else
      {
        rule= 0;
      }
    }
    else if((!(strncasecmp(s, "Disallow", 8))) && (rule))
    {
      if((e=strchr(s+9,'#')))*e=0;
      e=s+9;UDM_SKIP(e," \t");s=e;
      UDM_SKIPN(e," \t");*e=0;
      if(*s)
      {
        if(AddRobotRule(robot,UDM_METHOD_DISALLOW,s))
        {
          UdmLog(Indexer, UDM_LOG_ERROR, "AddRobotRule error: no memory ?");
          return UDM_ERROR;
        }
      }
      else
      { /* Empty Disallow == Allow all */
        if(AddRobotRule(robot, UDM_METHOD_GET, "/"))
        {
          UdmLog(Indexer, UDM_LOG_ERROR, "AddRobotRule error: no memory ?");
          return UDM_ERROR;
        }
      }
    }
    else if((!(strncasecmp(s, "Allow", 5))) && (rule))
    {
      if((e=strchr(s+6,'#')))*e=0;
      e=s+6;UDM_SKIP(e," \t");s=e;
      UDM_SKIPN(e," \t");*e=0;
      if(*s)
      {
        if(AddRobotRule(robot,UDM_METHOD_GET,s))
        {
          UdmLog(Indexer, UDM_LOG_ERROR, "AddRobotRule error: no memory ?");
          return UDM_ERROR;
        }
      }
    }
    else if (!strncasecmp(s, UDM_CSTR_WITH_LEN("Sitemap:")))
    {
      char *av[3];
      size_t ac= UdmGetArgs(s, av, 3);
      if (ac == 2)
      {
        UDM_HREF Href;
        /*UdmSGMLUnescape(av[1]);*/
        UdmHrefInit(&Href);
        Href.url= av[1];
        UdmHrefListAdd(Sitemap, &Href);
      }
    }
    s= udm_strtok_r(NULL, "\n\r", &lt);
  }
  return UDM_OK;
}
