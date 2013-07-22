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
#include "udm_common.h"
#include "udm_utils.h"
#include "udm_searchtool.h"


/*
 Normalize word distance values.
 If word distance is longer than 64,
 then consider this word distance as just 64.
 Perhaps it could be some logarithmic function instead.
*/
#define UDM_DST_THRESHOLD 63

static inline uint4
UdmDstNormalize(uint4 dst)
{
  return (dst > UDM_DST_THRESHOLD) ? UDM_DST_THRESHOLD : dst;
}


/*
  Calculate word distance in the case
  when we have only two coordinates in a section.
*/
static inline void
CalcAverageWordDistanceTwoCoords(size_t wf2_secno,
                                 UDM_COORD2 *phr, size_t num,
                                 UDM_WORD_DISTANCE_STAT *dist)
{
  /* TODO: word frame nuniq*2 */
  size_t ord0= phr[0].order;
  size_t ord1= phr[1].order;
  size_t pos0= phr[0].pos;
  size_t pos1= phr[1].pos;
  size_t res= (ord0 == ord1) ? 0 : pos1 > pos0 ? pos1 - pos0 : pos0 - pos1;
  dist->num++;
  dist->sum+= res > 0 ? (UdmDstNormalize(res) - 1) * wf2_secno : 0;
}


static uint4
diff_to_np2[UDM_DST_THRESHOLD+1]=
{
  1,
  64, /* 1  */
  32, /* 2  */
  16, /* 3  */
  8,  /* 4  */
  4,  /* 5  */
  2,  /* 6  */
  1,  /* 7  */
  1,  /* 8  */
  1,  /* 9  */
  1,  /* 10 */
  1,  /* 11 */
  1,  /* 12 */
  1,  /* 13 */
  1,  /* 14 */
  1,  /* 15 */
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};


/*
#define DEBUG_DISTANCE
*/

#ifdef DEBUG_DISTANCE
static void
UdmDebugDistance(UDM_COORD2 *phr, UDM_COORD2 *phrend, size_t sum, size_t num)
{
  static size_t prevsum;
  static size_t prevnum;
  float diff;
  char phrinfo[128]= "";
  size_t phrcount;
  UDM_COORD2 *p;
  
  if (phr == NULL)
  {
    prevsum= 0;
    prevnum= 0;
    return;
  }
  diff= ((float) sum / num) / ((float) prevsum / prevnum);
  prevsum= sum;
  prevnum= num;
  for (phrcount= 1, p= phr ; p < phrend - 1; p++)
  {
    if (p->pos + 1 == p[1].pos)
      phrcount++;
    else
      break;
  }
  if (phrcount > 1)
    udm_snprintf(phrinfo, sizeof(phrinfo), " phr[%d]", phrcount);
  fprintf(stderr, "%d[%d] %d/%d %.4f (%.4f)%s\n",
          phr->pos, phr->order, sum, num, (float) sum / num, diff, phrinfo);
}
#define UDM_DEBUG_DISTANCE(phr, phrend, sum, num) UdmDebugDistance(phr, phrend, sum, num)
#else
#define UDM_DEBUG_DISTANCE(phr, phrend, sum, num)
#endif


static inline void
UdmWordDistanceAdd(size_t nuniq, UDM_WORD_DISTANCE_STAT *dist, udm_pos_t diff)
{
  dist->sum+= UdmDstNormalize(diff);
  dist->num++; 
  if (nuniq == 2 && diff==1)
  {
    dist->num+= diff_to_np2[diff];
  }
}

