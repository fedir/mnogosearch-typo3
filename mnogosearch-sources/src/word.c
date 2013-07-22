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
#include "udm_utils.h"
#include "udm_unicode.h"
#include "udm_word.h"
#include "udm_searchtool.h"


#define WSIZE    1024
#define BSIZE    10

/*
  offs=0 means normal word
  offs=1 means seclen marker
*/
int
UdmWordListAddEx(UDM_WORDLIST *Words,
                 const char *word,
                 size_t secno,
                 size_t wordpos,
                 size_t offs)
{
  UDM_WORD *W;
  if (wordpos > 0x1FFFFF)
    return(UDM_OK);
#ifdef TRIAL_VER
  if (Doc->Words.nwords >= 256)
    return UDM_OK;
#else
  /* Realloc memory when required  */
  if(Words->nwords >= Words->mwords)
  {
    Words->mwords+= WSIZE;
    Words->Word= (UDM_WORD *) UdmRealloc(Words->Word, Words->mwords * sizeof(UDM_WORD));
  }
#endif
  /* Add new word */
  W= &Words->Word[Words->nwords];
  W->word= (char*) UdmStrdup(word);
  W->pos= wordpos /*+ offs*/;
  W->secno= secno;
  W->hash= 0;
  W->seclen_marker= offs;
  Words->nwords++;
  return UDM_OK;
}

/* This function adds a normalized word form(s) into list using Ispell */
int UdmWordListAdd(UDM_DOCUMENT * Doc, char *word, int secno)
{
  return UdmWordListAddEx(&Doc->Words, word, secno, ++Doc->CrossWords.wordpos[secno],0);
}


void
UdmWordListReset(UDM_WORDLIST *List)
{
  size_t i;
  for (i= 0; i < List->nwords; i++)
    UDM_FREE(List->Word[i].word);
  List->nwords= 0;
}


int UdmWordListFree(UDM_WORDLIST * List)
{
  size_t i;
  for(i=0;i<List->nwords;i++)
    UDM_FREE(List->Word[i].word);
  List->nwords=0;
  List->swords=0;
#ifndef TRIAL_VER
  UDM_FREE(List->Word);
#endif
  return(0);
}


void
UdmWordListListInit(UDM_WORDLISTLIST *WL)
{
  bzero((void*) WL, sizeof(*WL));
}


void
UdmWordListListFree(UDM_WORDLISTLIST *WL)
{
  size_t i;
  for (i= 0; i < 255; i ++)
  {
    UdmWordListFree(&WL->Item[i]);
  }
}


void
UdmWordListListReset(UDM_WORDLISTLIST *WL)
{
  size_t i;
  for (i= 0; i < 255; i ++)
  {
    UdmWordListReset(&WL->Item[i]);
  }
}

/*********************************************************/

void UdmWideWordInit(UDM_WIDEWORD *W)
{
  bzero((void*)W, sizeof(UDM_WIDEWORD));
  W->user_weight= UDM_DEFAULT_USER_WORD_WEIGHT;
}


void UdmWideWordFree(UDM_WIDEWORD *W)
{
  UDM_FREE(W->word);
}


static void
UdmWideWordCopy(UDM_WIDEWORD *Dst, UDM_WIDEWORD *Src)
{
  Dst->len=    Src->len;
  Dst->order=  Src->order;
  Dst->order_extra_width= Src->order_extra_width;
  Dst->phrpos= Src->phrpos;
  Dst->phrlen= Src->phrlen;
  Dst->count=  Src->count;
  Dst->word =  Src->word ? (char*)UdmStrdup(Src->word) : NULL;
  Dst->origin= Src->origin;
  UDM_ASSERT(Src->origin >= UDM_WORD_ORIGIN_QUERY &&
             Src->origin <= UDM_WORD_ORIGIN_COLLATION);
  Dst->match=  Src->match;
  Dst->secno=  Src->secno;
  Dst->phrwidth= Src->phrwidth;
  Dst->user_weight= Src->user_weight;
}


/*
  Replace the last three words in the list to range designator
*/
int
UdmWideWordListMakeRange(UDM_WIDEWORDLIST *WWL, int beg, int end)
{
  UDM_WIDEWORD *W= &WWL->Word[WWL->nwords - 3];
  char *word;
  size_t len;
  UDM_ASSERT(WWL->nwords >= 3);
  UDM_ASSERT(WWL->nuniq >= 3);
  len= 1 + W[0].len + 4 + W[2].len + 1;
  if (!(word= UdmMalloc(len + 1)))
    return UDM_ERROR;
  udm_snprintf(word, len + 1, "%c%s TO %s%c", beg, W[0].word, W[2].word, end);
  UdmWideWordFree(&W[0]);
  UdmWideWordFree(&W[1]);
  UdmWideWordFree(&W[2]);
  WWL->nwords-= 2;
  WWL->nuniq-= 2;
  W->word= word;
  W->len= len;
  W->match= UDM_MATCH_RANGE;
  return UDM_OK;
}



