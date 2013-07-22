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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "udm_common.h"

#include "udm_utils.h"
#include "udm_uniconv.h"
#include "udm_searchtool.h"
#include "udm_vars.h"

#if 0
static UDM_WIDEWORD*
UdmWordInWWList(UDM_WIDEWORDLIST *List, int *tok, size_t toklen,
                int hlstop, size_t phrpos)
{
  size_t uw;
  
  for(uw=0; uw < List->nwords; uw++)
  {
    size_t slen;
    UDM_WIDEWORD *W= &List->Word[uw];
    if (W->phrpos != phrpos)
      continue;
    if (!hlstop && W->origin == UDM_WORD_ORIGIN_STOP)
      continue;
    slen= W->ulen;
    if (wordlen < slen)
      continue;
    if (wordlen == slen && !UdmUniStrNCaseCmp(tok, W->uword, slen))
      return W;
      
    if (wordlen > slen) switch (W->match)
    {
      case UDM_MATCH_BEGIN:
        if (!UdmUniStrNCaseCmp(tok, W->uword, slen))
          return W;
        break;
      case UDM_MATCH_END:
        if (!UdmUniStrNCaseCmp(tok + wordlen - slen, W->uword, slen))
          return W;
        break;
      case UDM_MATCH_SUBSTR:
        {
          size_t l1, l2;
          for (l1 = 0; l1 < wordlen; l1++)
          {
            if (l1 + slen > wordlen) break;
            for (l2 = 0; l2 < slen; l2++)
            {
              if (UdmUniToLower(tok[l1 + l2]) != UdmUniToLower(W->uword[l2]))
                break;
            }
            if (l2 == slen)
            {
              return W;
              break;
            }
          }
        }
        break;
    }
  }
  return NULL;
}
#endif


void
UdmExcerptConvInit(UDM_HIGHLIGHT_CONV *cnv,
                   UDM_CHARSET *wcs, UDM_CHARSET *src, UDM_CHARSET *dst)
{
  UdmConvInit(&cnv->uni_wcs, &udm_charset_sys_int, wcs, UDM_RECODE_HTML);
  UdmConvInit(&cnv->src_uni, src, &udm_charset_sys_int, UDM_RECODE_HTML);
  UdmConvInit(&cnv->uni_dst, &udm_charset_sys_int, dst, UDM_RECODE_HTML);
}


static UDM_WIDEWORD*
UdmWordInWWList2(UDM_WIDEWORDLIST *List,
                 int *tok, size_t toklen,
                 UDM_CONV *uni_wcs, UDM_CONV *lc_uni, UDM_CONV *uni_bc,
                 int hlstop, size_t phrpos)
{
  size_t uw;
  char word[128]= "";
  size_t wordlen= 0;
  UDM_UNIDATA *unidata= udm_unidata_default; /* FIXME */

  if (List->wm != UDM_MATCH_FULL)
  {
    wordlen= UdmConv(uni_wcs, word, sizeof(word) - 1,
                              (char*)tok, toklen*sizeof(int));
    word[wordlen]= '\0';
    UdmStrToLowerExt(unidata, uni_wcs->to, word, wordlen, UDM_RECODE_HTML);
  }
  
  for(uw=0; uw < List->nwords; uw++)
  {
    size_t W_len;
    UDM_WIDEWORD *W= &List->Word[uw];

    if (W->phrpos != phrpos)
      continue;
    if (!hlstop && W->origin == UDM_WORD_ORIGIN_STOP)
      continue;

    if (W->match == UDM_MATCH_FULL)
    {
      int cmp= List->strip_noaccents ?
          UdmStrCaseAccentCmp2(unidata, uni_wcs,
                               (const char*) tok, toklen * sizeof(int),
                               W->word, W->len) :
          UdmStrCaseCmp2(unidata, uni_wcs,
                         (const char*) tok, toklen*sizeof(int),
                         W->word, W->len);
      if (!cmp)
        return W;
      continue;
    }

    W_len= W->len;

    if (wordlen < W_len)
      continue;
    if (wordlen == W_len && !memcmp(word, W->word, W_len))
      return W;

    if (wordlen > W_len) switch (W->match)
    {
      case UDM_MATCH_BEGIN:
        if (!memcmp(word, W->word, W_len))
          return W;
        break;
      case UDM_MATCH_END:
        if (!memcmp(word + wordlen - W_len, W->word, W_len))
          return W;
        break;
      case UDM_MATCH_SUBSTR:
        if (strstr(word, W->word))
          return W;
        break;
      default:
        UDM_ASSERT(0); /* Inpossible */
    }
  }
  return NULL;
}


