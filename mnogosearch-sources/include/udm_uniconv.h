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

#include <sys/types.h>

#ifndef UDM_CHARSET_H
#define UDM_CHARSET_H

#define UDM_RECODE_TEXT                 0
#define UDM_RECODE_HTML_NONASCII	1 /* Put unconvertable characters using &#123; notation */
#define UDM_RECODE_HTML_SPECIAL		2 /* recognize &#123; and &#x123; in input */
#define UDM_RECODE_HTML                 3
#define UDM_RECODE_HTML_NONASCII_HEX    4 /* Put unconvertable characters using &#x123; notation */

#define UDM_CHARSET_UNKNOWN             0
#define UDM_CHARSET_ARABIC              1
#define UDM_CHARSET_ARMENIAN            2
#define UDM_CHARSET_BALTIC              3
#define UDM_CHARSET_CELTIC              4
#define UDM_CHARSET_CENTRAL             5
#define UDM_CHARSET_CHINESE_SIMPLIFIED  6
#define UDM_CHARSET_CHINESE_TRADITIONAL 7
#define UDM_CHARSET_CYRILLIC            8
#define UDM_CHARSET_GREEK               9
#define UDM_CHARSET_HEBREW             10
#define UDM_CHARSET_ICELANDIC          11
#define UDM_CHARSET_JAPANESE           12
#define UDM_CHARSET_KOREAN             13
#define UDM_CHARSET_NORDIC             14
#define UDM_CHARSET_SOUTHERN           15
#define UDM_CHARSET_THAI               16
#define UDM_CHARSET_TURKISH            17
#define UDM_CHARSET_UNICODE            18
#define UDM_CHARSET_VIETNAMESE         19
#define UDM_CHARSET_WESTERN            20
#define UDM_CHARSET_INDIAN             21
#define UDM_CHARSET_GEORGIAN           22

typedef struct
{
  int id;
  const char * name;
} UDM_CHARSETGROUP;

#define UDM_CHARSET_8859_1      0
#define UDM_CHARSET_8859_10     1
#define UDM_CHARSET_8859_11     2
#define UDM_CHARSET_8859_13     3
#define UDM_CHARSET_8859_14     4
#define UDM_CHARSET_8859_15     5
#define UDM_CHARSET_8859_16     6
#define UDM_CHARSET_8859_2      7
#define UDM_CHARSET_8859_3      8
#define UDM_CHARSET_8859_4      9
#define UDM_CHARSET_8859_5      10
#define UDM_CHARSET_8859_6      11
#define UDM_CHARSET_8859_7      12
#define UDM_CHARSET_8859_8      13
#define UDM_CHARSET_8859_9      14
#define UDM_CHARSET_ARMSCII_8   15
#define UDM_CHARSET_CP1250      16
#define UDM_CHARSET_CP1251      17
#define UDM_CHARSET_CP1252      18
#define UDM_CHARSET_CP1253      19
#define UDM_CHARSET_CP1254      20
#define UDM_CHARSET_CP1255      21
#define UDM_CHARSET_CP1256      22
#define UDM_CHARSET_CP1257      23
#define UDM_CHARSET_CP1258      24
#define UDM_CHARSET_CP437       25
#define UDM_CHARSET_CP850       26
#define UDM_CHARSET_CP852       27
#define UDM_CHARSET_CP855       28
#define UDM_CHARSET_CP857       29
#define UDM_CHARSET_CP860       30
#define UDM_CHARSET_CP861       31
#define UDM_CHARSET_CP862       32
#define UDM_CHARSET_CP863       33
#define UDM_CHARSET_CP864       34
#define UDM_CHARSET_CP865       35
#define UDM_CHARSET_CP866       36
#define UDM_CHARSET_CP869       37
#define UDM_CHARSET_CP874       38
#define UDM_CHARSET_KOI8_R      39
#define UDM_CHARSET_KOI8_U      40
#define UDM_CHARSET_MACARABIC   41
#define UDM_CHARSET_MACCE       42
#define UDM_CHARSET_MACCROATIAN 43
#define UDM_CHARSET_MACCYRILLIC 44
#define UDM_CHARSET_MACGREEK    45
#define UDM_CHARSET_MACHEBREW   46
#define UDM_CHARSET_MACICELAND  47
#define UDM_CHARSET_MACROMAN    48
#define UDM_CHARSET_MACROMANIA  49
#define UDM_CHARSET_MACTHAI     50
#define UDM_CHARSET_MACTURKISH  51
#define UDM_CHARSET_US_ASCII    52
#define UDM_CHARSET_VISCII      53
#define UDM_CHARSET_UTF8        54
#define UDM_CHARSET_GB2312      55
#define UDM_CHARSET_BIG5        56
#define UDM_CHARSET_SJIS        57
#define UDM_CHARSET_EUC_KR      58
#define UDM_CHARSET_EUC_JP      60
#define UDM_CHARSET_GBK         61
#define UDM_CHARSET_GUJARATI    62
#define UDM_CHARSET_TSCII       63
#define UDM_CHARSET_ISO2022JP   64
#define UDM_CHARSET_GEOSTD8     65
#define UDM_CHARSET_SYS_INT     255

