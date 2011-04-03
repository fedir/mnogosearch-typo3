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


#include <string.h>

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_searchtool.h"


/*
  A qsort-alike function to compare two URL_IDs
*/
int
UdmCmpURLID(urlid_t *s1, urlid_t *s2)
{
  if (*s1 > *s2) return 1;
  if (*s1 < *s2) return -1;
  return 0;
}


/* Sort URL ID list in ID order */
int
UdmURLIdListSort(UDM_URLID_LIST *List)
{
  if (List->nurls > 1)
  {
    UdmSort(List->urls, List->nurls, sizeof(urlid_t), (udm_qsort_cmp) UdmCmpURLID);
  }
  return UDM_OK;
}


/* merging two non-empty limits together */
int
UdmURLIdListJoin(UDM_URLID_LIST *urls, UDM_URLID_LIST *fl_urls)
{
  size_t src, dst;
  int bfound= fl_urls->exclude ? 0 : 1;
  int bnotfound= fl_urls->exclude ? 1 : 0;
  for (dst= 0, src= 0; src < urls->nurls; src++)
  {
    int allow= UdmBSearch(&urls->urls[src],
                          fl_urls->urls,
                          fl_urls->nurls, sizeof(urlid_t),
                          (udm_qsort_cmp) UdmCmpURLID) ? bfound : bnotfound;
    if (allow)
    {
      urls->urls[dst]= urls->urls[src];
      dst++;
    }
  }
  urls->nurls= dst;
  if (!urls->nurls)
    urls->empty= 1;
  return UDM_OK;
}


int
UdmURLIdListUnion(UDM_URLID_LIST *a, UDM_URLID_LIST *b)
{
  if (b->nurls)
  {
    size_t nbytes= sizeof(urlid_t) * (a->nurls + b->nurls);
    a->urls= (urlid_t*) UdmRealloc(a->urls, nbytes);
    memcpy(a->urls + a->nurls, b->urls, b->nurls * sizeof(urlid_t));
    a->nurls+= b->nurls;
    UdmSort(a->urls, a->nurls, sizeof(urlid_t), (udm_qsort_cmp) UdmCmpURLID);
  }
  return UDM_OK;
}


int
UdmURLIdListCopy(UDM_URLID_LIST *a, UDM_URLID_LIST *b)
{
  size_t nbytes= sizeof(urlid_t) * b->nurls;
  a->urls= (urlid_t*) UdmRealloc(a->urls, nbytes);
  a->nurls= b->nurls;
  a->exclude= b->exclude;
  memcpy(a->urls, b->urls, nbytes);
  return UDM_OK;
}


/*
  Merge two limits (some of them can be empty).
  The second limit is kept untouched.
*/
int
UdmURLIdListMerge(UDM_URLID_LIST *a, UDM_URLID_LIST *b)
{
  int rc;
  if (a->exclude && b->exclude)
  {
    rc= UdmURLIdListUnion(a, b);
  }
  if (a->nurls && b->nurls)
  {
    rc= UdmURLIdListJoin(a, b);
  }
  else if (!a->nurls && b->nurls)
  {
    rc= UdmURLIdListCopy(a, b);
  }
  else if (!b->nurls)
  {
    if (!b->exclude)
      a->empty= 1;
  }
  return rc;
}
