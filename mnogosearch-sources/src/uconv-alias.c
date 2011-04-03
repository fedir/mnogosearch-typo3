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
#include "udm_unidata.h"
#include "uconv-8bit.h"
#include "udm_utils.h"


#define lcase_mb   UdmStrToLower
#define lcase_8bit UdmStrToLower8bit


UDM_CHARSET udm_charset_sys_int=
{
  UDM_CHARSET_SYS_INT,
  udm_mb_wc_sys_int,
  udm_wc_mb_sys_int,
  lcase_mb,
  NULL, /* septoken */
  "sys-int",
  UDM_CHARSET_UNICODE,
  NULL,
  NULL,
  NULL
};

static UDM_CHARSET built_charsets[]={
{
  UDM_CHARSET_8859_1,
  udm_mb_wc_latin1,
  udm_wc_mb_latin1,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "iso-8859-1",
  UDM_CHARSET_WESTERN,
  ctype_iso_8859_1,
  tab_8859_1_uni,
  idx_uni_8859_1
},
{
  UDM_CHARSET_8859_10,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "iso-8859-10",
  UDM_CHARSET_NORDIC,
  ctype_iso_8859_10,
  tab_8859_10_uni,
  idx_uni_8859_10
},
{
  UDM_CHARSET_8859_11,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "iso-8859-11",
  UDM_CHARSET_THAI,
  ctype_iso_8859_11,
  tab_8859_11_uni,
  idx_uni_8859_11
},
{
  UDM_CHARSET_8859_13,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "iso-8859-13",
  UDM_CHARSET_BALTIC,
  ctype_iso_8859_13,
  tab_8859_13_uni,
  idx_uni_8859_13
},
{
  UDM_CHARSET_8859_14,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "iso-8859-14",
  UDM_CHARSET_CELTIC,
  ctype_iso_8859_14,
  tab_8859_14_uni,
  idx_uni_8859_14
},
{
  UDM_CHARSET_8859_15,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "iso-8859-15",
  UDM_CHARSET_WESTERN,
  ctype_iso_8859_15,
  tab_8859_15_uni,
  idx_uni_8859_15
},
{
  UDM_CHARSET_8859_16,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "iso-8859-16",
  UDM_CHARSET_CENTRAL,
  ctype_iso_8859_16,
  tab_8859_16_uni,
  idx_uni_8859_16
},
{
  UDM_CHARSET_8859_2,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "iso-8859-2",
  UDM_CHARSET_CENTRAL,
  ctype_iso_8859_2,
  tab_8859_2_uni,
  idx_uni_8859_2
},
{
  UDM_CHARSET_8859_3,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "iso-8859-3",
  UDM_CHARSET_SOUTHERN,
  ctype_iso_8859_3,
  tab_8859_3_uni,
  idx_uni_8859_3
},
{
  UDM_CHARSET_8859_4,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "iso-8859-4",
  UDM_CHARSET_BALTIC,
  ctype_iso_8859_4,
  tab_8859_4_uni,
  idx_uni_8859_4
},
{
  UDM_CHARSET_8859_5,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "iso-8859-5",
  UDM_CHARSET_CYRILLIC,
  ctype_iso_8859_5,
  tab_8859_5_uni,
  idx_uni_8859_5
},
{
  UDM_CHARSET_8859_6,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "iso-8859-6",
  UDM_CHARSET_ARABIC,
  ctype_iso_8859_6,
  tab_8859_6_uni,
  idx_uni_8859_6
},
{
  UDM_CHARSET_8859_7,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "iso-8859-7",
  UDM_CHARSET_GREEK,
  ctype_iso_8859_7,
  tab_8859_7_uni,
  idx_uni_8859_7
},
{
  UDM_CHARSET_8859_8,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "iso-8859-8",
  UDM_CHARSET_HEBREW,
  ctype_iso_8859_8,
  tab_8859_8_uni,
  idx_uni_8859_8
},
{
  UDM_CHARSET_8859_9,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "iso-8859-9",
  UDM_CHARSET_TURKISH,
  ctype_iso_8859_9,
  tab_8859_9_uni,
  idx_uni_8859_9
},
{
  UDM_CHARSET_ARMSCII_8,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "armscii-8",
  UDM_CHARSET_ARMENIAN,
  ctype_armscii_8,
  tab_armscii_8_uni,
  idx_uni_armscii_8
},
{
  UDM_CHARSET_CP1250,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "windows-1250",
  UDM_CHARSET_CENTRAL,
  ctype_windows_1250,
  tab_cp1250_uni,
  idx_uni_cp1250
},
{
  UDM_CHARSET_CP1251,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "windows-1251",
  UDM_CHARSET_CYRILLIC,
  ctype_windows_1251,
  tab_cp1251_uni,
  idx_uni_cp1251
},
{
  UDM_CHARSET_CP1252,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "windows-1252",
  UDM_CHARSET_WESTERN,
  ctype_windows_1252,
  tab_cp1252_uni,
  idx_uni_cp1252
},
{
  UDM_CHARSET_CP1253,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "windows-1253",
  UDM_CHARSET_GREEK,
  ctype_windows_1253,
  tab_cp1253_uni,
  idx_uni_cp1253
},
{
  UDM_CHARSET_CP1254,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "windows-1254",
  UDM_CHARSET_TURKISH,
  ctype_windows_1254,
  tab_cp1254_uni,
  idx_uni_cp1254
},
{
  UDM_CHARSET_CP1255,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "windows-1255",
  UDM_CHARSET_HEBREW,
  ctype_windows_1255,
  tab_cp1255_uni,
  idx_uni_cp1255
},
{
  UDM_CHARSET_CP1256,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "windows-1256",
  UDM_CHARSET_ARABIC,
  ctype_windows_1256,
  tab_cp1256_uni,
  idx_uni_cp1256
},
{
  UDM_CHARSET_CP1257,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "windows-1257",
  UDM_CHARSET_BALTIC,
  ctype_windows_1257,
  tab_cp1257_uni,
  idx_uni_cp1257
},
{
  UDM_CHARSET_CP1258,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "windows-1258",
  UDM_CHARSET_VIETNAMESE,
  ctype_windows_1258,
  tab_cp1258_uni,
  idx_uni_cp1258
},
{
  UDM_CHARSET_CP437,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "cp437",
  UDM_CHARSET_WESTERN,
  ctype_cp437,
  tab_cp437_uni,
  idx_uni_cp437
},
{
  UDM_CHARSET_CP850,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "cp850",
  UDM_CHARSET_WESTERN,
  ctype_cp850,
  tab_cp850_uni,
  idx_uni_cp850
},
{
  UDM_CHARSET_CP852,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "cp852",
  UDM_CHARSET_CENTRAL,
  ctype_cp852,
  tab_cp852_uni,
  idx_uni_cp852
},
{
  UDM_CHARSET_CP855,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "cp855",
  UDM_CHARSET_CYRILLIC,
  ctype_cp855,
  tab_cp855_uni,
  idx_uni_cp855
},
{
  UDM_CHARSET_CP857,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "cp857",
  UDM_CHARSET_TURKISH,
  ctype_cp857,
  tab_cp857_uni,
  idx_uni_cp857
},
{
  UDM_CHARSET_CP860,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "cp860",
  UDM_CHARSET_WESTERN,
  ctype_cp860,
  tab_cp860_uni,
  idx_uni_cp860
},
{
  UDM_CHARSET_CP861,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "cp861",
  UDM_CHARSET_ICELANDIC,
  ctype_cp861,
  tab_cp861_uni,
  idx_uni_cp861
},
{
  UDM_CHARSET_CP862,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "cp862",
  UDM_CHARSET_HEBREW,
  ctype_cp862,
  tab_cp862_uni,
  idx_uni_cp862
},
{
  UDM_CHARSET_CP863,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "cp863",
  UDM_CHARSET_WESTERN,
  ctype_cp863,
  tab_cp863_uni,
  idx_uni_cp863
},
{
  UDM_CHARSET_CP864,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "cp864",
  UDM_CHARSET_ARABIC,
  ctype_cp864,
  tab_cp864_uni,
  idx_uni_cp864
},
{
  UDM_CHARSET_CP865,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "cp865",
  UDM_CHARSET_NORDIC,
  ctype_cp865,
  tab_cp865_uni,
  idx_uni_cp865
},
{
  UDM_CHARSET_CP866,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "cp866",
  UDM_CHARSET_CYRILLIC,
  ctype_cp866,
  tab_cp866_uni,
  idx_uni_cp866
},
{
  UDM_CHARSET_CP869,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "cp869",
  UDM_CHARSET_GREEK,
  ctype_cp869,
  tab_cp869_uni,
  idx_uni_cp869
},
{
  UDM_CHARSET_CP874,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "cp874",
  UDM_CHARSET_THAI,
  ctype_cp874,
  tab_cp874_uni,
  idx_uni_cp874
},
{
  UDM_CHARSET_KOI8_R,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "koi8-r",
  UDM_CHARSET_CYRILLIC,
  ctype_koi8_r,
  tab_koi8_r_uni,
  idx_uni_koi8_r
},
{
  UDM_CHARSET_KOI8_U,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "koi8-u",
  UDM_CHARSET_CYRILLIC,
  ctype_koi8_u,
  tab_koi8_u_uni,
  idx_uni_koi8_u
},
{
  UDM_CHARSET_MACARABIC,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "MacArabic",
  UDM_CHARSET_ARABIC,
  ctype_MacArabic,
  tab_macarabic_uni,
  idx_uni_macarabic
},
{
  UDM_CHARSET_MACCE,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "MacCE",
  UDM_CHARSET_CENTRAL,
  ctype_MacCE,
  tab_macce_uni,
  idx_uni_macce
},
{
  UDM_CHARSET_MACCROATIAN,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "MacCroatian",
  UDM_CHARSET_CENTRAL,
  ctype_MacCroatian,
  tab_maccroatian_uni,
  idx_uni_maccroatian
},
{
  UDM_CHARSET_MACCYRILLIC,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "MacCyrillic",
  UDM_CHARSET_CYRILLIC,
  ctype_MacCyrillic,
  tab_maccyrillic_uni,
  idx_uni_maccyrillic
},
{
  UDM_CHARSET_MACGREEK,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "MacGreek",
  UDM_CHARSET_GREEK,
  ctype_MacGreek,
  tab_macgreek_uni,
  idx_uni_macgreek
},
{
  UDM_CHARSET_MACHEBREW,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "MacHebrew",
  UDM_CHARSET_HEBREW,
  ctype_MacHebrew,
  tab_machebrew_uni,
  idx_uni_machebrew
},
{
  UDM_CHARSET_MACICELAND,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "MacIceland",
  UDM_CHARSET_ICELANDIC,
  ctype_MacIceland,
  tab_maciceland_uni,
  idx_uni_maciceland
},
{
  UDM_CHARSET_MACROMAN,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "MacRoman",
  UDM_CHARSET_WESTERN,
  ctype_MacRoman,
  tab_macroman_uni,
  idx_uni_macroman
},
{
  UDM_CHARSET_MACROMANIA,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "MacRomania",
  UDM_CHARSET_CENTRAL,
  ctype_MacRomania,
  tab_macromania_uni,
  idx_uni_macromania
},
{
  UDM_CHARSET_MACTHAI,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "MacThai",
  UDM_CHARSET_THAI,
  ctype_MacThai,
  tab_macthai_uni,
  idx_uni_macthai
},
{
  UDM_CHARSET_MACTURKISH,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "MacTurkish",
  UDM_CHARSET_TURKISH,
  ctype_MacTurkish,
  tab_macturkish_uni,
  idx_uni_macturkish
},
{
  UDM_CHARSET_US_ASCII,
  udm_mb_wc_ascii,
  udm_wc_mb_ascii,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "us-ascii",
  UDM_CHARSET_WESTERN,
  ctype_us_ascii,
  tab_us_ascii_uni,
  idx_uni_us_ascii
},
{
  UDM_CHARSET_VISCII,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "viscii",
  UDM_CHARSET_VIETNAMESE,
  ctype_viscii,
  tab_viscii_uni,
  idx_uni_viscii
},
{
  UDM_CHARSET_GEOSTD8,
  udm_mb_wc_8bit,
  udm_wc_mb_8bit,
  lcase_8bit,
  UdmStrGetSepToken8bit,
  "geostd8",
  UDM_CHARSET_GEORGIAN,
  ctype_geostd8,
  tab_geostd8_uni,
  idx_uni_geostd8
},
{
  UDM_CHARSET_UTF8,
  udm_mb_wc_utf8,
  udm_wc_mb_utf8,
  lcase_mb,
  NULL, /* septoken */
  "utf-8",
  UDM_CHARSET_UNICODE,
  NULL,
  NULL,
  NULL
},
{
  UDM_CHARSET_SYS_INT,
  udm_mb_wc_sys_int,
  udm_wc_mb_sys_int,
  lcase_mb,
  NULL, /* septok */
  "sys-int",
  UDM_CHARSET_UNICODE,
  NULL,
  NULL,
  NULL
},

#ifdef HAVE_CHARSET_gb2312
{
  UDM_CHARSET_GB2312,
  udm_mb_wc_gb2312,
  udm_wc_mb_gb2312,
  lcase_mb,
  NULL, /* septok */
  "gb2312",
  UDM_CHARSET_CHINESE_SIMPLIFIED,
  NULL,
  NULL,
  NULL
},
#endif

#ifdef HAVE_CHARSET_big5
{
  UDM_CHARSET_BIG5,
  udm_mb_wc_big5,
  udm_wc_mb_big5,
  lcase_mb,
  NULL, /* septok */
  "big5",
  UDM_CHARSET_CHINESE_TRADITIONAL,
  NULL,
  NULL,
  NULL
},
#endif

#ifdef HAVE_CHARSET_japanese
{
  UDM_CHARSET_SJIS,
  udm_mb_wc_sjis,
  udm_wc_mb_sjis,
  lcase_mb,
  NULL, /* septok */
  "Shift-JIS",
  UDM_CHARSET_JAPANESE,
  NULL,
  NULL,
  NULL
},
#endif

#ifdef HAVE_CHARSET_euc_kr
{
  UDM_CHARSET_EUC_KR,
  udm_mb_wc_euc_kr,
  udm_wc_mb_euc_kr,
  lcase_mb,
  NULL, /* septok */
  "EUC-KR",
  UDM_CHARSET_KOREAN,
  NULL,
  NULL,
  NULL
},
#endif

#ifdef HAVE_CHARSET_japanese
{
  UDM_CHARSET_EUC_JP,
  udm_mb_wc_euc_jp,
  udm_wc_mb_euc_jp,
  lcase_mb,
  NULL, /* septok */
  "EUC-JP",
  UDM_CHARSET_JAPANESE,
  NULL,
  NULL,
  NULL
},
#endif


#ifdef HAVE_CHARSET_gbk
{
  UDM_CHARSET_GBK,
  udm_mb_wc_gbk,
  udm_wc_mb_gbk,
  lcase_mb,
  NULL, /* septok */
  "GBK",
  UDM_CHARSET_CHINESE_SIMPLIFIED,
  NULL,
  NULL,
  NULL
},
#endif

#ifdef HAVE_CHARSET_gujarati
{
  UDM_CHARSET_GUJARATI,
  udm_mb_wc_gujarati,
  udm_wc_mb_gujarati,
  lcase_mb,
  NULL, /* septok */
  "MacGujarati",
  UDM_CHARSET_INDIAN,
  NULL,
  NULL,
  NULL
},
#endif

#ifdef HAVE_CHARSET_tscii
{
  UDM_CHARSET_TSCII,
  udm_mb_wc_tscii,
  udm_wc_mb_tscii,
  lcase_mb,
  NULL, /* septok */
  "tscii",
  UDM_CHARSET_INDIAN,
  NULL,
  NULL,
  NULL
},
#endif

#ifdef HAVE_CHARSET_japanese
{
  UDM_CHARSET_ISO2022JP,
  udm_mb_wc_iso2022jp,
  udm_wc_mb_iso2022jp,
  lcase_mb,
  NULL, /* septok */
  "iso-2022-jp",
  UDM_CHARSET_JAPANESE,
  NULL,
  NULL,
  NULL
},
#endif

{
  0,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  NULL,
  NULL,
  NULL
}

};