/*
  Remove hilight markers from a string and return
  its new length, in bytes.
*/
static size_t
UdmRemoveHl(UDM_CHARSET *cs, char *str, size_t from, size_t to)
{
  if (cs == &udm_charset_sys_int)
  {
    int *stri= (int*) str;
    int *s= stri + from / sizeof(int);
    int *e= stri + to / sizeof(int);
    int *dst= s;
    for (; s < e; s++)
    { 
      if (*s != 2 && *s != 3)
        *dst++= *s;
    }
    return (dst - stri) * sizeof(int);
  }
  else
  {
    char *s= str + from, *e= str + to, *dst= s;
    for (; s < e; s++)
    { 
      if (*s != 2 && *s != 3)
        *dst++= *s;
    }
    return dst - str;
  }
}


static size_t
UdmHlAppend(UDM_CONV *uni_bc, UDM_WIDEWORD *found,
            char *dst, size_t dstlen, size_t dstmaxlen,
            int *tok, size_t toklen)
{
  int i2= 2, i3= 3;

  if (found)
  {
    dstlen+= UdmConv(uni_bc, dst + dstlen, dstmaxlen, (char*) &i2, sizeof(i2));
  }
  if (uni_bc->to == &udm_charset_sys_int)
  {
    memcpy(dst + dstlen, tok, sizeof(*tok) * toklen);
    dstlen+= sizeof(*tok) * toklen;
  }
  else
    dstlen+= UdmConv(uni_bc, dst + dstlen, dstmaxlen, (char*) tok, sizeof(*tok) * toklen);

  if (found)
    dstlen+= UdmConv(uni_bc, dst + dstlen, dstmaxlen, (char*) &i3, sizeof(i3));

  /*fprintf(stderr, "appended to '%.*s'\n", dstlen, dst);*/

  return dstlen;
}

/*
#define DEBUG_HL 0
*/

/* Returns a 0-terminated string */

