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
#include "udm_uniconv.h"


__C_LINK int __UDMCALL
udm_wc_mb_sys_int (UDM_CONV *conv, UDM_CHARSET *cs, int *wc, unsigned char *s, unsigned char *e)
{
  if (s + sizeof(int) > e)
    return UDM_CHARSET_TOOSMALL;
  
  memcpy(s, wc, sizeof(int));
  
  return sizeof(int);
}


__C_LINK int __UDMCALL
udm_mb_wc_sys_int (UDM_CONV *conv, UDM_CHARSET *cs,int *pwc,
          const unsigned char *s,
          const unsigned char *e)
{
  
  if (s+sizeof(int)>e)
    return UDM_CHARSET_TOOFEW(0);
  
  memcpy(pwc,s,sizeof(int));
  
  return sizeof(int);
}

