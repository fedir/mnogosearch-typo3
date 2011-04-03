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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "udm_uniconv.h"
#include "udm_unicode.h"


/* Calculates UNICODE string length */

size_t __UDMCALL UdmUniLen(register const int * u){
	register size_t ulen=0;
	while(*u++)ulen++;
	return(ulen);
}

/* Allocates a copy of unicode string */

int * UdmUniDup(const int * s){
	int * res;
	size_t size;
	
	size=(UdmUniLen(s)+1)*sizeof(*s);
	if((res=(int*)UdmMalloc(size))==NULL)
		return(NULL);
	memcpy(res,s,size);
	return(res);
}

int * UdmUniNDup(const int *s, size_t len) {
	int *res;
	size_t size = UdmUniLen(s);
	if (size > len) size = len;
	if((res = (int*)UdmMalloc((size + 1) * sizeof(*s))) == NULL) return(NULL);
	memcpy(res, s, size * sizeof(*s));
	res[size] = 0;
	return res;
}


/* Compare unicode strings */

int UdmUniStrCmp(register const int * s1, register const int * s2)
{
	while (*s1 == *s2++)
		if (*s1++ == 0)
			return (0);
	return (*s1 - *(s2 - 1));
}


/* backward unicode string compaire */
int UdmUniStrBCmp(const int *s1, const int *s2) { 
  register ssize_t l1 = UdmUniLen(s1)-1, l2 = UdmUniLen(s2)-1;
  while (l1 >= 0 && l2 >= 0) {
    if (s1[l1] < s2[l2]) return -1;
    if (s1[l1] > s2[l2]) return 1;
    l1--;
    l2--;
  }
  if (l1 < l2) return -1;
  if (l1 > l2) return 1;
/*  if (*s1 < *s2) return -1;
  if (*s1 > *s2) return 1;*/
  return 0;
}

int UdmUniStrBNCmp(const int *s1, const int *s2, size_t count) { 
  register ssize_t l1 = UdmUniLen(s1) - 1, l2 = UdmUniLen(s2) - 1, l = count;
  while (l1 >= 0 && l2 >= 0 && l > 0) {
    if (s1[l1] < s2[l2]) return -1;
    if (s1[l1] > s2[l2]) return 1;
    l1--;
    l2--;
    l--;
  }
  if (l == 0) return 0;
  if (l1 < l2) return -1;
  if (l1 > l2) return 1;
  if (*s1 < *s2) return -1;
  if (*s1 > *s2) return 1;
  return 0;
}

/* string copy */
int *UdmUniStrCpy(int *dst, const int *src) {
  register int *d = dst; register const int *s = src;
  while (*s) {
    *d = *s; d++; s++;
  }
  *d = *s;
  return dst;
}
int *UdmUniStrNCpy(int *dst, const int *src, size_t len) {
  register int *d = dst; register const int *s = src; register size_t l = len;
  while (*s && l) {
    *d = *s; d++; s++;
    l--;
  }
  if (l) *d = *s;
  return dst;
}
/* string append */
int *UdmUniStrCat(int *s, const int *append) {
  size_t len = UdmUniLen(s);
  UdmUniStrCpy(&s[len], append);
  return s;
}

