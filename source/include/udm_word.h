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

#ifndef _UDM_WORD_H
#define _UDM_WORD_H

extern int UdmWordListAddEx(UDM_WORDLIST *List, const char *word,
                            size_t secno, size_t wordpos, size_t where);
extern int UdmWordListAdd(UDM_DOCUMENT* Doc,char *word,int where);
extern int UdmWordListFree(UDM_WORDLIST * List);
extern int UdmWordListSaveSectionSize(UDM_DOCUMENT *Doc);
extern UDM_WORDLIST * UdmWordListInit(UDM_WORDLIST * List);
extern void UdmWordListReset(UDM_WORDLIST *List);

extern void UdmWideWordInit(UDM_WIDEWORD *W);
extern void UdmWideWordFree(UDM_WIDEWORD *W);

extern UDM_WIDEWORDLIST * UdmWideWordListInit(UDM_WIDEWORDLIST * List);
extern size_t UdmWideWordListAdd(UDM_WIDEWORDLIST * List,UDM_WIDEWORD * Word);
extern size_t UdmWideWordListAddLike(UDM_WIDEWORDLIST *WWList,
                                     UDM_WIDEWORD *orig, const char *word);
extern size_t UdmWideWordListAddForStat(UDM_WIDEWORDLIST * List,UDM_WIDEWORD * Word);
extern void UdmWideWordListFree(UDM_WIDEWORDLIST * List);
extern int UdmWideWordListCopy(UDM_WIDEWORDLIST *Dst, UDM_WIDEWORDLIST *Src);

extern void UdmWordListListInit(UDM_WORDLISTLIST *LL);
extern void UdmWordListListReset(UDM_WORDLISTLIST *LL);
extern void UdmWordListListFree(UDM_WORDLISTLIST *LL);

#endif
