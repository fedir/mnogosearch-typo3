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
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#include <errno.h>
#include <math.h>

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_searchtool.h"
#include "udm_boolean.h"
#include "udm_vars.h"
#include "udm_conf.h"
#include "udm_log.h"


/********** QSORT functions *******************************/


static int
cmppattern(UDM_URLDATALIST *Data, UDM_URLDATA *D,
           long j, const char *pattern)
{
  int rc;

  for(; *pattern != '\0'; pattern++)
  {
    switch(*pattern)
    {
      case 'R':
      case 'r':
        if (D->score > Data->Item[j].score) return (*pattern == 'R') ? 1 : -1;
        if (D->score < Data->Item[j].score) return (*pattern == 'R') ? -1 : 1;
        break;
      case 'P':
      case 'p':
        if (D->pop_rank > Data->Item[j].pop_rank) return (*pattern == 'P') ? 1 : -1;
        if (D->pop_rank < Data->Item[j].pop_rank) return (*pattern == 'P') ? -1 : 1;
        break;
      case 'D':
      case 'd':
        if (D->last_mod_time > Data->Item[j].last_mod_time) return (*pattern == 'D') ? 1 : -1;
        if (D->last_mod_time < Data->Item[j].last_mod_time) return (*pattern == 'D') ? -1 : 1;
        break;
      case 'U':
      case 'u':
        rc= strcmp(UDM_NULL2EMPTY(D->url), UDM_NULL2EMPTY(Data->Item[j].url));
        if (rc) return(*pattern == 'U' ? -rc : rc);
        break;
      case 'S':
      case 's':
        rc= strcmp(UDM_NULL2EMPTY(D->section),
                   UDM_NULL2EMPTY(Data->Item[j].section));
        if (rc) return(*pattern == 'S' ? -rc : rc);
        break;
    }
  }
  return 0;
}


static int
cmppatternRP(UDM_URLDATA *D1, UDM_URLDATA *D2)
{
  if (D1->score > D2->score) return -1;
  if (D1->score < D2->score) return  1;
  
  if (D1->pop_rank > D2->pop_rank) return -1;
  if (D1->pop_rank < D2->pop_rank) return 1;
  return 0;
}


static int
cmppatternR(UDM_URLDATA *D1, UDM_URLDATA *D2)
{
  if (D1->score > D2->score) return -1;
  if (D1->score < D2->score) return  1;
  return 0;
}



/****************************************************/

static int
cmpsiteid2(UDM_URLDATA *p1, UDM_URLDATA *p2)
{
  if (p1->site_id  > p2->site_id)  return -1;
  if (p1->site_id  < p2->site_id)  return 1;
#if 0
  if (p1->score    > p2->score)    return -1;
  if (p1->score    < p2->score)    return 1;
  if (p1->pop_rank > p2->pop_rank) return -1;
  if (p1->pop_rank < p2->pop_rank) return 1;
#endif
  return 0;
}


void
UdmURLDataSortBySite(UDM_URLDATALIST *L)
{
  UdmSort((void*) L->Item, L->nitems, sizeof(*L->Item), (udm_qsort_cmp) cmpsiteid2);
#if 0
  {
    size_t i;
    printf("L->num: %d num: %d\n", L->ncoords, num);
    for (i= 0; i < num; i++)
      printf("%d %d %d %d %.6f\n", i,
      L->Data[i].url_id, L->Data[i].site_id, L->Data[i].coord, L->Data[i].pop_rank);
  }
#endif
}


static void
UdmURLDataSortByPatternRP(UDM_URLDATALIST *L)
{
  UdmSort((void*) L->Item, L->nitems, sizeof(*L->Item), (udm_qsort_cmp) cmppatternRP);
}


static void
UdmURLDataSortByPatternR(UDM_URLDATALIST *L)
{
  UdmSort((void*) L->Item, L->nitems, sizeof(*L->Item), (udm_qsort_cmp) cmppatternR);
}


static size_t UdmH[] = {1, 5, 19, 41, 109, 209, 505, 929, 2161,
                        3905, 8929, 16001, 36289, 64769};
void UdmURLDataSortByPattern(UDM_URLDATALIST *L, const char *pattern)
{
  register ssize_t h, i, j;
  int s = 13;
  size_t num= L->nitems;
  UDM_URLDATA Dat;

  if (!strcmp(pattern, "RP"))
  {
    UdmURLDataSortByPatternRP(L);
    goto ret;
  }
  if (!strcmp(pattern, "R"))
  {
    UdmURLDataSortByPatternR(L);
    goto ret;
  }

  while( (s > 0) && ((num / 3) < UdmH[s])) s--;
  while(s >= 0)
  {
    h= UdmH[s];
    for (j= h; j < (ssize_t) num; j++)
    {
      Dat= L->Item[j];
      i= j - h;
D4:
      if (cmppattern(L, &Dat, i, pattern) <= 0) goto D6;
      L->Item[i + h]= L->Item[i];
      i-= h;
      if (i >= 0) goto D4;
D6:
      L->Item[i + h]= Dat;
    }
    s--;
  }

ret:

#if 0
  {
    size_t i;
    printf("L->num: %d num:%d pattern:%s\n", L->ncoords, num, pattern);
    for (i= 0; i < num; i++)
    {
      printf("[%d]%d %d %d %d %.6f\n", i,
             L->Coords[i].url_id, L->Coords[i].coord,
             L->Data[i].url_id, L->Data[i].coord, (float) L->Data[i].pop_rank);
    }
  }
#endif
  return;
}


/* TODO: Reuse the same function in sql.c */
static int
cmp_data_urls(UDM_URLDATA *d1, UDM_URLDATA *d2)
{
  if (d1->url_id > d2->url_id) return 1;
  if (d1->url_id < d2->url_id) return -1;
  return 0;
}


int UdmURLDataListSearch(UDM_URLDATALIST *List, urlid_t id)
{
  UDM_URLDATA d;
  void *found;
  if (!List->nitems)
    return 0;
  d.url_id= id;
  found= UdmBSearch(&d, List->Item, List->nitems, sizeof(UDM_URLDATA),
                    (udm_qsort_cmp) cmp_data_urls);
  return found ? 1 : 0;
}


/*
  Clear all params.
  Keep url_id untouched.
*/
int
UdmURLDataListClearParams(UDM_URLDATALIST *DataList, size_t num_best_rows)
{
  size_t i;
  for (i= 0; i < num_best_rows; i++)
  {
    UDM_URLDATA *D= &DataList->Item[i];
    D->site_id= 0;
    D->pop_rank= 0;
    D->last_mod_time= 0;
    D->url= NULL;
    D->section= NULL;
  }
  return UDM_OK;
}


/*
  Skip empty records and return the number of non-empty records
*/
size_t
UdmURLDataCompact(UDM_URLDATA *dst, UDM_URLDATA *src, size_t n)
{
  UDM_URLDATA *dst0= dst, *srcend= src + n;
  for ( ; src < srcend ; src++)
  {
    if (src->site_id)
    {
      *dst++= *src;
    }
  }
  return dst - dst0;
}
