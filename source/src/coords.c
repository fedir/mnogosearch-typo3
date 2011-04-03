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
#include "udm_db.h" /* For UDM_FINDWORD_ARGS */



static int
cmpurlid(UDM_URL_CRD *s1, UDM_URL_CRD *s2)
{
  if (s1->url_id > s2->url_id) return(1);
  if (s1->url_id < s2->url_id) return(-1);
  if (s1->secno > s2->secno) return 1;
  if (s1->secno < s2->secno) return -1;
  return (int) s1->pos - (int) s2->pos;
}


void
UdmURLCRDListSortByURLThenSecnoThenPos(UDM_URLCRDLIST *L)
{
  if(L->Coords && L->ncoords)
    UdmSort((void*)L->Coords, L->ncoords,
            sizeof(*L->Coords),(udm_qsort_cmp)cmpurlid);
}


static void
UdmURLCRDListMergeMultiWord(UDM_URLCRDLIST *Phrase, size_t wordnum, size_t nparts)
{
  UDM_URL_CRD *To= Phrase->Coords;
  UDM_URL_CRD *End= Phrase->Coords + Phrase->ncoords;
  UDM_URL_CRD *From= Phrase->Coords + nparts - 1;
  UDM_URL_CRD *Prev= Phrase->Coords + nparts - 2;
  
#if 0
  fprintf(stderr, "merge: wordnum=%d nparts=%d ncoords=%d\n",
          wordnum, nparts, Phrase->ncoords);
#endif
  
  if (nparts < 2) /* If one part, keep Phrase unchanged */
    return;
  
  if (Phrase->ncoords < nparts) /* Nothing found */
  {
    Phrase->ncoords= 0;
    return;
  }
  
  for ( ; From < End ; From++, Prev++)
  {
    if (Prev->url_id == From->url_id)
    {
      size_t pos= From->pos;
      size_t sec= From->secno;
      size_t num= From->num;
      if (pos == Prev->pos + 1 &&
          sec == Prev->secno  &&
          num == Prev->num + 1)
      {
        size_t i, nmatches;
        for (nmatches= 2, i= 2; i < nparts; i++)
        {
          if (From[-i].url_id != From->url_id         ||
              From[-i].secno != sec       ||
              From[-i].pos != (pos - i) ||
              From[-i].num != (num - i))
            break;
            
          nmatches++;
        }
        if (nmatches == nparts)
        {
          To->url_id= From->url_id;
          To->pos= pos - nparts + 1;
          To->secno= sec;
          To->num= wordnum;
          To++;
        }
      }
    }
  }
  
  Phrase->ncoords= To - Phrase->Coords;
}


/*
  Convert URLCRDList to SectionList.
  Only single word is supported.
  Crd->num must be the same for all coords.
*/
static int
UdmURLCRDListToSectionList(UDM_SECTIONLIST *SectionList,
                           UDM_URLCRDLIST *CoordList,
                           udm_wordnum_t wordnum,
                           udm_wordnum_t order)
{
  size_t ncoords= CoordList->ncoords;
  UDM_URL_CRD *CrdFrom, *CrdCurr;
  UDM_URL_CRD *CrdLast= CoordList->Coords + ncoords;
  UDM_COORD2 *Coord;
  UDM_SECTION *Section;
  
  UdmSectionListAlloc(SectionList, CoordList->ncoords, CoordList->ncoords);

#if 0
  UdmCoordListPrint(CoordList);
#endif
  
  if (!CoordList->ncoords)
    return UDM_OK;
  Coord= SectionList->Coord;
  Section= SectionList->Section;
  
  for (CrdFrom= CoordList->Coords; CrdFrom < CrdLast; CrdFrom= CrdCurr)
  {
    Section->PackedCoord= NULL; /* Not packed */
    Section->Coord= Coord;
    Section->secno= CrdFrom->secno;
    Section->wordnum= CrdFrom->num;
    UDM_ASSERT(CrdFrom->num == wordnum);
    Section->order= order; /* args->WWList->Word[CrdFrom->num].order; */
    for (CrdCurr= CrdFrom;
         CrdCurr < CrdLast &&
         CrdCurr->url_id == CrdFrom->url_id &&
         CrdCurr->secno == CrdFrom->secno ;
         CrdCurr++)
    {
      Coord->pos= CrdCurr->pos;
      Coord->order= Section->order;
      Section->maxpos= CrdCurr->pos;
      Coord++;
    }
    Section->url_id= CrdFrom->url_id;
    Section->ncoords= CrdCurr - CrdFrom;
    Section->seclen= CrdFrom->seclen;
    Section->minpos= Section->Coord->pos;
    Section++;
  }
  SectionList->ncoords= Coord - SectionList->Coord;
  SectionList->nsections= Section - SectionList->Section;
  
#if 0
  UdmSectionListPrint(SectionList);
#endif
  
  return UDM_OK;
}


