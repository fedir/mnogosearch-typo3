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

#ifdef HAVE_ZLIB

#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "udm_common.h"
#include "udm_contentencoding.h"
#include "udm_utils.h"
#include "udm_xmalloc.h"


size_t
UdmInflate(char *dst, size_t dstlen, const char *src, size_t srclen)
{
  z_stream z;
  z.zalloc= Z_NULL;
  z.zfree= Z_NULL;
  z.opaque= Z_NULL;
  z.next_in= (Byte *) src;
  z.next_out= (Byte *) dst;
  z.avail_in= srclen;
  z.avail_out= dstlen;
  
  if (inflateInit2(&z, 15) != Z_OK)
    return 0;
  
  inflate(&z, Z_FINISH);
  inflateEnd(&z);
  return z.total_out;
}


size_t
UdmDeflate(char *dst, size_t dstlen, const char *src, size_t srclen)
{
  z_stream z;
  z.zalloc= Z_NULL;
  z.zfree= Z_NULL;
  z.opaque= Z_NULL;
  z.next_in= (Byte*) src;
  z.next_out= (Byte*) dst;
  z.avail_in= srclen;
  z.avail_out= dstlen;
  
  if (deflateInit2(&z, 9, Z_DEFLATED, 15, 9, Z_DEFAULT_STRATEGY) != Z_OK)
    return 0;
  
  deflate(&z, Z_FINISH);
  deflateEnd(&z);
  return (size_t) z.total_out;
}


__C_LINK int __UDMCALL UdmDocInflate(UDM_DOCUMENT *Doc)
{

  z_stream zstream;
  size_t csize;
  void *pfree;
  
  if( (Doc->Buf.size) <= (size_t)(Doc->Buf.content-Doc->Buf.buf+6) )
    return -1;
    
  csize= Doc->Buf.size - (Doc->Buf.content-Doc->Buf.buf);

  zstream.zalloc = Z_NULL;
  zstream.zfree = Z_NULL;
  zstream.opaque = Z_NULL;

  inflateInit2(&zstream, -MAX_WBITS);

  if ((pfree = zstream.next_in = (Byte*)UdmMalloc((size_t)Doc->Buf.maxsize)) == NULL) {
    inflateEnd(&zstream);
    return -1;
  }
  if ((Doc->Buf.content[0] == (char)0x1f) && (Doc->Buf.content[1] == (char)0x8b)) {
    memcpy(zstream.next_in, &Doc->Buf.content[2], csize - 2);
    /* 2 bytes - header, 4 bytes - trailing CRC */
    zstream.avail_in = csize - 6;   
  } else {
    memcpy(zstream.next_in, Doc->Buf.content, csize);
    zstream.avail_in = csize;
  }

  zstream.next_out = (Byte*)Doc->Buf.content;
  zstream.avail_out = (uLong)(Doc->Buf.maxsize-(Doc->Buf.content-Doc->Buf.buf)-1);

  inflate(&zstream, Z_FINISH);
  inflateEnd(&zstream);

  UDM_FREE(pfree);
  if(zstream.total_out == 0)
    return -1;
  
  Doc->Buf.content[zstream.total_out]='\0';
  Doc->Buf.size=Doc->Buf.content-Doc->Buf.buf+zstream.total_out;
  return 0;
}


struct gztrailer {
    int crc32;
    int zlen;
};

__C_LINK int __UDMCALL UdmDocUnGzip(UDM_DOCUMENT *Doc)
{

  const unsigned char gzheader[10] = { 0x1f, 0x8b, Z_DEFLATED, 0, 0, 0, 0, 0, 0, 3 }, *cpData;
  z_stream zstream;
  Byte *buf;
  size_t csize, xlen;
  
  if( (Doc->Buf.size) <= (Doc->Buf.content-Doc->Buf.buf+sizeof(gzheader)) )
    return -1;

  /* check magic identificator */
  if (memcmp(Doc->Buf.content, gzheader, 2) != 0) return -1;

  csize= Doc->Buf.size - (Doc->Buf.content-Doc->Buf.buf);

  zstream.zalloc = Z_NULL;
  zstream.zfree = Z_NULL;
  zstream.opaque = Z_NULL;

  inflateInit2(&zstream, -MAX_WBITS);

  buf = zstream.next_in = (Byte*)UdmMalloc((size_t)Doc->Buf.maxsize);
  cpData = (Byte*)Doc->Buf.content + sizeof(gzheader);
  csize -= sizeof(gzheader);

  if (Doc->Buf.content[3] & 4) { /* FLG.FEXTRA */
    xlen = (unsigned char) cpData[1];
    xlen <<= 8; 
    xlen += (unsigned char) *cpData;
    cpData += xlen + 2;
    csize -= xlen + 2;
  }
  if (Doc->Buf.content[3] & 8) { /* FLG.FNAME */
    while (*cpData != '\0') cpData++,csize--;
    cpData++; csize--;
  }
  if (Doc->Buf.content[3] & 16) { /* FLG.FCOMMENT */
    while (*cpData != '\0') cpData++,csize--;
    cpData++; csize--;
  }
  if (Doc->Buf.content[3] & 2) { /* FLG.FHCRC */
    cpData += 2;
    csize -= 2;
  }

  memcpy(zstream.next_in, cpData, csize);
  zstream.next_out = (Byte*)Doc->Buf.content;
  zstream.avail_in = csize - sizeof(struct gztrailer);
  zstream.avail_out = (uLong)(Doc->Buf.maxsize-(Doc->Buf.content-Doc->Buf.buf)-1);

  inflate(&zstream, Z_FINISH);
  inflateEnd(&zstream);

  UDM_FREE(buf);

  Doc->Buf.content[zstream.total_out]='\0';
  Doc->Buf.size=Doc->Buf.content-Doc->Buf.buf+zstream.total_out;
  return 0;
}


__C_LINK int __UDMCALL UdmDocUncompress(UDM_DOCUMENT *Doc)
{
  Byte *buf;
  uLong   Len;
  size_t csize;
  int res;
  
  if( (Doc->Buf.size) <= (size_t)(Doc->Buf.content-Doc->Buf.buf) )
    return -1;
  csize = Doc->Buf.size - (Doc->Buf.content-Doc->Buf.buf);

  buf = (Byte*)UdmMalloc((size_t)Doc->Buf.maxsize);
  memcpy(buf, Doc->Buf.content, csize);
  
  Len = (uLong)(Doc->Buf.maxsize-(Doc->Buf.content-Doc->Buf.buf)-1);
  res = uncompress((Byte*)Doc->Buf.content, &Len, buf, csize);
  
  UDM_FREE(buf);
  
  if(res != Z_OK) return -1;
  
  Doc->Buf.content[Len]='\0';
  Doc->Buf.size=Doc->Buf.content-Doc->Buf.buf+Len;
  return 0;
}


#endif
