/* Copyright (C) 2000-2013 Lavtech.com corp. All rights reserved.

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

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#include <stdio.h>

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_unicode.h"
#include "udm_unidata.h"
#include "udm_uniconv.h"
#include "udm_searchtool.h"
#include "udm_vars.h"
#include "udm_mutex.h"
#include "udm_chinese.h"
#include "udm_conf.h"

#ifdef CHASEN
#include <chasen.h>
#endif

#ifdef MECAB
#include <mecab.h>
#endif


/*
  Returns a segmented string.
  The original string is freed.
*/
static int *
UdmSegmentCJK_internal(UDM_AGENT *Agent, int *ustr, int separator)
{
  size_t dstlen= (1 + UdmUniLen(ustr) * 3) * sizeof(int);
  const int *src;
  int *dst, *dst0;
  UDM_UNIDATA *unidata= udm_unidata_default;

  if (!(dst0= (int*) UdmMalloc(dstlen)))
    return ustr;
    
  for (dst= dst0, src= ustr; *src; src++)
  {
    int ctype0= UdmUniCType(unidata, src[0]);
    int ctype1= UdmUniCType(unidata, src[1]);
    int is_cjk0= (UDM_UNI_CJK == ctype0);
    int is_cjk1= (UDM_UNI_CJK == ctype1);
    int is_pnc0= !ctype0;
    int is_pnc1= !ctype1;
    
    *dst++= *src;
    /*
      If two consequent CJK characters one ofter another
      then insert space between them.
    */
    if ((is_cjk0 && !is_pnc1) ||
        (is_cjk1 && !is_pnc0))
      *dst++= separator;
  }
  *dst= 0;
  UdmFree(ustr);
  return dst0;
}


static int *
UdmSegmentCJK(UDM_AGENT *Agent, int *ustr, int ch)
{
  return UdmSegmentCJK_internal(Agent, ustr, ch);
}

static int *
UdmSegmentCJK_phrase(UDM_AGENT *Agent, int *ustr)
{
  return UdmSegmentCJK_internal(Agent, ustr, '-');
}


#ifdef CHASEN
static int *
UdmUniSegmentChasen(UDM_AGENT *Indexer, int *ustr)
{
  char        *eucstr, *eucstr_seg;
  UDM_CHARSET *eucjp_cs;
  UDM_CONV    uni_eucjp, eucjp_uni;
  size_t      reslen;
  size_t       dstlen = UdmUniLen(ustr);
  
  eucjp_cs = UdmGetCharSet("euc-jp");
  if (!eucjp_cs) eucjp_cs = &udm_charset_sys_int;
  UdmConvInit(&uni_eucjp, &udm_charset_sys_int, eucjp_cs, UDM_RECODE_HTML);
  UdmConvInit(&eucjp_uni, eucjp_cs, &udm_charset_sys_int, UDM_RECODE_HTML);
  eucstr = (char*)UdmMalloc(12 * dstlen + 1);
  UdmConv(&uni_eucjp, eucstr, 12 * dstlen + 1, (char*)ustr, sizeof(*ustr)*(dstlen + 1));
  
  UDM_GETLOCK(Indexer, UDM_LOCK_SEGMENTER);
  eucstr_seg = chasen_sparse_tostr(eucstr);
  UDM_RELEASELOCK(Indexer, UDM_LOCK_SEGMENTER);
  
  reslen = strlen(eucstr_seg) + 1;
  ustr = (int*)UdmRealloc(ustr, reslen * sizeof(int));
  UdmConv(&eucjp_uni, (char*)ustr, reslen * sizeof(int), eucstr_seg, reslen);
  UDM_FREE(eucstr);
  return ustr;
}
#endif


#ifdef MECAB
static int *
UdmUniSegmentMecab(UDM_AGENT *Indexer, int *ustr)
{
  UDM_CHARSET *sjis_cs;
  UDM_CONV    uni_sjis, sjis_uni;
  char        *sjisstr;
  const char  *sjisstr_seg;
  size_t      reslen;
  size_t       dstlen = UdmUniLen(ustr);
  const mecab_dictionary_info_t *info= mecab_dictionary_info(Indexer->Conf->mecab);
  const char *csname= (info && info->charset) ? info->charset : "euc-jp";

  sjis_cs= UdmGetCharSet(csname);
  if (!sjis_cs) sjis_cs = &udm_charset_sys_int;
  UdmConvInit(&uni_sjis, &udm_charset_sys_int, sjis_cs, UDM_RECODE_HTML);
  UdmConvInit(&sjis_uni, sjis_cs, &udm_charset_sys_int, UDM_RECODE_HTML);

  sjisstr= (char*)UdmMalloc(12 * dstlen + 1);
  reslen= UdmConv(&uni_sjis, sjisstr, 12 * dstlen, (char*)ustr, sizeof(*ustr) * dstlen);
  sjisstr[reslen]= '\0';
    
  UDM_GETLOCK(Indexer, UDM_LOCK_CONF);
#ifdef HAVE_PTHREADS
  mecab_lock(Indexer->Conf->mecab);
#endif
  sjisstr_seg = mecab_sparse_tostr(Indexer->Conf->mecab, sjisstr);
#ifdef HAVE_PTHREADS
  mecab_unlock(Indexer->Conf->mecab);
#endif
  UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);

  reslen= strlen(sjisstr_seg);
  ustr= (int*)UdmRealloc(ustr, (reslen + 1) * sizeof(int));
  reslen= UdmConv(&sjis_uni, (char*)ustr, reslen * sizeof(int),
                    sjisstr_seg, reslen) / sizeof(int);
  ustr[reslen]= '\0';
  UDM_FREE(sjisstr);
  return ustr;
}
#endif


