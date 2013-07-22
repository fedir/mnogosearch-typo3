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
#include "udm_utils.h"
#include "udm_searchtool.h"

void
UdmSectionListPrint(UDM_SECTIONLIST *SectionList)
{
  UDM_SECTIONLIST *L= SectionList;
  size_t section;
  fprintf(stderr, "ncoords=%d nsec=%d\n",
          (int) SectionList->ncoords, (int) SectionList->nsections);
  for (section= 0; section < L->nsections; section++)
  {
    UDM_SECTION *S= &L->Section[section];
    size_t coord;
    /*
    fprintf(stderr,
            "       [%d:%d] ncoords=%d seclen=%d secno=%d min=%d max=%d offs=%d\n",
            S->url_id, S->secno, S->ncoords, S->seclen, S->secno,
            S->minpos, S->maxpos,
            S->Coord - SectionList->Coord);
    */
    if (!S->Coord)
      continue;
    for (coord= 0; coord < S->ncoords; coord++)
    {
      UDM_COORD2 *C= &S->Coord[coord];
      fprintf(stderr, "[%d]secno=%d pos=%d seclen=%d num=%d order=%d ncoords=%d min=%d max=%d\n",
                       S->url_id, S->secno, C->pos, S->seclen, S->wordnum, S->order,
                       S->ncoords, S->minpos, S->maxpos);
    }
  }
}


int
UdmSectionListAlloc(UDM_SECTIONLIST *List, size_t ncoords, size_t nsections)
{
  bzero((void*)List, sizeof(*List));
  if (ncoords)
  {
    List->Coord= (UDM_COORD2*) UdmMalloc(ncoords * sizeof(UDM_COORD2));
    List->Section= (UDM_SECTION*) UdmMalloc(nsections * sizeof(UDM_SECTION));
    List->mcoords= ncoords;
    List->msections= nsections;
    UDM_ASSERT(nsections > 0);
  }
  return UDM_OK;
}


void
UdmSectionListFree(UDM_SECTIONLIST *List)
{
  UDM_FREE(List->Coord);
  UDM_FREE(List->Section);
}


int
UdmSectionListListAdd(UDM_SECTIONLISTLIST *List, UDM_SECTIONLIST *Item)
{
  if (List->nitems >= List->mitems)
  {
    List->mitems+= 256;
    List->Item= (UDM_SECTIONLIST*) UdmRealloc(List->Item, List->mitems * sizeof(*Item));
  }
  List->Item[List->nitems]= *Item;
  List->nitems++;
  return UDM_OK;
}


void
UdmSectionListListInit(UDM_SECTIONLISTLIST *List)
{
  bzero((void*) List, sizeof(*List));
}


void
UdmSectionListListFree(UDM_SECTIONLISTLIST *List)
{
  size_t i;
  for (i= 0; i < List->nitems; i++)
    UdmSectionListFree(&List->Item[i]);
  UDM_FREE(List->Item);
}


/*
  Compare two UDM_SECTIONLISTs by nsections,
  return the list with more sections first.
*/
static int
cmp_section_list_by_nsections_rev(UDM_SECTIONLIST *s1, UDM_SECTIONLIST *s2)
{
  if (s1->nsections > s2->nsections) return -1;
  if (s1->nsections < s2->nsections) return  1;
  return 0;
}


/*
  Compare two UDM_SECTIONLISTs by secno,
  return the list with smaller secno first.
*/
static int
cmp_section_list_by_secno(UDM_SECTIONLIST *s1, UDM_SECTIONLIST *s2)
{
  return (int) (s1->Section->secno)  -  (int) (s2->Section->secno);
}


typedef struct udm_section_merge_st
{
  UDM_SECTION *p;
  UDM_SECTION *e;
} UDM_SECTION_TAPE;



