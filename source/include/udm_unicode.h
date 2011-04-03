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

#ifndef UDM_UNICODE_H
#define UDM_UNICODE_H

extern __C_LINK size_t __UDMCALL UdmUniLen(register const int * u);
int    UdmUniStrCmp(register const int * s1, register const int * s2);
int    UdmUniStrBCmp(const int *s1, const int *s2);
int    UdmUniStrBNCmp(const int *s1, const int *s2, size_t count);

int   *UdmUniStrCpy(int *dst, const int *src);
int   *UdmUniStrNCpy(int *dst, const int *src, size_t len);
int   *UdmUniStrCat(int *s, const int *append);
int   *UdmUniDup(const int * s);
int   *UdmUniNDup(const int * s, size_t len);

#endif
