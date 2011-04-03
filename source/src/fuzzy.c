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
#include <sys/stat.h>
#include <stdio.h>


#include "udm_common.h"
#include "udm_utils.h"
#include "udm_unidata.h"
#include "udm_searchtool.h"
#include "udm_spell.h"
#include "udm_word.h"
#include "udm_vars.h"
#include "udm_db.h"
#include "udm_db_int.h"
#include "udm_synonym.h"
#include "udm_sqldbms.h"


#define UDM_MAX_FORMS 256
#define UDM_MAX_NORMS 64

/*
  All the following combinations should
  work and get as many uword forms as possible:

  1. uword doesn't exist in ispell, its synonym doesn't exist in ispell.
     This last combination should also work if no ispell dictionaries loaded.
     Just copy all synonyms into result.
  2. DONE: both norm(uword) and its synonym exist in ispell
  3. norm(uword) exists in ispell, its synonym doesn't exist in ispell.
  4. uword doesn't exist in ispell, its synonym exists in ispell.
*/

static UDM_WIDEWORDLIST *UdmAllForms1(UDM_AGENT *Indexer,
                                      UDM_WIDEWORDLIST *result,
                                      const UDM_WIDEWORD *uword)
{
  UDM_SPELLLISTLIST *SLL= &Indexer->Conf->Spells;
  UDM_AFFIXLISTLIST *ALL= &Indexer->Conf->Affixes;
  UDM_SYNONYMLISTLIST *SYN= &Indexer->Conf->Synonym;
  char *Res[UDM_MAX_FORMS];
  char **ResCur= Res;
  char **ResEnd= Res + UDM_MAX_FORMS;
  char **R;
  UDM_AFFIXLIST *Al;  
  UDM_WIDEWORD w;
  UDM_CHARSET *lcs= Indexer->Conf->lcs;
  int sy= UdmVarListFindInt(&Indexer->Conf->Vars, "sy", 1);
  int sp= UdmVarListFindInt(&Indexer->Conf->Vars, "sp", 1);

  if (!sp)
    return NULL;

  for (Al= ALL->Item; Al < &ALL->Item[ALL->nitems]; Al++)
  {
    UDM_SPELLLIST *Sl;
    for (Sl= SLL->Item; Sl < &SLL->Item[SLL->nitems]; Sl++)
    {
      if (!strcasecmp(Al->lang, Sl->lang) && !strcasecmp(Al->cset, Sl->cset))
      {
        UDM_SPELL Norm[UDM_MAX_NORMS];
        UDM_SPELL *NormEnd= Norm + UDM_MAX_NORMS;
        UDM_SPELL *NormCur= Norm;
        UDM_SPELL *N;
        char tmp[256];
        char *word= uword->word;
        UDM_CONV scs_lcs, lcs_scs;

        UdmConvInit(&scs_lcs, Sl->cs, lcs, UDM_RECODE_HTML);
        UdmConvInit(&lcs_scs, lcs, Sl->cs, UDM_RECODE_HTML);
        
        if (lcs != Sl->cs)
        {
          size_t len= strlen(word);
          UdmConv(&lcs_scs, tmp, sizeof(tmp), word, len + 1);
          word= tmp;
        }
        
        NormCur+= UdmSpellNormalize(Sl, Al, word, NormCur, NormEnd-NormCur);
        
        if (sy && SYN->nitems)
        {
          UDM_WIDEWORD ww;
          UDM_WIDEWORDLIST *syn;
          UdmWideWordInit(&ww);
          ww.order= uword->order;
          ww.word= tmp;
          /* 
            Find synonyms for each normal form
            and add the found synonyms into normalized
            list for futher denormalization.
          */
          for (N= Norm; N < NormCur; N++)
          {
            ww.len= UdmConv(&scs_lcs,tmp,sizeof(tmp),N->word,strlen(N->word)+1) - 1;
            ww.origin= uword->origin;
            if ((syn= UdmSynonymListListFind(SYN, &ww)))
            {
              UDM_WIDEWORD *W;
              for (W= syn->Word; W < syn->Word + syn->nwords; W++)
              {
                size_t nbytes= W->len + 1;
                UdmConv(&lcs_scs, tmp, sizeof(tmp), (char*) W->word, nbytes);
                if (NormCur < NormEnd)
                {
                  NormCur+= UdmSpellNormalize(Sl, Al, tmp, NormCur, NormEnd-NormCur);
                }
              }
              UdmWideWordListFree(syn);
              UdmFree(syn);
            }
          }
        }
        
        for (N= Norm ; N < NormCur; N++)
        {
          if (ResCur < ResEnd)
          {
            size_t cres= 1;
            *ResCur= UdmStrdup(N->word);
            cres+= UdmSpellDenormalize(Sl, Al, N, ResCur+1, ResEnd-ResCur-1);
            if (lcs != Sl->cs)
            {
              size_t i;
              for (i=0; i < cres; i++)
              {
                UdmConv(&scs_lcs, tmp, sizeof(tmp),
                        ResCur[i], strlen(ResCur[i])+1);
                UdmFree(ResCur[i]);
                ResCur[i]= UdmStrdup(tmp);
              }
            }
            ResCur+= cres;
          }
        }
      }
    }
  }
  
  UdmWideWordInit(&w);
  for (R=Res; R < ResCur; R++)
  {
    w.order= uword->order;
    w.phrpos= uword->phrpos;
    w.phrlen= uword->phrlen;
    w.count= 0;
    w.origin= UDM_WORD_ORIGIN_SPELL;
    w.word= *R;
    w.len= strlen(w.word);
    UdmWideWordListAdd(result, &w);
    UdmFree(*R);
  }
  return result;
}


