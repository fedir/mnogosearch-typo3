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

#include <udm_config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <fcntl.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif


#include "udm_common.h"
#include "udm_utils.h"
#include "udm_db.h"
#include "udm_agent.h"
#include "udm_env.h"
#include "udm_conf.h"
#include "udm_services.h"
#include "udm_sdp.h"
#include "udm_log.h"
#include "udm_xmalloc.h"
#include "udm_doc.h"
#include "udm_result.h"
#include "udm_searchtool.h"
#include "udm_vars.h"
#include "udm_match.h"
#include "udm_spell.h"
#include "udm_mutex.h"
#include "udm_signals.h"
#include "udm_http.h"

/* This should be last include */
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned long)-1)
#endif


static int
httpd_main(UDM_AGENT *Agent, int ctl_sock,
           int (*routine)(int sock, UDM_AGENT *A))
{
  fd_set mask;
  int verb= UDM_LOG_ERROR;
  int res= UDM_OK;

  FD_ZERO(&mask);
  FD_SET(ctl_sock,&mask);

  for (; ;)
  {
    
    int sel;
    struct timeval tval;
    fd_set msk;
    
    tval.tv_sec= 300;
    tval.tv_usec= 0;
    msk= mask;
    sel= select(16,&msk,0,0,&tval);

    if(have_sigpipe)
    {
      UdmLog(Agent, verb, "SIGPIPE arrived. Broken pipe!");
      have_sigpipe = 0;
      res= UDM_ERROR;
      break;
    }
    
    if (have_sigint)
    {
      UdmLog(Agent, verb, "SIGINT arrived.");
      have_sigterm = 0;
      res= UDM_ERROR;
      break;
    }

    if(sel == 0)
      continue;
    
    if(sel==-1)
    {
      switch(errno)
      {
        case EINTR:  /* Child */
          break;
        default:
          UdmLog(Agent,verb,"FIXME select error %d %s",errno,strerror(errno));
      }
      continue;
    }
    
    if(FD_ISSET(ctl_sock,&msk))
    {
      int  ns;
      struct sockaddr_in client_addr;
      socklen_t  addrlen=sizeof(client_addr);
      char  addr[128]= "";
      
      /*
      int   method;
      UDM_MATCH  *M;
      UDM_MATCH_PART  P[10];
      */
      
      if ((ns= accept(ctl_sock, (struct sockaddr *) &client_addr, &addrlen)) == -1)
      {
        UdmLog(Agent,verb,"accept() error %d %s",errno,strerror(errno));
        UdmEnvFree(Agent->Conf);
        UdmAgentFree(Agent);
        exit(1);
      }
      UdmLog(Agent,verb,"Connect %s", inet_ntoa(client_addr.sin_addr));
      udm_snprintf(addr,sizeof(addr)-1,inet_ntoa(client_addr.sin_addr));
      
      /*
      M=UdmMatchListFind(&Agent->Conf->Filters,addr,10,P);
      method=M?UdmMethod(M->arg):UDM_METHOD_GET;
      UdmLog(Agent,verb,"%s %s %s%s",M?M->arg:"",addr,M?M->pattern:"",M?"":"Allow by default");
      
      if(method==UDM_METHOD_DISALLOW)
      {
        UdmLog(Agent,verb,"Reject client");
        close(ns);
        continue;
      }
      */
      
      if (0)
      {
        pid_t fres;
        if(!(fres= fork()))
        {
          alarm(300); /* 5 min. - maximum time of child execution */
          closesocket(ctl_sock);
          routine(ns, Agent);
          closesocket(ns);
          exit(0);
        }
        else
        {
          close(ns);
          if(fres > 0)
          {
            /* Do nothing */
          }
          else
          {
            UdmLog(Agent,verb,"fork error %d",fres);
          }
        }
      }
      else
      {
        routine(ns, Agent);
        closesocket(ns);
      }
      
    }
  }
  return res;
}


int UdmStartHTTPD(UDM_AGENT *Agent, int (*routine)(int sock, UDM_AGENT *A))
{
  int nport= UDM_SEARCHD_PORT;

  struct sockaddr_in server_addr;
  int ctl_sock, on=1;
  int verb= UDM_LOG_ERROR;
  int res=0;
  const char *lstn;

  UdmLog(Agent,verb,"Starting HTTP daemon");
    
  if ((ctl_sock= socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    UdmLog(Agent, verb, "socket() error %d", errno);
    return UDM_ERROR;
  }

  if (setsockopt(ctl_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) != 0)
  {
    UdmLog(Agent,verb,"setsockopt() error %d",errno);
    return UDM_ERROR;
  }

  bzero((void*)&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  if((lstn= UdmVarListFindStr(&Agent->Conf->Vars, "Listen", NULL)))
  {
    char *cport;
    if((cport= strchr(lstn,':')))
    {
      UdmLog(Agent,verb,"Listening '%s'", lstn);
      *cport= '\0';
      server_addr.sin_addr.s_addr= inet_addr(lstn);
      nport= atoi(cport + 1);
    }
    else
    {
      nport= atoi(lstn);
      server_addr.sin_addr.s_addr= htonl(INADDR_ANY);
      UdmLog(Agent, verb,"Listening port %d", nport);
    }
  }
  else
  {
    UdmLog(Agent,verb,"Listening port %d", nport);
    server_addr.sin_addr.s_addr= htonl(INADDR_ANY);
  }
  server_addr.sin_port= htons((u_short)nport);

  if (bind(ctl_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
  {
    UdmLog(Agent,verb,"Can't bind: error %d %s",errno,strerror(errno));
    return UDM_ERROR;
  }

  if (listen(ctl_sock, 32) == -1)
  {
      UdmLog(Agent,verb,"listen() error %d %s",errno,strerror(errno));
      return UDM_ERROR;
  }
    
  UdmLog(Agent,verb,"HTTPD Ready");
  
  res= httpd_main(Agent, ctl_sock, routine);
  closesocket(ctl_sock);

  UdmLog(Agent,verb,"HTTPD Shutdown");
  return 0;
}