/*
  When we're searching for a multiword part,
  it is possible that
    wordnum >= args->WWList->nwords
  In this case "order" is not important.
  It will be later set to the correct value
  in UdmMultiWordAdd().
  
  In other cases "wordnum < args->WWList->nwords" should be true.
  TODO: add an assert about that.
*/
int
UdmURLCRDListListAddWithSort2(UDM_FINDWORD_ARGS *args,
                              UDM_URLCRDLIST *CoordList)
{
  UDM_SECTIONLIST SectionList;
  udm_wordnum_t wordnum= args->Word.order;
  udm_wordnum_t order= wordnum < args->WWList->nwords ?
                       args->WWList->Word[wordnum].order :
                       0 /* possible when in a multiword */;
  UdmURLCRDListToSectionList(&SectionList, CoordList, wordnum, order);
  UdmSectionListListAdd(&args->SectionListList, &SectionList);
  UDM_FREE(CoordList->Coords);
  return UDM_OK;
}



/************** multi word routines **************************/

static int
UdmSectionListListToURLCRDList(UDM_URLCRDLIST *List,
                               UDM_SECTIONLISTLIST *SrcList)
{
  size_t list, ncoords= 0, nsections= 0;
  UDM_URL_CRD *Crd;
  for (list=0; list < SrcList->nitems; list++)
  {
    /*UdmSectionListPrint(&SrcList->Item[list]);*/
    ncoords+= SrcList->Item[list].ncoords;
    nsections+= SrcList->Item[list].nsections;
  }
  bzero((void*) List, sizeof(*List));
  if (!(List->Coords= (UDM_URL_CRD*) UdmMalloc(ncoords * sizeof(UDM_URL_CRD))))
    return UDM_ERROR;
  for (Crd= List->Coords, list=0; list < SrcList->nitems; list++)
  {
    size_t sec;
    UDM_SECTIONLIST *SectionList= &SrcList->Item[list];
    for (sec=0; sec < SectionList->nsections; sec++)
    {
      UDM_SECTION *Section= &SectionList->Section[sec];
      size_t coord;
      for (coord= 0; coord < Section->ncoords; coord++)
      {
        UDM_COORD2 *Coord2= &Section->Coord[coord];
        Crd->url_id= Section->url_id;
        Crd->seclen= Section->seclen;
        Crd->pos= Coord2->pos;
        Crd->num= Section->wordnum;
        Crd->secno= Section->secno;
        Crd++;
      }
    }
  }
  UDM_ASSERT(ncoords == Crd - List->Coords);
  List->ncoords= ncoords;
  /*UdmCoordListPrint(List);*/

  return UDM_OK;
}



static int
UdmMultiWordMergeCoords(UDM_URLCRDLIST *Phrase, 
                        UDM_SECTIONLISTLIST *SrcList,
                        size_t orig_wordnum,
                        size_t nparts)
{
  UdmSectionListListToURLCRDList(Phrase, SrcList);
  UdmURLCRDListSortByURLThenSecnoThenPos(Phrase);
  UdmURLCRDListMergeMultiWord(Phrase, orig_wordnum, nparts);
  /*UdmCoordListPrint(Phrase);*/
  return UDM_OK;
}


int
UdmMultiWordAdd(UDM_FINDWORD_ARGS *args,
                UDM_SECTIONLISTLIST *OriginalSectionListList,
                size_t orig_wordnum, size_t nparts)
{
  UDM_URLCRDLIST Phrase;
  UDM_SECTIONLIST SectionList;
  bzero((void*) &Phrase, sizeof(Phrase));
  UdmMultiWordMergeCoords(&Phrase, &args->SectionListList, orig_wordnum, nparts);
  UDM_ASSERT(orig_wordnum < args->WWList->nwords);
  if (args->urls.nurls)
    UdmApplyFastLimit(&Phrase, &args->urls);
  if (Phrase.ncoords)
  {
    udm_wordnum_t order= args->WWList->Word[orig_wordnum].order;
    UdmURLCRDListToSectionList(&SectionList, &Phrase, orig_wordnum, order);
    UdmSectionListListAdd(OriginalSectionListList, &SectionList);
  }
  UDM_FREE(Phrase.Coords);
  args->Word.count= Phrase.ncoords;
  return UDM_OK;
}