typedef struct
{
  unsigned short from;
  unsigned short to;
  unsigned char  *tab;
} UDM_UNI_IDX;

struct udm_conv_st;

struct udm_unidata_st;

typedef struct udm_cset_st
{
  int id;
  int (*mb_wc)(struct udm_conv_st *conv, struct udm_cset_st *cs,int *wc,
               const unsigned char *s,const unsigned char *e);
  int (*wc_mb)(struct udm_conv_st *conv, struct udm_cset_st *cs, int *wc,
               unsigned char *s,unsigned char *e);
  void (*lcase)(struct udm_unidata_st *, struct udm_cset_st *cs, char *str, size_t bytelen);
  char *(*septoken)(struct udm_unidata_st *, struct udm_cset_st *cs,
                    const char *str, const char *strend,
                    const char **last, int *ctype0);
  const char * name;
  int family;
  unsigned char  *ctype;
  unsigned short *tab_to_uni;
  UDM_UNI_IDX    *tab_from_uni;
} UDM_CHARSET;

typedef struct udm_conv_st
{
  UDM_CHARSET    *from;
  UDM_CHARSET    *to;
  char           flags;
  char           istate;
  char           ostate;
} UDM_CONV;


/************** Language and charset guesser *************/


#define UDM_LM_MAXGRAM		6
#define UDM_LM_HASHMASK		0x0FFF
#define UDM_LM_TOPCNT           200

typedef struct
{
  size_t count;
  size_t index;
  char   str[UDM_LM_MAXGRAM+1];
} UDM_LANGITEM;

typedef struct
{
  float        expectation;		/**< Average value   */
  int          needsave;
  char         *lang;			/**< Map Language    */
  char         *charset;		/**< Map charset     */
  char         *filename;		/**< Filename to write updates, if need */
  UDM_LANGITEM memb[UDM_LM_HASHMASK+1];	/**< Items list      */
} UDM_LANGMAP;

typedef struct
{
  size_t      nmaps;
  UDM_LANGMAP *Map;
} UDM_LANGMAPLIST;

/********************************************/

/* Input string in xxx2uni                  */
/* convertion  has bad multi-byte sequence  */
#define UDM_CHARSET_ILSEQ     0
#define UDM_CHARSET_ILSEQ2   -1
#define UDM_CHARSET_ILSEQ3   -2
#define UDM_CHARSET_ILSEQ4   -3
#define UDM_CHARSET_ILSEQ5   -4
#define UDM_CHARSET_ILSEQ6   -5

/* Input buffer in xxx2uni was terminated   */
/* in the middle of multi-byte sequence     */
#define UDM_CHARSET_TOOFEW(n) (-6-(n))

/* Can't convert unicode into given charset */
#define UDM_CHARSET_ILUNI     0

/* Output buffer in uni2xxx is too small    */
#define UDM_CHARSET_TOOSMALL  -1

/*
  "Unicode value was returned from cache"
  For character sets like tscii, when one
  native code corresponds to several Unicode characters:
  8C -> U+0BE8 U+0BB7 U+0B82
*/
#define UDM_CHARSET_CACHEDUNI -100

/* Character types */
#define UDM_UNI_SEPAR    0
#define UDM_UNI_LETTER   1
#define UDM_UNI_DIGIT    2
#define UDM_UNI_CJK 3

extern __C_LINK const char * __UDMCALL UdmCsGroup(const UDM_CHARSET *cs);

int udm_mb_wc_utf8(UDM_CONV *conv, UDM_CHARSET *c,int *pwc, 
                   const unsigned char *s, const unsigned char *e);
extern __C_LINK int __UDMCALL udm_wc_mb_utf8(UDM_CONV *conv, UDM_CHARSET *c,int *wc, 
                                             unsigned char *s, unsigned char *e);

extern __C_LINK int __UDMCALL udm_mb_wc_latin1(UDM_CONV *conv, UDM_CHARSET *c,int *pwc,
                                               const unsigned char *s, 
                                               const unsigned char *e);
extern __C_LINK int __UDMCALL udm_wc_mb_latin1(UDM_CONV *conv, UDM_CHARSET *c,int *wc,
                                               unsigned char *s, unsigned char *e);


extern __C_LINK int __UDMCALL udm_mb_wc_ascii(UDM_CONV *conv, UDM_CHARSET *c,int *pwc,
                                              const unsigned char *s, 
                                              const unsigned char *e);
extern __C_LINK int __UDMCALL udm_wc_mb_ascii(UDM_CONV *conv, UDM_CHARSET *c,int *wc,
                                              unsigned char *s, unsigned char *e);


extern __C_LINK int __UDMCALL udm_mb_wc_8bit(UDM_CONV *conv, UDM_CHARSET *c,int *pwc,
                                             const unsigned char *s, 
                                             const unsigned char *e);
