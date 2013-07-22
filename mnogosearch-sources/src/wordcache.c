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

#include "udm_common.h"
#include "udm_utils.h"

#include "udm_mutex.h"
#include "udm_db.h"
#include "udm_log.h"
#include "udm_hash.h"


/*
  WordCache - for delayed db write
*/

int UdmWordCacheFlush
(UDM_AGENT *Indexer)
{
#ifdef HAVE_SQL
  size_t i;
  UDM_GETLOCK(Indexer, UDM_LOCK_CONF);
  for (i= 0; i < Indexer->Conf->dbl.nitems; i++)
  {
    int rc= UDM_OK;
    UDM_DB *db= &Indexer->Conf->dbl.db[i];
    UDM_GETLOCK(Indexer, UDM_LOCK_DB);
    switch (db->DBMode) {
      case UDM_DBMODE_MULTI:
        rc= UdmWordCacheWrite(Indexer, db, 0);
        break;
    }
    if (rc != UDM_OK)
    {
      UdmLog(Indexer, UDM_LOG_ERROR, "%s", db->errstr);
      return rc;
    }
    UDM_RELEASELOCK(Indexer, UDM_LOCK_DB);
  }
  UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);
#endif
  return UDM_OK;
}


UDM_WORD_CACHE*
UdmWordCacheInit (UDM_WORD_CACHE *cache)
{
  if (! cache)
  {
    cache = UdmMalloc(sizeof(UDM_WORD_CACHE));
    if (! cache) return(NULL);
    cache->free = 1;
  }
  else
  {
    cache->free = 0;
  }
  cache->nbytes = sizeof(UDM_WORD_CACHE);
  cache->nwords = 0;
  cache->awords = 0;
  cache->words = NULL;
  cache->nurls = 0;
  cache->aurls = 0;
  cache->urls = NULL;

  return(cache);
}


void
UdmWordCacheFree(UDM_WORD_CACHE *cache)
{
  size_t i;

  if (! cache) return;
  for (i = 0; i < cache->nwords; i++) UDM_FREE(cache->words[i].word);
  UDM_FREE(cache->words);
  UDM_FREE(cache->urls);
  cache->nbytes = sizeof(UDM_WORD_CACHE);
  cache->nwords = 0;
  cache->awords = 0;
  cache->nurls = 0;
  cache->aurls = 0;

  if (cache->free) UdmFree(cache);
}


static int
wccmpwrd(UDM_WORD_CACHE_WORD *s1, UDM_WORD_CACHE_WORD *s2)
{
  register int _;
  if ((_= s1->seed - s2->seed))
    return _;
  if ((_= s1->secno - s2->secno))
    return _;
  if (s1->url_id > s2->url_id)
    return 1;
  if (s1->url_id < s2->url_id)
    return -1;
  if ((_= strcmp(s1->word, s2->word)))
    return _;
  return s1->pos - s2->pos;
}


void
UdmWordCacheSort(UDM_WORD_CACHE *cache)
{
  UdmSort(cache->words, cache->nwords, sizeof(UDM_WORD_CACHE_WORD), (udm_qsort_cmp)wccmpwrd);
}


int UdmWordCacheAdd
(UDM_WORD_CACHE *cache, urlid_t url_id, UDM_WORD *W)
{
  if (!W->word) return(UDM_OK);

  if (cache->nwords == cache->awords)
  {
    UDM_WORD_CACHE_WORD *tmp;
    tmp= UdmRealloc(cache->words, (cache->awords + 256) * sizeof(UDM_WORD_CACHE_WORD));
    if (!tmp)
    {
      fprintf(stderr, "Realloc failed while adding word\n");
      return(UDM_ERROR);
    }
    cache->words= tmp;
    cache->awords+= 256;
    cache->nbytes+= sizeof(UDM_WORD_CACHE_WORD) * 256;
  }

  if (!(cache->words[cache->nwords].word= UdmStrdup(W->word)))
    return(UDM_ERROR);
  cache->words[cache->nwords].url_id= url_id;
  cache->words[cache->nwords].secno= W->secno & 0xFF;
  cache->words[cache->nwords].pos= W->pos & 0x1FFFFF;
  cache->words[cache->nwords].seed= UdmStrHash32(W->word) & MULTI_DICTS;
  cache->nwords++;
  cache->nbytes+= strlen(W->word) + 1;
  return(UDM_OK);
}


int
UdmWordCacheAddURL(UDM_WORD_CACHE *cache, urlid_t url_id)
{
  if (cache->nurls == cache->aurls)
  {
    urlid_t *tmp;
    tmp = UdmRealloc(cache->urls, (cache->aurls + 256) * sizeof(urlid_t));
    if (!tmp)
    {
      fprintf(stderr, "Realloc failed while adding word\n");
      return(UDM_ERROR);
    }
    cache->urls = tmp;
    cache->aurls += 256;
    cache->nbytes += sizeof(urlid_t) * 256;
  }

  cache->urls[cache->nurls] = url_id;
  cache->nurls++;
  return(UDM_OK);
}


int
UdmWordCacheAddWordList(UDM_WORD_CACHE *WordCache,
                        UDM_WORDLIST *WordList,
                        urlid_t url_id)
{
  size_t i;
  for (i= 0; i < WordList->nwords; i++)
  {
    if (WordList->Word[i].secno)
      UdmWordCacheAdd(WordCache, url_id, &WordList->Word[i]);
  }
  return UDM_OK;
}