size_t
UdmHlConvertExtWithConv(UDM_AGENT *Agent, char *dst, size_t dstmaxlen,
                        UDM_WIDEWORDLIST *List,
                        const char *src, size_t srclen,
                        UDM_HIGHLIGHT_CONV *ec,
                        int hilight_stopwords, int segmenter)
{
  int		*tok, *lt, ctype, *uni, *uend;
  int           i0= 0;
  size_t        dstlen= 0, dstlen_phr= 0, nfound=0, ulen;
  size_t        unimaxlen, expected_phrpos= 0;
  UDM_UNIDATA *unidata= udm_unidata_default; /* FIXME */

#ifdef DEBUG_HL
  fprintf(stderr, "wcs=%s fromcs=%s tocs=%s srclen=%d src='%s'\n",
          uni_wcs->to->name, lc_uni->from->name, uni_bc->to->name, srclen, src);
#endif


  /* Convert to unicode */
  unimaxlen= (srclen + 10) * sizeof(int);
  uni= (int *)UdmMalloc(unimaxlen);
  ulen= UdmConv(&ec->src_uni,(char*)uni, unimaxlen, src, srclen) / sizeof(int);
  uni[ulen]= '\0';
  if (segmenter)
  {
    uni= UdmUniSegmentByType(Agent, uni, segmenter, 0x09);
    ulen= UdmUniLen(uni);
  }
  
  uend= uni + ulen;

  /* Parse unicode string */
  for (tok= UdmUniGetSepToken(unidata, uni, uend, &lt, &ctype) ; tok ;
       tok= UdmUniGetSepToken(unidata, NULL, uend, &lt, &ctype))
  {
    size_t toklen= lt - tok;

    if (ctype == UDM_UNI_SEPAR || !List)
    {
      /* Don't append delimiters added by segmenter */
      if (segmenter && toklen == 1 && *tok == 0x09)
        continue;
      dstlen= UdmHlAppend(&ec->uni_dst, NULL, dst, dstlen, dstmaxlen, tok, toklen);
    }
    else
    {
      UDM_WIDEWORD *found= UdmWordInWWList2(List, tok, toklen,
                                            &ec->uni_wcs, &ec->src_uni, &ec->uni_dst,
                                            hilight_stopwords, expected_phrpos);
      if (found)
      {
        dstlen= UdmHlAppend(&ec->uni_dst, found, dst, dstlen, dstmaxlen, tok, toklen);
        nfound++;
        if (found->phrpos + 1 == found->phrlen)
        {
          /* last in phrase found */
          expected_phrpos= 0;
          dstlen_phr= dstlen;
          nfound= 0;
        }
        else
        {
          /* middle in phrase found */
          expected_phrpos++;
        }
      }
      else
      {
        /* No word found on expected phrase position, rollback */
        if (nfound)
        {          
          dstlen= UdmRemoveHl(ec->uni_dst.to, dst, dstlen_phr, dstlen);
          /*
            Search the same word on the first position,
            it can start new phrase.
            For example:
            Document: "a a b b"
            Query: "a-b"
          */
          found= UdmWordInWWList2(List, tok, toklen,
                                  &ec->uni_wcs, &ec->src_uni, &ec->uni_dst,
                                  hilight_stopwords, 0);
          if (found)
          {
            dstlen_phr= dstlen;
            expected_phrpos= 1;
            nfound= 1;
            dstlen= UdmHlAppend(&ec->uni_dst, found, dst, dstlen, dstmaxlen, tok, toklen);
            continue;
          }
        }
        
        dstlen= UdmHlAppend(&ec->uni_dst, NULL, dst, dstlen, dstmaxlen, tok, toklen);
        dstlen_phr= dstlen;
        expected_phrpos= 0;
        nfound= 0;
      }
    }
  }


#ifdef DEBUG_HL  
  fprintf(stderr, "end: expected_phrpos=%d dstlen=%d dstlen_phr=%d\n", expected_phrpos, dstlen, dstlen_phr);
#endif
  if (expected_phrpos > 0)
  {
    /* Roll back: incomplete last phrase */
    dstlen= UdmRemoveHl(ec->uni_dst.to, dst, dstlen_phr, dstlen);
  }

  UdmConv(&ec->uni_dst, dst + dstlen, dstmaxlen, (char*) &i0, sizeof(i0));
  UdmFree(uni);
  return dstlen;
}

static char*
UdmHlConvertExtWithConvDup(UDM_WIDEWORDLIST *List,
                           const char *src, size_t srclen,
                           UDM_HIGHLIGHT_CONV *ec,
                           int hlstop)
{
  size_t dstlen;
  char *dst;
  if(!src || !srclen)return NULL;
  dstlen= srclen * 14 + 10;
  dst= (char*)UdmMalloc(dstlen);
  UdmHlConvertExtWithConv(NULL, dst, dstlen,
                          List, src, srclen,
                          ec, hlstop, 0);
  return dst;
}


