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

#ifndef UDM_UNIDATA_H
#define UDM_UNIDATA_H

/* Character types */
#define UDM_UNI_SEPAR	0
#define UDM_UNI_LETTER	1
#define UDM_UNI_DIGIT	2
#define UDM_UNI_CJK	3
#define UDM_UNI_MAX	3

struct udm_unidata_st;
typedef struct udm_unidata_st UDM_UNIDATA;
extern UDM_UNIDATA *udm_unidata_default;
extern UDM_UNIDATA *udm_unidata_turkish;


extern int  UdmUniIsSpace(int uni);
extern int  UdmUniToLower(UDM_UNIDATA *, int uni);
extern __C_LINK void __UDMCALL UdmUniStrToLower(UDM_UNIDATA *, int * unistr);
extern void UdmUniStrStripAccents(UDM_UNIDATA *, int *unistr);
extern int  UdmUniCType(UDM_UNIDATA *, int u);
extern void UdmStrToLowerExt(UDM_UNIDATA *, UDM_CHARSET *cs,
                             char *str, size_t bytelen, int flag);
extern void UdmStrToLower(UDM_UNIDATA *, UDM_CHARSET *cs,
                          char *str, size_t bytelen);
extern void UdmStrToLower8bit(UDM_UNIDATA *, UDM_CHARSET *cs,
                              char *str, size_t bytelen);

extern int  *UdmUniGetToken(UDM_UNIDATA *, int * s, int ** last);
extern int  *UdmUniGetSepToken(UDM_UNIDATA *,
                               int *str, int *strend,
                               int **last, int *ctype0);
extern int  *UdmUniGetSepToken2(UDM_UNIDATA *,
                                int *str, int *strend,
                                int **last, int *ctype0);

extern char *UdmStrGetSepToken8bit(UDM_UNIDATA *, UDM_CHARSET *cs,
                                   const char *str, const char *strend,
                                   const char **last, int *ctype0);
extern int UdmStrCaseCmp2(UDM_UNIDATA *, UDM_CONV *conv,
                          const char *s, size_t slen,
                          const char *t, size_t tlen);
                          
extern int UdmStrCaseAccentCmp2(UDM_UNIDATA *, UDM_CONV *conv,
                                const char *s, size_t slen,
                                const char *t, size_t tlen);

extern int UdmUniStrNCaseCmp(UDM_UNIDATA *,
                             const int *s1, const int *s2, size_t len);

extern int UdmAutoPhraseChar(int ch);

extern void UdmSoundex(UDM_CHARSET *cs, char *dst, const char *src, size_t srclen);

extern UDM_UNIDATA *UdmUnidataGetByName(const char *name);

#endif
