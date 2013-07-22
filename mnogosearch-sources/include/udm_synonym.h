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

#ifndef _UDM_SYNONYM_H
#define _UDM_SYNONYM_H

extern void UdmSynonymListInit(UDM_SYNONYMLIST * List);
extern __C_LINK int __UDMCALL UdmSynonymListLoad(UDM_ENV * Env,const char * filename);
extern void UdmSynonymListFree(UDM_SYNONYMLIST * List);
extern __C_LINK void __UDMCALL UdmSynonymListSort(UDM_SYNONYMLIST * List);

extern void UdmSynonymListListInit(UDM_SYNONYMLISTLIST *L);
extern void UdmSynonymListListFree(UDM_SYNONYMLISTLIST *L);
extern int  UdmSynonymListListAdd(UDM_SYNONYMLISTLIST *L, UDM_SYNONYMLIST *I);
extern UDM_WIDEWORDLIST *UdmSynonymListListFind(const UDM_SYNONYMLISTLIST *List,
                                                UDM_WIDEWORD *wword);
extern UDM_WIDEWORDLIST *UdmSynonymListFind(UDM_WIDEWORDLIST *Res,
                                            const UDM_SYNONYMLIST *List,
                                            const UDM_WIDEWORD *wword);
extern __C_LINK void __UDMCALL UdmSynonymListListSortItems(UDM_SYNONYMLISTLIST * List);

extern size_t UdmMultiWordPhraseLength(const char *word);

#endif
