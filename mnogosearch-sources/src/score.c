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
#include "udm_db.h" /* For UdmBlobPackedCoordsToUnpackedCoords */

/*
  Performance statistics for various math operation (per 1 second):
  Float numbers used in experiments: x= 123.456, y= 456.789.
  Integer number used in experiments: x= 123, y= 456.
  Compiler flags: no optimization, no debug info.
  
  Pentium Duo CPU E4500 2.20GHz:  
  Operations/Sec    Operation
  --------------    ---------
     240,000,000    float*float
     240,000,000    float+float
     240,000,000    float-float
      35,000,000    float/float
      18,000,000    sqrt
      12,000,000    log

     240,000,000    int*int
     240,000,000    int+int
     180,000,000    int/int
     180,000,000    int-int
     
           5,200    bzero(1MB block)
           1,200    memcpy(1Mb block)
     
  Pentium Duo CPU 6400 2.13GHz:
  Operations/Sec    Operation
  --------------    ---------
     400,000,000    float*float
     400,000,000    float+float
     400,000,000    float-float
      57,000,000    float/float
      33,000,000    sqrt
      22,000,000    log

     400,000,000    int*int
     240,000,000    int+int
     360,000,000    int/int
     300,000,000    int-int
     
       9,000,000    { udm_timer_t t= UdmStartTimer(); }
*/

/********** QSORT functions *******************************/

static int
cmp_pos(UDM_COORD2 *s1, UDM_COORD2 *s2)
{
  return (int) s1->pos - (int) s2->pos;
}


static int
cmp_score(UDM_URL_SCORE *c1, UDM_URL_SCORE *c2)
{
  return ((int4) c2->score) - ((int4) c1->score);
}


static int
cmp_score_then_url_id(UDM_URL_SCORE *c1, UDM_URL_SCORE *c2)
{
  int diff= ((int4) c2->score) - ((int4) c1->score);
  if (diff)
    return diff;
  return c1->url_id - c2->url_id;
}




typedef struct
{
  size_t   acoords;
  size_t   ncoords;
  UDM_COORD2  *Coords;
  udm_secno_t secno;
  UDM_SECTION *Src;
  size_t  nsections;
} UDM_SECTIONLIST2; /* TODO: perhaps join with UDM_SECTIONLIST */


static inline void
UdmSectionList2Realloc(UDM_SECTIONLIST2 *List, size_t ncoords)
{
  if (ncoords >= List->acoords)
  {
    size_t nbytes;
    nbytes= (List->acoords= (ncoords + 1024)) * sizeof(UDM_COORD2);
    List->Coords= (UDM_COORD2*) UdmRealloc(List->Coords, nbytes);
  }
}


#define UDM_SEC_TO_CRD(Coord, ord, Crd, count) \
{ \
  (Coord)->order= (ord); \
  (Coord)->pos= (Crd)->pos; \
  (Crd)++; \
  (count)--; \
  (Coord)++; \
}


static size_t
UdmMergeCoordsN2(UDM_SECTIONLIST2 *MergedSection,
                 UDM_COORD2 *dst, UDM_SECTION *S1, UDM_SECTION *S2)
{
  UDM_COORD2 *src1= S1->Coord;
  UDM_COORD2 *src2= S2->Coord;
  size_t n1= S1->ncoords;
  size_t n2= S2->ncoords;
  udm_wordnum_t order1= S1->order;
  udm_wordnum_t order2= S2->order;
  udm_wordnum_t order;
  size_t n, total= n1 + n2;
  UDM_COORD2 *src;
  
  MergedSection->Coords= dst;
  MergedSection->secno= S1->secno;
  
  if (!n1 || !n2)
  {
    total= 0;
    goto end;
  }
  
  for ( ; ; )
  {
    if (src1->pos < src2->pos)
    {
      UDM_SEC_TO_CRD(dst, order1, src1, n1);
      /* Skip all coords from src1, except the last one */
      for (src= src1; n1 && src1->pos < src2->pos; n1--, src1++, total--);
      if (src < src1)
      {
        n1++;
        src1--;
        total++;
        UDM_SEC_TO_CRD(dst, order1, src1, n1);
      }
      if (!n1)
      {
        n= n2;
        src= src2;
        order= order2;
        goto single;
      }
    }
    else
    {
      UDM_SEC_TO_CRD(dst, order2, src2, n2);
      /* Skip all coords from src1, except the last one */
      for (src= src2; n2 && src2->pos < src1->pos; n2--, src2++, total--);
      if (src < src2)
      {
        n2++;
        src2--;
        total++;
        UDM_SEC_TO_CRD(dst, order2, src2, n2);
      }
      if (!n2)
      {
        n= n1;
        src= src1;
        order= order1;
        goto single;
      }
    }
  }
  
single:

/*
  for ( ; n ; )
  {
    UDM_SEC_TO_CRD(dst, S, src, n);
  }
  return total;
*/

  /*
    Skip middle coords - they affect neither score, nor phrase.
    Only the first and the last coords are of interest.
  */
  if (n)
  {
    /* Put first coord, for WordDistance */
    UDM_SEC_TO_CRD(dst, order, src, n);
    
    if (n)
    {
      /* Put last coord, for MinMaxFactor */
      src+= n - 1;
      UDM_SEC_TO_CRD(dst, order, src, n);
    }
    total-= n;
  }
end:
  return (MergedSection->ncoords= total);
}


static size_t
UdmMergeCoordsUsingSort(UDM_COORD2 *dst, UDM_SECTION *S, size_t srecs)
{
  size_t num;
  UDM_COORD2 *dst0= dst;
  for (num= 0 ;srecs; srecs--, S++)
  {
    UDM_COORD2 *src= S->Coord;
    size_t ncoords= S->ncoords;
  
    for (num+= ncoords ; ncoords; )
    {
      UDM_SEC_TO_CRD(dst, S->order, src, ncoords);
    }
  }
  if (num > 1)
    UdmSort((void*) dst0, num, sizeof(UDM_COORD2), (udm_qsort_cmp) cmp_pos);
  return num;
}


#define MAX_NUM_FOR_MERGE 10
static size_t
UdmMergeCoordsMany(UDM_SECTIONLIST2 *MergedSection,
                   UDM_COORD2 *dst, UDM_SECTION *S, size_t srecs)
{
  UDM_COORD2 *p[MAX_NUM_FOR_MERGE];
  UDM_COORD2 *e[MAX_NUM_FOR_MERGE];
  UDM_SECTION *s[MAX_NUM_FOR_MERGE];
  size_t num, list, nlists;

  MergedSection->Coords= dst;
  MergedSection->secno= S->secno;

  if (srecs >= MAX_NUM_FOR_MERGE)
    return (MergedSection->ncoords= UdmMergeCoordsUsingSort(dst, S, srecs));

  for (num= 0, nlists=0, list= 0; list < srecs; list++)
  {
    size_t ncoords= S[list].ncoords;
    if (ncoords)
    {
      num+= ncoords;
      p[nlists]= S[list].Coord;
      e[nlists]= S[list].Coord + ncoords;
      s[nlists]= &S[list];
      nlists++;
    }
  }

  if (!nlists)
    return (MergedSection->ncoords= 0);

  if (nlists == 1)
    goto single;

  for ( ; ; )
  {
    size_t i, min= 0;
    urlid_t p_min_pos= p[0]->pos;
    for (i= 1; i < nlists; i++)
    {
      if (p[i]->pos < p_min_pos)
      {
        min= i;
        p_min_pos= p[i]->pos;
      }
    }
    
    /**dst++= *p[min]++;*/
    dst->order= s[min]->order;
    dst->pos= p[min]->pos; 
    dst++;
    p[min]++;
    
    if (p[min] == e[min])
    {
      nlists--;
      p[min]= p[nlists];
      e[min]= e[nlists];
      s[min]= s[nlists];
      if (nlists == 1)
        break;
    }
  }

single:
  {
    size_t n= *e - *p;
    for ( ; n ; )
    {
      UDM_SEC_TO_CRD(dst, (*s)->order, *p, n);
    }
  }
  return (MergedSection->ncoords= num);
}


#ifdef HAVE_SQL
/*
  Unpack coordinates and merge into UDM_CRD
*/
static size_t
UdmMergeCoordsPacked(UDM_SECTIONLIST2 *MergedSection,
                     UDM_COORD2 *dst, UDM_SECTION *Section, size_t nsections)
{
  size_t i;
  
  UDM_ASSERT(nsections > 0);
  
  MergedSection->Coords= dst;
  MergedSection->secno= Section->secno;
  
  for (i= 0; i < nsections; i++)
  {
    UDM_SECTION *S= &Section[i];
    const unsigned char *s;
    if ((s= S->PackedCoord)) /* If not unpacked yet */
    {
      UDM_COORD2 C;
      /*
        TODO: Make sure we don't go beyond allocated memory in "e":
      */
      const unsigned char *e= s + S->ncoords * 4 + 1, *end;
      C.pos= 0;
      C.order= S->order;
      UdmBlobPackedCoordsToUnpackedCoords(s, e, S->ncoords,
                                          &C, S->Coord, &end);
      S->PackedCoord= NULL; /* Mark as unpacked */
    }
  }

  /* Mark section as unpacked */
  MergedSection->nsections= 0;
  MergedSection->Src= 0;

  return UdmMergeCoordsMany(MergedSection, dst, Section, nsections);
}
#endif


/* Find topcount best results */

#define UDM_URL_SCORE_IS_SMALLER(x,y) \
 (x->score < y->score ||(x->score == y->score && x->url_id >= y->url_id))


static inline UDM_URL_SCORE *
UdmURLScoreListSortByScoreThenURLTopFindPos(UDM_URL_SCORE *First,
                                            UDM_URL_SCORE *Right,
                                            UDM_URL_SCORE *Curr)
{
  UDM_URL_SCORE *Left;
  for(Left= First; Left < Right; )
  {
    UDM_URL_SCORE *Middle= Left + (Right - Left) / 2;
    if (UDM_URL_SCORE_IS_SMALLER(Curr, Middle))
      Left= Middle + 1;
    else
      Right= Middle;
  }
  return Right;
}