static int
UdmSectionListListMergeSorted1(UDM_SECTIONLISTLIST *SrcList,
                               UDM_SECTIONLIST *Dst, int opt)
{
  size_t list, nlists, nsections, nreminder;
  UDM_SECTION *Section;
  UDM_SECTION_TAPE *Tape;
  UDM_SECTIONLIST Short;
  int rc= UDM_OK;
  urlid_t min_id;

  if (!SrcList->nitems)
    return rc;

  bzero((void*) Dst, sizeof(*Dst));
  bzero((void*) &Short, sizeof(Short));
  
  
  /* Calc the number of destination sections */
  for (nsections= 0, nlists= 0; nlists <  SrcList->nitems; nlists++)
  {
    nsections+= SrcList->Item[nlists].nsections;
    UDM_ASSERT(SrcList->Item[nlists].nsections);
  }
  
  /* Sort in secno order */
  UdmSort((void*) SrcList->Item, SrcList->nitems, sizeof(UDM_SECTIONLIST),
          (udm_qsort_cmp) cmp_section_list_by_secno);
  
  if (!(Tape= UdmMalloc(SrcList->nitems * sizeof(UDM_SECTION_TAPE))))
    return UDM_ERROR;
  
  /* Install pointers and find min_id */
  for (list= 0, nreminder= nsections, min_id= SrcList->Item[0].Section->url_id;
       list < nlists;
       list++)
  {
    UDM_SECTION_TAPE *T= &Tape[list];
    size_t nsec= SrcList->Item[list].nsections;
    T->p= SrcList->Item[list].Section;
    T->e= T->p + nsec;
    /*
    fprintf(stderr, "[%d] secno=%d count=%d reminder=%d/%d\n",
            list, T->p->secno, nsec, nreminder, nsections);
    */

    if (opt && list > 2 && (nlists - list > 2) && nreminder < nsections / 10)
    {
      UDM_SECTIONLISTLIST Tmp= *SrcList;
      Tmp.Item+= list;
      Tmp.nitems-= list;
      /*fprintf(stderr, "cut - merge %d lists\n", Tmp.nitems);*/
      UdmSectionListListMergeSorted(&Tmp, &Short, 0);
      Tape[list].p= Short.Section;
      Tape[list].e= Short.Section + Short.nsections;
      nlists= list + 1;
        
      /*fprintf(stderr, "[%d] nsec=%d MERGED\n", list, Short.nsections);*/
        
      break;
    }
    
    nreminder-= nsec;
  }
  
  if (!(Section= (UDM_SECTION*)UdmMalloc(nsections * sizeof(UDM_SECTION))))
  {
    rc= UDM_ERROR;
    goto ret;
  }
  Dst->Section= Section;
  Dst->msections= nsections;
  
  if (nlists == 1)
    goto one;
  
  for ( ;  ; )
  {
    size_t i;
    urlid_t next_min_id= UDM_MAX_URLID_T;
    
    /* Copy section with min_id to destination */
    for (i= 0; i < nlists; i++)
    {
      UDM_SECTION_TAPE *T= &Tape[i];
repeat:
      for ( ; T->p->url_id == min_id ;)
      {
        /*fprintf(stderr, "[%d] id=%d secno=%d nlists=%d next_min_id=%d\n",
                i, T->p->url_id, T->p->secno, nlists, next_min_id);*/
        
        *Section++= *T->p++;
        
        if (T->p == T->e)
        {
          /* fprintf(stderr, "reduce\n"); */
          memmove(T, T+1, (nlists - i - 1) * sizeof(UDM_SECTION_TAPE));
          nlists--;
          if (nlists == 1)
            goto one;
          goto repeat;
        }
      }
      
      if (next_min_id > T->p->url_id)
      {
        next_min_id= T->p->url_id;
      }
      /* fprintf(stderr, "[%d] skip id=%d  next_min_id=%d\n", i, T->p->url_id, next_min_id); */
    }
    
    min_id= next_min_id;
    
    /* Find new min */
    /*
    if (Tape[0].p->url_id <= Tape[1].p->url_id)
    {
      min_id= Tape[0].p->url_id;
    }
    else
    {
      min_id= Tape[1].p->url_id;
    }
    for (i= 2; i < nlists; i++)
    {
      if (Tape[i].p->url_id < min_id)
      {
        min_id= Tape[i].p->url_id;
      }
    }
    */
    
    /* fprintf(stderr, "min_id=%d next_min_id=%d nlists=%d\n", min_id, next_min_id, nlists); */
    
    /*UDM_ASSERT(min_id == next_min_id);*/
    
  }
  
one:
  memcpy(Section, Tape->p, (Tape->e - Tape->p) * sizeof(UDM_SECTION));
  Dst->nsections= nsections;
  
ret:
  UDM_FREE(Short.Section);
  UdmFree(Tape);
  return rc;
}