#ifdef NOT_YET
/*
  For character sets having "septok" and when lcs == bcs
  
  TODO: this function does not support HTML entities:
    &Auml;
    &#196;
    &#xC4;

  UdmWordInWWList2() should also be modified to unserstand entitites
  when searching words.
*/
static char *
UdmHlConvertExtQuick(UDM_WIDEWORDLIST *List, const char *src,
                     UDM_CHARSET *cs,
                     int hilight_stopwords)
{
  char        *dst;
  const char  *srcend, *tok, *lt;
  size_t      srclen, dstlen= 0, dstlen_phr= 0;
  size_t      dstmaxlen, expected_phrpos= 0;
  int ctype;

  if(!src || !(srclen = strlen(src)))
    return NULL;

  srcend= src + srclen;
  dstmaxlen= srclen * 14 + 10;
  dst= (char*)UdmMalloc(dstmaxlen + 1);

  for (tok= cs->septoken(cs, src, srcend, &lt, &ctype) ; tok ;
       tok= cs->septoken(cs, NULL, srcend, &lt, &ctype))
  {
    size_t toklen= lt - tok;

    if (ctype == UDM_UNI_SEPAR || !List || toklen > 127)
    {
      memcpy(dst + dstlen, tok, toklen);
      dstlen+= toklen;
    }
    else
    {
      UDM_WIDEWORD *found;
      {
        char tmp[128];
        memcpy(tmp, tok, toklen);
        tmp[toklen]= '\0';
        cs->lcase(cs, tmp, toklen);
        found= UdmWordInWWList2(List, tmp, toklen, hilight_stopwords, expected_phrpos);
      }

      if (dstlen + toklen + 2 >= dstmaxlen)
        break;
      
      if (found)
      {
        dst[dstlen++]= '\2';
        memcpy(dst + dstlen, tok, toklen);
        dstlen+= toklen;
        dst[dstlen++]= '\3';
      }
      else
      {
        memcpy(dst + dstlen, tok, toklen);
        dstlen+= toklen;
      }
      
      if (found)
      {
        if (found->phrpos + 1 == found->phrlen)
        {
          /* last in phrase found */
          expected_phrpos= 0;
          dstlen_phr= dstlen;
        }
        else
        {
          /* middle in phrase found */
          expected_phrpos++;
        }
      }
      else
      {
        /* No word found on expected phrase position, rollback */
        dstlen= UdmRemoveHl(cs, dst, dstlen_phr, dstlen);
        dstlen_phr= dstlen;
        expected_phrpos= 0;
      }
    }
  }

  if (expected_phrpos > 0)
  {
    /* Roll back: incomplete last phrase */
    dstlen= UdmRemoveHl(cs, dst, dstlen_phr, dstlen);
  }

  dst[dstlen]= '\0';
  return dst;
}
#endif


size_t UdmHlConvertExt(UDM_AGENT *Agent, char *dst, size_t dstlen,
                       UDM_WIDEWORDLIST *List, UDM_CHARSET *wcs,
                       const char * src, size_t length,
                       UDM_CHARSET * lcs, UDM_CHARSET * bcs,
                       int hlstop, int segmenter)
{
  UDM_HIGHLIGHT_CONV ec;
  UdmExcerptConvInit(&ec, wcs, lcs, bcs);
  return UdmHlConvertExtWithConv(Agent, dst, dstlen, List, src, length,
                                 &ec, hlstop, segmenter);
}

static
char *UdmHlConvertExtDup(UDM_WIDEWORDLIST *List, UDM_CHARSET *wcs,
                         const char * src, size_t srclen,
                         UDM_CHARSET * lcs, UDM_CHARSET * bcs, int hlstop)
{
  UDM_HIGHLIGHT_CONV ec;
  UdmExcerptConvInit(&ec, wcs, lcs, bcs);
  return UdmHlConvertExtWithConvDup(List, src, srclen, &ec, hlstop);
}


/* For PHP module compatibility */
char * UdmHlConvert(UDM_WIDEWORDLIST *List,const char * src,
                    UDM_CHARSET * lcs, UDM_CHARSET * bcs)
{
  return UdmHlConvertExtDup(List, lcs, src, strlen(src), lcs, bcs, 1);
}