void
UdmURLScoreListSortByScoreThenURLTop(UDM_URLSCORELIST *ScoreList,
                                     size_t topcount)
{
  UDM_URL_SCORE *First= ScoreList->Item;
  UDM_URL_SCORE *Last= ScoreList->Item + ScoreList->nitems;
  UDM_URL_SCORE *TopCount= ScoreList->Item + topcount;
  UDM_URL_SCORE *Curr;

  UDM_ASSERT(topcount < ScoreList->nitems);
/*
  for (Curr= First; Curr < Last; Curr++)
    fprintf(stderr, "%d:%d\n", Curr->score, Curr->url_id);
*/  
  UdmSort((void*) ScoreList->Item, topcount + 1,
          sizeof(UDM_URL_SCORE), (udm_qsort_cmp) cmp_score_then_url_id);

  for (Curr= TopCount; Curr < Last; Curr++)
  {
    UDM_URL_SCORE tmp, *Right;
    
    if (UDM_URL_SCORE_IS_SMALLER(Curr, TopCount))
        continue;

    /*
      The Curr item is bigger than the TopCount item.
      Find its sorted place, and insert into the "topcount" list.
      Put the old TopCount item instead of the Curr item.
    */
    Right= UdmURLScoreListSortByScoreThenURLTopFindPos(First, TopCount, Curr);
    tmp= *TopCount;
    memmove(Right + 1, Right, (TopCount-Right)*sizeof(UDM_URL_SCORE));
    *Right= *Curr;
    *Curr= tmp;
  }
}


void UdmURLScoreListSortByScore(UDM_URLSCORELIST *ScoreList)
{
  UdmSort((void*) ScoreList->Item, ScoreList->nitems,
          sizeof(UDM_URL_SCORE), (udm_qsort_cmp) cmp_score);
}


void UdmURLScoreListSortByScoreThenURL(UDM_URLSCORELIST *ScoreList)
{
  UdmSort((void*) ScoreList->Item, ScoreList->nitems,
          sizeof(UDM_URL_SCORE), (udm_qsort_cmp) cmp_score_then_url_id);
}


#define NUMWORD_FACTOR_SIZE 256
#define NUMWORD_FACTOR(nwf, n) (nwf[n >= NUMWORD_FACTOR_SIZE ? NUMWORD_FACTOR_SIZE - 1 : n])

typedef struct udm_score_param_st
{
  unsigned int *R;
  unsigned int *D;
  unsigned int *Dsum_add;
  unsigned int *RDsum_add;
  size_t D_size;
  size_t ncosine;
  size_t nsections;
  float Rsum_factor;
  size_t dst_offs;
  size_t nwf_offs;
  size_t nwf_num;
  unsigned int dst_weight;
  UDM_WIDEWORDLIST WWList;
  int search_mode; /* ALL/ANY */
  char *count;
  size_t count_size;
  UDM_STACKITEMLIST ItemList;
  char wf[256];
  char wf2[256];
  char nwf[256];
  float numword_factor[NUMWORD_FACTOR_SIZE];
  float numdistinctword_factor[NUMWORD_FACTOR_SIZE];
  float max_coord_factor;
  unsigned int MinCoordFactor;
  unsigned int NumDistinctWordFactor;
  int have_WordFormFactor;
  float WordFormFactor;
  float WordFormFactorReminder;
  int SaveSectionSize;
  float WordDensityFactor;
  float WordDensityFactorReminder;
  float SkipWordDistanceThreshold;
  unsigned int IDFFactor;
  urlid_t debug_url_id;
  udm_secno_t min_secno; /* In all result set */
  udm_secno_t max_secno; /* In all result set */
  unsigned int SingleWordDistance;
  UDM_ENV *Env;
} UDM_SCORE_PARAM;


typedef struct udm_density_param_item_st
{
  uint4 seclen;
  uint4 seccnt;
  udm_secno_t secno;
} UDM_DENSITY_PARAM_ITEM;


typedef struct udm_density_param_st
{
  size_t nitems;
  UDM_DENSITY_PARAM_ITEM Item[256];
  uint4 max_word_weight;
} UDM_DENSITY_PARAM;


typedef struct udm_section_breakdown_st
{
  size_t RDsum;
  size_t Dsum;
  size_t Dsum_nodistance;
} UDM_COSINE_STAT;


typedef struct udm_score_parts_st
{
  UDM_COSINE_STAT Cosine;
  UDM_WORD_DISTANCE_STAT distance;
  float min_max_factor;
  float density_factor;
  float numword_factor;
  float wordform_factor;
  float numdistinctword_factor;
  float cosine_factor;
  float cosine_factor_max;
  float cosine_factor_distance_threshold;
  UDM_DENSITY_PARAM density_param;
  size_t ncoords_for_malloc;
  udm_secno_t max_secno; /* In document */
  UDM_SECTIONLIST2 MergedSectionLists[256];
  size_t nuniqsections;
  urlid_t url_id;
} UDM_SCORE_PARTS;



/********** Density utils **************/
#if 1
#define MixDensityLengthAndWeight(x,y)  ((x)*(y))
#else
#define MixDensityLengthAndWeight(x,y)  ((x))
#endif


static inline void
UdmDensityParamReset(UDM_DENSITY_PARAM *density_param)
{
  density_param->nitems= 0;
}


static inline void
UdmDensityParamItemInit(UDM_DENSITY_PARAM *prm, udm_secno_t secno,
                        UDM_DENSITY_PARAM_ITEM *item,
                        UDM_SECTION *S1, UDM_WIDEWORD *W1)
{
  item->seclen= MixDensityLengthAndWeight(S1->seclen, prm->max_word_weight);
  item->secno= secno;
  item->seccnt= W1 ? MixDensityLengthAndWeight(S1->ncoords, W1->weight) : 0; 
}


static inline void
UdmDensityParamItemAddWord(UDM_DENSITY_PARAM_ITEM *item,
                           UDM_SECTION *S1, UDM_WIDEWORD *W1)
{
  if (S1->seclen)
    item->seccnt+= MixDensityLengthAndWeight(S1->ncoords, W1->weight);
}


static inline void
UdmDensityParamAdd(UDM_DENSITY_PARAM *prm, udm_secno_t secno,
                   UDM_SECTION *S1, UDM_WIDEWORD *W1)
{
  if (S1->seclen)
  {
    UDM_DENSITY_PARAM_ITEM *item= &prm->Item[prm->nitems];
    UdmDensityParamItemInit(prm, secno, item, S1, W1);
    prm->nitems++;
  }
}


static inline void
UdmDensityParamAdd2(UDM_DENSITY_PARAM *prm, udm_secno_t secno,
                    UDM_SECTION *S1, UDM_WIDEWORD *W1,
                    UDM_SECTION *S2, UDM_WIDEWORD *W2)
{
  if (S1->seclen)
  {
    UDM_DENSITY_PARAM_ITEM *item= &prm->Item[prm->nitems];
    UdmDensityParamItemInit(prm, secno, item, S1, W1);
    UdmDensityParamItemAddWord(item, S2, W2);
    prm->nitems++;
  }
}


static inline void
UdmDensityParamAddItem(UDM_DENSITY_PARAM *prm, UDM_DENSITY_PARAM_ITEM *item)
{
  UDM_DENSITY_PARAM_ITEM *dst= &prm->Item[prm->nitems];
  *dst= *item;
  prm->nitems++;
}


static uint4
UdmCalcDensity(UDM_SCORE_PARTS *score_parts, UDM_SCORE_PARAM *param)
{
  uint4 i, wfsum;
  for (wfsum= 0, i= 0; i < score_parts->density_param.nitems; i++)
  {
    UDM_DENSITY_PARAM_ITEM *Item= &score_parts->density_param.Item[i];
    if (Item->seclen)
    {
      float add= (float) param->wf2[Item->secno] * Item->seccnt / Item->seclen;
      wfsum+= param->wf2[Item->secno];
      score_parts->density_factor+= add;
    }
  }
  return wfsum;
}


/*
  TODO: avoid calculation of WordDistance in some cases.
  
  As far as we use "0" as "model" word distance,
  - Rsum does not depend on whether we use distance in cosine
  - RDsum does not depend on whether we use distance in cosine
  - Only Dsum depends
  
  Dsum_nodistance= Sum{vector consisting of word-section coordinates}
  Dsum_distance= Dsum_nodistance + dst*dst
  
  Cosine_nodistance=  RDsum / sqrt(Rsum) / sqrt(Dsum_nodistance)
  Cosine_distance=    RDsum / sqrt(Rsum) / sqrt(Dsum_nodistance + dst*dst)
  
  Task: we know Cosine_nodistance and dst.
  What is Cosine_distance value?
  
  Cosine_distance= Cosine_nodistance * sqrt(Dsum_nodistance) / sqrt(Dsum_nodistance + dst*dst)
  Coside_distance= Cosine_nodistance * sqrt(Dsum_nodistance/(Dsum_nodistance + dst*dst))
  
     sqrt(     Dsum_nodistance     )
  K=      ------------------------- 
     sqrt(Dsum_nodistance + dst*dst)
  
  Approximate algorithm:

  1. Calculate score_nodistance: i.e. score without knowing word distance,
    but including WordFormFactor, Density, NumDistinctWord, NWF, MinMax.
  
  2. Sort results by score_nodistance
  
  3. Loop through highest score_nodistance to lowest score_distance
     calculating distance and converting score_nodistance to real score.
  
  4. Stop calculating distance as soon as score_nodistance is low enough.
  
*/

static float
UdmWithDistanceToWithoutDistanceFactor(UDM_COSINE_STAT *Cosine)
{
  return Cosine->Dsum ? sqrt((float) Cosine->Dsum_nodistance / Cosine->Dsum) :
                        1;
}


/****************************************/


