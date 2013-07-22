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

#ifndef _UDM_URL_H
#define _UDM_URL_H

#define UDM_URL_OK	0
#define UDM_URL_LONG	1
#define UDM_URL_BAD	2

#define UDM_URL_CANONIZE_MULTIPLY 3

extern __C_LINK UDM_URL * __UDMCALL UdmURLInit(UDM_URL *url);
extern __C_LINK void __UDMCALL UdmURLFree(UDM_URL *url);
extern int UdmURLParse(UDM_URL *url,const char *s);
extern const char *UdmURLErrorStr(int rc);
extern char * UdmURLNormalizePath(char * path);
extern size_t UdmURLCanonize(const char *src, char *dst, size_t dstsize);
extern size_t UdmURLCanonizePath(char *dst, size_t dstsize, const char *src);

/* Functions from idn.c */
extern size_t UdmIDNEncode(UDM_CHARSET *cs, const char *src, char *dst, size_t dstlen);
extern size_t UdmIDNDecode(UDM_CHARSET *cs, const char *src, char *dst, size_t dstlen);

#endif

