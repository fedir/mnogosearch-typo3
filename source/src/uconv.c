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
#include "udm_common.h" /* For assert() */
#include "udm_uniconv.h"


void __UDMCALL UdmConvInit(UDM_CONV *cnv,UDM_CHARSET *from,UDM_CHARSET *to, int fl)
{
  cnv->from= from;
  cnv->to= to;
  cnv->flags= fl;
}


int __UDMCALL UdmConv(UDM_CONV *c, char *d, size_t dlen,const char *s, size_t slen)
{
  int        res, wc[16]; /* is 16 is enough? */
  char	     *d_o= d;
  const char *s_e= s+slen;
  char	     *d_e= d+dlen;
  int        (*mb_wc)(struct udm_conv_st *conv_arg,
                      struct udm_cset_st *cs_arg, int *wc_arg,
                      const unsigned char *s_arg,
                      const unsigned char *e_arg);
  int        (*wc_mb)(struct udm_conv_st *conv_arg,
                      struct udm_cset_st *cs, int *wc_arg,
                      unsigned char *s_arg,
                      unsigned char *e_arg);
/*
if (slen && s[0] != 0)
  fprintf(stderr, "> %s %s %d '%.*s'\n", c->from->name, c->to->name, slen, slen, s);
*/
  
  c->istate= 0; /* set default state */
  c->ostate= 0; /* set default state */
  mb_wc= c->from->mb_wc;
  wc_mb= c->to->wc_mb;
  
  while(s <s_e && d <d_e)
  {
    res= mb_wc(c, c->from, wc, (const unsigned char*)s,
                               (const unsigned char*)s_e);
    if (res > 0)
    {
      s+=res;
    }
    else if ((res == UDM_CHARSET_ILSEQ)  || 
             (res == UDM_CHARSET_ILSEQ2) ||
             (res == UDM_CHARSET_ILSEQ3) ||
             (res == UDM_CHARSET_ILSEQ4) ||
             (res == UDM_CHARSET_ILSEQ5) ||
             (res == UDM_CHARSET_ILSEQ6))
    {
      /* Move at least one character */
      s+= ((res == UDM_CHARSET_ILSEQ) ? 1 : - res);
      wc[0]= '?';
      res= wc_mb(c, c->to, &wc[0], (unsigned char*) d, (unsigned char*) d_e);
      if (res <= 0)
        goto outaway;
    }
    else if (res != UDM_CHARSET_CACHEDUNI)
      goto outaway;

    res= wc_mb(c, c->to, &wc[0], (unsigned char*)d, (unsigned char*)d_e);
    if (res > 0)
    {
      d+= res;
    }
    else if (res == UDM_CHARSET_ILUNI)
    {
      if (c->flags & (UDM_RECODE_HTML_NONASCII|UDM_RECODE_HTML_NONASCII_HEX))
      {
        if ( d_e - d > 8)
        {
          if (c->flags & UDM_RECODE_HTML_NONASCII_HEX)
            res= sprintf(d, "&#x%X;", wc[0]);
          else
            res= sprintf(d, "&#%d;", wc[0]);
          d+= res;
        }
        else
          break;
      }
      else
      {
        wc[0]= '?';
        res= wc_mb(c, c->to, &wc[0], (unsigned char*) d, (unsigned char*) d_e);
        if (res <= 0)
         goto outaway;
      }
    }
    else
      goto outaway;
      
    UDM_ASSERT(wc[0] != 0 || s >= s_e);
  }
  
outaway:  

/*
if (d - d_o  && d_o[0] != 0)
  fprintf(stderr, "< %s %s %d '%.*s'\n", c->from->name, c->to->name, d - d_o, d - d_o, d_o);
*/
  return d - d_o;
}

__C_LINK const char * __UDMCALL UdmCsGroup(const UDM_CHARSET *cs)
{
  switch(cs->family)
  {
    case UDM_CHARSET_ARABIC             : return "Arabic";
    case UDM_CHARSET_ARMENIAN           : return "Armenian";
    case UDM_CHARSET_BALTIC             : return "Baltic";
    case UDM_CHARSET_CELTIC             : return "Celtic";
    case UDM_CHARSET_CENTRAL            : return "Central Eur";
    case UDM_CHARSET_CHINESE_SIMPLIFIED : return "Chinese Simplified";
    case UDM_CHARSET_CHINESE_TRADITIONAL: return "Chinese Traditional";
    case UDM_CHARSET_CYRILLIC           : return "Cyrillic";
    case UDM_CHARSET_GREEK              : return "Greek";
    case UDM_CHARSET_HEBREW             : return "Hebrew";
    case UDM_CHARSET_ICELANDIC          : return "Icelandic";
    case UDM_CHARSET_JAPANESE           : return "Japanese";
    case UDM_CHARSET_KOREAN             : return "Korean";
    case UDM_CHARSET_NORDIC             : return "Nordic";
    case UDM_CHARSET_SOUTHERN           : return "South Eur";
    case UDM_CHARSET_THAI               : return "Thai";
    case UDM_CHARSET_TURKISH            : return "Turkish";
    case UDM_CHARSET_UNICODE            : return "Unicode";
    case UDM_CHARSET_VIETNAMESE         : return "Vietnamese";
    case UDM_CHARSET_WESTERN            : return "Western";
    case UDM_CHARSET_GEORGIAN           : return "Georgian";
    case UDM_CHARSET_INDIAN             : return "Indian";
    default                             : return "Unknown";
  }
}