static void
UdmDebugScore(char *str, size_t nbytes,
              UDM_SCORE_PARAM *score_param,
              UDM_SCORE_PARTS *score_parts,
              uint4 score)
{
  char numdistinctword_factor_str[64]= "";
  size_t dstsum= score_parts->distance.num ?
                 score_parts->distance.sum *
                 score_param->dst_weight / score_parts->distance.num / 255 :
                 0;
  if (score_param->search_mode != UDM_MODE_ALL)
    udm_snprintf(numdistinctword_factor_str, sizeof(numdistinctword_factor_str),
                 "distinctword=%.8f ", score_parts->numdistinctword_factor);
  udm_snprintf(str, nbytes,
               "url_id=%d cos=%.4f (Dsum_nodst=%d Dsum=%d RDsum=%d) distance=%.4f (%d=%d/%d) "
               "minmax=%.4f density=%.4f "
               "numword=%.4f %swordform=%.4f score=%d",
               score_parts->url_id, score_parts->cosine_factor,
               (int) score_parts->Cosine.Dsum_nodistance,
               (int) score_parts->Cosine.Dsum,
               (int) score_parts->Cosine.RDsum,
               UdmWithDistanceToWithoutDistanceFactor(&score_parts->Cosine),
               (int) dstsum,
               (int) score_parts->distance.sum,
               (int) score_parts->distance.num,
               score_parts->min_max_factor, score_parts->density_factor,
               score_parts->numword_factor, numdistinctword_factor_str,
               score_parts->wordform_factor,(int) score);
}


/*
  R[i] and D[i] are in the range 0..64.
  ns is between 1..256
*/


#define MAXCOORD_FACTOR(factor, max_coord) ((1-((float)factor*(float)(max_coord > 0xFFFF ? 0xFFFF :  max_coord))))
#define MINCOORD_FACTOR(factor, coord) ((float) 0x1000 / (float) (0x1000 + factor * ((coord > 0xFFFF ? 0xFFFF : coord) - 1)))


static inline float
UdmMinMaxCoordFactor(UDM_SCORE_PARAM *score_param,
                     udm_pos_t min_pos, udm_pos_t max_pos)
{
  float x;
  x= MAXCOORD_FACTOR(score_param->max_coord_factor, max_pos) *
     MINCOORD_FACTOR(score_param->MinCoordFactor, min_pos);
  /*
  fprintf(stderr, "[%d] secno=%d min=%d max=%d x=%.8f\n",
                  B->url_id, (int) B->secno, (int) B->pos, (int) E->pos, x);
  */
  return x;
}




/*
 Normalize word distance values.
 If word distance is longer than 64,
 then consider this word distance as just 64.
 Perhaps it could be some logarithmic function instead.
*/


#define UDM_WORD_RDSUM_FACTOR        16
#define UDM_WORD_ORIGIN_QUERY_WEIGHT (6*UDM_WORD_RDSUM_FACTOR/2)
#define UDM_WORD_ORIGIN_SPELL_WEIGHT (6*UDM_WORD_RDSUM_FACTOR/2)


static inline uint4
UdmWordCosineWeight(uint4 sec_weight, size_t word_weight)
{
  return sec_weight * UDM_WORD_RDSUM_FACTOR + word_weight;
}


static int UdmOriginWeight(int origin)
{
  switch(origin)
  {
    case UDM_WORD_ORIGIN_QUERY:          return UDM_WORD_ORIGIN_QUERY_WEIGHT;
    case UDM_WORD_ORIGIN_SPELL:          return UDM_WORD_ORIGIN_SPELL_WEIGHT;
    case UDM_WORD_ORIGIN_SYNONYM:        return UDM_WORD_ORIGIN_SPELL_WEIGHT;
    case UDM_WORD_ORIGIN_SYNONYM_FINAL:  return UDM_WORD_ORIGIN_SPELL_WEIGHT;
    case UDM_WORD_ORIGIN_STOP:           return 0;
    case UDM_WORD_ORIGIN_SUGGEST:        return 0;
    case UDM_WORD_ORIGIN_COLLATION:      return 0;
  }
  UDM_ASSERT(0); /* Should not reach this point */
  return 0;
}


/*
  Merge coords and calculate word distance
  
  Performance statistics:
  Total: 0.08 (32% GroupByURL time)
*/
static void
UdmCalcScoreWordDistance(UDM_SCORE_PARAM *param, UDM_SCORE_PARTS *score_parts)
{
#ifdef HAVE_SQL
  if (new_version)
  {
    /*
      Now to unpack and merge coords,
      if they have not been merged before.
    */
    size_t i;
    for (i= 0; i < score_parts->nuniqsections; i++)
    {
      UDM_SECTIONLIST2 *MergedSection= &score_parts->MergedSectionLists[i];
      /*
      fprintf(stderr, "url_id=%d sec=%d ptr=%p nsections=%d\n",
              url_id, MergedSection->secno,
              MergedSection->Src,
              MergedSection->nsections);
      */
      if (MergedSection->Src)
        UdmMergeCoordsPacked(MergedSection, MergedSection->Coords,
                             MergedSection->Src,
                             MergedSection->nsections);
    }
  }
#endif

  /*
    Calculate WordDistance and MinMaxFactor
    As this is a heavy step - try to postpone it as late as possible.
  */
  {
    size_t i;
    score_parts->distance.sum= 0;
    score_parts->distance.num= 0;
    for (i= 0; i < score_parts->nuniqsections; i++)
    {
      UDM_SECTIONLIST2 *MergedSection= &score_parts->MergedSectionLists[i];
      CalcAverageWordDistance(param->wf2[MergedSection->secno],
                              MergedSection->Coords,
                              MergedSection->ncoords,
                              param->WWList.nuniq,
                              &score_parts->distance);
      if (MergedSection->ncoords)
      {
        UDM_COORD2 *Coords= MergedSection->Coords;
        score_parts->min_max_factor*= UdmMinMaxCoordFactor(param,
                                                           Coords->pos,
                                                           Coords[MergedSection->ncoords-1].pos);
      }
      else
      {
        /*
          Only a single unique word presents in a section
        */
        if (param->SingleWordDistance)
        {
          score_parts->distance.sum+= param->SingleWordDistance * param->wf2[MergedSection->secno];
          score_parts->distance.num++;
        }
      }
    }
  }
}

#define OFFS_FROM_NUNIQ_SECNO_ORDER(n,s,o) ((n)*(s)+(o))

static void
UdmDebugCosine(UDM_SCORE_PARAM *score_param,
               char *added, size_t max_added_offs)
{
  size_t offs;
  UDM_DSTR d;
  UdmDSTRInit(&d, 1024);
  for (offs= 0; offs <= max_added_offs; offs++)
  {
    if (added[offs])
    {
      UdmDSTRAppendf(&d, "(s%dw%d:%d:%d)",
                     (int) (offs / score_param->WWList.nuniq),
                     (int) (offs % score_param->WWList.nuniq),
                     score_param->Dsum_add[offs],
                     score_param->RDsum_add[offs]);
    }
    UdmVarListReplaceStr(&score_param->Env->Vars, "DebugCosine", d.data);
  }
  UdmDSTRFree(&d);
}


/*
  Collect section breakdown data to
  score_parts->Dsum and score_parts->RDsum.
  Returns the maximum offset used in the "added" array,
  for later clean-up.
*/
static void
UdmCalcCosineSectionBreakDown(UDM_SCORE_PARAM *score_param,
                              UDM_SECTION *Section, size_t nsections,
                              UDM_SCORE_PARTS *score_parts)
{
  size_t score_param_nuniq= score_param->WWList.nuniq;
  size_t Dsum= 0, RDsum= 0, max_added_offs= 0;
  char *added= (char*) score_param->D;

  for( ; nsections; Section++, nsections--)
  {
    size_t offs= OFFS_FROM_NUNIQ_SECNO_ORDER(score_param_nuniq, Section->secno, Section->order);

    if (!added[offs])
    {
      Dsum+= score_param->Dsum_add[offs];
      RDsum+= score_param->RDsum_add[offs];
      added[offs]= 1;
      if (offs > max_added_offs)
        max_added_offs= offs;
    }
  }
  score_parts->Cosine.RDsum= RDsum;
  score_parts->Cosine.Dsum= Dsum;
  score_parts->Cosine.Dsum_nodistance= Dsum;

  if (score_param->debug_url_id == score_parts->url_id)
  {
    UdmDebugCosine(score_param, added, max_added_offs);
  }

  /* Clear for the next call */
  bzero(added, max_added_offs + 1);
}


/*
  Add word distance statistics
  into score_parts->Cosine.
*/
static void
UdmCosineInfoAddDistance(UDM_SCORE_PARAM *score_param,
                         UDM_SCORE_PARTS *score_parts)
{
  float no_distance_cosine_factor= score_param->Rsum_factor *
                             sqrt(score_parts->Cosine.RDsum);
  /*
  fprintf(stderr, "no_distance_cosine=%.2f max=%.2f trh=%.2f\n",
          no_distance_cosine_factor,
          score_parts->cosine_factor_max,
          score_parts->cosine_factor_distance_threshold);
  */
  if (no_distance_cosine_factor <= score_parts->cosine_factor_distance_threshold)
  {
    /*fprintf(stderr, "skip\n");*/
    return;
  }

  
  /*fprintf(stderr, "calc\n");*/
  UdmCalcScoreWordDistance(score_param, score_parts);
  if (score_parts->distance.num)
  {
    size_t dstnum= score_parts->distance.num;
    size_t dstsum= UDM_WORD_RDSUM_FACTOR * score_parts->distance.sum * score_param->dst_weight / dstnum / 255;
    score_parts->Cosine.Dsum+= UDM_SQR(dstsum);
    /*
      Note, R[dst_offs] should actually be 0.
      We use 0 as "model" word distance.
      So the next line can probaly be removed
      unless we want to use other number as
      the "model" word distance.
    */
    score_parts->Cosine.RDsum+=
      score_param->R[score_param->dst_offs] * dstsum;
  }
}


/*
  Calculate section breakdown and
  mix with word distance.
*/
static void
UdmCalcCosineWeightManyCoords(UDM_SCORE_PARAM *score_param,
                              UDM_SECTION *Section, size_t nsections,
                              char *wf2,
                              UDM_SCORE_PARTS *score_parts)
{
  UdmCalcCosineSectionBreakDown(score_param, Section, nsections, score_parts);

  if (score_param->WWList.nwords != 1)
    UdmCosineInfoAddDistance(score_param, score_parts);


  score_parts->cosine_factor= score_param->Rsum_factor * 
                              (float) score_parts->Cosine.RDsum /
                                      sqrt(score_parts->Cosine.Dsum);  
  
  
  if (score_parts->cosine_factor_max < score_parts->cosine_factor)
  {
    score_parts->cosine_factor_max= score_parts->cosine_factor;
    score_parts->cosine_factor_distance_threshold= 
      score_parts->cosine_factor *
      score_param->SkipWordDistanceThreshold;
  }
}


