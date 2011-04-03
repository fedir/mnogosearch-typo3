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
#include <string.h>
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