UDM_WORDLIST * UdmWordListInit(UDM_WORDLIST * List)
{
  bzero((void*)List, sizeof(*List));
  return(List);
}


UDM_WIDEWORDLIST * UdmWideWordListInit(UDM_WIDEWORDLIST * List)
{
  bzero((void*)List, sizeof(*List));
  return(List);
}

static size_t
UdmWideWordListAddInternal(UDM_WIDEWORDLIST * List,
                           UDM_WIDEWORD * Word,
                           int for_stat)
{
  size_t i;

  for (i = 0; i < List->nwords; i++)
  {
    if (List->Word[i].len == Word->len &&
        strcmp(List->Word[i].word, Word->word) == 0)
    {
      if (for_stat)
      {
        List->Word[i].count+= Word->count;
        return List->nwords;
      }
      else if (List->Word[i].order == Word->order)
      {
        List->Word[i].count+= Word->count;
        if (List->Word[i].phrpos != Word->phrpos ||
            List->Word[i].phrlen != Word->phrlen)
        {
          List->Word[i].phrpos= 0; /* No certain in-phrase position */
          List->Word[i].phrlen= 0;
        }
        switch (Word->origin)
        {
        case UDM_WORD_ORIGIN_QUERY:
          if (List->Word[i].origin == UDM_WORD_ORIGIN_STOP) break;
        case UDM_WORD_ORIGIN_STOP:
          List->Word[i].origin = Word->origin;
        default:
          break;
        }
        List->Word[i].order= Word->order;
        return List->nwords;
      }
    }
  }
  
  /* Realloc memory */
  List->Word= (UDM_WIDEWORD*)UdmRealloc(List->Word,sizeof(*(List->Word))*(List->nwords+1));
  UdmWideWordInit(&List->Word[List->nwords]);
  
  /* Copy data */
  UdmWideWordCopy(&List->Word[List->nwords], Word);
  List->nwords++;
  return(List->nwords);
}


size_t UdmWideWordListAdd(UDM_WIDEWORDLIST * List, UDM_WIDEWORD * Word)
{

  return UdmWideWordListAddInternal(List, Word, 0);
}


size_t
UdmWideWordListAddLike(UDM_WIDEWORDLIST *WWList,
                       UDM_WIDEWORD *orig, const char *word)
{
  UDM_WIDEWORD tmp= *orig;
  tmp.word= (char*) word;
  tmp.len= strlen(word);
  return UdmWideWordListAdd(WWList, &tmp);
}


size_t UdmWideWordListAddForStat(UDM_WIDEWORDLIST * List, UDM_WIDEWORD * Word)
{
  return UdmWideWordListAddInternal(List, Word, 1);
}


void UdmWideWordListFree(UDM_WIDEWORDLIST * List)
{
  size_t i;
  for(i=0;i<List->nwords;i++)
    UdmWideWordFree(&List->Word[i]);
  UDM_FREE(List->Word);
  UdmWideWordListInit(List);
}


int UdmWideWordListCopy(UDM_WIDEWORDLIST *Dst, UDM_WIDEWORDLIST *Src)
{
  size_t i;
  *Dst= *Src;
  Dst->Word= (UDM_WIDEWORD*) UdmMalloc(sizeof(*(Src->Word))*(Src->nwords));
  for (i= 0; i < Src->nwords; i++)
    UdmWideWordCopy(&Dst->Word[i], &Src->Word[i]);
  return UDM_OK;
}


static int wwcmp(const UDM_WIDEWORD *w1, const UDM_WIDEWORD *w2)
{
  int rc;
  if ((rc= strcmp(w1->word, w2->word)))
    return rc;
  return (int) w1->secno - (int) w2->secno;
}


void UdmWideWordListSort(UDM_WIDEWORDLIST *L)
{
  if (L->nwords)
    UdmSort(L->Word, L->nwords, sizeof(UDM_WIDEWORD), (udm_qsort_cmp) wwcmp);
}


static int wlcmp(UDM_WORD *w1, UDM_WORD *w2)
{
  register int _;
  if ((_= strcmp(w1->word, w2->word)))
    return _;
  return (int) w1->secno - (int) w2->secno;
}


int UdmWordListSaveSectionSize(UDM_DOCUMENT *Doc)
{
  size_t i= Doc->Words.nwords;
  int prev_sec= 0, res;
  const char *prev_word= "#non-existing";
  if (i)
    UdmSort(Doc->Words.Word, i, sizeof(UDM_WORD), (udm_qsort_cmp)wlcmp);
  while (i--)
  {
    /* 
       This assignement must be inside the loop, since Word could be
       realloced by AddOneWord
    */
    UDM_WORD *W= &Doc->Words.Word[i];
    if (W->secno != prev_sec || strcmp(W->word, prev_word))
    {
      prev_word= W->word;
      prev_sec= W->secno;
      if (UDM_OK != (res= UdmWordListAddEx(&Doc->Words, prev_word, prev_sec,
                                           Doc->CrossWords.wordpos[prev_sec] +
                                           1, 1)))
        return res;
    }
  }
  return UDM_OK;
}