static void UdmWideWordListSetOriginWeight(UDM_WIDEWORDLIST *WWList)
{
  size_t i;
  for (i=0; i < WWList->nwords; i++)
    WWList->Word[i].weight= UdmOriginWeight(WWList->Word[i].origin);
}


#if 0
static void
UdmPrintCoords(UDM_COORD2 *Coords, size_t ncoords)
{
  size_t i;
  for (i=0 ; i < ncoords; i++)
    fprintf(stderr, "[%d]secno=%d pos=%d seclen=%d num=%d\n", i,
                    Coords[i].secno,
                    Coords[i].pos,
                    Coords[i].seclen,
                    Coords[i].num);
}
#endif


static int
CheckOnePhrase(UDM_WIDEWORD *Word,
               UDM_STACK_ITEM *CmdBeg, UDM_STACK_ITEM *CmdEnd,
               UDM_COORD2 *CoordBeg, UDM_COORD2 *CoordEnd,
               int cmd_arg, size_t seclen)
{
  UDM_STACK_ITEM *CmdCur;
  UDM_COORD2 *CoordFirst;
  
  for (CoordFirst= CoordBeg; CoordFirst < CoordEnd; CoordFirst++)
  {
    size_t delta;
    UDM_COORD2 *CoordPrev, *CoordCurr;

    /* Skip words with wrong order */
    if (CmdBeg->arg != CoordFirst->order)
      continue;

    CoordPrev= CoordFirst;
    CoordCurr= CoordPrev + 1;
    delta= 1;

    for (CmdCur= CmdBeg + 1; CmdCur < CmdEnd; CmdCur++)
    {
      if (CmdCur->cmd == UDM_STACK_STOP)
      {
        delta++;
        continue;
      }

      /* find coord for this word */
      while (CoordCurr < CoordEnd &&
             (CoordPrev->pos == CoordCurr->pos ||
              (CoordPrev->pos + delta == CoordCurr->pos &&
               CoordCurr->order != CmdCur->arg)))
        CoordCurr++;

      if (CoordCurr == CoordEnd ||
          CoordPrev->pos != CoordCurr->pos - delta ||
          CmdCur->arg != CoordCurr->order)
        break;

      delta= 1;
      CoordPrev= CoordCurr;
    }
    
    if (CmdCur == CmdEnd)
    {
      if (cmd_arg == '=' && CmdEnd - CmdBeg + 1 != seclen)
        continue;
      
      return 1; /* Phrase found */
    }
  }
  return 0;
}
                          
static inline void
CheckPhrase(UDM_WIDEWORD *Word,
            UDM_STACK_ITEM *query, size_t nitems,
            UDM_COORD2 *coords, size_t ncoords,
            char *count, size_t seclen)
{
  size_t q;
  UDM_COORD2 *CoordEnd= coords + ncoords;

  /* find opening phrase command */
  for (q= 0; q < nitems; q++)
  {
    size_t i;
    size_t start, end, arg;
    size_t rstart, rend;
    int cmd_arg;
    if (query[q].cmd != UDM_STACK_PHRASE) continue;
    cmd_arg= query[q].arg;
    
    /* find closing phrase command */
    start= q + 1;
    for (end= start; end < nitems && query[end].cmd != UDM_STACK_PHRASE; end++);
    q= end;
    arg= 0;

    /* skip trailing stopwords for now */
    /* TODO: we have to check document length (for phrases like "word1 stopword1") */
    for (rstart= start; rstart < end && query[rstart].cmd == UDM_STACK_STOP; rstart++);
    for (rend= end; rend > rstart && query[rend].cmd == UDM_STACK_STOP; rend--);

    /* if phrase contains stopwords only, we assume this document is found */
    if (rstart == rend) arg= 1;
    else
      arg= CheckOnePhrase(Word, &query[rstart], &query[rend],
                          coords, CoordEnd, cmd_arg, seclen);

    /*
      count[] was previously filled in the loop
      in UdmGroupByURL2. Set count to 0 for those
      words which must be in a phrase, but they
      are not in a phrase.
      TODO: Call CheckPhrase per-section.
    */
    for (i= rstart; i < rend; i++)
    {
      if (query[i].cmd == UDM_STACK_WORD)
        count[query[i].arg]|= arg;
    }
  }
}

/*
  TODO: implement tf*idf as follows:
  w_ik = tf_ik * idf_k

  where

  T_k = term k in document D_i
  tf_ik = frequency of term T_k in document D_i
  idf_k = inversed document frequency of term T_k in the collection C:

  idf_k = log( N / n_k)

  N = total number of documents in the collection C (i.e. in the result)
  n_k = the number of documents in C that contain T_k

  QQ: how to combine several words and sections?
  QQ: "number of documents in C" should probably mean
      "number of documents in the current search result" in our case.
*/



static
void UdmNumWordFactorInit(float k, float *numword_factor, size_t ncoords)
{
  size_t i;
  if (k > 1) k= 1;
  if (k < 0) k= 0;
  for (i= 0; i < NUMWORD_FACTOR_SIZE; i++)
  {
    numword_factor[i]= (1-k) + ((float) i / ncoords)*k;
  }
}


#define UDM_MIX_FACTOR(Factor, newfactor, Reminder, oldfactor) \
((Factor) * (newfactor) + (Reminder) * (oldfactor))


static inline void
UdmApplyNWF(UDM_SCORE_PARAM *param,
            UDM_SECTION *Section, size_t nsections, float *nword_factor)
{
  size_t i;
  unsigned char secno_min= 255;
  unsigned char secno_max= 0;
  char section_vector[256];
  UDM_SECTION *Tmp;
  
  bzero((void*) &section_vector, param->nwf_num);
  for (Tmp= Section, i= nsections; i > 0; i--, Tmp++)
  {
    unsigned char secno= Tmp->secno;
    section_vector[secno]= 1;
    if (secno_min > secno)
      secno_min= secno;
    if (secno_max < secno)
      secno_max= secno;
  }
  if (secno_min == secno_max && param->nwf[secno_min])
    *nword_factor*= (1 -  (float) param->nwf[secno_min] / 16);
}


static inline void
UdmApplyNumDistinctWordFactor(UDM_SCORE_PARAM *param, char *count,
                              float *nword_factor,
                              UDM_SCORE_PARTS *score_parts)
{
  size_t z, nuniq;
  for (nuniq=0, z= 0; z < param->WWList.nuniq; z++)
  {
    if (count[z])
      nuniq++;
  }
  if (nuniq < param->WWList.nuniq)
  {
    score_parts->numdistinctword_factor= param->numdistinctword_factor[nuniq];
    *nword_factor*= param->numdistinctword_factor[nuniq];
  }
  else
  {
    score_parts->numdistinctword_factor= 1;
  }
}


static inline void
CalcWordFormFactor(UDM_SCORE_PARAM *param,
                   UDM_SECTION *S, size_t nsections,
                   UDM_SCORE_PARTS *score_parts)
{
  size_t form_phr_n, ncoords;
  static int spell_factor[5]= {0, 2, 1, 1, 1};

  for (form_phr_n= 0, ncoords= 0; nsections; S++, nsections--)
  {
    int origin= param->WWList.Word[S->wordnum].origin;
    UDM_ASSERT(origin < 6);
    form_phr_n+= S->ncoords * spell_factor[origin];
    ncoords+= S->ncoords;
  }
  score_parts->wordform_factor=
     param->WordFormFactor +
     param->WordFormFactorReminder * (float) form_phr_n / ncoords / 2;
}




static inline uint4
UdmCalcScore(UDM_SCORE_PARAM *param,
             UDM_SECTION *Section, size_t nsections,
             UDM_SCORE_PARTS *score_parts)
{
  uint4 score;
  float nword_factor;

  score_parts->numword_factor= NUMWORD_FACTOR(param->numword_factor,
                                              score_parts->ncoords_for_malloc);
  
  nword_factor= score_parts->numword_factor;

  if (param->have_WordFormFactor)
  {
    CalcWordFormFactor(param, Section, nsections, score_parts);
    nword_factor*= score_parts->wordform_factor;
  }


  /* Calulate Density */
  {
    uint4 wfsum= UdmCalcDensity(score_parts, param);
    if (wfsum)
    {
      score_parts->density_factor= (score_parts->density_factor > wfsum) ?
                                   1 : score_parts->density_factor / wfsum;
      /* Apply word density */
      nword_factor= UDM_MIX_FACTOR(param->WordDensityFactor,
                                   score_parts->density_factor,
                                   param->WordDensityFactorReminder,
                                   nword_factor);
    }
  }
  
  
  if (param->search_mode != UDM_MODE_ALL)
  {
    UdmApplyNumDistinctWordFactor(param, param->count, &nword_factor, score_parts);
  }

  if (param->nwf_num)
  {
    UdmApplyNWF(param, Section, nsections, &nword_factor);
  }

  /*
    TODO: perhaps we can pass {MergedSectionLists, nuniqsections} here
  */
  UdmCalcCosineWeightManyCoords(param, Section, nsections,
                                param->wf2, score_parts);

  score= 0.5 +
  (score_parts->cosine_factor * nword_factor * score_parts->min_max_factor);

/*
fprintf(stderr, "RDsum=%d Dsum=%d dst=%d score=%.8f\n",
        score_parts->RDsum, score_parts->Dsum,
        score_parts->dstsum / score_parts->dstnum,
        score_parts->cosine_factor);
*/

  if (!score)
    score++;

  return score;
}


/*
  Check if the boolean expression is simple enough
  and can be optimized the same way as "m=all" can.
*/
static int
UdmOptimizeUsingAND(UDM_STACK_ITEM *Item, size_t nitems, int search_mode)
{
  if (search_mode == UDM_MODE_ALL)
    return 1;
  if (search_mode == UDM_MODE_BOOL)
  {
    UDM_STACK_ITEM *Last;
    for (Last= Item + nitems; Item < Last; Item++)
    {
      if (Item->cmd != UDM_STACK_PHRASE &&
          Item->cmd != UDM_STACK_AND &&
          Item->cmd != UDM_STACK_WORD)
        return 0;
    }
    return 1;
  }
  return 0;
}




