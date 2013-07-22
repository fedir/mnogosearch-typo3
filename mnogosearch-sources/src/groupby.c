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

#include <string.h>

#include "udm_common.h"
#include "udm_log.h"
#include "udm_utils.h"
#include "udm_searchtool.h"
#include "udm_db.h"
#include "udm_hash.h"


/*******************  GroupBySite  *********************************/


void
UdmURLDataApplySiteRank(UDM_AGENT *A, UDM_URLDATALIST *DataList,
                        int is_aggregation_point)
{
  size_t i, prev_site_id, in_site_rank;
  for (i= 0, prev_site_id= in_site_rank= 1; i < DataList->nitems; i++)
  {
    UDM_URLDATA *D= &DataList->Item[i];
    if (prev_site_id != D->site_id)
    {
      in_site_rank= 0;
      prev_site_id= D->site_id;
    }
    in_site_rank++;
    if (is_aggregation_point)
    {
      if (in_site_rank > 1)
      {
        /* Keep machine number in the low byte */
        D->score= (D->score & 0xFF) + ((D->score / in_site_rank) & 0x7FFFFF00);
      }
    }
    else
      D->score/= in_site_rank;
  }
}


int
UdmURLDataListGroupBySiteUsingSort(UDM_AGENT *A,
                                   UDM_URLDATALIST *R,
                                   UDM_DB *db)
{
  udm_timer_t ticks;
  UDM_URLDATA *Dat, *End;

  /* Initialize per_site */
  for (Dat= R->Item, End= R->Item + R->nitems;
       Dat < End; Dat++)
    Dat->per_site= 1;

  UdmLog(A,UDM_LOG_DEBUG,"Start sort by site_id %d docs", (int) R->nitems);
  ticks=UdmStartTimer();
  UdmURLDataSortBySite(R);
  UdmLog(A,UDM_LOG_DEBUG,"Stop sort by site_id:\t%.2f",UdmStopTimer(&ticks));
  
  UdmLog(A,UDM_LOG_DEBUG,"Start group by site_id %d docs", (int) R->nitems);
  ticks=UdmStartTimer();
  UdmURLDataGroupBySite(R);
  UdmLog(A,UDM_LOG_DEBUG,"Stop group by site_id:\t%.2f", UdmStopTimer(&ticks));

#ifdef HAVE_SQL
  if (UDM_OK != UdmUserSiteScoreListLoadAndApplyToURLDataList(A, R, db))
    return UDM_ERROR;
#endif

  return UDM_OK;
}


static size_t
test_key(const void *ptr)
{
  size_t key= (size_t) ((const UDM_URLDATA*)ptr)->site_id;
  return key;
}


static int
test_join(UDM_URLDATA *dst, UDM_URLDATA *src)
{
  dst->per_site+= src->per_site;
  if (dst->score > src->score)
  {
    return UDM_OK;
  }
  else if (dst->score == src->score)
  {
    if (dst->pop_rank > src->pop_rank)
    {
      return UDM_OK;
    }
    else if (dst->pop_rank == src->pop_rank)
    {
      if (dst->url_id < src->url_id)
        return UDM_OK;
    }
  }
  dst->url_id=        src->url_id;
  dst->score=         src->score;
  dst->last_mod_time= src->last_mod_time;
  dst->pop_rank=      src->pop_rank;
  dst->url=           src->url;
  dst->section=       src->section;
  return UDM_OK;
}


int
UdmURLDataListGroupBySiteUsingHash(UDM_AGENT *A,
                                  UDM_URLDATALIST *DataList,
                                  const char *rec_id_str, size_t rec_id_len,
                                  const char *site_id_str, size_t site_id_len)
{
  UDM_HASH Hash;
  UDM_URLDATA *HashData, *Item= DataList->Item, D;
  size_t hcoords= UdmHashSize(DataList->nitems);
  size_t nbytes= hcoords * sizeof(UDM_URLDATA);
  size_t j, i, nbad, ncoords= DataList->nitems;
  size_t rec_id_count= rec_id_len / 4;
  udm_timer_t ticks;
  bzero(&D, sizeof(D));
  D.per_site= 1;
  HashData= (UDM_URLDATA*) UdmMalloc(nbytes);
  bzero((void*)HashData, nbytes);

  UdmHashInit(&Hash, HashData, hcoords, sizeof(UDM_URLDATA),
              test_key, (udm_hash_join) test_join);

  for (nbad= 0, j = 0, i = 0; i < rec_id_count && j < ncoords; )
  {
    D.url_id= udm_get_int4(rec_id_str + i * 4);

    /*fprintf(stderr, "%d/%d j=%d\n", D.url_id, Item[j].url_id, j);*/
 
    if (D.url_id == Item[j].url_id)
    {
      D.site_id= udm_get_int4(site_id_str + i * 4);
      D.score= Item[j].score;
      UdmHashPut(&Hash, &D);
      j++;
      i++;
    }
    else if (D.url_id > Item[j].url_id)
    {
      nbad++;
      if (nbad < 4)
      {
        UdmLog(A, UDM_LOG_DEBUG, "URL_ID=%d found in word index but not in '#rec_id' record", D.url_id);
      }
      
      /*
        url_id found in results but missing in "#rec_id"
        This is possible when one runs "indexer -Erewriteurl"
        after deleting some urls;
      */
      j++;
    }
    else
      i++;
  }


  if (j < ncoords)
  {
    UdmLog(A, UDM_LOG_ERROR, "'#rec_id' ended unexpectedly: no data for rec_id=%d", Item[j].url_id);
    nbad+= ncoords - j;
  }

  if (nbad >= 4)
    UdmLog(A, UDM_LOG_DEBUG, "GroupBySite may have worked incorrectly. "
                             "Total URL_IDs not found in '#rec_id': %d",
                             (int) nbad);

  ticks= UdmStartTimer();
  j= DataList->nitems= UdmURLDataCompact(DataList->Item, HashData, hcoords);
  UdmLog(A, UDM_LOG_DEBUG, "HashCompact %d to %d done: %.2f",
         (int) hcoords, (int) DataList->nitems, UdmStopTimer(&ticks));
  UdmFree(HashData);
  return UDM_OK;
}


void UdmURLDataGroupBySite(UDM_URLDATALIST *List)
{
  UDM_URLDATA *src= List->Item + 1;
  UDM_URLDATA *dst= List->Item;
  UDM_URLDATA *end= List->Item + List->nitems;
  uint4 count;
  
  if (!List->nitems)
    return;

  for(count= List->Item[0].per_site; src < end; src++)
  {
    /* Group by site_id */
    if(dst->site_id == src->site_id)
    {
      count+= src->per_site;
      if (dst->score > src->score)
      {
        continue;
      }
      else if (dst->score == src->score)
      {
        if (dst->pop_rank > src->pop_rank)
        {
          continue;
        }
        else if (dst->pop_rank == src->pop_rank)
        {
          if (dst->url_id < src->url_id)
            continue;
        }
      }
      dst->url_id=        src->url_id;
      dst->score=         src->score;
      dst->last_mod_time= src->last_mod_time;
      dst->pop_rank=      src->pop_rank;
      dst->url=           src->url;
      dst->section=       src->section;
    }
    else
    {
      /* Next site */
      dst->per_site= count;
      *++dst= *src;
      count= src->per_site;
    }
  }
  dst->per_site= count;
  List->nitems= dst - List->Item + 1;
  return;
}