static const char *translit_cyr_lat[]=
{
  "a",  "b",  "v",  "g",  "d",  "e",  "zh", "z",
  "i",  "j",  "k",  "l",  "m",  "n",  "o",  "p",
  "r",  "s",  "t",  "u",  "f",  "h",  "c",  "ch",
  "sh", "sch","`",  "y",  "'",  "`e", "yu", "ya",
  "",   "yo"
};


static const char *translit_lat_cyr[]=
{
  "&#x430;", "&#x431;", "&#x446;", "&#x434;",
  "&#x435;", "&#x444;", "&#x433;", "&#x445;",
  "&#x438;", "&#x439;", "&#x43a;", "&#x43b;",
  "&#x43c;", "&#x43d;", "&#x43e;", "&#x43f;",
  "&#x433;", "&#x440;", "&#x441;", "&#x442;",
  "&#x443;", "&#x432;", "&#x432;", "&#x43a;&#x441;",
  "&#x44b;", "&#x437;"
};


typedef struct udm_translit_complex_subst_st
{
  const char *from;
  const char *to;
} UDM_TRANSLIT_COMPLEX_SUBST;


static UDM_TRANSLIT_COMPLEX_SUBST translit_lat_cyr_complex[]=
{
#if NOT_YET
  {"`"  , "&#x44a;"},
  {"'"  , "&#x44c;"},
  {"`e" , "&#x44d;"},
#endif
  {"ch" , "&#x447;"},
  {"sch", "&#x449;"},
  {"ya" , "&#x44f;"},
  {"zh" , "&#x436;"},
  {"yo" , "&#x451;"},
  {"kh" , "&#x445;"},
  {"sh" , "&#x448;"},
#ifdef NOT_YET
  {"yu" , "&#x44e;"}, /* ambiguous: YERU + U, or YU   */
#endif
  {NULL, NULL}
};


typedef struct udm_translit_st
{
  size_t from;
  size_t to;
  const char * const *translit;
  UDM_TRANSLIT_COMPLEX_SUBST *complex;
} UDM_TRANSLIT_TABLE;


static UDM_TRANSLIT_TABLE tr_cyr_lat=
{
  0x430, 0x451, translit_cyr_lat, NULL
};


static UDM_TRANSLIT_TABLE tr_lat_cyr=
{
  0x61, 0x7A, translit_lat_cyr, translit_lat_cyr_complex
};


