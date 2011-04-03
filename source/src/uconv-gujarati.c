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

#include <stdio.h>
#include <string.h>
#include "udm_uniconv.h"
#include "udm_sgml.h"

#ifdef HAVE_CHARSET_gujarati

static unsigned short tab_gujarati[]={
       0,      0,      0,      0,      0,      0,      0,      0,  /* 0x00 */
       0,      0,      0,      0,      0,      0,      0,      0,  /* 0x08 */
       0,      0,      0,      0,      0,      0,      0,      0,  /* 0x10 */
       0,      0,      0,      0,      0,      0,      0,      0,  /* 0x18 */
  0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,  /* 0x20 */
  0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,  /* 0x28 */
  0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,  /* 0x30 */
  0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,  /* 0x38 */
  0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,  /* 0x40 */
  0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,  /* 0x48 */
  0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,  /* 0x50 */
  0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,  /* 0x58 */
  0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,  /* 0x60 */
  0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,  /* 0x68 */
  0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,  /* 0x70 */
  0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x0000,  /* 0x78 */
  0x00D7, 0x2212, 0x2013, 0x2014, 0x2018, 0x2019, 0x2026, 0x2022,  /* 0x80 */
  0x00A9, 0x00AE, 0x2122, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  /* 0x88 */
  0x0965, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  /* 0x90 */
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  /* 0x98 */
  0x0000, 0x0A81, 0x0A82, 0x0A83, 0x0A85, 0x0A86, 0x0A87, 0x0A88,  /* 0xA0 */
  0x0A89, 0x0A8A, 0x0A8B, 0x0000, 0x0A8F, 0x0A90, 0x0A8D, 0x0000,  /* 0xA8 */
  0x0A93, 0x0A94, 0x0A91, 0x0A95, 0x0A96, 0x0A97, 0x0A98, 0x0A99,  /* 0xB0 */
  0x0A9A, 0x0A9B, 0x0A9C, 0x0A9D, 0x0A9E, 0x0A9F, 0x0AA0, 0x0AA1,  /* 0xB8 */
  0x0AA2, 0x0AA3, 0x0AA4, 0x0AA5, 0x0AA6, 0x0AA7, 0x0AA8, 0x0000,  /* 0xC0 */
  0x0AAA, 0x0AAB, 0x0AAC, 0x0AAD, 0x0AAE, 0x0AAF, 0x0000, 0x0AB0,  /* 0xC8 */
  0x0000, 0x0AB2, 0x0AB3, 0x0000, 0x0AB5, 0x0AB6, 0x0AB7, 0x0AB8,  /* 0xD0 */
  0x0AB9, 0x200E, 0x0ABE, 0x0ABF, 0x0AC0, 0x0AC1, 0x0AC2, 0x0AC3,  /* 0xD8 */
  0x0000, 0x0AC7, 0x0AC8, 0x0AC5, 0x0000, 0x0ACB, 0x0ACC, 0x0AC9,  /* 0xE0 */
  0x0ACD, 0x0ABC, 0x0964, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  /* 0xE8 */
  0x0000, 0x0AE6, 0x0AE7, 0x0AE8, 0x0AE9, 0x0AEA, 0x0AEB, 0x0AEC,  /* 0xF0 */
  0x0AED, 0x0AEE, 0x0AEF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  /* 0xF8 */
/*   0       1       2       3       4       5       6       7      */
/*   8       9       A       B       C       D       E       F      */
};

static unsigned char tab_0A8F[] = {0xAC, 0xAD, 0xB2};
static unsigned char tab_0A93[] = {
  0xB0, 0xB1, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8,
  0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0,
  0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6
};
static unsigned char tab_0AC7[] = {0xE1, 0xE2, 0xE7};

