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

#ifndef _UDM_SGML_H
#define _UDM_SGML_H

extern __C_LINK char * __UDMCALL UdmSGMLUnescape(char * str);
extern size_t UdmHTMLEncode(char *d, size_t dlen, const char *s, size_t slen);
extern void UdmSGMLUniUnescape(int * ustr);
extern int UdmSgmlToUni(const char *sgml);
extern int UdmSGMLScan(int *wc,
                       const unsigned char *str, const unsigned char *strend);
#endif
