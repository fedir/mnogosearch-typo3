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

#ifndef _UDM_CONTENTENCODING_H
#define _UDM_CONTENTENCODING_H

/*
#define UDM_ENCODING_IDENTITY 0
#define UDM_ENCODING_DEFLATE  1
#define UDM_ENCODING_GZIP     2
#define UDM_ENCODING_COMPRESS 4
#define UDM_ENCODING_UNKNOWN  7
*/

extern __C_LINK int __UDMCALL UdmDocUnGzip(UDM_DOCUMENT *Doc);
extern __C_LINK int __UDMCALL UdmDocInflate(UDM_DOCUMENT *Doc);
extern __C_LINK int __UDMCALL UdmDocUncompress(UDM_DOCUMENT *Doc);

#endif