static void
UdmApplyIDFToWWList(UDM_AGENT *A, UDM_WIDEWORDLIST *WWList,
                    size_t order, size_t doccount, float k)
{
  size_t i;
  for (i= 0; i < WWList->nwords; i++)
  {
    UDM_WIDEWORD *W= &WWList->Word[i];
    if (W->order == order)
    {
      size_t old_w= W->weight;
      W->doccount= doccount;
      W->weight= (int) (k * W->weight + 0.5);
      if (W->weight > 8192) /* Avoid overflow in cosine */
        W->weight= 8192;
      UdmLog(A, UDM_LOG_DEBUG,
             "Weight[%d]: doccount=%d factor=%.2f old=%d new=%d '%s'",
             (int) i, (int) W->doccount, k, (int) old_w, W->weight, W->word);
    }
  }
}


static size_t
UdmWWListMaxWordWeight(UDM_WIDEWORDLIST *WWL)
{
  size_t i, max_word_weight;
  for (i= 0, max_word_weight= 0; i < WWL->nwords; i++)
  {
    UDM_WIDEWORD *W= &WWL->Word[i];
    if (max_word_weight < W->weight)
      max_word_weight= W->weight;
  }
  return max_word_weight;
}


static void
UdmSectionListMinMaxSecno(UDM_SCORE_PARAM *param,
                          UDM_SECTIONLIST *SectionList)
{
  UDM_SECTION *S= SectionList->Section;
  UDM_SECTION *E= SectionList->Section + SectionList->nsections;
  udm_secno_t min_secno= 255; /* Can't use S->secno because S may be NULL */
  udm_secno_t max_secno= 0;
  
  for ( ; S < E; S++)
  {
    if (min_secno > S->secno)
      min_secno= S->secno;
    if (max_secno < S->secno)
      max_secno= S->secno;
  }
  param->min_secno= min_secno;
  param->max_secno= max_secno;
}


/*
  Calc individual word weights according to 
  user supplied "importance" query syntax:
  "importance10:word1 importance20:word2"
*/
static void
UdmCalcUserWordWeight(UDM_AGENT *A, UDM_SCORE_PARAM *param)
{
  size_t i;
  UDM_WIDEWORDLIST *WWL= &param->WWList;
  for (i= 0; i < WWL->nwords; i++)
  {
    UDM_WIDEWORD *W= &WWL->Word[i];
    if (W->user_weight != UDM_DEFAULT_USER_WORD_WEIGHT)
    {
      float factor= (float) W->user_weight / UDM_DEFAULT_USER_WORD_WEIGHT;
      int new_weight= (int) (factor * W->weight);
      UdmLog(A, UDM_LOG_DEBUG,
             "Weight[%d]: importance=%d factor=%.2f old=%d new=%d '%s'",
             (int) i, W->user_weight, factor, W->weight, new_weight, W->word);
      W->weight= new_weight;
    }
  }
}


/*
  Calculate the number of unique documents,
  and the number of every word appearance.
  
  SectionList must be sorted by url_id.
*/
static size_t
UdmSectionListCalcNDocsAndWordStatistics(UDM_SECTIONLIST *SectionList,
                                         size_t *order, size_t order_size)
{
  UDM_SECTION *S;
  UDM_SECTION *B= SectionList->Section;
  UDM_SECTION *E= SectionList->Section + SectionList->nsections;
  /*
  urlid_t min_id= B->url_id;
  urlid_t max_id= B->url_id;
  */
  urlid_t prev_id= 0;
  size_t ndocs= 0;
  uint4 word_mask= 0; /* TODO: change to uint64 or a special bitmask type */

  UDM_ASSERT(SectionList->nsections);    /* Must be non-empty */
  UDM_ASSERT(B->url_id <= E[-1].url_id); /* Must be sorted    */

  bzero(order, sizeof(*order) * order_size);
  
  for (S= SectionList->Section; S < E; S++)
  {
    uint4 this_word_mask= (1 << S->order);
    if (prev_id != S->url_id)
    {
      prev_id= S->url_id;
      ndocs++;
      word_mask= 0;
    }
    /*
    if (min_id > S->url_id)
      min_id= S->url_id;
    if (max_id < S->url_id)
      max_id= S->url_id;
    */
    if (!(word_mask & this_word_mask))
    {
      order[S->order]++;
      word_mask|= this_word_mask;
    }
  }
  return ndocs;
  /* return max_id + 1 - min_id; */
}


static void
UdmCalcIDF(UDM_AGENT *A, UDM_SCORE_PARAM *param, UDM_SECTIONLIST *SectionList)
{
  int UseNewIDF= UdmVarListFindBool(&A->Conf->Vars, "UseNewIDF", 1);
  size_t count[UDM_MAXWORDPERQUERY], i;
  size_t nsec= param->max_secno - param->min_secno;
  size_t ndocs= UdmSectionListCalcNDocsAndWordStatistics(SectionList, count,
                                                         UDM_MAXWORDPERQUERY);
  size_t ndocsec= ndocs * nsec;
  size_t divisor= UseNewIDF ? 64 : 64;
  size_t multiplier= UseNewIDF ? ndocs : ndocsec;
  float k[UDM_MAXWORDPERQUERY], k_min= 1;
  
  /*
    There was a bug that "ndocsec" instead of "ndocs" was passed to log(),
    therefore log() IDF for oftern words was never smaller than 10% of
    the rariest word. Using "UseNewIDF" for the old version compatibility.
  */
  UdmLog(A, UDM_LOG_DEBUG,
         "max_secno=%d min_secno=%d ndocs=%d",
         param->max_secno, param->min_secno, (int) ndocs);
  
  for (i= 0; i < param->WWList.nuniq; i++)
  {
    if (count[i] != 0 && count[i] < multiplier)
    {
      k[i]= 1 + log((float) multiplier / count[i]) * param->IDFFactor / divisor;
    }
    else
    {
      k[i]= 1;
    }
    if (i == 0 || k_min > k[i])
      k_min= k[i];
  }
  
  for (i= 0; i < param->WWList.nuniq; i++)
  {
    float k_normalized= UseNewIDF ? k [i] / k_min : k[i];
    UdmApplyIDFToWWList(A, &param->WWList, i, count[i], k_normalized);
  }
}


static void
UdmInitWordMask(uint4 *mask, UDM_WIDEWORDLIST *WWL)
{
  size_t i;
  bzero((void*) mask, WWL->nwords * sizeof(uint4));
  for (i= 0; i < WWL->nwords; i++)
  {
    size_t bit;
    UDM_WIDEWORD *W= &WWL->Word[i];
    mask[W->order]= (1 << W->order);
    for (bit= 1; bit < W->order_extra_width; bit++)
      mask[W->order] |= (1 << (W->order + bit));
    /*
    fprintf(stderr, "[%d] '%s' width=%d mask=%d\n",
           i, W->word, W->order_width, mask[i]);
    */
  }
}


/*
  Minimum allowed count of unuque words for a search mode
*/
static size_t
UdmCountThreshold(int mode, size_t nuniq)
{
  switch (mode)
  {
    case UDM_MODE_ALL_MINUS: return nuniq - 1;
    case UDM_MODE_ALL_MINUS_HALF: return 1 + (size_t) (nuniq - 1) / 2;
    default: break;
  }
  return 0;
}


#define UpdateCount(count, W, limit)  {if((W)->phrlen < (limit)) (count)[(W)->order]= 1;}


static void
UdmRareWordCountInit(UDM_AGENT *A, UDM_WIDEWORDLIST *WWL, size_t *count)
{
  size_t i, RareWordNumDocs= UdmVarListFindInt(&A->Conf->Vars, "RareWordNumDocs", 0);
  
  bzero(count, UDM_MAXWORDPERQUERY * sizeof(*count));
  for (i= 0; i < WWL->nwords; i++)
  {
    UDM_WIDEWORD *W= &WWL->Word[i];
    if (RareWordNumDocs && W->doccount < RareWordNumDocs)
    {
      if (!count[W->order])
      {
        count[W->order]= 1;
        UdmLog(A, UDM_LOG_DEBUG,
               "Rare word %d count=%d", (int) W->order, (int) W->doccount);
      }
    }
  }
}


/*
  Calculate score for all UDM_SECTION records for the same section.
  Return the number of coordinates merged.
*/
static size_t
UdmCalcUniqSection(UDM_SCORE_PARAM *param,
                   UDM_SCORE_PARTS *score_parts,
                   UDM_SECTION *S1, size_t srecs,
                   UDM_COORD2 *TmpCrd,
                   udm_secno_t usec,
                   UDM_WIDEWORD *Res_WWList_Word,
                   size_t update_count_phrlen_limit)
{
  size_t nmergedcoords= 0;
  UDM_SECTIONLIST2 *MergedCoordsCur= &score_parts->MergedSectionLists[usec];
  udm_secno_t secno= S1->secno;

  if (score_parts->max_secno < secno)
    score_parts->max_secno= secno;

  if (srecs == 1)
  {
    UDM_WIDEWORD *W1= &Res_WWList_Word[S1[0].wordnum];
    UpdateCount(param->count, W1, update_count_phrlen_limit);
    UdmDensityParamAdd(&score_parts->density_param, secno, S1, W1);
    score_parts->min_max_factor*= UdmMinMaxCoordFactor(param, S1->minpos, S1->maxpos);
    MergedCoordsCur->Coords= TmpCrd;
    MergedCoordsCur->ncoords= 0;
    MergedCoordsCur->secno= secno;
    MergedCoordsCur->Src= NULL;
    return 0;
  }
  else if (srecs == 2)
  {
    UDM_SECTION *S2= S1 + 1;
    UDM_WIDEWORD *W1= &Res_WWList_Word[S1->wordnum];
    UDM_WIDEWORD *W2= &Res_WWList_Word[S2->wordnum];

    UpdateCount(param->count, W1, update_count_phrlen_limit);
    UpdateCount(param->count, W2, update_count_phrlen_limit);
    UdmDensityParamAdd2(&score_parts->density_param, secno, S1, W1, S2, W2);

    if (!new_version)
      nmergedcoords= UdmMergeCoordsN2(MergedCoordsCur, TmpCrd, S1, S2);
  }
  else
  {
    size_t rec;
    UDM_SECTION *S2;/* We don't touch S1 here, it's passed to CheckPhrase */
    UDM_DENSITY_PARAM_ITEM item;
    
    UdmDensityParamItemInit(&score_parts->density_param, secno, &item, S1, NULL);

    for (rec= 0, S2= S1 ; rec < srecs; rec++, S2++)
    {
      UDM_WIDEWORD *W1= &Res_WWList_Word[S2->wordnum];
      /*
        If word is not in a phrase, mark it as found.
        Words in phrase will be marked as found later in CheckPhrase()
      */
      UpdateCount(param->count, W1, update_count_phrlen_limit);
      UdmDensityParamItemAddWord(&item, S2, W1);
    }
    UdmDensityParamAddItem(&score_parts->density_param, &item);

    if (!new_version)
      nmergedcoords= UdmMergeCoordsMany(MergedCoordsCur, TmpCrd, S1, srecs);
  }
  
  if (param->search_mode == UDM_MODE_BOOL)
  {
#ifdef HAVE_SQL
    if (new_version)
      nmergedcoords= UdmMergeCoordsPacked(MergedCoordsCur, TmpCrd, S1, srecs);
#endif
    if (nmergedcoords > 1)
      CheckPhrase(param->WWList.Word,
                  param->ItemList.items, param->ItemList.nitems,
                  TmpCrd, nmergedcoords,
                  param->count, S1->seclen);
  }

  if (new_version)
  {
    size_t i;
    MergedCoordsCur->Src= S1;
    MergedCoordsCur->Coords= TmpCrd + nmergedcoords;
    MergedCoordsCur->nsections= srecs;
    for (nmergedcoords= 0, i=0; i < srecs; i++)
      nmergedcoords+= S1[i].ncoords;
  }
  return nmergedcoords;    
}