/*
  Merge sorted lists
*/
int
UdmSectionListListMergeSorted(UDM_SECTIONLISTLIST *SrcList,
                              UDM_SECTIONLIST *Dst, int opt)
{
  UDM_SECTION_TAPE *Tape;
  UDM_SECTION *Section;
  size_t list, nlists, nsections;
  int rc= UDM_OK;
  urlid_t *id;
  UDM_SECTIONLIST Short;

  {
    const char *env;
    if (!(env= getenv("NEWSORT")) || strcmp(env, "yes"))
      goto old;
  }
  return UdmSectionListListMergeSorted1(SrcList, Dst, 1);

old:

  bzero((void*) Dst, sizeof(*Dst));
  bzero((void*) &Short, sizeof(Short));
  if (!SrcList->nitems)
    return rc;
  
  if (!(Tape= UdmMalloc(SrcList->nitems * sizeof(UDM_SECTION_TAPE))))
    return UDM_ERROR;
  if (!(id= (urlid_t*) UdmMalloc(SrcList->nitems * sizeof(urlid_t))))
    return UDM_ERROR;

  /*
    Sort in descending order of nsections.
    To move most populous items on the top.
  */
  UdmSort((void*) SrcList->Item, SrcList->nitems, sizeof(UDM_SECTIONLIST),
          (udm_qsort_cmp) cmp_section_list_by_nsections_rev);

  /*
    Calc number of non-empty lists
    and total number of sections
  */
  nsections= nlists= 0;
  for (list= 0; list < SrcList->nitems; list++) 
  {
    size_t nsec= SrcList->Item[list].nsections;
    if (nsec)
    {
      /*fprintf(stderr, "[%d] nsec=%d\n", list, nsec); */
      
      if (opt && list > 2 && nsec < SrcList->Item[0].nsections / 10)
      {
        UDM_SECTIONLISTLIST Tmp= *SrcList;
        Tmp.Item+= list;
        Tmp.nitems-= list;
        /*UdmSectionListListMergeUsingSort(&Tmp, &Short);*/
        /* fprintf(stderr, "cut\n");*/
        UdmSectionListListMergeSorted(&Tmp, &Short, 0);
        nsections+= Short.nsections;
        Tape[nlists].p= Short.Section;
        Tape[nlists].e= Short.Section + Short.nsections;
        id[nlists]= Tape[nlists].p->url_id;
        nlists++;
        /*
        fprintf(stderr, "[%d] nsec=%d ptr=%p MERGED\n",
                list, Short.nsections, Short.Section);
        */
        break;
      }
      nsections+= nsec;
      Tape[nlists].p= SrcList->Item[list].Section;
      Tape[nlists].e= Tape[nlists].p + nsec;
      id[nlists]= Tape[nlists].p->url_id;
      nlists++;
    }
  }
  if (!nlists)
    goto ret;

  if (!(Section= (UDM_SECTION*)UdmMalloc(nsections * sizeof(UDM_SECTION))))
  {
    rc= UDM_ERROR;
    goto ret;
  }
  Dst->Section= Section;
  Dst->msections= nsections;
  
  if (nlists == 1)
    goto one;

  for ( ; ; )
  {
    size_t i, min= 0;
    urlid_t p_min_url_id= id[0];

    /* Find quickly min between the first and the second lists */
    if (id[1] > p_min_url_id)
    {
      /* do nothing */
    }
    else if (id[1] < p_min_url_id)
    {
      min= 1;
      p_min_url_id= id[1];
    }
    else if (Tape[1].p->secno < Tape[0].p->secno)
    {
      min= 1;
    }

    /* Check the other lists */
    for (i= 2; i < nlists; i++)
    {
      if (id[i] > p_min_url_id)
        continue;
      
      if (id[i] < p_min_url_id)
      {
        min= i;
        p_min_url_id= id[i];
        continue;
      }
      
      if (Tape[i].p->secno < Tape[min].p->secno)
        min= i;
    }
    
    *Section++= *Tape[min].p++;
    
    if (Tape[min].p == Tape[min].e) /* The tape has finished */
    {
      nlists--;
      Tape[min]= Tape[nlists];
      id[min]= id[nlists];
      if (nlists == 1)
        break;
    }
    else
    {
      id[min]= Tape[min].p->url_id;
      /*
      fprintf(stderr, "min=%d\n", min);
      Some statistics for query q=1+2+3
        1783315 min=0
         557939 min=1
         489055 min=2
          32459 min=3
            811 min=4
            720 min=5
            494 min=6
            165 min=7
            101 min=8                              
      */
    }
  }

one:
  memcpy(Section, Tape[0].p, (Tape[0].e - Tape[0].p) * sizeof(UDM_SECTION));
  Dst->nsections= nsections;
  
ret:
  UDM_FREE(Short.Section);
  UdmFree(Tape);
  UdmFree(id);
  return rc;
}



