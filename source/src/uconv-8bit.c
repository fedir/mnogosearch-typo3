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

/*
  Special characters:
  0x22 0010.0010 "
  0x26 0010.0110 &
  0x3C 0011.1100 <
  0x3E 0011.1110 >
       001?.???0
  
  0xE1 1110.0001 Mask
  0x20 0010.0000 Result
  
*/

#define IS_SPECIAL(x) ((x) == '"' || (x) == '&' || (x) == '<' || (x) == '>')

#define IS_SPECIAL_QUICK(x) ((((x) & 0xE1) == 0x20) && IS_SPECIAL(x))

__C_LINK int __UDMCALL udm_mb_wc_ascii(UDM_CONV *conv, UDM_CHARSET *cs,int *wc,
                                       const unsigned char *str,
                                       const unsigned char *end)
{
  if (*str == '&' && (conv->flags & UDM_RECODE_HTML_SPECIAL))
    return UdmSGMLScan(wc, str, end);
  if (*str > 0x7F)
    return UDM_CHARSET_ILSEQ;
  *wc= *str;
  return 1;
}


__C_LINK int __UDMCALL udm_wc_mb_ascii(UDM_CONV *conv, UDM_CHARSET *cs, int *wc,
                                       unsigned char *s, unsigned char *e)
{
  if (*wc > 0x7F)
    return UDM_CHARSET_ILUNI;

  *s= *wc;

   if ((conv->flags & UDM_RECODE_HTML_SPECIAL) && IS_SPECIAL_QUICK(*s))
     return UDM_CHARSET_ILUNI;
  return 1;
}



__C_LINK int __UDMCALL udm_mb_wc_latin1(UDM_CONV *conv, UDM_CHARSET *cs,int *wc,
                                        const unsigned char *str,
                                        const unsigned char *end)
{
  if (*str == '&' && (conv->flags & UDM_RECODE_HTML_SPECIAL))
    return UdmSGMLScan(wc, str, end);
  *wc= *str;
  return 1;
}


__C_LINK int __UDMCALL udm_wc_mb_latin1(UDM_CONV *conv, UDM_CHARSET *cs, int *wc,
                                        unsigned char *s, unsigned char *e)
{
  if (*wc > 0xFF)
    return UDM_CHARSET_ILUNI;

  *s= *wc;

   if ((conv->flags & UDM_RECODE_HTML_SPECIAL) && IS_SPECIAL_QUICK(*s))
     return UDM_CHARSET_ILUNI;
  return 1;
}

__C_LINK int __UDMCALL udm_mb_wc_8bit(UDM_CONV *conv, UDM_CHARSET *cs,int *wc,
                                      const unsigned char *str,
                                      const unsigned char *end)
{
  if (*str == '&' && (conv->flags & UDM_RECODE_HTML_SPECIAL))
    return UdmSGMLScan(wc, str, end);
  *wc= cs->tab_to_uni[*str];
  return (!wc[0] && str[0]) ? UDM_CHARSET_ILSEQ : 1;
}


__C_LINK int __UDMCALL udm_wc_mb_8bit(UDM_CONV *conv, UDM_CHARSET *cs, int *wc,
                                      unsigned char *s, unsigned char *e)
{
  UDM_UNI_IDX *idx;
     
  for(idx=cs->tab_from_uni; idx->tab ; idx++)
  {
    if(idx->from <= *wc && idx->to >= *wc)
    {
      s[0]=idx->tab[*wc - idx->from];
      if ((conv->flags & UDM_RECODE_HTML_SPECIAL) && IS_SPECIAL_QUICK(*s))
        return UDM_CHARSET_ILUNI;
      return (!s[0] && *wc) ? UDM_CHARSET_ILUNI : 1;
    }
  }
  return UDM_CHARSET_ILUNI;
}
