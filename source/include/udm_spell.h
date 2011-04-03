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

#ifndef _UDM_SPELL_H                                 
#define _UDM_SPELL_H

void UdmSpellListListInit(UDM_SPELLLISTLIST *L);
void UdmSpellListListFree(UDM_SPELLLISTLIST *L);
int  UdmSpellListListAdd(UDM_SPELLLISTLIST *L,
                         const char *lang, const char *cset, const char *name);
int  UdmSpellListListLoad(UDM_SPELLLISTLIST *L,
                         char *err, size_t errlen);
int  UdmSpellListListWriteHash(UDM_SPELLLISTLIST *L,
                               char *err, size_t errlen);
int  UdmSpellDump(UDM_SPELLLISTLIST *L, UDM_AFFIXLISTLIST *A,
                  char *err, size_t errlen);

void UdmAffixListListInit(UDM_AFFIXLISTLIST *L);
void UdmAffixListListFree(UDM_AFFIXLISTLIST *L);
int  UdmAffixListListAdd(UDM_AFFIXLISTLIST *L,
                         const char *lang, const char *cset, const char *name);
int UdmAffixListListLoad(UDM_AFFIXLISTLIST *L, int flags, 
                         char *err, size_t errlen);

size_t
UdmSpellNormalize(UDM_SPELLLIST *Sl, UDM_AFFIXLIST *Al,
                  const char *word, UDM_SPELL *Res, size_t nres);
size_t
UdmSpellDenormalize(UDM_SPELLLIST *Sl,
                    UDM_AFFIXLIST *Al,
                    UDM_SPELL *S,
                    char **Res, size_t mres);

/* Backward capability with php-4.3.x */
#define UdmSpellListFree UdmSpellListListFree
#define UdmAffixListFree UdmAffixListListFree
#define UdmImportDictionary(Conf, lang, charset, filename, skip_noflag, first_letters) UdmSpellListListAdd(&Conf->Spells, lang, charset, filename)
#define UdmImportAffixes(Conf, lang, charset, filename) UdmAffixListListAdd(&Conf->Affixes, lang, charset, filename)
#define UdmSortDictionary(Spells)
#define UdmSortAffixes(Affixes, Spells)
#endif
