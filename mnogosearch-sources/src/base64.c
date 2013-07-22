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


static char base64[]= "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


int __UDMCALL UdmHex2Int(int h)
{
  if((h>='0')&&(h<='9'))return(h-'0');
  if((h>='A')&&(h<='F'))return(h-'A'+10);
  if((h>='a')&&(h<='f'))return(h-'a'+10);
  return 0;
}


int __UDMCALL UdmInt2Hex(int i)
{
  if((i>=0)&&(i<=9))return(i+'0');
  if((i>=10)&&(i<=15))return(i+'A'-10);
  return '0';
}	


char *
udm_rfc1522_decode(char * dst, const char *src)
{
  const char *s;
  char  *d;

  s=src;
  d=dst;
  *dst=0;

  while(*s){
    char *e;

    if((e=strstr(s,"=?"))){
      char *schema;
      char *data;

      if(e>s){
        /* Copy last plain text buffer */
        strncpy(d,s,(size_t)(e-s));
        d+=(e-s);
        *d=0;
      }
      e+=2;
      if(!(schema=strchr(e,'?'))){
        /* Error */
        break;
      }
      schema++;
      data=schema+2;

      if(!(e=strstr(data,"?="))){
        /* This is an error */
        break;
      }
      switch(*schema){
        case 'Q':
        case 'q':
          while(data<e){
            char c;
            if(*data=='='){
              c=(char)(UdmHex2Int(data[1])*16+UdmHex2Int(data[2]));
              data+=3;
            }else{
              c=data[0];
              data++;
            }
            /* Copy one char to dst */
            *d=c;d++;*d=0;
          }
          break;
        case 'B':
        case 'b':
          while(data<e){
            char *p;
            int x0,x1,x2,x3,res;

            p=strchr(base64,data[0]);x0=p?p-base64:0;
            p=strchr(base64,data[1]);x1=p?p-base64:0;
            p=strchr(base64,data[2]);x2=p?p-base64:0;
            p=strchr(base64,data[3]);x3=p?p-base64:0;

            res=x3+x2*64+x1*64*64+x0*64*64*64;

            p=(char*)(&res);

            if(p[2])*d=p[2];d++;*d=0;
            if(p[1])*d=p[1];d++;*d=0;
            if(p[0])*d=p[0];d++;*d=0;

            data+=4;
          }
          break;
        default:
          /* Error */
          schema=NULL;
          break;
      }
      if(schema==NULL){
        /* Error */
        break;
      }
      s=e+2;
    }else{
      /* Copy plain text tail */
      strcpy(d,s);
      break;
    }
  }
  return(dst);
}


/************* Base 64 *********************************************/
/* BASE64 encoding converts  3x8 bits into 4x6 bits                */

__C_LINK size_t __UDMCALL
udm_base64_encode (const char *src, char *store, size_t length)
{
  unsigned char *p= (unsigned char *)store;
  unsigned const char *s= (unsigned const char *)src;

  for ( ; length > 2 ; length-= 3, s+= 3)
  {
    *p++ = base64[s[0] >> 2];
    *p++ = base64[((s[0] & 3) << 4) + (s[1] >> 4)];
    *p++ = base64[((s[1] & 0xf) << 2) + (s[2] >> 6)];
    *p++ = base64[s[2] & 0x3f];
  }

  if (length > 0)
  {
    *p++= base64[s[0] >> 2];
    if (length > 1)
    {
      *p++ = base64[((s[0] & 3) << 4) + (s[1] >> 4)];
      *p++ = base64[(s[1] & 0xf) << 2];
      *p++ = '=';
    }
    else
    {
      *p++ = base64[(s[0] & 0x03) << 4];
      *p++ = '=';
      *p++ = '=';
    }
  }

  *p= '\0';
  return p - (unsigned char*)store;
}