static int UdmAllFormsTranslit(UDM_AGENT *A, UDM_WIDEWORDLIST *result,
                               const UDM_WIDEWORD *uword,
                               const UDM_TRANSLIT_TABLE *tr)
{
  int *wrd, tword[128], tmp[128], *t, *te= tword + 128 - 2;
  int subst= 0;
  UDM_CHARSET *latin1= UdmGetCharSet("iso-8859-1");
  UDM_CONV l1_uni, lcs_uni;
  UdmConvInit(&l1_uni, latin1, &udm_charset_sys_int, UDM_RECODE_HTML);
  UdmConvInit(&lcs_uni, A->Conf->lcs, &udm_charset_sys_int, UDM_RECODE_HTML);
  UdmConv(&lcs_uni, (char*) tmp, sizeof(tmp),
                    uword->word, strlen(uword->word) + 1);

  for (wrd= tmp, t= tword; wrd[0] && t < te; )
  {
    if (*wrd >= tr->from && *wrd <= tr->to)
    {
      const char *repl= NULL;
      size_t len;
      UDM_TRANSLIT_COMPLEX_SUBST *cmpl;
      for (cmpl= tr->complex; cmpl && cmpl->from; cmpl++)
      {
        size_t pos;
        const char *from= cmpl->from;
        for (pos=0; from[pos] && from[pos] == wrd[pos]; pos++);
        if (!from[pos])
        {
          repl= cmpl->to;
          wrd+= pos;
          break;
        }
      }
      if (!repl)
      {
        repl= tr->translit[*wrd - tr->from];
        wrd++;
      }
      
      len= strlen(repl);
      len= UdmConv(&l1_uni, (char*) t, (te - t) * sizeof(*t), repl, len);
      t+= len / sizeof(*t);
      *t= 0;
      subst++;
    }
    else
    {
      *t++= *wrd++;
    }
  }
  *t= 0;
  if (subst)
  {
    UDM_WIDEWORD w;
    char lcsword[128];
    UDM_CONV uni_lcs;
    size_t nbytes= (t - tword + 1) * sizeof(*t);
    UdmWideWordInit(&w);
    UdmConvInit(&uni_lcs, &udm_charset_sys_int, A->Conf->lcs, UDM_RECODE_HTML);
    UdmConv(&uni_lcs, lcsword, sizeof(lcsword), (const char *) tword, nbytes);
    w.order= uword->order;
    w.phrpos= uword->phrpos;
    w.phrlen= uword->phrlen;
    w.count= 0;
    w.origin= UDM_WORD_ORIGIN_SYNONYM;
    w.word= lcsword;
    w.len= strlen(w.word);
    UdmWideWordListAdd(result, &w);
  }
  return UDM_OK;
}


static int
UdmAllForms2(UDM_AGENT *Indexer,
             UDM_WIDEWORDLIST *result,
             UDM_WIDEWORD *uw)
{
  UdmAllForms1(Indexer, result, uw);
#ifdef HAVE_SQL
  {
    const char *sql= UdmVarListFindStr(&Indexer->Conf->Vars, "SQLWordForms", NULL);
    if (sql && Indexer->Conf->dbl.nitems)
    {
      char *word= uw->word;
      char buf[1024];
      size_t i, nrows;
      UDM_SQLRES SQLRes;
      UDM_WIDEWORD form;
      UDM_DB *db= &Indexer->Conf->dbl.db[0];
      
      if (db->DBType == UDM_DB_SEARCHD)
      {
        sprintf(db->errstr, "SQLWordForms is not supported by this DBAddr type");
        db->errcode= 1;
        return UDM_ERROR;
      }
      UdmBuildParamStr(buf, sizeof(buf), sql, &word, 1);
      if (UDM_OK != UdmSQLQuery(db, &SQLRes, buf))
        return UDM_ERROR;
      nrows= UdmSQLNumRows(&SQLRes);
      UdmWideWordInit(&form);
      form.order= uw->order;
      form.phrpos= uw->phrpos;
      form.phrlen= uw->phrlen;
      form.count= 0;
      form.origin= UDM_WORD_ORIGIN_SYNONYM;
      form.weight= 0;
      form.match= uw->match;
      for (i= 0; i < nrows; i++)
      {
        form.len= UdmSQLLen(&SQLRes, 0, i);
        form.word= (char*) UdmSQLValue(&SQLRes, 0, i);
        UdmWideWordListAdd(result, &form);
      }
      UdmSQLFree(&SQLRes);
    }
  }
#endif
  return UDM_OK;
}


