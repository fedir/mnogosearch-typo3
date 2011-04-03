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


#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
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

#include "udm_common.h"
#include "udm_socket.h"
#include "udm_host.h"
#include "udm_proto.h"
#include "udm_utils.h"
#include "udm_mutex.h"

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned long) -1)
#endif

#define MAX_HOST_ADDR_SIZE	512


static int cmphost(const void * v1, const void * v2)
{
  return(strcasecmp(((const UDM_HOST_ADDR*)v1)->hostname,
                    ((const UDM_HOST_ADDR*)v2)->hostname));
}


static int host_addr_add(UDM_HOSTLIST *List,
                         const char *hostname, struct in_addr *addr)
{
  int min_id;
  
  if((List->nhost_addr>=List->mhost_addr)&&
     (List->mhost_addr<MAX_HOST_ADDR_SIZE))
  {
    List->mhost_addr+=32;
    List->host_addr= UdmRealloc(List->host_addr, List->mhost_addr*sizeof(UDM_HOST_ADDR));
    bzero((void*)&List->host_addr[List->nhost_addr], (List->mhost_addr-List->nhost_addr)*sizeof(UDM_HOST_ADDR));
  }

  if((List->nhost_addr<List->mhost_addr)&&
     (List->mhost_addr<=MAX_HOST_ADDR_SIZE))
  {
    min_id= List->nhost_addr;
    List->nhost_addr++;
  }
  else
  {
    int min;
    size_t i;

    min= List->host_addr[0].last_used;
    min_id= 0;
    /* find last used host */
    for (i= 0; i<List->nhost_addr; i++)
      if(List->host_addr[i].last_used < List->host_addr[min_id].last_used){
        min= List->host_addr[i].last_used;
        min_id=i;
      }
  }

  List->host_addr[min_id].last_used= time(NULL);
  if(addr)memcpy(&List->host_addr[min_id].addr, addr, sizeof( struct in_addr));
  
  UDM_FREE(List->host_addr[min_id].hostname);
  List->host_addr[min_id].hostname= (char*)UdmStrdup(hostname);
  List->host_addr[min_id].net_errors=0;

  UdmSort(List->host_addr, List->nhost_addr, sizeof(UDM_HOST_ADDR), cmphost);
  return 0;
}

UDM_HOST_ADDR *
UdmHostFind(UDM_HOSTLIST *List,const char *hostname)
{
  int l,c,r,res;

  if (!hostname)
    return NULL;
  
  /* Find current URL in sorted part of list */
  l=0;r=List->nhost_addr-1;
  while(l<=r)
  {
    c=(l+r)/2;
    if(!(res= strcasecmp(List->host_addr[c].hostname, hostname)))
      return(&List->host_addr[c]);
    
    if(res<0)
      l=c+1;
    else
      r=c-1;
  }
  return NULL;
}


void
UdmHostErrorReset(UDM_HOSTLIST *List, const char *hostname)
{
  UDM_HOST_ADDR *h= UdmHostFind(List, hostname);
  if (h)
    h->net_errors= 0;
}


void
UdmHostErrorIncrement(UDM_HOSTLIST *List, const char *hostname)
{
  UDM_HOST_ADDR *h= UdmHostFind(List, hostname);
  if (h)
    h->net_errors++;
}


int UdmHostLookup(UDM_HOSTLIST *List, UDM_CONN *connp)
{
  struct hostent *he= NULL;
  UDM_HOST_ADDR  *Host;

  connp->net_errors= 0;
  
  if (!connp->hostname)
    return -1;
  
  bzero((void*)&connp->sin, sizeof(struct sockaddr_in));
  
  /*  Set port  */
  if (connp->port) 
    connp->sin.sin_port= htons(connp->port);
  else
  {
    connp->err= UDM_NET_ERROR;
    return -1;
  }

  /* Check if hostname in standard numbers-and-dots notation */ 
  if ((connp->sin.sin_addr.s_addr= inet_addr(connp->hostname)) == INADDR_NONE)
  {
    int i;

    if((Host= UdmHostFind(List,connp->hostname)))
    {
      Host->last_used=time(NULL);
      connp->net_errors= Host->net_errors;
      connp->host_last_used= Host->last_used;
      if(Host->addr.s_addr)
      {
        memcpy(&connp->sin.sin_addr, &Host->addr, sizeof(struct in_addr));
      }
      else
      {
        connp->err= UDM_NET_CANT_RESOLVE;
        return -1;
      }
      return 0;
    }
    
    /* 
      Note:
      Caller must use mutex lock: 
      gethostbyname() isn't reentrant 
    */
      
    for (i= 0; (i < 3) && (he == NULL); i++)
    {
      he= gethostbyname(connp->hostname);
    }

    if (he == NULL)
    {
      host_addr_add(List,connp->hostname, NULL);
      connp->err= UDM_NET_CANT_RESOLVE;
      return -1;
    }
    memcpy((char *)&(connp->sin.sin_addr), he->h_addr, (size_t)he->h_length);
    host_addr_add(List,connp->hostname, &connp->sin.sin_addr);
  }
  else
  {
    if(!(Host= UdmHostFind(List,connp->hostname)))
    {
      host_addr_add(List,connp->hostname, &connp->sin.sin_addr);
    }
  }
  return 0;
}