static int *
UdmUniSegmentFreqChi(UDM_AGENT *Indexer, int *ustr)
{
  int *seg_ustr;
  UDM_GETLOCK(Indexer, UDM_LOCK_CONF);
  seg_ustr= UdmSegmentByFreq(&Indexer->Conf->Chi, ustr);
  UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);
  if (seg_ustr != NULL)
  {
    UDM_FREE(ustr);
    ustr= seg_ustr;
  }
  return ustr;
}


static int *
UdmUniSegmentFreqThai(UDM_AGENT *Indexer, int *ustr)
{
  int *seg_ustr;
  UDM_GETLOCK(Indexer, UDM_LOCK_CONF);
  seg_ustr= UdmSegmentByFreq(&Indexer->Conf->Thai, ustr);
  UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);
  if (seg_ustr != NULL)
  {
    UDM_FREE(ustr);
    ustr= seg_ustr;
  }
  return ustr;
}


#define UDM_SEGMENTER_NONE     0
#define UDM_SEGMENTER_CJK      1
#define UDM_SEGMENTER_MECAB    2
#define UDM_SEGMENTER_CHASEN   3
#define UDM_SEGMENTER_FREQ_CN  4
#define UDM_SEGMENTER_FREQ_TH  5
#define UDM_SEGMENTER_CJK_PHR  6
#define UDM_SEGMENTER_MAX      6


int
UdmUniSegmenterFind(UDM_AGENT *Indexer, const char *lang, const char *seg)
{
  if (seg && !strcasecmp(seg, "CJK"))
    return UDM_SEGMENTER_CJK;

  if (seg && !strcasecmp(seg, "CJK-PHRASE"))
    return UDM_SEGMENTER_CJK_PHR;

#ifdef CHASEN
  if ((!seg  || !strcasecmp(seg, "Chasen")) &&
      (!lang || !strncasecmp(lang, "ja", 2)))
  {
    return UDM_SEGMENTER_CHASEN;
  }
#endif

#ifdef MECAB
  if ((!seg  || !strcasecmp(seg, "Mecab")) &&
      (!lang || !strncasecmp(lang, "ja", 2)))
  {
    return UDM_SEGMENTER_MECAB;
  }
#endif

#ifdef HAVE_CHARSET_gb2312
  if ((!seg  || !strcasecmp(seg, "Freq")) && Indexer->Conf->Chi.nwords &&
      (!lang || !lang[0] || 
       !strncasecmp(lang, "zh", 2) ||
       !strncasecmp(lang, "cn", 2)))
  {
    return UDM_SEGMENTER_FREQ_CN;
  }
#endif

  if ((!seg  || !strcasecmp(seg, "Freq")) && Indexer->Conf->Thai.nwords &&
      (!lang || !strncasecmp(lang, "th", 2)))
  {
    return UDM_SEGMENTER_FREQ_TH;
  }

  return UDM_SEGMENTER_NONE;
}


int *UdmUniSegmentByType(UDM_AGENT *Indexer, int *ustr, int type, int ch)
{
  UDM_ASSERT(type >= 0 && type <= UDM_SEGMENTER_MAX);
  
  switch (type)
  {
    case UDM_SEGMENTER_CJK: return UdmSegmentCJK(Indexer, ustr, ch);

    case UDM_SEGMENTER_CJK_PHR: return UdmSegmentCJK_phrase(Indexer, ustr);

#ifdef CHASEN
    case UDM_SEGMENTER_CHASEN: return UdmUniSegmentChasen(Indexer, ustr);
#endif

#ifdef MECAB
    case UDM_SEGMENTER_MECAB: return UdmUniSegmentMecab(Indexer, ustr);
#endif

#ifdef HAVE_CHARSET_gb2312
    case UDM_SEGMENTER_FREQ_CN: return UdmUniSegmentFreqChi(Indexer, ustr);
#endif

    case UDM_SEGMENTER_FREQ_TH: return UdmUniSegmentFreqThai(Indexer, ustr);

    default:;
  }
  return ustr;
}


int *UdmUniSegment(UDM_AGENT *Indexer, int *ustr, const char *lang, const char *seg)
{
  int segmenter= UdmUniSegmenterFind(Indexer, lang, seg);
  UDM_ASSERT(Indexer || !segmenter);
  return UdmUniSegmentByType(Indexer, ustr, segmenter, (int) ' ');
}
