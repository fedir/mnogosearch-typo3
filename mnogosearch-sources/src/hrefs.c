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
#include "udm_spell.h"
#include "udm_hrefs.h"
#include "udm_utils.h"
#include "udm_xmalloc.h"
#include "udm_sgml.h"
#include "udm_url.h"
#include "udm_vars.h"

/* Max URLs in cache: 4K URLs will use about 200K of RAM         */
/* This should be a configurable parameter but we'll use 4K now  */

#define HSIZE		256	/* Length of buffer increment  TUNE */
#define RESORT_HREFS	256	/* Max length of unsorted part TUNE */

UDM_HREF *UdmHrefInit(UDM_HREF * H)
{
  bzero((void*)H, sizeof(*H));
  return(H);
}


void UdmHrefFree(UDM_HREF * H)
{
  if (H->url)
    UdmFree(H->url);
  UdmVarListFree(&H->Vars);
}


/* Function to sort URLs in alphabetic order */
static int cmphrefs(const void * v1, const void * v2)
{
  return(strcmp(((const UDM_HREF*)v1)->url,((const UDM_HREF*)v2)->url));
}


__C_LINK int __UDMCALL UdmHrefListAdd(UDM_HREFLIST * HrefList,UDM_HREF * Href)
{
  int l,r,c,res;
  size_t len;
  char *ehref;
  
  /* Don't add an empty link */
  if (!(len= strlen(Href->url)) || 
      !(ehref = (char*)UdmMalloc(3*len + 1)))
    return 0;
  
  UdmURLCanonize(Href->url, ehref, 3*len+1);
  UdmSGMLUnescape(ehref);
  
  /* Find current URL in sorted part of list */
  l=0;r=HrefList->shrefs-1;
  while(l<=r)
  {
    c=(l+r)/2;
    if(!(res=strcmp(HrefList->Href[c].url,ehref)))
    {
      HrefList->Href[c].stored = Href->stored;
      HrefList->Href[c].referrer = Href->referrer;
      HrefList->Href[c].hops = Href->hops;
      HrefList->Href[c].method = Href->method;
      HrefList->Href[c].stored = Href->stored;
      HrefList->Href[c].site_id = Href->site_id;
      HrefList->Href[c].server_id = Href->server_id;
      HrefList->Href[c].rec_id = Href->rec_id;
      HrefList->Href[c].max_doc_per_site = Href->max_doc_per_site;
      HrefList->Href[c].collect_links= Href->collect_links;
      UdmVarListFree(&HrefList->Href[c].Vars);
      UdmVarListInit(&HrefList->Href[c].Vars);
      UdmVarListReplaceLst(&HrefList->Href[c].Vars, &Href->Vars, NULL, "*");
      UDM_FREE(ehref);
      return(0);
    }
    if(res<0)
      l=c+1;
    else
      r=c-1;
  }
  
  /* Find in unsorted part */
  for(c= HrefList->shrefs; c < HrefList->nhrefs; c++)
  {
    if(!strcmp(HrefList->Href[c].url,ehref))
    {
      HrefList->Href[c].stored = Href->stored;
      HrefList->Href[c].referrer = Href->referrer;
      HrefList->Href[c].hops = Href->hops;
      HrefList->Href[c].method = Href->method;
      HrefList->Href[c].stored = Href->stored;
      HrefList->Href[c].site_id = Href->site_id;
      HrefList->Href[c].server_id = Href->server_id;
      HrefList->Href[c].rec_id = Href->rec_id;
      HrefList->Href[c].max_doc_per_site = Href->max_doc_per_site;
      HrefList->Href[c].collect_links= Href->collect_links;
      UdmVarListFree(&HrefList->Href[c].Vars);
      UdmVarListInit(&HrefList->Href[c].Vars);
      UdmVarListReplaceLst(&HrefList->Href[c].Vars, &Href->Vars, NULL, "*");
      UDM_FREE(ehref);
      return(0);
    }
  }
  if(HrefList->nhrefs>=HrefList->mhrefs)
  {
    HrefList->mhrefs+=HSIZE;
    HrefList->Href=(UDM_HREF *)UdmRealloc(HrefList->Href,HrefList->mhrefs*sizeof(UDM_HREF));
  }
  HrefList->Href[HrefList->nhrefs].url = (char*)UdmStrdup(ehref);
  HrefList->Href[HrefList->nhrefs].referrer=Href->referrer;
  HrefList->Href[HrefList->nhrefs].hops=Href->hops;
  HrefList->Href[HrefList->nhrefs].method=Href->method;
  HrefList->Href[HrefList->nhrefs].stored=Href->stored;
  HrefList->Href[HrefList->nhrefs].site_id = Href->site_id;
  HrefList->Href[HrefList->nhrefs].server_id = Href->server_id;
  HrefList->Href[HrefList->nhrefs].rec_id = Href->rec_id;
  HrefList->Href[HrefList->nhrefs].max_doc_per_site= Href->max_doc_per_site;
  HrefList->Href[HrefList->nhrefs].collect_links= Href->collect_links;
  UdmVarListInit(&HrefList->Href[HrefList->nhrefs].Vars);
  UdmVarListReplaceLst(&HrefList->Href[HrefList->nhrefs].Vars,
                       &Href->Vars, NULL, "*");
  HrefList->nhrefs++;

  /* Sort unsorted part */
  if((HrefList->nhrefs-HrefList->shrefs)>RESORT_HREFS)
  {
    UdmSort(HrefList->Href,HrefList->nhrefs,sizeof(UDM_HREF),cmphrefs);
    /* Remember count of sorted URLs  */
    HrefList->shrefs=HrefList->nhrefs;
    /* Count of stored URLs became 0  */
    HrefList->dhrefs=0;
  }
  UDM_FREE(ehref);
  return(1);
}


extern __C_LINK void __UDMCALL UdmHrefListFree(UDM_HREFLIST * HrefList)
{
  size_t i;
  for(i=0;i<HrefList->nhrefs;i++)
    UdmHrefFree(&HrefList->Href[i]);
  UDM_FREE(HrefList->Href);
  bzero((void*)HrefList, sizeof(*HrefList));
}


__C_LINK UDM_HREFLIST * __UDMCALL UdmHrefListInit(UDM_HREFLIST * Hrefs)
{
  bzero((void*)Hrefs, sizeof(*Hrefs));
  return(Hrefs);
}