extern __C_LINK int __UDMCALL udm_wc_mb_8bit(UDM_CONV *conv, UDM_CHARSET *c,int *wc,
                                             unsigned char *s, unsigned char *e);

extern __C_LINK int __UDMCALL udm_mb_wc_sys_int(UDM_CONV *conv, UDM_CHARSET *c,int *pwc,
                                                const unsigned char *s,
                                                const unsigned char *e);
extern __C_LINK int __UDMCALL udm_wc_mb_sys_int(UDM_CONV *conv, UDM_CHARSET *c,int *wc,
                                                unsigned char *s, unsigned char *e);

/* Unicode, system dependent integer */
extern UDM_CHARSET udm_charset_sys_int;

#ifdef HAVE_CHARSET_gb2312
int udm_mb_wc_gb2312(UDM_CONV *conv, UDM_CHARSET *c,int *pwc,
                     const unsigned char *s,const unsigned char *e);
int udm_wc_mb_gb2312(UDM_CONV *conv, UDM_CHARSET *c,int *wc,
                     unsigned char *s,unsigned char *e);
#endif

#ifdef HAVE_CHARSET_japanese
int udm_mb_wc_sjis(UDM_CONV *conv, UDM_CHARSET *c,int *pwc,
                   const unsigned char *s,const unsigned char *e);
int udm_wc_mb_sjis(UDM_CONV *conv, UDM_CHARSET *c,int *wc,
                   unsigned char*s,unsigned char *e);
#endif

#ifdef HAVE_CHARSET_euc_kr
int udm_mb_wc_euc_kr(UDM_CONV *conv, UDM_CHARSET *c,int *pwc,
                     const unsigned char *s, const unsigned char *e);
int udm_wc_mb_euc_kr(UDM_CONV *conv, UDM_CHARSET *c,int *wc,
                     unsigned char *s,unsigned char *e);
#endif

#ifdef HAVE_CHARSET_japanese
int udm_mb_wc_euc_jp(UDM_CONV *conv, UDM_CHARSET *c,int *pwc,
                     const unsigned char *s, const unsigned char *e);
int udm_wc_mb_euc_jp(UDM_CONV *conv, UDM_CHARSET *c,int *wc,
                     unsigned char *s, unsigned char *e);
#endif

#ifdef HAVE_CHARSET_big5
int udm_mb_wc_big5(UDM_CONV *conv, UDM_CHARSET *c,int *pwc,
                   const unsigned char *s, const unsigned char *e);
int udm_wc_mb_big5(UDM_CONV *conv, UDM_CHARSET *c,int *wc,
                   unsigned char *s,unsigned char *e);
#endif

#ifdef HAVE_CHARSET_gbk
int udm_mb_wc_gbk(UDM_CONV *conv, UDM_CHARSET *c,int *pwc,
                  const unsigned char *s, const unsigned char *e);
int udm_wc_mb_gbk(UDM_CONV *conv, UDM_CHARSET *c,int *wc,
                  unsigned char *s,unsigned char *e);
#endif

#ifdef HAVE_CHARSET_gujarati
int udm_mb_wc_gujarati(UDM_CONV *conv, UDM_CHARSET *c, int *pwc,
                       const unsigned char *s, const unsigned char *e);
int udm_wc_mb_gujarati(UDM_CONV *conv, UDM_CHARSET *c, int *wc,
                       unsigned char *s, unsigned char *e);
#endif

#ifdef HAVE_CHARSET_tscii
int udm_mb_wc_tscii(UDM_CONV *conv, UDM_CHARSET *c, int *pwc,
                    const unsigned char *s, const unsigned char *e);
int udm_wc_mb_tscii(UDM_CONV *conv, UDM_CHARSET *c, int *wc,
                    unsigned char *s, unsigned char *e);
#endif

#ifdef HAVE_CHARSET_japanese
int udm_mb_wc_iso2022jp(UDM_CONV *conv, UDM_CHARSET *c,int *pwc,
                        const unsigned char *s, const unsigned char *e);
int udm_wc_mb_iso2022jp(UDM_CONV *conv, UDM_CHARSET *c,int *wc,
                        unsigned char *s, unsigned char *e);
#endif


extern __C_LINK UDM_CHARSET * __UDMCALL UdmGetCharSet(const char * name);
extern __C_LINK UDM_CHARSET * __UDMCALL UdmGetCharSetByID(int id);
extern  const char *UdmCharsetCanonicalName(const char * aslias);
extern __C_LINK void __UDMCALL UdmConvInit(UDM_CONV *c,
                                           UDM_CHARSET *from,UDM_CHARSET *to, int fl);
extern __C_LINK int  __UDMCALL UdmConv(UDM_CONV *c,
                                       char *d, size_t dlen,
                                       const char *s, size_t slen);
extern void UdmConvFree(UDM_CONV *c);

#endif
