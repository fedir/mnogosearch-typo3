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

#ifndef UDM_GUESSER_H
#define UDM_GUESSER_H

#include "udm_uniconv.h"

/* Structure to sort guesser results */
typedef struct {
     UDM_LANGMAP * map;
     size_t hits, miss;
} UDM_MAPSTAT;

extern int UdmLMstatcmp(const void * i1, const void * i2);
extern int UdmLMcmpCount(UDM_LANGITEM *m1, UDM_LANGITEM *m2);
extern int UdmLMcmpIndex(UDM_LANGITEM *m1, UDM_LANGITEM *m2);

extern void  UdmBuildLangMap(UDM_LANGMAP * map,const char * text,size_t text_len, int StrFlag);
extern void  UdmPrepareLangMap(UDM_LANGMAP * map);
extern void  UdmCheckLangMap(UDM_LANGMAP * map, UDM_LANGMAP * text, UDM_MAPSTAT *mstat, size_t InfMiss);
extern void  UdmLangMapListFree(UDM_LANGMAPLIST *);
extern void UdmLangMapListSave(UDM_LANGMAPLIST *List);
#ifdef _UDM_COMMON_H
extern  int  UdmGuessCharSet(UDM_AGENT *Indexer, UDM_DOCUMENT * D, UDM_LANGMAPLIST *L, UDM_LANGMAP *M);
#endif

extern int   UdmLoadLangMapList(UDM_LANGMAPLIST *L, const char * mapdir);
extern __C_LINK int __UDMCALL UdmLoadLangMapFile(UDM_LANGMAPLIST *L, const char * mapname);

extern const char *UdmGuessContentType(const char *beg, size_t len, const char *def);

#endif