/*
  Merge sorted lists
*/
#if 0
int
UdmSectionListListMergeSorted2(UDM_SECTIONLISTLIST *SrcList,
                              UDM_SECTIONLIST *Dst, int opt)
{
  UDM_SECTION **p, **e, *Section;
  size_t list, nlists, nsections;
  int rc= UDM_OK;
  urlid_t *id;
  UDM_SECTIONLIST Short;
  
  bzero((void*) Dst, sizeof(*Dst));
  bzero((void*) &Short, sizeof(Short));
  if (!SrcList->nitems)
    return rc;
  
  if (!(p= UdmMalloc(SrcList->nitems * sizeof(UDM_SECTIONLIST*) * 2)))
    return UDM_ERROR;
  e= p + SrcList->nitems;
  if (!(id= (urlid_t*) UdmMalloc(SrcList->nitems * sizeof(urlid_t))))
    return UDM_ERROR;

  /*
    Sort in descending order of nsections.
    To move most populous items on the top.
  */
  UdmSort((void*) SrcList->Item, SrcList->nitems, sizeof(UDM_SECTIONLIST),
          (udm_qsort_cmp) cmp_section_list_by_nsections_rev);

  /*
    Calc number of non-empty lists
    and total number of sections
  */
  nsections= nlists= 0;
  for (list= 0; list < SrcList->nitems; list++) 
  {
    size_t nsec= SrcList->Item[list].nsections;
    if (nsec)
    {
      /*fprintf(stderr, "[%d] nsec=%d\n", list, nsec); */
      
      if (opt && list > 2 && nsec < SrcList->Item[0].nsections / 10)
      {
        UDM_SECTIONLISTLIST Tmp= *SrcList;
        Tmp.Item+= list;
        Tmp.nitems-= list;
        /*UdmSectionListListMergeUsingSort(&Tmp, &Short);*/
        /* fprintf(stderr, "cut\n");*/
        UdmSectionListListMergeSorted(&Tmp, &Short, 0);
        nsections+= Short.nsections;
        p[nlists]= Short.Section;
        e[nlists]= p[nlists] + Short.nsections;
        id[nlists]= p[nlists]->url_id;
        nlists++;
        /*
        fprintf(stderr, "[%d] nsec=%d ptr=%p MERGED\n",
                list, Short.nsections, Short.Section);
        */
        break;
      }
      nsections+= nsec;
      p[nlists]= SrcList->Item[list].Section;
      e[nlists]= p[nlists] + nsec;
      id[nlists]= p[nlists]->url_id;
      nlists++;
    }
  }
  if (!nlists)
    goto ret;

  if (!(Section= (UDM_SECTION*)UdmMalloc(nsections * sizeof(UDM_SECTION))))
  {
    rc= UDM_ERROR;
    goto ret;
  }
  Dst->Section= Section;
  Dst->msections= nsections;
  
  if (nlists == 1)
    goto one;

  for ( ; ; )
  {
    size_t i, min= 0;
    urlid_t p_min_url_id= id[0];

    /* Find quickly min between the first and the second lists */
    if (id[1] > p_min_url_id)
    {
      /* do nothing */
    }
    else if (id[1] < p_min_url_id)
    {
      min= 1;
      p_min_url_id= id[1];
    }
    else if (p[1]->secno < p[0]->secno)
    {
      min= 1;
    }

    /* Check the other lists */
    for (i= 2; i < nlists; i++)
    {
      if (id[i] > p_min_url_id)
        continue;
      
      if (id[i] < p_min_url_id)
      {
        min= i;
        p_min_url_id= id[i];
        continue;
      }
      
      if (p[i]->secno < p[min]->secno)
        min= i;
    }
    
    *Section++= *p[min]++;
    
    if (p[min] == e[min])
    {
      nlists--;
      p[min]= p[nlists];
      e[min]= e[nlists];
      id[min]= id[nlists];
      if (nlists == 1)
        break;
    }
    else
      id[min]= p[min]->url_id;
  }

one:
  memcpy(Section, *p, (*e - *p) * sizeof(UDM_SECTION));
  Dst->nsections= nsections;
  
ret:
  UDM_FREE(Short.Section);
  UdmFree(p);
  UdmFree(id);
  return rc;
}
#endif


