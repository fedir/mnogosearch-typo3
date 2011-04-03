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

#ifndef _UDM_MATCH_H
#define _UDM_MATCH_H

extern int  UdmMatchComp(UDM_MATCH *Match,char *errstr,size_t errstrsize);
extern void UdmMatchFree(UDM_MATCH *Match);
extern int  UdmMatchExec(UDM_MATCH *Match, const char *string, size_t stringlen,
                         const char *net_string, size_t nparts,UDM_MATCH_PART *Parts);
extern size_t UdmMatchApply(char *res,size_t ressize,const char *src,const char *rpl,UDM_MATCH *Match,size_t nparts, UDM_MATCH_PART *Parts);

extern UDM_MATCHLIST *UdmMatchListInit(UDM_MATCHLIST *L);
extern void UdmMatchListFree(UDM_MATCHLIST *L);
extern int  UdmMatchListAdd(UDM_AGENT *A, UDM_MATCHLIST *L, UDM_MATCH *M, char *err, size_t errsize, int ordre);

extern __C_LINK UDM_MATCH * __UDMCALL UdmMatchListFind(UDM_MATCHLIST * List,const char *string,size_t nparts, UDM_MATCH_PART *Parts);
extern __C_LINK UDM_MATCH * __UDMCALL UdmMatchSectionListFind(UDM_MATCHLIST * List, UDM_DOCUMENT *Doc, size_t nparts, UDM_MATCH_PART *Parts);

extern const char *UdmMatchTypeStr(int m);

extern UDM_MATCH *UdmMatchInit(UDM_MATCH *M);

#endif