int UdmConvert(UDM_ENV *Conf, UDM_RESULT *Res,
               UDM_CHARSET *lcs, UDM_CHARSET *bcs)
{
  size_t i;
  UDM_CONV lc_bc;
  UDM_HIGHLIGHT_CONV ec;
  int hlstop= UdmVarListFindBool(&Conf->Vars, "ExcerptStopword", 1);

  /* Init converters */
  UdmConvInit(&lc_bc, lcs, bcs, UDM_RECODE_HTML);
  UdmExcerptConvInit(&ec, bcs, lcs, bcs);

  /* Convert word list */
  for(i=0;i<Res->WWList.nwords;i++)
  {
    UDM_WIDEWORD *W=&Res->WWList.Word[i];
    size_t len= strlen(W->word);
    size_t newlen= UdmConvSizeNeeded(&lc_bc, len, UDM_RECODE_HTML);
    char *newval= (char*)UdmMalloc(newlen + 1);
    if (newval)
    {
      len= UdmConv(&lc_bc, newval, newlen, W->word, len);
      newval[len]= '\0';
      UDM_FREE(W->word);
      W->word=newval;
      W->len= len;
    }
  }
  
  /* Convert document sections */
  for(i=0;i<Res->num_rows;i++)
  {
    UDM_DOCUMENT  *D=&Res->Doc[i];
    size_t    sec;
    
    for(sec=0; sec < D->Sections.nvars; sec++)
    {
      UDM_VAR *Var= &D->Sections.Var[sec];
      
      /*
         A temporary fix to skip URL and CachedCopy:
         We will skip these sections for now.
         But this need a further fix in search.htm
         to distinguish two HTML formats:
         - HTML with &<>" escaped to &amp;&lt;&gt;&quot;
         - HTML with &<>" printed as is, no word hilight
         - HTML with &<>" printed as is, search word hilighted.
      */
      
      if (strcasecmp(Var->name,"URL") &&
          strcasecmp(Var->name,"CachedCopy") &&
          strcasecmp(Var->name,"Content-Type"))
      {
        char *newval;
        /* skip if highlight markers already exist - cluster node */
        if (Var->flags & UDM_VARFLAG_HL)
          continue;
        newval= UdmHlConvertExtWithConvDup(&Res->WWList,
                                           Var->val, Var->curlen,
                                           &ec, hlstop);
        UDM_FREE(Var->val);
        Var->val= newval;
      }
    }
  }
  
  /* Convert Env->Vars */
  for (i = 0; i < Conf->Vars.nvars; i++)
  {
    UDM_VAR *Var= &Conf->Vars.Var[i];
    if (UdmVarType(Var) == UDM_VAR_STR &&
        strcasecmp(Var->name, "HlBeg") &&
        strcasecmp(Var->name, "HlEnd"))
    {
      size_t len= strlen(Var->val);
      size_t newlen= UdmConvSizeNeeded(&lc_bc, len, UDM_RECODE_HTML);
      char *newval= (char*) UdmMalloc(newlen + 1);
      if (newval)
      {
        size_t convlen= UdmConv(&lc_bc, newval, newlen, Var->val, len);
        newval[convlen]= '\0';
        UDM_FREE(Var->val);
        Var->val= newval;
      }
    }
  }
  
  return UDM_OK;
}


static char rm_hl_special[256]=
{
/*00*/  1,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,
/*10*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*20*/  0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,  /*  !"#$%&'()*+,-./ */
/*30*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* 0123456789:;<=>? */
/*40*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* @ABCDEFGHIJKLMNO */
/*50*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* PQRSTUVWXYZ[\]^_ */
/*60*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* `abcdefghijklmno */
/*70*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* pqrstuvwxyz{|}~  */
/*80*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*90*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*A0*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*B0*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*C0*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*D0*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*E0*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*F0*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};


char* UdmRemoveHiLightDup(const char *s)
{
  size_t len=strlen(s)+1;
  char   *d, *res = (char*)UdmMalloc(len);
  
  for(d= res; ; s++)
  {
    if ((((unsigned char) *s) <= '&') &&
        rm_hl_special[(unsigned char) *s])
    {
      switch(s[0])
      {
        case '\0':
          goto ex;
        case '\2':
        case '\3':
          break;
        case '&':
          if (s[1] == '#')
          {
            const char *e;
            int code= 0;
          
            for (e= s+2; (*e >= '0') && (*e <= '9'); code= code*10 + e[0]-'0', e++);
            if (*e == ';')
            {
              *d++= (code < 128) ? code : '?';
              s= e;
              break;
            }
          }
          /* pass through */
        
        default:
          *d++=*s;
      }
    }
    else
      *d++= *s;
  }
ex:
  *d='\0';
  return res;
}