static void
UdmGroupByURLLoop2(UDM_AGENT *A,
                   UDM_RESULT *Res,
                   UDM_SECTIONLIST *SectionList,
                   UDM_URLSCORELIST *ScoreList,
                   UDM_SCORE_PARAM *param)
{
  UDM_SECTION *From= SectionList->Section;
  UDM_SECTION *Last= SectionList->Section + SectionList->nsections;
  UDM_SECTION *End;
  UDM_SECTIONLIST2 MergedSectionList;
  UDM_URL_SCORE *CrdTo= ScoreList->Item;
  UDM_WIDEWORD *Res_WWList_Word= param->WWList.Word;
  char *count= param->count;
  size_t count_size= param->count_size;
  int search_mode= param->search_mode;
  int check_count_all= (search_mode == UDM_MODE_ALL && param->WWList.nuniq > 30);
  int check_count_all_minus= (search_mode == UDM_MODE_ALL_MINUS ||
                              search_mode == UDM_MODE_ALL_MINUS_HALF);
  int check_count_threshold= UdmCountThreshold(search_mode, param->WWList.nuniq);
  int optimize_using_all, optimize_using_minmax_pos2;
  urlid_t debug_url_id= param->debug_url_id;
  uint4 expected_and_word_mask= (1UL << param->WWList.nuniq) - 1;
  UDM_SCORE_PARTS score_parts;
  uint4 *word_order_mask;
  size_t update_count_phrlen_limit= (search_mode == UDM_MODE_BOOL) ?
                                     2 : UDM_MAXWORDPERQUERY;
  size_t rare_word_extra_count[UDM_MAXWORDPERQUERY];
  
  UdmRareWordCountInit(A, &param->WWList, rare_word_extra_count);
  bzero((void*) &MergedSectionList, sizeof(MergedSectionList));
  bzero((void*) &score_parts, sizeof(score_parts));
  if (!(word_order_mask= (uint4*) UdmMalloc(sizeof(uint4) * param->WWList.nwords)))
    goto end;
  UdmInitWordMask(word_order_mask, &param->WWList);
  score_parts.density_param.max_word_weight= UdmWWListMaxWordWeight(&param->WWList);

  optimize_using_all= UdmOptimizeUsingAND(param->ItemList.items,
                                       param->ItemList.nitems,
                                       search_mode);
  if (search_mode != UDM_MODE_ALL && optimize_using_all)
    UdmLog(A, UDM_LOG_DEBUG, "Using 'all words' optimization");

  if ((optimize_using_minmax_pos2= optimize_using_all &&
                                   param->WWList.nuniq == 2 &&
                                   Res_WWList_Word[1].phrpos > 0))
    UdmLog(A, UDM_LOG_DEBUG, "Using 'MinMaxPos2' phrase optimization");
  
  {
    char *added= (char*) param->D;
    bzero((void*) added, param->ncosine);
  }
  
  for (; From < Last ; From= End)
  {
    UDM_COORD2 *TmpCrd;
    size_t nsections;
    udm_secno_t prev_secno;
    UDM_SECTION *UniqSec[256];
    UDM_SECTION *DocLast;
    size_t usec;

    score_parts.url_id= From->url_id;

    /* Find the last section from this document */
    {
      uint4 word_mask= 0;
      for (End= From, nsections= 0;
           End < Last && End->url_id == score_parts.url_id;
           End++, nsections++)
      {
        word_mask|= word_order_mask[End->order];
      }
      /* Quickly skip documents not having all words when m=all */
      if (optimize_using_all && word_mask != expected_and_word_mask)
        continue;
      /* Quickly skip documents not having phrase */
      if (optimize_using_minmax_pos2 && nsections == 2)
      {
        if (From->minpos > From[1].maxpos + 1 ||
            From[1].minpos > From->maxpos + 1)
          continue; 
      }
      DocLast= End;
    }
    /* Total: 0.02   Self: 0.02  (8%) */
    
    score_parts.max_secno= 0;
    score_parts.ncoords_for_malloc= 0;
    score_parts.min_max_factor= 1;
    score_parts.density_factor= 0;
    score_parts.nuniqsections= 0;

    bzero((void*) count, count_size);
    UdmDensityParamReset(&score_parts.density_param);

    /* Fill UniqSec array */
    UniqSec[0]= From;
    prev_secno= From->secno + 1;
    for (End= From, MergedSectionList.ncoords= 0; End < DocLast; End++)
    {
      score_parts.ncoords_for_malloc+= End->ncoords;
      if (prev_secno != End->secno)
      {
        UniqSec[score_parts.nuniqsections++]= End;
        prev_secno= End->secno;
      }
    }
    UniqSec[score_parts.nuniqsections]= End;

    UdmSectionList2Realloc(&MergedSectionList, score_parts.ncoords_for_malloc);

    /* Total: 0.03   Self: 0.01  (4%) */

    for (TmpCrd= MergedSectionList.Coords, usec= 0;
         usec < score_parts.nuniqsections;
         usec++)
    {
      size_t srecs= UniqSec[usec+1] - UniqSec[usec];
      UDM_SECTION *S1= UniqSec[usec];
      size_t nmergedcoords= UdmCalcUniqSection(param, &score_parts,
                                               S1, srecs, TmpCrd, usec,
                                               Res_WWList_Word,
                                               update_count_phrlen_limit);
      TmpCrd+= nmergedcoords;
      MergedSectionList.ncoords+= nmergedcoords;
    }

    /* Total: 0.14    Self: 0.11  (44%) */

    if (search_mode == UDM_MODE_BOOL)
    {
      if(!UdmCalcBoolItems(param->ItemList.items, param->ItemList.nitems, count))
        continue;
    }

    if (check_count_all)
    {
      size_t z;
      for (z = 0; z < param->WWList.nuniq; z++)
      {
        if (count[z] == 0) break;
      }
      if (z < param->WWList.nuniq && count[z] == 0)
        continue;
    }
    else if (check_count_all_minus)
    {
      size_t z, tmp;
      for (z= tmp= 0; z < param->WWList.nuniq; z++)
      {
        if (count[z])
        {
          tmp++;
          if (rare_word_extra_count[z])
          {
            /*
            fprintf(stderr, "HERE[%d]: extra=%d urlid=%d\n",
                    z, rare_word_extra_count[z], score_parts.url_id);
            */
            tmp+= rare_word_extra_count[z];
          }
        }
      }
      if (tmp < check_count_threshold)
        continue;
    }
    /* Total: 0.14    Self: 0.00 */
    
    CrdTo->score= UdmCalcScore(param, From, nsections, &score_parts);
    CrdTo->url_id= score_parts.url_id;
    
    /* Total: 0.25    Self: 0.11  (44%) */
    if (debug_url_id == score_parts.url_id)
    {
      char str[255];
      UdmDebugScore(str, sizeof(str), param, &score_parts, CrdTo->score);
      UdmVarListAddStr(&A->Conf->Vars, "DebugScore", str);
    }

    CrdTo++;
    
  }
end:
  UDM_FREE(word_order_mask);
  UDM_FREE(MergedSectionList.Coords);
  ScoreList->nitems= CrdTo - ScoreList->Item;
}