typedef struct
{
  const	char *name;
  int        id;
} UDM_CHARSET_ALIAS;


/* Must be in alphabetic order */
static UDM_CHARSET_ALIAS alias[]={
	{"437",			UDM_CHARSET_CP437},
	{"850",			UDM_CHARSET_CP850},
	{"852",			UDM_CHARSET_CP852},
	{"855",			UDM_CHARSET_CP855},
	{"857",			UDM_CHARSET_CP857},
	{"860",			UDM_CHARSET_CP860},
	{"861",			UDM_CHARSET_CP861},
	{"862",			UDM_CHARSET_CP862},
	{"863",			UDM_CHARSET_CP863},
	{"864",			UDM_CHARSET_CP864},
	{"865",			UDM_CHARSET_CP865},
	{"866",			UDM_CHARSET_CP866},
	{"869",			UDM_CHARSET_CP869},
	{"874",			UDM_CHARSET_CP874},
	{"ansi_x3.4-1968",	UDM_CHARSET_US_ASCII},
	{"arabic",		UDM_CHARSET_8859_6},
	{"armscii-8",		UDM_CHARSET_ARMSCII_8},
	{"armscii8",		UDM_CHARSET_ARMSCII_8},
	{"ascii",		UDM_CHARSET_US_ASCII},
	{"asmo-708",		UDM_CHARSET_8859_6},
	{"big-5",		UDM_CHARSET_BIG5},
	{"big-five",		UDM_CHARSET_BIG5},
	{"big5",		UDM_CHARSET_BIG5},
	{"bigfive",		UDM_CHARSET_BIG5},
	{"chinese",		UDM_CHARSET_GB2312},
	{"cn-big5",		UDM_CHARSET_BIG5},
	{"cp-1250",		UDM_CHARSET_CP1250},
	{"cp-1251",		UDM_CHARSET_CP1251},
	{"cp-1252",		UDM_CHARSET_CP1252},
	{"cp-1253",		UDM_CHARSET_CP1253},
	{"cp-1254",		UDM_CHARSET_CP1254},
	{"cp-1255",		UDM_CHARSET_CP1255},
	{"cp-1256",		UDM_CHARSET_CP1256},
	{"cp-1257",		UDM_CHARSET_CP1257},
	{"cp-1258",		UDM_CHARSET_CP1258},
	{"cp1250",		UDM_CHARSET_CP1250},
	{"cp1251",		UDM_CHARSET_CP1251},
	{"cp1252",		UDM_CHARSET_CP1252},
	{"cp1253",		UDM_CHARSET_CP1253},
	{"cp1254",		UDM_CHARSET_CP1254},
	{"cp1255",		UDM_CHARSET_CP1255},
	{"cp1256",		UDM_CHARSET_CP1256},
	{"cp1257",		UDM_CHARSET_CP1257},
	{"cp1258",		UDM_CHARSET_CP1258},
	{"cp367",		UDM_CHARSET_US_ASCII},
	{"cp437",		UDM_CHARSET_CP437},
	{"cp819",		UDM_CHARSET_8859_1},
	{"cp850",		UDM_CHARSET_CP850},
	{"cp852",		UDM_CHARSET_CP852},
	{"cp855",		UDM_CHARSET_CP855},
	{"cp857",		UDM_CHARSET_CP857},
	{"cp860",		UDM_CHARSET_CP860},
	{"cp861",		UDM_CHARSET_CP861},
	{"cp862",		UDM_CHARSET_CP862},
	{"cp863",		UDM_CHARSET_CP863},
	{"cp864",		UDM_CHARSET_CP864},
	{"cp865",		UDM_CHARSET_CP865},
	{"cp866",		UDM_CHARSET_CP866},
	{"cp869",		UDM_CHARSET_CP869},
	{"cp874",		UDM_CHARSET_CP874},
	{"cp936",		UDM_CHARSET_GBK},
	{"cp950",		UDM_CHARSET_BIG5},
	{"cs874",		UDM_CHARSET_CP874},
	{"csascii",		UDM_CHARSET_US_ASCII},
	{"csbig5",		UDM_CHARSET_BIG5},
	{"cseucjp",		UDM_CHARSET_EUC_JP},
	{"cseuckr",		UDM_CHARSET_EUC_KR},
	{"csgb2312",		UDM_CHARSET_GB2312},
	{"csibm866",		UDM_CHARSET_CP866},
	{"csibm869",		UDM_CHARSET_CP869},
	{"csiso58gb231280",	UDM_CHARSET_GB2312},
	{"csisolatin1",		UDM_CHARSET_8859_1},
	{"csisolatin2",		UDM_CHARSET_8859_2},
	{"csisolatin3",		UDM_CHARSET_8859_3},
	{"csisolatin4",		UDM_CHARSET_8859_4},
	{"csisolatin5",		UDM_CHARSET_8859_9},
	{"csisolatin6",		UDM_CHARSET_8859_10},
	{"csisolatinarabic",	UDM_CHARSET_8859_6},
	{"csisolatincyrillic",	UDM_CHARSET_8859_5},
	{"csisolatingreek",	UDM_CHARSET_8859_7},
	{"csisolatinhebrew",	UDM_CHARSET_8859_8},
	{"cskoi8r",		UDM_CHARSET_KOI8_R},
	{"csmacintosh",		UDM_CHARSET_MACROMAN},
	{"cspc850multilingual",	UDM_CHARSET_CP850},
	{"csshiftjis",		UDM_CHARSET_SJIS},
	{"csviscii",		UDM_CHARSET_VISCII},
	{"cyrillic",		UDM_CHARSET_8859_5},
	{"ecma-114",		UDM_CHARSET_8859_6},
	{"ecma-118",		UDM_CHARSET_8859_7},
	{"elot_928",		UDM_CHARSET_8859_7},
	{"euc-jp",		UDM_CHARSET_EUC_JP},
	{"euc-kr",		UDM_CHARSET_EUC_KR},
	{"eucjp",		UDM_CHARSET_EUC_JP},
	{"euckr",		UDM_CHARSET_EUC_KR},
	{"gb2312",		UDM_CHARSET_GB2312},
	{"gb_2312-80",		UDM_CHARSET_GB2312},
	{"gbk",			UDM_CHARSET_GBK},
	{"geo8-gov",            UDM_CHARSET_GEOSTD8},
	{"geostd8",             UDM_CHARSET_GEOSTD8},
	{"greek",		UDM_CHARSET_8859_7},
	{"greek8",		UDM_CHARSET_8859_7},
	{"hebrew",		UDM_CHARSET_8859_8},
	{"ibm367",		UDM_CHARSET_US_ASCII},
	{"ibm437",		UDM_CHARSET_CP437},
	{"ibm819",		UDM_CHARSET_8859_1},
	{"ibm850",		UDM_CHARSET_CP850},
	{"ibm852",		UDM_CHARSET_CP852},
	{"ibm855",		UDM_CHARSET_CP855},
	{"ibm857",		UDM_CHARSET_CP857},
	{"ibm860",		UDM_CHARSET_CP860},
	{"ibm861",		UDM_CHARSET_CP861},
	{"ibm862",		UDM_CHARSET_CP862},
	{"ibm863",		UDM_CHARSET_CP863},
	{"ibm864",		UDM_CHARSET_CP864},
	{"ibm865",		UDM_CHARSET_CP865},
	{"ibm866",		UDM_CHARSET_CP866},
	{"ibm869",		UDM_CHARSET_CP869},
	{"ibm874",		UDM_CHARSET_CP874},
	{"iso-2022-jp",         UDM_CHARSET_ISO2022JP},
	{"iso-8859-1",		UDM_CHARSET_8859_1},
	{"iso-8859-10",		UDM_CHARSET_8859_10},
	{"iso-8859-11",		UDM_CHARSET_8859_11},
	{"iso-8859-13",		UDM_CHARSET_8859_13},
	{"iso-8859-14",		UDM_CHARSET_8859_14},
	{"iso-8859-15",		UDM_CHARSET_8859_15},
	{"iso-8859-16",		UDM_CHARSET_8859_16},
	{"iso-8859-2",		UDM_CHARSET_8859_2},
	{"iso-8859-3",		UDM_CHARSET_8859_3},
	{"iso-8859-4",		UDM_CHARSET_8859_4},
	{"iso-8859-5",		UDM_CHARSET_8859_5},
	{"iso-8859-6",		UDM_CHARSET_8859_6},
	{"iso-8859-7",		UDM_CHARSET_8859_7},
	{"iso-8859-8",		UDM_CHARSET_8859_8},
	{"iso-8859-9",		UDM_CHARSET_8859_9},
	{"iso-ir-100",		UDM_CHARSET_8859_1},
	{"iso-ir-101",		UDM_CHARSET_8859_2},
	{"iso-ir-109",		UDM_CHARSET_8859_3},
	{"iso-ir-110",		UDM_CHARSET_8859_4},
	{"iso-ir-126",		UDM_CHARSET_8859_7},
	{"iso-ir-127",		UDM_CHARSET_8859_6},
	{"iso-ir-138",		UDM_CHARSET_8859_8},
	{"iso-ir-144",		UDM_CHARSET_8859_5},
	{"iso-ir-148",		UDM_CHARSET_8859_9},
	{"iso-ir-157",		UDM_CHARSET_8859_10},
	{"iso-ir-179",		UDM_CHARSET_8859_13},
	{"iso-ir-199",		UDM_CHARSET_8859_14},
	{"iso-ir-203",		UDM_CHARSET_8859_15},
	{"iso-ir-226",		UDM_CHARSET_8859_16},
	{"iso-ir-58",		UDM_CHARSET_GB2312},
	{"iso-ir-6",		UDM_CHARSET_US_ASCII},
	{"iso646-us",		UDM_CHARSET_US_ASCII},
	{"iso8859-1",		UDM_CHARSET_8859_1},
	{"iso8859-10",		UDM_CHARSET_8859_10},
	{"iso8859-11",		UDM_CHARSET_8859_11},
	{"iso8859-13",		UDM_CHARSET_8859_13},
	{"iso8859-14",		UDM_CHARSET_8859_14},
	{"iso8859-15",		UDM_CHARSET_8859_15},
	{"iso8859-16",		UDM_CHARSET_8859_16},
	{"iso8859-2",		UDM_CHARSET_8859_2},
	{"iso8859-3",		UDM_CHARSET_8859_3},
	{"iso8859-4",		UDM_CHARSET_8859_4},
	{"iso8859-5",		UDM_CHARSET_8859_5},
	{"iso8859-6",		UDM_CHARSET_8859_6},
	{"iso8859-7",		UDM_CHARSET_8859_7},
	{"iso8859-8",		UDM_CHARSET_8859_8},
	{"iso8859-9",		UDM_CHARSET_8859_9},
	{"iso_646.irv:1991",	UDM_CHARSET_US_ASCII},
	{"iso_8859-1",		UDM_CHARSET_8859_1},
	{"iso_8859-10",		UDM_CHARSET_8859_10},
	{"iso_8859-10:1992",	UDM_CHARSET_8859_10},
	{"iso_8859-11",		UDM_CHARSET_8859_11},
	{"iso_8859-11:1992",	UDM_CHARSET_8859_11},
	{"iso_8859-13",		UDM_CHARSET_8859_13},
	{"iso_8859-14",		UDM_CHARSET_8859_14},
	{"iso_8859-14:1998",	UDM_CHARSET_8859_14},
	{"iso_8859-15",		UDM_CHARSET_8859_15},
	{"iso_8859-15:1998",	UDM_CHARSET_8859_15},
	{"iso_8859-16",		UDM_CHARSET_8859_16},
	{"iso_8859-16:2000",	UDM_CHARSET_8859_16},
	{"iso_8859-1:1987",	UDM_CHARSET_8859_1},
	{"iso_8859-2",		UDM_CHARSET_8859_2},
	{"iso_8859-2:1987",	UDM_CHARSET_8859_2},
	{"iso_8859-3",		UDM_CHARSET_8859_3},
	{"iso_8859-3:1988",	UDM_CHARSET_8859_3},
	{"iso_8859-4",		UDM_CHARSET_8859_4},
	{"iso_8859-4:1988",	UDM_CHARSET_8859_4},
	{"iso_8859-5",		UDM_CHARSET_8859_5},
	{"iso_8859-5:1988",	UDM_CHARSET_8859_5},
	{"iso_8859-6",		UDM_CHARSET_8859_6},
	{"iso_8859-6:1987",	UDM_CHARSET_8859_6},
	{"iso_8859-7",		UDM_CHARSET_8859_7},
	{"iso_8859-7:1987",	UDM_CHARSET_8859_7},
	{"iso_8859-8",		UDM_CHARSET_8859_8},
	{"iso_8859-8:1988",	UDM_CHARSET_8859_8},
	{"iso_8859-9",		UDM_CHARSET_8859_9},
	{"iso_8859-9:1989",	UDM_CHARSET_8859_9},
	{"koi8-r",		UDM_CHARSET_KOI8_R},
	{"koi8-u",		UDM_CHARSET_KOI8_U},
	{"koi8r",		UDM_CHARSET_KOI8_R},
	{"koi8u",		UDM_CHARSET_KOI8_U},
	{"l1",			UDM_CHARSET_8859_1},
	{"l2",			UDM_CHARSET_8859_2},
	{"l3",			UDM_CHARSET_8859_3},
	{"l4",			UDM_CHARSET_8859_4},
	{"l5",			UDM_CHARSET_8859_9},
	{"l6",			UDM_CHARSET_8859_10},
	{"l7",			UDM_CHARSET_8859_13},
	{"l8",			UDM_CHARSET_8859_14},
	{"latin1",		UDM_CHARSET_8859_1},
	{"latin2",		UDM_CHARSET_8859_2},
	{"latin3",		UDM_CHARSET_8859_3},
	{"latin4",		UDM_CHARSET_8859_4},
	{"latin5",		UDM_CHARSET_8859_9},
	{"latin6",		UDM_CHARSET_8859_10},
	{"latin7",		UDM_CHARSET_8859_13},
	{"latin8",		UDM_CHARSET_8859_14},
	{"mac",			UDM_CHARSET_MACROMAN},
	{"macarabic",		UDM_CHARSET_MACARABIC},
	{"macce",		UDM_CHARSET_MACCE},
	{"maccentraleurope",	UDM_CHARSET_MACCE},
	{"maccroation",		UDM_CHARSET_MACCROATIAN},
	{"maccyrillic",		UDM_CHARSET_MACCYRILLIC},
	{"macgreek",		UDM_CHARSET_MACGREEK},
	{"macgujarati",         UDM_CHARSET_GUJARATI},
	{"machebrew",		UDM_CHARSET_MACHEBREW},
	{"macintosh",		UDM_CHARSET_MACROMAN},
	{"macisland",		UDM_CHARSET_MACICELAND},
	{"macroman",		UDM_CHARSET_MACROMAN},
	{"macromania",		UDM_CHARSET_MACROMANIA},
	{"macthai",		UDM_CHARSET_MACTHAI},
	{"macturkish",		UDM_CHARSET_MACTURKISH},
	{"ms-ansi",		UDM_CHARSET_CP1252},
	{"ms-arab",		UDM_CHARSET_CP1256},
	{"ms-cyr",		UDM_CHARSET_CP1251},
	{"ms-ee",		UDM_CHARSET_CP1250},
	{"ms-greek",		UDM_CHARSET_CP1253},
	{"ms-hebr",		UDM_CHARSET_CP1255},
	{"ms-turk",		UDM_CHARSET_CP1254},
	{"ms_kanji",		UDM_CHARSET_SJIS},
	{"s-jis",		UDM_CHARSET_SJIS},
	{"shift-jis",		UDM_CHARSET_SJIS},
	{"shift_jis",		UDM_CHARSET_SJIS},
	{"sjis",		UDM_CHARSET_SJIS},
	{"sys-int",		UDM_CHARSET_SYS_INT},
	{"tactis",		UDM_CHARSET_8859_11},
	{"tis-620",		UDM_CHARSET_8859_11},
	{"tis620",		UDM_CHARSET_8859_11},
	{"tscii",               UDM_CHARSET_TSCII},
	{"ujis",		UDM_CHARSET_EUC_JP},
	{"us",			UDM_CHARSET_US_ASCII},
	{"us-ascii",		UDM_CHARSET_US_ASCII},
	{"utf-8",		UDM_CHARSET_UTF8},
	{"utf8",		UDM_CHARSET_UTF8},
	{"viscii",		UDM_CHARSET_VISCII},
	{"viscii1.1-1",		UDM_CHARSET_VISCII},
	{"win-1251",	        UDM_CHARSET_CP1251},
	{"win1251",	        UDM_CHARSET_CP1251},
	{"winbaltrim",		UDM_CHARSET_CP1257},
	{"windows-1250",	UDM_CHARSET_CP1250},
	{"windows-1251",	UDM_CHARSET_CP1251},
	{"windows-1252",	UDM_CHARSET_CP1252},
	{"windows-1253",	UDM_CHARSET_CP1253},
	{"windows-1254",	UDM_CHARSET_CP1254},
	{"windows-1255",	UDM_CHARSET_CP1255},
	{"windows-1256",	UDM_CHARSET_CP1256},
	{"windows-1257",	UDM_CHARSET_CP1257},
	{"windows-1258",	UDM_CHARSET_CP1258},
	{"windows-874",		UDM_CHARSET_CP874},
	{"windows-utf-8",	UDM_CHARSET_UTF8},
	{"x-euc-jp",		UDM_CHARSET_EUC_JP},
	{"x-mac-cyrillic",	UDM_CHARSET_MACCYRILLIC},
	{"x-sjis",		UDM_CHARSET_SJIS},
	{NULL,0}
};



__C_LINK UDM_CHARSET * __UDMCALL UdmGetCharSetByID(int id)
{
  UDM_CHARSET *c;
  
  for (c=built_charsets; c->name; c++)
    if (c->id==id)
      return c;
  return NULL;
}

const char * UdmCharsetCanonicalName(const char * s)
{
  UDM_CHARSET *c= UdmGetCharSet(s);
  if(!c)return NULL;
  return c->name;
}

UDM_CHARSET * __UDMCALL UdmGetCharSet(const char * name)
{
  int l,m,r,s;

  l= 0;
  s= r= sizeof(alias)/sizeof(UDM_CHARSET_ALIAS)-1;
  
  while(l < r)
  {
    m= (l+r)/2;
    if(strcasecmp(alias[m].name,name)<0)
      l=m+1;
    else
      r=m;
  }
  if(s==r)
    return(NULL);
  else
  {
    if(!strcasecmp(alias[r].name,name))
      return UdmGetCharSetByID(alias[r].id);
    else  
      return NULL;
  }
  return NULL;
}
