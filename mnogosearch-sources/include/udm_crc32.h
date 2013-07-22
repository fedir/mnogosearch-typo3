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

#ifndef _UDM_CRC32_H
#define _UDM_CRC32_H

#include "udm_config.h"

/* Returns unsigned int crc32 of data block */
extern udmcrc32_t UdmCRC32(const char * buf, size_t size);

/* Updates previous value with new data block*/
extern udmcrc32_t UdmCRC32Update(int prev,const char * buf, size_t size);

/* Updates previous value with new Unicode block*/
extern udmcrc32_t UdmCRC32UpdateUnicode(int prev, const int *buf, size_t charlen);

/* Defines for null-terminated string */

#define UdmStrCRC32(buf)		UdmCRC32((buf),strlen(buf))

extern udmcrc32_t UdmCRC32UpdateStr(udmcrc32_t prev, const char *buf);

#endif /* _UDM_CRC32_H */
