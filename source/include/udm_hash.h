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

#ifndef _UDM_HASH_H
#define _UDM_HASH_H

#include "udm_config.h"

/* Returns unsigned  hash64 of data block */
extern udmhash64_t UdmHash64(const char * buf, size_t size);
extern __C_LINK udmhash32_t __UDMCALL UdmHash32(const char * buf, size_t size);

#define UdmStrHash64(buf)		UdmHash64((buf), strlen(buf))
#define UdmStrHash32(buf)		UdmHash32((buf), strlen(buf))

typedef struct udm_hash_st
{
  void *base;
  size_t nmemb;
  size_t size;
  size_t (*key)(const void *);
  int (*join)(void *, void *);
} UDM_HASH;

typedef int (*udm_hash_join)(void*, void*);

void
UdmHashInit(UDM_HASH *hash, void* base, size_t nmemb, size_t size,
            size_t(*key)(const void*),
            int(*join)(void *, void *));

size_t
UdmHashSize(size_t n);

void
UdmHashPut(UDM_HASH *hash, void *ptr);



#endif
