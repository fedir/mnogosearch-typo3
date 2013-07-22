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

#ifndef _UDM_STOPWORDS_H
#define _UDM_STOPWORDS_H

extern UDM_STOPWORD  *UdmStopListFind(UDM_STOPLIST *,const char *word, const char *lang);
extern void           UdmStopListFree(UDM_STOPLIST *);
extern void           UdmStopListSort(UDM_STOPLIST *);
extern __C_LINK int __UDMCALL UdmStopListLoad(UDM_ENV *,const char *fname);
extern int            UdmStopListAdd(UDM_STOPLIST *,UDM_STOPWORD *);

extern void UdmStopListListInit(UDM_STOPLISTLIST *Lst);
extern void UdmStopListListFree(UDM_STOPLISTLIST *Lst);
extern int  UdmStopListListAdd(UDM_STOPLISTLIST *Lst, UDM_STOPLIST *Item);
extern UDM_STOPWORD *UdmStopListListFind(UDM_STOPLISTLIST *Lst,
                                         const char *word, const char *lang);
#endif