int 
udm_mb_wc_gujarati(UDM_CONV *conv, UDM_CHARSET *cs, int *pwc,
                   const unsigned char *s, const unsigned char *e)
{
  int hi= s[0];

  if (conv->istate > 0)
  {
    pwc[0]= pwc[1];
    pwc[1]= 0;
    conv->istate= 0;
    return UDM_CHARSET_CACHEDUNI;
  }

  if(hi < 0x80)
  {
    if (*s == '&' && (conv->flags & UDM_RECODE_HTML_SPECIAL))
      return UdmSGMLScan(pwc, s, e);
    pwc[0]= hi;
    pwc[1]= 0;
    return 1;
  }

  if (hi == 0xA1)
  {
    if ((s + 2 > e) || (s[1] != 0xE9))
    {
      pwc[0]= tab_gujarati[0xA1];
      pwc[1]= 0;
      return 1;
    }
    pwc[0]= 0x0AD0;
    pwc[1]= 0;
    return 2;
  }

  if (hi == 0xAA)
  {
    if ((s + 2 > e) || (s[1] != 0xE9))
    {
      pwc[0]= tab_gujarati[0xAA];
      pwc[1]= 0;
      return 1;
    }
    pwc[0]= 0x0AE0;
    pwc[1]= 0;
    return 2;
  }

  if (hi == 0xDF)
  {
    if ((s + 2 > e) || (s[1] != 0xE9))
    {
      pwc[0]= tab_gujarati[0xDF];
      pwc[1]= 0;
      return 1;
    }
    pwc[0]= 0x0AC4;
    pwc[1]= 0;
    return 2;
  }

  if (hi == 0xE8)
  {
    if ((s + 2 > e) || ((s[1] != 0xE8) &&(s[1] != 0xE9)))
    {
      pwc[0]= tab_gujarati[0xE8];
      pwc[1]= 0;
      return 1;
    }
    pwc[0]= 0x0ACD;
    pwc[1]= (s[1] == 0xE8) ? 0x200C : 0x200D;
    conv->istate= 1;
    return 2;
  }

  pwc[0]= tab_gujarati[hi];
  return 1;
}

