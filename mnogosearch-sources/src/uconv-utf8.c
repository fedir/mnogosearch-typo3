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

#include <stdio.h>
#include <string.h>
#include "udm_uniconv.h"
#include "udm_sgml.h"

/* UTF8 RFC 2279 */

/*
UTF-8 encoding components
-------------------------
[00-7F] ASCII characters, 00-7F, 128 characters.
[80-BF] Tail characters.
[C0-C1] Overlong encoding: 2 byte sequence, but code point <= 127
[C2-DF] Two-byte lead characters, 000080-0007FF, 1920 codes.
[E0-EF] Three-byte lead characte, 000800-00FFFF, 63488 codes
[F0-F4] Four-byte lead character, 010000–10FFFF, 1048576 codes
[F5-F7] Four-byte lead character, codepoints above 10FFFF
[F8-FB] Five-byte lead character, 00200000-03FFFFFF
[FC-FD] Six-byte lead character.  04000000-7FFFFFFF
[FE-FF] Invalid: reserved for lead bytes of a 7 or 8 byte sequence

  Char. number range  |        UTF-8 octet sequence
     (hexadecimal)    |              (binary)
  --------------------+---------------------------------------------
1 0000 0000-0000 007F | 0xxxxxxx
2 0000 0080-0000 07FF | 110xxxxx 10xxxxxx
3 0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
4 0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
5 0020 0000-03FF FFFF | 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
6 0400 0000-7FFF FFFF | 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
*/


__C_LINK int __UDMCALL
udm_mb_wc_utf8 (UDM_CONV *conv, UDM_CHARSET *cs,int *pwc,
          const unsigned char *s,
          const unsigned char *e)
{
  unsigned char c= s[0];
  int n=e-s;
  
  if (c < 0x80)
  {
    if (*s == '&' && (conv->flags & UDM_RECODE_HTML_SPECIAL))
      return UdmSGMLScan(pwc, s, e);
    *pwc = c;
    return 1;
  } else if (c < 0xc2) {
    return UDM_CHARSET_ILSEQ;
  } else if (c < 0xe0) {
    if (n < 2)return UDM_CHARSET_TOOFEW(0);
    if (!((s[1] ^ 0x80) < 0x40))return UDM_CHARSET_ILSEQ2;
    *pwc = ((unsigned int) (c & 0x1f) << 6) | (unsigned int) (s[1] ^ 0x80);
    return 2;
  } else if (c < 0xf0) {
    if (n < 3)return UDM_CHARSET_TOOFEW(0);
    if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40 && (c >= 0xe1 || s[1] >= 0xa0)))
      return UDM_CHARSET_ILSEQ3;
    *pwc = ((unsigned int) (c & 0x0f) << 12) | ((unsigned int) (s[1] ^ 0x80) << 6) | (unsigned int) (s[2] ^ 0x80);
    return 3;
  } else if (c < 0xf8 && sizeof(unsigned int)*8 >= 32) {
    if (n < 4)return UDM_CHARSET_TOOFEW(0);
    if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40 && (s[3] ^ 0x80) < 0x40 && (c >= 0xf1 || s[1] >= 0x90)))
      return UDM_CHARSET_ILSEQ4;
    *pwc = ((unsigned int) (c & 0x07) << 18) | ((unsigned int) (s[1] ^ 0x80) << 12) | ((unsigned int) (s[2] ^ 0x80) << 6) | (unsigned int) (s[3] ^ 0x80);
    return 4;
  } else if (c < 0xfc && sizeof(unsigned int)*8 >= 32) {
    if (n < 5)return UDM_CHARSET_TOOFEW(0);
    if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40 && (s[3] ^ 0x80) < 0x40 && (s[4] ^ 0x80) < 0x40 && (c >= 0xf9 || s[1] >= 0x88)))
      return UDM_CHARSET_ILSEQ5;
    *pwc = ((unsigned int) (c & 0x03) << 24)
      | ((unsigned int) (s[1] ^ 0x80) << 18)
      | ((unsigned int) (s[2] ^ 0x80) << 12)
      | ((unsigned int) (s[3] ^ 0x80) << 6)
      | (unsigned int) (s[4] ^ 0x80);
    return 5;
  } else if (c < 0xfe && sizeof(unsigned int)*8 >= 32) {
     if (n < 6)return UDM_CHARSET_TOOFEW(0);
    if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40
      && (s[3] ^ 0x80) < 0x40 && (s[4] ^ 0x80) < 0x40
      && (s[5] ^ 0x80) < 0x40
      && (c >= 0xfd || s[1] >= 0x84)))
      return UDM_CHARSET_ILSEQ6;
    *pwc = ((unsigned int) (c & 0x01) << 30)
      | ((unsigned int) (s[1] ^ 0x80) << 24)
      | ((unsigned int) (s[2] ^ 0x80) << 18)
      | ((unsigned int) (s[3] ^ 0x80) << 12)
      | ((unsigned int) (s[4] ^ 0x80) << 6)
      | (unsigned int) (s[5] ^ 0x80);
    return 6;
  } else
    return UDM_CHARSET_ILSEQ;
}



__C_LINK int __UDMCALL
udm_wc_mb_utf8(UDM_CONV *conv, UDM_CHARSET *cs, int *wc, unsigned char *r, unsigned char *e)
{
  int count;

  if (*wc < 0x80) {
    r[0] = *wc;
    if ((conv->flags & UDM_RECODE_HTML_SPECIAL) &&
        (r[0] == '"' || r[0] == '&' || r[0] == '<' || r[0] == '>')) 
      return UDM_CHARSET_ILUNI;
    return 1;
  }
  else if (*wc < 0x800) 
    count = 2;
  else if (*wc < 0x10000) 
    count = 3;
  else if (*wc < 0x200000) 
    count = 4;
  else if (*wc < 0x4000000) 
    count = 5;
  else if (*wc <= 0x7fffffff) 
    count = 6;
  else 
    return UDM_CHARSET_ILUNI;
  
  if ( r+count > e)
    return UDM_CHARSET_TOOSMALL;
  
  switch (count) { /* Fall through all cases. */
    case 6: r[5] = 0x80 | (*wc & 0x3f); *wc = *wc >> 6; *wc |= 0x4000000;
    case 5: r[4] = 0x80 | (*wc & 0x3f); *wc = *wc >> 6; *wc |= 0x200000;
    case 4: r[3] = 0x80 | (*wc & 0x3f); *wc = *wc >> 6; *wc |= 0x10000;
    case 3: r[2] = 0x80 | (*wc & 0x3f); *wc = *wc >> 6; *wc |= 0x800;
    case 2: r[1] = 0x80 | (*wc & 0x3f); *wc = *wc >> 6; *wc |= 0xC0;
    case 1: r[0] = *wc;
  }
  return count;
}

