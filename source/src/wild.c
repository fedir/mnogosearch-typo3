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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>

#include "udm_common.h"
#include "udm_wild.h"
#include "udm_utils.h"


int UdmWildCmp(const char *str, const char *wexp)
{
    int x, y;

    for (x = 0, y = 0; wexp[y]; ++y, ++x) {
	if ((!str[x]) && (wexp[y] != '*'))return -1;
	if (wexp[y] == '*') {
	    while (wexp[++y] == '*');
	    if (!wexp[y])return 0;
	    while (str[x]) {
		int ret;
		if ((ret = UdmWildCmp(&str[x++], &wexp[y])) != 1)return ret;
	    }
	    return -1;
	}else
	if ((wexp[y] != '?') && (str[x] != wexp[y]))return 1;
    }
    return (str[x] != '\0');
}


int __UDMCALL UdmWildCaseCmp(const char *str, const char *wexp)
{
  for ( ; *wexp; ++str, ++wexp)
  {
    if (!*str && *wexp != '*')return -1;
    if (*wexp == '*')
    {
      while (*++wexp == '*');
      if (!*wexp)return 0;
      while (*str)
      {
        int ret;
        if ((ret= UdmWildCaseCmp(str++, wexp)) != 1)return ret;
      }
      return -1;
    }
    else if ((*wexp != '?') &&
             (udm_l1tolower[(unsigned char)*str] !=
              udm_l1tolower[(unsigned char)*wexp]))
      return 1;
  }
  return (*str != '\0');
}