/*
  Compare two UDM_SECTIONs by url_id then secno
*/
/*
static int
cmp_section_by_urlid_then_secno(UDM_SECTION *s1, UDM_SECTION *s2)
{
  if (s1->url_id < s2->url_id) return -1;
  if (s1->url_id > s2->url_id) return  1;
  if (s1->secno < s2->secno) return -1;
  if (s1->secno > s2->secno) return 1;
  return 0;
}


static int
UdmSectionListListMergeUsingSort(UDM_SECTIONLISTLIST *SrcList, UDM_SECTIONLIST *Dst)
{
  size_t i, nsections;
  UDM_SECTION *p;
  
  for (i= 0, nsections=0 ; i < SrcList->nitems; i++)
    nsections+= SrcList->Item[i].nsections;
  fprintf(stderr, "skip, remain items=%d sections=%d\n",
          SrcList->nitems, nsections);
  if (!nsections)
    return UDM_OK;

  if (!(p= (UDM_SECTION*) UdmMalloc(nsections * sizeof(UDM_SECTION))))
    return UDM_ERROR;

  Dst->nsections= nsections;
  Dst->Section= p;
  
  for (i= 0; i < SrcList->nitems; i++)
  {
    UDM_SECTIONLIST *S= &SrcList->Item[i];
    size_t nbytes= S->nsections * sizeof(UDM_SECTION);
    memcpy(p, S->Section, nbytes);
    p+= S->nsections;
  }
  UdmSort((void*) Dst->Section, Dst->nsections, sizeof(UDM_SECTION),
          (udm_qsort_cmp) cmp_section_by_urlid_then_secno);
  return UDM_OK;
}
*/