/*
  Similar to UdmWideWordAddLike()
  but changes count, origin, and weight.
*/
static int
UdmWideWordListAddForDehyphenate(UDM_WIDEWORDLIST *result,
                                 const UDM_WIDEWORD *uword,
                                 char *tmpword)
{
  UDM_WIDEWORD form;
  UdmWideWordInit(&form);
  form.order= uword->order;
  form.phrpos= uword->phrpos;
  form.phrlen= uword->phrlen;
  form.count= 0;
  form.origin= UDM_WORD_ORIGIN_SYNONYM;
  form.weight= 0;
  form.match= uword->match;
  form.word= tmpword;
  form.len= strlen(tmpword);
  return UdmWideWordListAdd(result, &form);
}


static int
UdmAllFormsDehyphenate(UDM_AGENT *A, UDM_WIDEWORDLIST *result, UDM_WIDEWORD *uword)
{
  char tmpword[128], *s, *d;
  strcpy(tmpword, uword->word);
  for (s= uword->word, d= tmpword; ; s++)
  {
    *d++= *s;
    if (UdmAutoPhraseChar((unsigned char) *s))
      d--;
    
    if (!*d)
      break;
  }
  return UdmWideWordListAddForDehyphenate(result, uword, tmpword);
}


/*
  Adding hyphenated alnumeric forms: utf8 -> utf-8
*/
static int
UdmAllFormsHyphenateNumbers(UDM_AGENT *A, UDM_WIDEWORDLIST *result, UDM_WIDEWORD *uword)
{
  char tmpword[128], *s, *d;
  int prev_ctype= UDM_UNI_SEPAR, have_hyphen= 0;
  if (strlen(uword->word) + 1 >= sizeof(tmpword))
    return UDM_OK;
  
  for (s= uword->word, d= tmpword; *s ; *d++= *s++)
  {
    int next_ctype= (*s >= '0' && *s <= '9') ? UDM_UNI_DIGIT :
                    UdmAutoPhraseChar(*s)    ? UDM_UNI_SEPAR :
                    UDM_UNI_LETTER;
    if ((prev_ctype == UDM_UNI_LETTER && next_ctype == UDM_UNI_DIGIT) ||
        (prev_ctype == UDM_UNI_DIGIT  && next_ctype == UDM_UNI_LETTER))
    {
      have_hyphen= 1;
      *d++= '-';
    }
    prev_ctype= next_ctype;
  }

  if (!have_hyphen)
    return UDM_OK;
  *d++= '\0';

  return UdmWideWordListAddForDehyphenate(result, uword, tmpword);
}


int
UdmAllForms(UDM_AGENT *Indexer, UDM_WIDEWORDLIST *result, UDM_WIDEWORD *uword)
{
  int rc;
  UDM_WIDEWORDLIST *uwordsyn;
  
  /*
    Generate all possible word forms for uword.
  */
  if (UDM_OK != (rc= UdmAllForms2(Indexer, result, uword)))
    return rc;
  
  if (UdmVarListFindBool(&Indexer->Conf->Vars, "tl", 0))
  {
    UDM_TRANSLIT_TABLE *tbl[]= {&tr_cyr_lat, &tr_lat_cyr, NULL}, **cur;
    for (cur= tbl; *cur; cur++)
    {
      UDM_WIDEWORDLIST translit;
      UdmWideWordListInit(&translit);
      UdmAllFormsTranslit(Indexer, &translit, uword, *cur);
      if (translit.nwords)
      {
        UDM_WIDEWORD *ww= &translit.Word[0];
        UdmWideWordListAdd(result, ww);
        UdmAllForms2(Indexer, result, ww);
      }
      UdmWideWordListFree(&translit);
    }
  }

  if (UdmVarListFindBool(&Indexer->Conf->Vars, "Dehyphenate", 0))
    UdmAllFormsDehyphenate(Indexer, result, uword);
  
  
  if (UdmVarListFindBool(&Indexer->Conf->Vars, "HyphenateNumbers", 0))
    UdmAllFormsHyphenateNumbers(Indexer, result, uword);
  
  
  if (!UdmVarListFindInt(&Indexer->Conf->Vars, "sy", 1))
    return rc;
  /*
     Combination one: uword is possibly a normalized form.
     Find all uword synonyms and then process then through
     ispell to generate all word forms for the synonyms.
  */
  if ((uwordsyn= UdmSynonymListListFind(&Indexer->Conf->Synonym, uword)))
  {
    UDM_WIDEWORD *ww;
    for (ww= uwordsyn->Word; ww < &uwordsyn->Word[uwordsyn->nwords]; ww++)
    {
      UdmWideWordListAdd(result, ww);
      UdmAllForms2(Indexer, result, ww);
    }
    UdmWideWordListFree(uwordsyn);
    UdmFree(uwordsyn);
  }

  return rc;
}