/*
  All words must be from the same section.
*/
void
CalcAverageWordDistance(size_t wf2_secno,
                        UDM_COORD2 *phr, size_t num, size_t nuniq,
                        UDM_WORD_DISTANCE_STAT *distance)
{
  UDM_COORD2 *last, *last1, *phr2;
  udm_wordnum_t prev_order;
  UDM_WORD_DISTANCE_STAT dist;

  if (num < 2)
    return;
  
  if (num == 2)
  {
    CalcAverageWordDistanceTwoCoords(wf2_secno, phr, num, distance);
    return;
  }

  dist.sum= dist.num= 0;
  phr2= phr + 2;
  last= phr + num;
  last1= last - 1;

  /* Add special case: "BEG w1 w2 w2" */
  if (phr[1].order == phr[2].order &&
      phr[1].order != phr[0].order)
  {
    UdmWordDistanceAdd(nuniq, &dist, phr[1].pos - phr->pos);
  }
  
  UDM_DEBUG_DISTANCE(NULL, NULL, 0, 0);
  
  prev_order= phr->order;
  for (phr++; phr < last1; phr++)
  {
    udm_wordnum_t current_order= phr->order;
    
    UDM_DEBUG_DISTANCE(phr - 1, last, dist.sum, dist.num);
    /*
      Detect these sequences:
        "w1 w1 W2 w2"
           "w1 W2 w3"
           "w1 W2 w1"
    */
    
    if (prev_order != current_order)
    {
      UDM_COORD2 *phr1= phr + 1;
      if (phr1->order != current_order)
      {
        uint4 diff1= phr->pos - phr[-1].pos;
        uint4 diff2= phr1->pos - phr->pos;
        if (prev_order == phr1->order)
        {
          /* "w1 W2 w1" - add min distance */
          UdmWordDistanceAdd(nuniq, &dist, UDM_MIN(diff1, diff2));
        }
        else
        {
          UDM_COORD2 *phr3= phr1 + 1;
          /* "w1 W2 w3" - add two distances */
          uint4 diff= UdmDstNormalize(diff1) + UdmDstNormalize(diff2);
          dist.sum+= diff;
          dist.num+= 2;
          if (diff <= 2)
          {
            /*
             We have three different words consequently.
             This is much better than just pairs.
            */
            dist.num+=2;
            if (nuniq == 3)
              dist.num+= 512; /* Exact phrase */
            if (phr3 < last &&
                phr3->order != phr1->order &&
                phr3->pos - phr1->pos <= 1)
            {
              UDM_COORD2 *phr4= phr3 + 1;
              /* Four different words consequently */
              dist.num+= 2;
              if (nuniq == 4) /* Exact phrase - all four words from the query */
                dist.num+= 512;
              
              if (phr4 < last &&
                  phr4->order != phr3->order &&
                  phr4->pos - phr3->pos <= 1)
              {
                /* Exact phrase - all five words consequently */
                dist.num+= 512;
              }
            }
          }
          else if (diff <= 4)
          {
            /*
               Check if all words appear in a frame 2*nuniq words.
               For example:
               
               w1 .  w2 .  w3
               w1 .  .  w2 w3
               w1 w2 .  .  w2
            */
            UDM_COORD2 *pe= phr + nuniq * 2;
            if (pe < last &&
                phr3->order != phr2->order &&
                phr3->order != phr->order)
            {
              UDM_COORD2 *p;
              udm_pos_t rpos= phr->pos + nuniq * 2;
              uint4 mask= 0;
              for (p= phr - 1; p < pe && p->pos < rpos; p++)
              {
                /*fprintf(stderr, "%d[%d] %04X\n", p->pos, p->order, 1 << p->order);*/
                mask|= (1 << p->order);
              }
              
              if (mask == ((1UL << nuniq) - 1))
              {
                /* All words appear in a frame 2*nuniq  */
                dist.num+= 512 * nuniq;
              }
              /*fprintf(stderr, "end--%d expected=%d\n", mask, ((1UL << nuniq) - 1));*/
            }
          }
        }
      }
      else
      {
        /* "w1 W2 w2" */
        if (phr >= phr2 &&
            phr[-2].order == prev_order)
        {
          /* w1 w1 W2 w2 */
          UdmWordDistanceAdd(nuniq, &dist, phr->pos - phr[-1].pos);
          phr++;
          prev_order= phr->order;
          continue;
        }
      }
    }
    prev_order= current_order;
  }
  
  phr= last1;
  /* Add special case: "w1 w1 w2 END" */
  if (phr[-1].order == phr[-2].order &&
      phr[-1].order != phr->order)
  {
    UdmWordDistanceAdd(nuniq, &dist, phr->pos - phr[-1].pos);
  }
  
  /* Reduce all diffs by 1. */
  if (dist.sum > dist.num)
    dist.sum-= dist.num;
  else
    dist.sum= 0;

  distance->sum+= dist.sum * wf2_secno;
  distance->num+= dist.num;
}