static void
UdmGroupByURLLoop_OneWord_All2(UDM_AGENT *A,
                               UDM_RESULT *Res,
                               UDM_SECTIONLIST *SectionList,
                               UDM_URLSCORELIST *ScoreList,
                               UDM_SCORE_PARAM *score_param)
{
  UDM_URL_SCORE *CrdTo= ScoreList->Item;
  UDM_SECTION *From;
  UDM_SECTION *Last= SectionList->Section + SectionList->nsections;
  char *wf2= score_param->wf2;
  char *nwf= score_param->nwf;
  size_t nwf_num= score_param->nwf_num;
  float *numword_factor= score_param->numword_factor;
  float Rsum_factor= score_param->Rsum_factor;
  char *added= (char*) score_param->D;
  size_t ncosine= score_param->ncosine;
  size_t weight= score_param->WWList.Word[0].weight;
  int have_WordFormFactor= score_param->have_WordFormFactor &&
                           Res->WWList.nwords > 1;
  float WordFormFactor= score_param->WordFormFactor;
  float WordFormFactorReminder= score_param->WordFormFactorReminder;
  /*
  TODO:
  float numword_factor_1= NUMWORD_FACTOR(numword_factor, 1);
  float Rsum_factor_mul_nword_factor_1= Rsum_factor * numword_factor_1;
  */
  unsigned int *R= score_param->R;
  unsigned int phr_n2= 0;
  unsigned int form_phr_n= 0;
  UDM_SCORE_PARTS score_parts;
  
  bzero((void*) added, ncosine);
  bzero((void*) &score_parts, sizeof(score_parts));
  
  for (From= SectionList->Section ; From < Last ; )
  {
    uint4 min_pos= From->minpos;
    uint4 max_pos= From->maxpos;
    size_t ncoords= 0;
    uint4 wfsum= 0;
    unsigned char prev_secno= From->secno;
    unsigned char min_secno= 255;
    
    score_parts.Cosine.Dsum= 0;
    score_parts.Cosine.RDsum= 0;
    score_parts.Cosine.Dsum_nodistance= 0;
    score_parts.min_max_factor= 1;
    score_parts.density_factor= 0;
    score_parts.max_secno= 0;
    score_parts.url_id= From->url_id;
    
    /*
    DONE: size_t RDsum;
    DONE: size_t Dsum;
    UDM_WORD_DISTANCE_STAT distance;
    DONE float min_max_factor;
    DONE float density_factor;
    float numword_factor;
    float wordform_factor;
    float numdistinctword_factor;
    float cosine_factor;
    float cosine_factor_max;
    float cosine_factor_distance_threshold;
    UDM_DENSITY_PARAM density_param;
    size_t ncoords_for_malloc;
    DONE: udm_secno_t max_secno;
    UDM_SECTIONLIST2 MergedSectionLists[256];
    size_t nuniqsections;
    DONE urlid_t url_id;
    */
    
    /* TODO: one coord, for nsections==1 and ncoords=1 */
    
    for (; ;)
    {
      unsigned char secno= From->secno;
      size_t order= score_param->WWList.Word[From->wordnum].order;
      size_t offs= OFFS_FROM_NUNIQ_SECNO_ORDER(score_param->WWList.nuniq,
                                               secno, order);
      ncoords+= From->ncoords;
      
      if (prev_secno != secno)
      {
        score_parts.min_max_factor*= UdmMinMaxCoordFactor(score_param, min_pos, max_pos);
        min_pos= From->minpos;
        max_pos= From->maxpos;
        prev_secno= secno;
      }
      else
      {
        if (score_parts.max_secno < secno)
          score_parts.max_secno= secno;
        if (min_secno > secno)
          min_secno= secno;
        if (min_pos > From->minpos)
          min_pos= From->minpos;
        if (max_pos < From->maxpos)
          max_pos= From->maxpos;
      }
      
      if (From->seclen)
      {
        float add= (float) wf2[secno] * From->ncoords / From->seclen;
        wfsum+= wf2[secno];
        score_parts.density_factor+= add;
      }
      
      if (!added[offs])
      {
        uint4 add= UdmWordCosineWeight(wf2[secno], weight);
        score_parts.Cosine.Dsum+= UDM_SQR(add);
        score_parts.Cosine.RDsum+= add * R[offs];
        added[secno]= 1;
        if (score_parts.max_secno < secno)
          score_parts.max_secno= secno;
      }
      
      if (have_WordFormFactor)
      {
        static int spell_factor_for_one[7]= {0, 2, 1, 1, 1, 1, 0};
        size_t wordnum= From->wordnum;
        int origin= wordnum < Res->WWList.nwords ? 
                    Res->WWList.Word[wordnum].origin : 1;
        UDM_ASSERT(origin < 6);
        UDM_ASSERT(From->Coord != NULL);
        form_phr_n+= From->ncoords * spell_factor_for_one[origin];
        phr_n2+= From->ncoords * 2;
      }
      
      From++;
      
      if (From >= Last || From->url_id != score_parts.url_id)
      {
        float nword_factor;
        score_parts.min_max_factor*= UdmMinMaxCoordFactor(score_param, min_pos, max_pos);
        score_parts.density_factor= (score_parts.density_factor > wfsum) ? 
                                     1 : score_parts.density_factor / wfsum;
        score_parts.numword_factor= nword_factor=
          NUMWORD_FACTOR(numword_factor, ncoords);

        /* Applying WordFormFactor*/
        if (have_WordFormFactor)
        {
          float k;
          k= WordFormFactor + WordFormFactorReminder * (float) form_phr_n / phr_n2;
          nword_factor*= k;
          phr_n2= 0;
          form_phr_n= 0;
        }
        
        if (wfsum)
          nword_factor= UDM_MIX_FACTOR(score_param->WordDensityFactor,
                                       score_parts.density_factor,
                                       score_param->WordDensityFactorReminder,
                                       nword_factor);
        /* Applying NWF */
        if (nwf_num && min_secno == score_parts.max_secno && nwf[min_secno])
          nword_factor*= (1 -  (float) nwf[min_secno] / 16);
        
        CrdTo->score= Rsum_factor * nword_factor * score_parts.min_max_factor *
                      (float) score_parts.Cosine.RDsum / 
                      sqrt(score_parts.Cosine.Dsum) + 0.5;
        CrdTo->url_id= score_parts.url_id;

        if (score_param->debug_url_id == score_parts.url_id)
        {
          char str[255];
          UdmDebugScore(str, sizeof(str), score_param, &score_parts, CrdTo->score);
          UdmVarListAddStr(&A->Conf->Vars, "DebugScore", str);
        }

        CrdTo++;
        
        bzero((void*)added, (size_t) score_parts.max_secno * score_param->WWList.nuniq + 1);

        break;
      }
    }
  }
  ScoreList->nitems= CrdTo - ScoreList->Item;
}


static void
UdmScoreParamInit(UDM_SCORE_PARAM *prm,
                  UDM_AGENT *query,
                  UDM_DB *db,
                  UDM_RESULT *Res,
                  UDM_SECTIONLIST *SectionList)
{
  size_t i;
  float DefWordDensityFactor;
  UDM_VARLIST *Vars= &query->Conf->Vars;
  bzero((void*) prm, sizeof(UDM_SCORE_PARAM));
  prm->Env= query->Conf;
  prm->nsections = UdmVarListFindInt(Vars, "NumSections", 256);
  prm->dst_offs= Res->WWList.nuniq * prm->nsections;
  prm->nwf_offs= Res->WWList.nuniq * prm->nsections + 1;
  prm->max_coord_factor= ((float)UdmVarListFindInt(Vars, "MaxCoordFactor", 255)) / 0xFFFFFF;
  prm->MinCoordFactor= UdmVarListFindInt(Vars, "MinCoordFactor", 0);
  prm->have_WordFormFactor= UdmVarListFindInt(Vars, "WordFormFactor", 255) != 255;
  prm->WordFormFactor= ((float)UdmVarListFindDouble(Vars, "WordFormFactor", 255)) / 255;
  prm->WordFormFactorReminder= 1 - prm->WordFormFactor;
  prm->SaveSectionSize= UdmVarListFindBool(Vars, "SaveSectionSize", 1);
  DefWordDensityFactor= prm->SaveSectionSize ? 25 : 0;
  prm->WordDensityFactor= ((float)UdmVarListFindDouble(Vars, "WordDensityFactor", DefWordDensityFactor)) / 256;
  prm->WordDensityFactorReminder= 1 - prm->WordDensityFactor;
  prm->dst_weight= (unsigned int) UdmVarListFindInt(Vars, "WordDistanceWeight", 255);
  UdmWeightFactorsInit2(prm->wf, Vars, &db->Vars, "wf");
  prm->nwf_num= UdmWeightFactorsInit2(prm->nwf, Vars, &db->Vars, "nwf");
  prm->debug_url_id= UdmVarListFindInt(Vars, "DebugURLID", 0);
  prm->IDFFactor= UdmVarListFindInt(Vars, "IDFFactor", 255);
  prm->SkipWordDistanceThreshold= (float) UdmVarListFindInt(Vars, "SkipWordDistanceThreshold", 0) / 256;
  prm->SingleWordDistance= UdmVarListFindInt(Vars, "SingleWordDistance", 0);

  for (i= 0; i < 256; i++)
    prm->wf2[i]= prm->wf[i] << 2;

  prm->ncosine= Res->WWList.nuniq * prm->nsections + 1;
  prm->D_size= prm->ncosine * sizeof(unsigned int);

  UdmWideWordListSetOriginWeight(&Res->WWList);
  prm->WWList= Res->WWList;
  UdmSectionListMinMaxSecno(prm, SectionList);
  if (SectionList->Section && Res->WWList.nuniq > 1 && prm->IDFFactor)
    UdmCalcIDF(query, prm, SectionList);
  UdmCalcUserWordWeight(query, prm);
}


static void
UdmNumDistinctWordFactorInit(unsigned int numdistinctwordfactor,
                             float *numdistinctword_factor, size_t nuniq)
{
  size_t i;
  for (i= 0; i <= nuniq; i++)
  {
    float k= (float) numdistinctwordfactor;
    float x= (float) i / nuniq;
    numdistinctword_factor[i]= ((x * x * x * x * k) + (256-k)) / 256;
  }
}