static int
UdmHostLookupR(UDM_AGENT *A, UDM_HOSTLIST *List, UDM_CONN *connp)
{
#ifdef HAVE_GETHOSTBYNAME_R_6    
  {
    int err, rc;
    char buf[2048];
    struct hostent *he, res;
    if (!(rc= gethostbyname_r(connp->hostname, &res,
                              buf, sizeof(buf), &he, &err)) && he)
    {
      memcpy((char *)&(connp->sin.sin_addr), he->h_addr, (size_t)he->h_length);
      return 0;
    }
    return -1;
  }
#else
  {
    int rc= -1;
    struct hostent *he;
    /*  gethostbyname() isn't reentrant */
    UDM_GETLOCK(A, UDM_LOCK_CONF);
    if ((he= gethostbyname(connp->hostname)))
    {
      memcpy((char *)&(connp->sin.sin_addr), he->h_addr, (size_t)he->h_length);
      rc= 0;
    }
    UDM_RELEASELOCK(A, UDM_LOCK_CONF);
    return rc;
  }
#endif
  return -1;
}


int
UdmHostLookup2(UDM_AGENT *A, UDM_HOSTLIST *List, UDM_CONN *connp)
{
  UDM_HOST_ADDR  *Host;

  connp->net_errors= 0;
  
  if (!connp->hostname)
    return -1;
  
  bzero((void*)&connp->sin, sizeof(struct sockaddr_in));
  
  /*  Set port  */
  if (connp->port) 
    connp->sin.sin_port= htons(connp->port);
  else
  {
    connp->err= UDM_NET_ERROR;
    return -1;
  }

  /* Check if hostname in standard numbers-and-dots notation */ 
  if ((connp->sin.sin_addr.s_addr= inet_addr(connp->hostname)) == INADDR_NONE)
  {
    /* Not numbers-and-dots notation */
    int rc= 0;

    UDM_GETLOCK(A, UDM_LOCK_CONF);
    if((Host= UdmHostFind(List,connp->hostname)))
    {
      Host->last_used=time(NULL);
      connp->net_errors= Host->net_errors;
      connp->host_last_used= Host->last_used;
      if(Host->addr.s_addr)
      {
        memcpy(&connp->sin.sin_addr, &Host->addr, sizeof(struct in_addr));
      }
      else
      {
        connp->err= UDM_NET_CANT_RESOLVE;
        rc= -1;
      }
    }
    UDM_RELEASELOCK(A, UDM_LOCK_CONF);
    if (Host)
      return rc;

    rc= UdmHostLookupR(A, List, connp);
    
    if (rc < 0)
    {
      UDM_GETLOCK(A, UDM_LOCK_CONF);
      host_addr_add(List,connp->hostname, NULL);
      UDM_RELEASELOCK(A, UDM_LOCK_CONF);
      connp->err= UDM_NET_CANT_RESOLVE;
      return rc;
    }
    UDM_GETLOCK(A, UDM_LOCK_CONF);
    host_addr_add(List,connp->hostname, &connp->sin.sin_addr);
    UDM_RELEASELOCK(A, UDM_LOCK_CONF);
  }
  else
  {
    /* Numbers-and-dots notation */
    UDM_GETLOCK(A, UDM_LOCK_CONF);
    if(!(Host= UdmHostFind(List,connp->hostname)))
    {
      host_addr_add(List,connp->hostname, &connp->sin.sin_addr);
    }
    UDM_RELEASELOCK(A, UDM_LOCK_CONF);
  }
  return 0;
}


void UdmHostListFree(UDM_HOSTLIST *List)
{
  size_t i;
  
  for(i=0; i<List->nhost_addr; i++)
    UDM_FREE(List->host_addr[i].hostname);
  
  UDM_FREE(List->host_addr);
  List->nhost_addr=0;
  return;
}
