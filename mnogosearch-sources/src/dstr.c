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

#include "udm_config.h"

#include <string.h>

#include "udm_common.h"
#include "udm_utils.h"


UDM_DSTR *
UdmDSTRInit(UDM_DSTR *dstr, size_t size_page)
{
  if (!size_page)
    return NULL;

  if (! dstr)
  {
    dstr= UdmMalloc(sizeof(UDM_DSTR));
    if (! dstr)
      return NULL;
    dstr->free = 1;
  }
  else
  {
    dstr->free = 0;
  }

  dstr->data= UdmMalloc(size_page);
  if (!dstr->data)
  {
    if (dstr->free)
      UdmFree(dstr);
    return NULL;
  }
  dstr->data[0]= 0;
  dstr->size_page= size_page;
  dstr->size_data= 0;
  dstr->size_total= size_page;
  return dstr;
}


void
UdmDSTRFree(UDM_DSTR *dstr)
{
  dstr->size_total= dstr->size_data= 0;
  UdmFree(dstr->data);
  if (dstr->free)
    UdmFree(dstr);
}


int
UdmDSTRAlloc
(UDM_DSTR *dstr, size_t size_data)
{
  size_t asize;
 
  if (size_data <= dstr->size_total)
    return UDM_OK;
   
  asize= (size_data / dstr->size_page + 1) * dstr->size_page;
  dstr->size_total= dstr->size_data= 0;
  UdmFree(dstr->data);
  if (!(dstr->data= UdmMalloc(asize)))
    return UDM_ERROR;
  dstr->size_total= asize;
  return UDM_OK;
}


int
UdmDSTRRealloc(UDM_DSTR *dstr, size_t size_data)
{
  char *tmp;
  size_t asize;
 
  if (size_data <= dstr->size_total)
    return UDM_OK;
  asize= (size_data / dstr->size_page + 1) * dstr->size_page;
  tmp= UdmRealloc(dstr->data, asize);
  if (!tmp)
    return UDM_ERROR;
  dstr->size_total= asize;
  dstr->data= tmp;
  return UDM_OK;
}

size_t
UdmDSTRAppend(UDM_DSTR *dstr, const char *data, size_t size_data)
{
  size_t bytes_left= dstr->size_total - dstr->size_data;
  size_t asize;
  char *tmp;

  if (!data || !size_data)
    return 0;

  if (bytes_left <= size_data)
  {
    asize= dstr->size_total + ((size_data - bytes_left) / dstr->size_page + 1) * dstr->size_page;
    tmp= UdmRealloc(dstr->data, asize);
    if (!tmp)
      return 0;
    dstr->data= tmp;
    dstr->size_total= asize;
  }
  memcpy(dstr->data + dstr->size_data, data, size_data);
  dstr->size_data+= size_data;
  dstr->data[dstr->size_data]= 0;
  return size_data;
}


static char hex_digits[]= "0123456789ABCDEF";

/*
  This function does not put trailing \x00 character.
*/
size_t
UdmDSTRAppendHex(UDM_DSTR *dstr, const char *s, size_t slen)
{
  char *d;
  size_t new_size= dstr->size_data + 2 * slen;
  if (!slen || (UDM_OK != UdmDSTRRealloc(dstr, new_size)))
    return 0;
  for (d= dstr->data + dstr->size_data; slen; slen--, s++)
  {
    unsigned char ch= *((const unsigned char*) s);
    *d++= hex_digits[(ch >> 4) & 0x0F];
    *d++= hex_digits[ch & 0x0F];
  }
  dstr->size_data= new_size;
  return dstr->size_data;
}


size_t
UdmDSTRAppendSTR(UDM_DSTR *dstr, const char *data)
{
  return UdmDSTRAppend(dstr, data, strlen(data));
}


size_t
UdmDSTRAppendINT4(UDM_DSTR *s, int i)
{
  char buf[4];
  udm_put_int4(i, buf);
  return UdmDSTRAppend(s, buf, 4);
}


size_t
UdmDSTRAppendINT2(UDM_DSTR *s, int i)
{
  char buf[2];
  buf[0]= (char) (((unsigned int) i) & 0xFF);
  buf[1]= (char) (((unsigned int) i) >> 8);
  return UdmDSTRAppend(s, buf, 2);
}


int
UdmDSTRAppendf(UDM_DSTR *dstr, const char *fmt, ...)
{
  va_list ap;
  size_t bytes_left, asize;
  int nc;
  char *tmp;

  while (1)
  {
    bytes_left= dstr->size_total - dstr->size_data;
    va_start(ap, fmt);
    nc= vsnprintf(dstr->data + dstr->size_data, bytes_left, fmt, ap);
    va_end(ap);
    
    if (nc > -1 && nc + 1 < bytes_left)
      break;

    if (nc < 0 || nc + 1 == bytes_left)
      asize= dstr->size_total + dstr->size_page;
    else
      asize= dstr->size_total + ((nc - bytes_left) / dstr->size_page + 1) * dstr->size_page;

    tmp= UdmRealloc(dstr->data, asize);
    if (!tmp)
    {
      nc= 0;
      break;
    }
    dstr->size_total= asize;
    dstr->data= tmp;
  }

  dstr->size_data += nc;
  return(nc);
}

void
UdmDSTRReset(UDM_DSTR *dstr)
{
  dstr->size_data= 0;
}