/*
  Initializing ncoords and search_mode dependent member,
  and members requiring allocation
*/
static int
UdmScoreParamInitStep2(UDM_SCORE_PARAM *score_param,
                       UDM_AGENT *query, UDM_RESULT *Res,
                       size_t ncoords, int search_mode)
{
  size_t wordnum, secno;
  UDM_WIDEWORD *Res_WWList_Word= Res->WWList.Word;
  double Rsum; /* Can be very huge in case of big IDFFactor value */

  float numwordfactor= ((float)UdmVarListFindDouble(&query->Conf->Vars,
                                                "NumWordFactor", 25.5)) / 255;
  score_param->NumDistinctWordFactor= (unsigned int) UdmVarListFindInt(
                                                      &query->Conf->Vars,
                                                     "NumDistinctWordFactor", 0);
  UdmNumWordFactorInit(numwordfactor, score_param->numword_factor, ncoords);
  UdmNumDistinctWordFactorInit(score_param->NumDistinctWordFactor,
                               score_param->numdistinctword_factor,
                               Res->WWList.nuniq);  
  score_param->count_size= Res->WWList.nuniq;
  score_param->count= (char*)UdmMalloc(score_param->count_size);
  score_param->R= (unsigned int*)UdmMalloc(score_param->D_size);
  score_param->D= (unsigned int*)UdmMalloc(score_param->D_size);
  score_param->Dsum_add= (unsigned int*)UdmMalloc(score_param->D_size);
  score_param->RDsum_add= (unsigned int*)UdmMalloc(score_param->D_size);
  if (!score_param->count || !score_param->R || !score_param->D ||
      !score_param->Dsum_add || !score_param->RDsum_add)
    return UDM_ERROR;

  bzero((void*) score_param->R, score_param->D_size);
  
  for(Rsum=0, secno= 0; secno < score_param->nsections; secno++)
  {
    for (wordnum= 0; wordnum < score_param->WWList.nwords; wordnum++)
    {
      size_t offs, order, add;
      UDM_WIDEWORD *W= &Res_WWList_Word[wordnum];
      if (W->origin != UDM_WORD_ORIGIN_QUERY)
        continue;

      order= W->order;
      offs= OFFS_FROM_NUNIQ_SECNO_ORDER(score_param->WWList.nuniq, secno, order);
      add= UdmWordCosineWeight(score_param->wf2[secno], W->weight);
      
      Rsum+= UDM_SQR(add);
      
      score_param->R[offs]= add;
      score_param->Dsum_add[offs]= UDM_SQR(add);
      score_param->RDsum_add[offs]= UDM_SQR(add);
    }
  }
  
  Rsum+= UDM_SQR(score_param->R[score_param->dst_offs]);
  /*
  TODO: put this into UdmDebugScore()
  fprintf(stderr, "Rsum[dst]=%d RDsum_add[dst]=%d Dsum_add[dst]=%d Rsum=%d\n",
        score_param->R[score_param->dst_offs],
        score_param->RDsum_add[score_param->dst_offs],
        score_param->Dsum_add[score_param->dst_offs],
        Rsum);
  */
  score_param->Rsum_factor= 100000 / sqrt(Rsum);
  
  if (Res->ItemList.ncmds > 0 || search_mode == UDM_MODE_BOOL)
  {
    if (UDM_OK != UdmStackItemListCopy(&score_param->ItemList,
                                       &Res->ItemList, search_mode))
      return UDM_ERROR;
    if (score_param->ItemList.nitems)
    {
      search_mode= UDM_MODE_BOOL;
    }
    else
    {
      UdmLog(query, UDM_LOG_DEBUG, "Removing boolean condition");
    }
  }
  score_param->search_mode= search_mode;

  return UDM_OK;
}


static void
UdmScoreParamFreeStep2(UDM_SCORE_PARAM *score_param)
{
  UdmStackItemListFree(&score_param->ItemList);
  UDM_FREE(score_param->D);
  UDM_FREE(score_param->R);
  UDM_FREE(score_param->Dsum_add);
  UDM_FREE(score_param->RDsum_add);
  UDM_FREE(score_param->count);
}


static void
UdmGroupByURLInternal2(UDM_AGENT *query,
                       UDM_RESULT *Res,
                       UDM_SECTIONLIST *SectionList,
                       UDM_URLSCORELIST *ScoreList,
                       UDM_SCORE_PARAM *score_param,
                       int search_mode)
{
  size_t i, ncoords;
  for (ncoords=0, i= 0; i < SectionList->nsections; i++)
    ncoords+= SectionList->Section[i].ncoords;

  if(!ncoords) return;

  if (UdmScoreParamInitStep2(score_param, query, Res, ncoords, search_mode))
    goto err;

  if (Res->WWList.nuniq == 1)
  {
    /* Doesn't need coords */
    UdmGroupByURLLoop_OneWord_All2(query, Res, SectionList, ScoreList, score_param);
  }
  else
  {
    /* Need coords */
    UdmGroupByURLLoop2(query, Res, SectionList, ScoreList, score_param);
  }

err:

  UdmScoreParamFreeStep2(score_param);
  return;
}


void UdmGroupByURL2(UDM_AGENT *query,
                   UDM_DB *db,
                   UDM_RESULT *Res,
                   UDM_SECTIONLIST *SectionList,
                   UDM_URLSCORELIST *ScoreList)
{
  UDM_SCORE_PARAM *prm;
  int search_mode= UdmSearchMode(UdmVarListFindStr(&query->Conf->Vars, "m", "all"));
  size_t threshold= UdmVarListFindInt(&query->Conf->Vars, "StrictModeThreshold", 0);
  size_t ncoords= (search_mode == UDM_MODE_ALL) && threshold ?
                  SectionList->nsections : 0;
  size_t nbytes;

  
  if (!(prm= UdmMalloc(sizeof(UDM_SCORE_PARAM))))
    return;
  
  UdmScoreParamInit(prm, query, db, Res, SectionList);

  nbytes= SectionList->nsections * sizeof(UDM_URL_SCORE);
  ScoreList->Item= (UDM_URL_SCORE*) UdmMalloc(nbytes);
  UdmGroupByURLInternal2(query, Res, SectionList, ScoreList, prm, search_mode);
  if (ncoords && (ScoreList->nitems < threshold))
  {
    size_t strict_mode_found= ScoreList->nitems;
    const char *loose_mode_str= UdmVarListFindStr(&query->Conf->Vars,
                                "LooseMode", "any");
    int LooseMode= UdmSearchMode(loose_mode_str);
    UdmLog(query, UDM_LOG_DEBUG,
          "Too few results: %d, Threshold: %d, group using m=%s", 
          (int) strict_mode_found, (int) threshold, loose_mode_str);
    UdmGroupByURLInternal2(query, Res, SectionList, ScoreList, prm, LooseMode);
    if (ScoreList->nitems > strict_mode_found)
      UdmVarListReplaceInt(&query->Conf->Vars, "StrictModeFound", strict_mode_found);
  }
 
  UdmFree(prm);
}

/*************** UserScore and UserSiteScore functions ******************/


static void
UdmUserScoreFindMinMax(UDM_URL_INT4_LIST *List, int *minval, int *maxval)
{
  size_t i;
  *minval= *maxval= 0;
  for (i= 0; i < List->nitems; i++)
  {
    UDM_URL_INT4 *Item= &List->Item[i];
    if (*minval > Item->param)
      *minval= Item->param;
    if (*maxval < Item->param)
      *maxval= Item->param;
  }
}


int
UdmUserScoreListApplyToURLScoreList(UDM_AGENT *A,
                                    UDM_URLSCORELIST *List,
                                    UDM_URL_INT4_LIST *UserScoreList,
                                    int UserScoreFactor)
{
  size_t i, nfound;
  int4 minval= -1;
  int4 maxval= 1;
  UDM_URL_SCORE *Coords= List->Item;

  UdmUserScoreFindMinMax(UserScoreList, &minval, &maxval);
  
  for (i= 0, nfound= 0; i < List->nitems; i++)
  {
    urlid_t url_id= Coords[i].url_id;
    uint4 coord= Coords[i].score;
    UDM_URL_INT4 *found;
    found= (UDM_URL_INT4*) UdmBSearch(&url_id,
                                      UserScoreList->Item,
                                      UserScoreList->nitems,
                                      sizeof(UDM_URL_INT4),
                                      (udm_qsort_cmp)UdmCmpURLID);
    if (found && found->param != 0)
    {
      nfound++;
      if (found->param >= 0)
        coord= coord + 
              ((int4) (((float) (100000 - coord)) * found->param / maxval)) *
               UserScoreFactor / 255;
      else
        coord= coord -
               ((int4) (((float) coord) * found->param / minval)) *
               UserScoreFactor / 255;
    }
    Coords[i].score= coord;
  }
  UdmLog(A, UDM_LOG_DEBUG + 1,
         "UserScoreListApplyToURLScoreList: min=%d max=%d nitems=%d nfound=%d",
         minval, maxval, (int) UserScoreList->nitems, (int) nfound);
  return UDM_OK;
}


int
UdmUserScoreListApplyToURLDataList(UDM_AGENT *A,
                                   UDM_URLDATALIST *List,
                                   UDM_URL_INT4_LIST *UserScoreList,
                                   int UserScoreFactor)
{
  size_t i;
  int4 minval= -1;
  int4 maxval= 1;
  UDM_URLDATA *Coords= List->Item;

  UdmUserScoreFindMinMax(UserScoreList, &minval, &maxval);

  for (i= 0; i < List->nitems; i++)
  {
    urlid_t url_id= Coords[i].url_id;
    uint4 score= Coords[i].score;
    UDM_URL_INT4 *found;
    found= (UDM_URL_INT4*) UdmBSearch(&url_id,
                                      UserScoreList->Item,
                                      UserScoreList->nitems,
                                      sizeof(UDM_URL_INT4),
                                      (udm_qsort_cmp)UdmCmpURLID);
    if (found)
    {
      if (found->param >= 0)
        score= score + 
              ((int4) (((float) (100000 - score)) * found->param / maxval)) *
               UserScoreFactor / 255;
      else
        score= score -
               ((int4) (((float) score) * found->param / minval)) *
               UserScoreFactor / 255;
    }
    Coords[i].score= score;
  }
  return UDM_OK;
}


/************** DateFactor and RelevancyFactor **********/

int
UdmURLDataListApplyRelevancyFactors(UDM_AGENT *Agent,
                                    UDM_URLDATALIST *DataList,
                                    int RelevancyFactor,
                                    int DateFactor)
{
  int i, sum;
  time_t current_time;
  udm_timer_t ticks;
  UdmLog(Agent, UDM_LOG_DEBUG, "Start applying relevancy factors");
  ticks= UdmStartTimer();
  if (!(current_time= UdmVarListFindInt(&Agent->Conf->Vars, "CurrentTime", 0)))
    time(&current_time);
  sum= RelevancyFactor + DateFactor;
  sum= sum ? sum : 1;
  
  for (i= 0; i < DataList->nitems; i++)
  {
    time_t doc_time= DataList->Item[i].last_mod_time;
    uint4 *score= &DataList->Item[i].score;
    float rel= *score * RelevancyFactor;
    float dat= ((doc_time < current_time) ? 
                ((float) doc_time / current_time) :
                ((float) current_time / doc_time)) * DateFactor * 100000;
    /* 100000 = 100% * 1000 = scale in db.c */
    *score= (rel + dat) / sum;
  }
  UdmLog(Agent, UDM_LOG_DEBUG, "Stop applying relevancy factors\t\t%.2f",
         UdmStopTimer(&ticks));
  return UDM_OK;
}
