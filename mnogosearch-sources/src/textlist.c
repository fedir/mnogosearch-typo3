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
#include "udm_common.h"
#include "udm_utils.h"
#include "udm_textlist.h"
#include "udm_unicode.h"


__C_LINK void __UDMCALL UdmTextListAdd(UDM_TEXTLIST *tlist,
                                       const UDM_TEXTITEM *item)
{
  if(!item->str)return;
  
  tlist->Item=(UDM_TEXTITEM*)UdmRealloc(tlist->Item,(tlist->nitems+1)*sizeof(UDM_TEXTITEM));
     
  tlist->Item[tlist->nitems].str = (char*)UdmStrdup(item->str);
  tlist->Item[tlist->nitems].href = item->href ? (char*)UdmStrdup(item->href) : NULL;
  tlist->Item[tlist->nitems].section_name = item->section_name ? (char*)UdmStrdup(item->section_name) : NULL;
  tlist->Item[tlist->nitems].section=item->section;
  tlist->Item[tlist->nitems].flags= item->flags;
  tlist->nitems++;
  return;
}


__C_LINK void __UDMCALL UdmTextListAppend(UDM_TEXTLIST * tlist,
                                          const UDM_TEXTITEM *item)
{
  size_t oldlen, newlen;
  UDM_TEXTITEM *I;

  if(!item->str)return;
  
  if (item->href || !tlist->nitems)
  {
    UdmTextListAdd(tlist, item);
    return;
  }
  
  I= &tlist->Item[tlist->nitems-1];
  
  oldlen= strlen(I->str);
  newlen= oldlen + strlen(item->str) + 1;
  I->str= (char*)UdmRealloc(I->str, newlen);
  strcpy(I->str + oldlen, item->str);
  return;
}


__C_LINK void __UDMCALL UdmTextListFree(UDM_TEXTLIST *tlist)
{
  size_t i;

  for(i=0;i<tlist->nitems;i++)
  {
    UDM_FREE(tlist->Item[i].str);
    UDM_FREE(tlist->Item[i].href);
    UDM_FREE(tlist->Item[i].section_name);
  }
  UDM_FREE(tlist->Item);
  tlist->nitems = 0;
}