/*
  Appends synonym parts into string, recursively
  
  SL                    - List to find synonyms in
  strbeg                - The very beginning of the complex synonym
  str                   - The beginnig of the next part, we'll write here
  str_reminder_size     - Space available
  WWL                   - We'll add found synonyms here
  nwords                - number of words to check (to avoid endless loop)
  order                 - Word "order" to start building phrase at
  phrase_length_limit   - maximum possible complex synonym length
  phrase_length_current - current length of synonym which was
                          collected on the previous recursion step.
*/
static int
UdmComplexSynonymAdd(UDM_AGENT *A, UDM_SYNONYMLIST *SL,
                     char *strbeg, char *str, size_t str_reminder_size, 
                     UDM_WIDEWORDLIST *WWL, size_t nwords,
                     size_t order,
                     size_t phrase_length_limit,
                     size_t phrase_length_current)
{
  size_t i;
  for (i= 0; i < nwords; i++)
  {
    UDM_WIDEWORD *W= &WWL->Word[i];
    if (W->order == order)
    {
      int need_more= (phrase_length_limit > 0);
      size_t len= udm_snprintf(str, str_reminder_size, "%s%s",
                               phrase_length_current > 0 ? " " : "",
                               W->word);
      if (need_more)
      {
        UdmComplexSynonymAdd(A, SL,
                             strbeg, str + len, str_reminder_size - len,
                             WWL, nwords, order + 1,
                             phrase_length_limit - 1,
                             phrase_length_current + 1);
        str[len]= '\0'; /* Remove trailing space and trailing parts */
      }
      
      if (phrase_length_current > 0) /* Skip single word (on first level) */
      {
        UDM_WIDEWORDLIST Tmp;
        UDM_WIDEWORD WW;
        UdmWideWordListInit(&Tmp);
        WW= W[0];
        WW.word= strbeg;
        WW.len= strlen(strbeg);
        UdmSynonymListFind(&Tmp, SL, &WW);
        /*
        UdmLog(A, UDM_LOG_DEBUG, "ComplexSynonym: phrlen=%d: '%s' nfound=%d",
               phrase_length_current, strbeg, Tmp.nwords);
        */
        {
          size_t wrd;
          for (wrd= 0; wrd < Tmp.nwords; wrd++)
          {
            if ((WW.order_extra_width= UdmMultiWordPhraseLength(WW.word)))
            {
              UDM_ASSERT(W->order >= phrase_length_current);
              WW.order= W->order - phrase_length_current;
              WW.order_extra_width++;
            }
            else
              WW.order= order; /* Should not really happen */
            /*
            UdmLog(A, UDM_LOG_DEBUG, "FOUND: '%s' width=%d",
                   Tmp.Word[wrd].word, WW.order_width);
            */
            UdmWideWordListAddLike(WWL, &WW, Tmp.Word[wrd].word);
          }
        }
        UdmWideWordListFree(&Tmp);
      }
    }
  }
  return UDM_OK;
}



/*
  Add many-to-one and many-to-many synonyms
*/
int
UdmComplexSynonyms(UDM_AGENT *A, UDM_WIDEWORDLIST *WWL)
{
  size_t nwords= WWL->nwords; /* Remember nwords, to avoid endless loop */
  size_t i;
  UDM_SYNONYMLISTLIST *SSL= &A->Conf->Synonym;
  for (i= 0; i <  SSL->nitems; i++)
  {
    UDM_SYNONYMLIST *SL= &SSL->Item[i];
    char str[256]= "";
    if (SL->max_phrase_length > 0)
    {
      size_t order;
      for (order= 0; order < WWL->nuniq; order++)
        UdmComplexSynonymAdd(A, SL, str, str, sizeof(str),
                             WWL, nwords, order, SL->max_phrase_length, 0);
    }
  }
  return UDM_OK;
}