int udm_wc_mb_gujarati(UDM_CONV *conv, UDM_CHARSET *cs, int *wc,
                       unsigned char *s, unsigned char *e)
{
  if (*wc < 0x7F)
  {
    s[0]= *wc;
    if ((conv->flags & UDM_RECODE_HTML_SPECIAL) &&
        (s[0] == '"' || s[0] == '&' || s[0] == '<' || s[0] == '>')) 
      return UDM_CHARSET_ILUNI;
    return 1;
  }

  if (*wc == 0x00A9)
  {
    s[0]= 0x88;
    return 1;
  }
  if (*wc == 0x00AE)
  {
    s[0]= 0x89;
    return 1;
  }
  if (*wc == 0x00D7)
  {
    s[0]= 0x80;
    return 1;
  }
  if (*wc == 0x0964)
  {
    s[0]= 0xEA;
    return 1;
  }
  if (*wc == 0x0965)
  {
    s[0]= 0x90;
    return 1;
  }
  if (*wc >= 0x0A81 && *wc <= 0xA83)
  {
    s[0]= 0xA1 + (*wc - 0x0A81);
    return 1;
  }
  if (*wc >= 0x0A85 && *wc <= 0xA8B)
  {
    s[0]= 0xA4 + (*wc - 0x0A85);
    return 1;
  }
  if (*wc == 0x0A8D)
  {
    s[0]= 0xAE;
    return 1;
  }
  if (*wc >= 0x0A8F && *wc <= 0xA91)
  {
    s[0]= tab_0A8F[*wc - 0x0A8F];
    return 1;
  }
  if (*wc >= 0x0A93 && *wc <= 0xAA8)
  {
    s[0]= tab_0A93[*wc - 0x0A93];
    return 1;
  }
  if (*wc >= 0x0AAA && *wc <= 0xAAF)
  {
    s[0]= 0xC8 + (*wc - 0x0AAA);
    return 1;
  }
  if (*wc == 0x0AB0)
  {
    s[0]= 0xCF;
    return 1;
  }
  if (*wc >= 0x0AB2 && *wc <= 0xAB3)
  {
    s[0]= 0xD1 + (*wc - 0x0AB2);
    return 1;
  }
  if (*wc >= 0x0AB5 && *wc <= 0xAB9)
  {
    s[0]= 0xD4 + (*wc - 0x0AB5);
    return 1;
  }
  if (*wc == 0x0ABC)
  {
    s[0]= 0xE9;
    return 1;
  }
  if (*wc >= 0x0ABE && *wc <= 0xAC3)
  {
    s[0]= 0xDA + (*wc - 0x0ABE);
    return 1;
  }
  
  if (*wc == 0x0AC4)
  {
    if(s + 2 > e)
      return UDM_CHARSET_TOOSMALL;
    s[0]= 0xAA;
    s[1]= 0xE9;
    return 2;
  }

  if (*wc == 0x0AC5)
  {
    s[0]= 0xE3;
    return 1;
  }

  if (*wc >= 0x0AC7 && *wc <= 0xAC9)
  {
    s[0]= tab_0AC7[*wc - 0x0AC7];
    return 1;
  }

  if (*wc >= 0x0ACB && *wc <= 0xACC)
  {
    s[0]= 0xE5 + (*wc - 0x0ACB);
    return 1;
  }

  if (*wc == 0x0ACD)
  {
    if (wc[1] == 0x200C)
    {
      if(s + 2 > e)
	return UDM_CHARSET_TOOSMALL;
      s[0]= 0xE8;
      s[1]= 0xE8;
      wc[0]= wc[1]= 0;
      return 2;
    }
    if (wc[1] == 0x200D)
    {
      if(s + 2 > e)
	return UDM_CHARSET_TOOSMALL;
      s[0]= 0xE8;
      s[1]= 0xE9;
      wc[0]= wc[1]= 0;
      return 2;
    }
    s[0]= 0xE8;
    return 1;
  }

  if (*wc == 0x0AD0)
  {
    if(s + 2 > e)
      return UDM_CHARSET_TOOSMALL;
    s[0]= 0xA1;
    s[1]= 0xE9;
    return 2;
  }

  if (*wc == 0x0AE0)
  {
    if(s + 2 > e)
      return UDM_CHARSET_TOOSMALL;
    s[0]= 0xAA;
    s[1]= 0xE9;
    return 2;
  }

  if (*wc >= 0x0AE6 && *wc <= 0xAEF)
  {
    s[0]= 0xF1 + (*wc - 0x0AE6);
    return 1;
  }

  if (*wc == 0x200E)
  {
    s[0]= 0xD9;
    return 1;
  }
  if (*wc == 0x2013)
  {
    s[0]= 0x82;
    return 1;
  }
  if (*wc == 0x2014)
  {
    s[0]= 0x83;
    return 1;
  }
  if (*wc == 0x2018)
  {
    s[0]= 0x84;
    return 1;
  }
  if (*wc == 0x2019)
  {
    s[0]= 0x85;
    return 1;
  }
  if (*wc == 0x2022)
  {
    s[0]= 0x87;
    return 1;
  }
  if (*wc == 0x2026)
  {
    s[0]= 0x86;
    return 1;
  }
  if (*wc == 0x2122)
  {
    s[0]= 0x8A;
    return 1;
  }
  if (*wc == 0x2212)
  {
    s[0]= 0x81;
  }
  else
  {
    s[0]= '?';
    return UDM_CHARSET_ILUNI;
  }

  return 0;
}


#endif

#if 0

typedef struct back {
  unsigned char c;
  int u;
} B;

static int cmpB(const void *s1,const void *s2){
  B *b1 = (B*)s1;
  B *b2 = (B*)s2;
  return b1->u - b2->u;
}


int main() {
  B table[256];
  int i;

  for (i = 0; i < 256; i++) {
    table[i].c = i;
    table[i].u = tab_gujarati[i];
  }
  UdmSort((void*)table, 256, sizeof(B), cmpB);

  for (i = 0; i < 256; i++) {
    if (table[i].u)
      printf("%04x %02x %03d\n", table[i].u, table[i].c, table[i].c);
  }
  
}

#endif