static char from_base64[]=
{
/*00*/   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/*10*/   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/*20*/   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,62, 0, 0, 0,63, /*  !"#$%&'()*+,-./ */
/*30*/  52,53,54,55,56,57,58,59,60,61, 0, 0, 0, 0, 0, 0, /* 0123456789:;<=>? */
/*40*/   0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14, /* @ABCDEFGHIJKLMNO */
/*50*/  15,16,17,18,19,20,21,22,23,24,25, 0, 0, 0, 0, 0, /* PQRSTUVWXYZ[\]^_ */
/*60*/   0,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40, /* `abcdefghijklmno */
/*70*/  41,42,43,44,45,46,47,48,49,50,51, 0, 0, 0, 0, 0, /* pqrstuvwxyz{|}~  */
/*80*/   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/*90*/   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/*A0*/   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/*B0*/   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/*C0*/   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/*D0*/   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/*E0*/   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/*F0*/   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


__C_LINK size_t __UDMCALL
udm_base64_decode(char *dst, const char *src, size_t len)
{
  char *dst0= dst;
  
  for( ; *src && len > 3 ; len-= 4)
  {
    int b0= from_base64[(unsigned char) *src++];
    int b1= from_base64[(unsigned char) *src++];
    int b2= from_base64[(unsigned char) *src++];
    int b3= from_base64[(unsigned char) *src++];
    int res= b3 + b2*64 + b1*64*64 + b0*64*64*64;

    *dst++= (res>>16)&0xFF;
    *dst++= (res>>8)&0xFF;
    *dst++= res&0xFF;
  }
  *dst= '\0';
  return dst - dst0;
}

/***** Another implementation which can handle multi-line base64 data ***/
/*
  TODO34: join with the above?
*/

typedef struct udm_from_base64_st
{
  const char *src;           /* Current input position               */
  const char *end;           /* End of the input buffer              */
  unsigned int bits;         /* Collect bits into this number        */
  udm_bool_t error;          /* Error code                           */
  unsigned char byte_count;  /* Number of characters scanned (0..3)  */
  unsigned char mark_count;  /* Number of pad marks scanned          */
} UDM_FROM_BASE64;


#define B64SP  (-2)
#define B64ER  (-1)
#define UDM_BASE64_PAD_CHAR  ('=')


/**
  Base64 character types:
  -2       space character
  -1       bad characters
  0..63    valid base64 encoding characters
*/
static signed char
from_base64_map[]=
{
/*00*/  -1,-1,-1,-1,-1,-1,-1,-1,-1,-2,-2,-2,-2,-2,-1,-1,
/*10*/  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/*20*/  -2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63, /*  !"#$%&'()*+,-./ */
/*30*/  52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1, /* 0123456789:;<=>? */
/*40*/  -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14, /* @ABCDEFGHIJKLMNO */
/*50*/  15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1, /* PQRSTUVWXYZ[\]^_ */
/*60*/  -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40, /* `abcdefghijklmno */
/*70*/  41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1, /* pqrstuvwxyz{|}~  */
/*80*/  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/*90*/  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/*A0*/  -2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* A0 is nbsp */
/*B0*/  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/*C0*/  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/*D0*/  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/*E0*/  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/*F0*/  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};


/**
  Return true if a character is not a space character.
*/
static inline udm_bool_t
udm_base64_is_not_space(unsigned int ch)
{
  return from_base64_map[ch] != B64SP;
}


/**
  Return true if a character is not a valid base64 encoding character.
*/
static inline udm_bool_t
udm_base64_is_bad_encoding_char(unsigned int ch)
{
  return from_base64_map[ch] < 0;
}


/**
  Skip leading spaces in a base64 encoded stream, so
  d->src points either to the first non-space character,
  or to the end of the input string.
  In case when end-of-input met on an unexpected position,
  d->error is set to UDM_TRUE.

  @param  d    Pointer to a UDM_FROM_BASE64 structure.

  @return
    UDM_FALSE on success (there are some more non-space input characters)
    UDM_TRUE  on error   (end-of-input found)
*/
static inline udm_bool_t
udm_from_base64_skip_spaces(UDM_FROM_BASE64 *d)
{
  for ( ; d->src < d->end; d->src++)
  {
    if (udm_base64_is_not_space((unsigned char) *d->src))
      return UDM_FALSE;
  }
  /* End of input found */
  if (d->byte_count > 0)
    d->error= UDM_TRUE; /* Unexpected end-of-input */
  return UDM_TRUE;
}


/**
  Convert the next input character to a number in the range [0..63]
  and mix it with the previously collected values in d->bits.

  @param d   a UDM_FROM_BASE64 structure

  @return
    UDM_FALSE on success
    UDM_TRUE  on error (invalid base64 character found)
*/
static inline udm_bool_t
udm_from_base64_add(UDM_FROM_BASE64 *d)
{
  int value;
  d->bits <<= 6;
  value= (unsigned char) *(d->src++);
  if (udm_base64_is_bad_encoding_char(value))
    return (d->error= UDM_TRUE);
  d->bits+= (unsigned int) from_base64_map[value];
  return UDM_FALSE;
}


/**
  Get the next character from the source.
  Skip spaces, then scan the next base64 encoding character
  or pad character and collect bits into d->bits.

  @param     d   Pointer to a UDM_FROM_BASE64 structure.
  @return
   UDM_FALSE on success (a valid base64 encoding character found)
   UDM_TRUE  on error (unexpected character or unexpected end-of-input found)
*/
static udm_bool_t /* Inlining makes "gcc -O3" produce a warning. */
udm_from_base64_getch(UDM_FROM_BASE64 *d)
{
  if (udm_from_base64_skip_spaces(d))
    return UDM_TRUE; /* End of the input stream */

  if (!udm_from_base64_add(d)) /* Valid base64 character found */
  {
    if (d->mark_count)
    {
      /* If we have scanned pad character already, then only pad is valid */
      UDM_ASSERT(d->byte_count == 3); /* Should have exited on error earlier */
      d->error= UDM_TRUE;
      d->src--;
      return UDM_TRUE; /* wanted pad character, but encoding character found */
    }
    d->byte_count++;
    return UDM_FALSE;
  }

  /* Could not scan a valid encoding character, process error */
  switch (d->byte_count)
  {
  case 0:
  case 1:
    d->src--;
    return UDM_TRUE; /* Error: base64 character expected */
    break;

  case 2:
  case 3:
    if (d->src[-1] == UDM_BASE64_PAD_CHAR)
    {
      d->error= UDM_FALSE; /* Pad character found, not really an error */
      d->mark_count++;
    }
    else
    {
      d->src--;
      return UDM_TRUE; /* Error: Base64 character or pad character expected */
    }
    break;

  default:
    UDM_ASSERT(0);
    return UDM_TRUE; /* Wrong state, should not happen */
  }

  d->byte_count++;
  return UDM_FALSE;
}


/**
  Decode a base64 string.
  The base64-encoded data pointed by "src" will be decoded to "dst".
  "dst" must have enough space to store the decoded data.

  Decoding will stop after "len" characters read from "src",
  or if padding is met in the source. 
  If "end" is non-null, it will be set to the position
  after the last read character from "src".

  @param src     Pointer to a base64 encoded string.
  @param len     Length of string in "src".
  @param dst     Where to write the decoded data.
  @param end     If "end" is not null, it will be set 
                 to the last character read from "src".
  @flags         flags
  @return        Number of bytes written at 'dst', or -1 in case of failure.
*/
int
UdmBase64Decode(const char *src, size_t len,
                void *dst0, const char **end, int flags)
{
  UDM_FROM_BASE64 d;
  char *dst= (char*) dst0;

  d.src= src;
  d.end= src + len;
  d.error= UDM_FALSE;
  d.mark_count= 0;

  for ( ; ; )
  {
    d.bits= d.byte_count= 0;

    /* Scan the next four characters from the source */
    if (udm_from_base64_getch(&d) || udm_from_base64_getch(&d) ||
        udm_from_base64_getch(&d) || udm_from_base64_getch(&d))
      break;

    /* And put three characters in the destination */
    *dst++= (d.bits >> 16) & 0xff;
    *dst++= (d.bits >>  8) & 0xff;
    *dst++= (d.bits >>  0) & 0xff;

    if (d.mark_count)
    {
      dst-= d.mark_count;
      if (!(flags & UDM_BASE64_ALLOW_MULTIPLE_CHUNKS))
        break;
      d.mark_count= 0;
    }
  }

  /* Return error if there are any non-space characters left */
  d.byte_count= 0;
  if (!udm_from_base64_skip_spaces(&d))
    d.error= UDM_TRUE;

  if (end != NULL)
    *end= d.src;

  return (d.error != UDM_FALSE) ? -1 : (int) (dst - (char *) dst0);
}

