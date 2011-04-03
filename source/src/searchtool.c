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
#include <errno.h>
#include <math.h>

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_unicode.h"
#include "udm_unidata.h"
#include "udm_uniconv.h"
#include "udm_searchtool.h"
#include "udm_boolean.h"
#include "udm_stopwords.h"
#include "udm_word.h"
#include "udm_vars.h"
#include "udm_db.h"
#include "udm_db_int.h"
#include "udm_hash.h"
#include "udm_parsehtml.h"
#include "udm_store.h"
#include "udm_doc.h"
#include "udm_conf.h"
#include "udm_result.h"
#include "udm_log.h"
#include "udm_sgml.h"
#include "udm_mutex.h"
#include "udm_chinese.h"
#include "udm_synonym.h"


typedef struct udm_stack_parser_state_st
{
  int secno;
  int secno_match_type;
  int next_word_match_type;
  int use_numeric_operators;
  int word_match;
  int nphrasecmd;
  int auto_phrase;
  int dehyphenate;
  int strip_accents;
  int phrpos;
  int user_weight;
  size_t SubstringMatchMinWordLength;
  const char *lang;
} UDM_STACK_PARSER_STATE;



static int
UdmStackItemListAddCmd(UDM_RESULT *Res,
                       UDM_STACK_PARSER_STATE *state,
                       int *lex, size_t length)
{
  int rc= UDM_OK;
  size_t i;
  UDM_STACK_ITEM item;
  
  for (i = 0; i < length; i++)
  {
     switch(lex[i])
     {
       case '&':
       case '+':
         item.cmd= UDM_STACK_AND;
         state->next_word_match_type= state->word_match;
         break;
       case '|':
         item.cmd= UDM_STACK_OR;
         state->next_word_match_type= state->word_match;
         break;
       case '~':
         item.cmd= UDM_STACK_NOT;
         state->next_word_match_type= state->word_match;
         break;
       case '(':
         item.cmd= UDM_STACK_LEFT;
         state->next_word_match_type= state->word_match;
         break;
       case ')':
         item.cmd= UDM_STACK_RIGHT;
         state->next_word_match_type= state->word_match;
         break;
       case '"':
         item.cmd= UDM_STACK_PHRASE;
         state->next_word_match_type= state->word_match;
         state->nphrasecmd++;
         break;
       case '<':
         state->next_word_match_type= state->use_numeric_operators ?
                                      UDM_MATCH_NUMERIC_LT : state->word_match;
         break;
       case '>':
         state->next_word_match_type= state->use_numeric_operators ? 
                                      UDM_MATCH_NUMERIC_GT : state->word_match;
         break;
       default:
         if (state->auto_phrase && !UdmAutoPhraseChar(lex[i]))
         {
           int quot= '"';
           item.cmd= UDM_STACK_PHRASE;
           item.arg= state->secno_match_type;
           UdmStackItemListAddCmd(Res, state, &quot, 1);
           state->auto_phrase= 0;
         }
         continue;
     }
     /* Ignore all operators if we are in a phrase,  except phrase end. */
    if (!(state->nphrasecmd % 2) || item.cmd == UDM_STACK_PHRASE)
    {
      item.arg= state->secno_match_type;
      rc= UdmStackItemListAdd(&Res->ItemList, &item);
      Res->ItemList.ncmds++;
    }
  }
  return rc;
}


static int
UdmWordMatchApplyLength(UDM_STACK_PARSER_STATE *S, size_t wlen)
{
  return (wlen < S->SubstringMatchMinWordLength &&
          (S->next_word_match_type == UDM_MATCH_BEGIN ||
           S->next_word_match_type == UDM_MATCH_SUBSTR ||
           S->next_word_match_type == UDM_MATCH_END)) ?
  UDM_MATCH_FULL : S->next_word_match_type;
}


static int
UdmStackItemListAddWord(UDM_AGENT *query, UDM_RESULT *Res,
                        UDM_STACK_PARSER_STATE *state,
                        size_t length,
                        /* Pointers to the next token and the query end*/
                        int *lt, int *ltend, 
                        char *wrd)
{
  UDM_WIDEWORD OWord;
  UDM_WIDEWORDLIST Forms;
  UDM_STACK_ITEM item;
  int origin, rc, lt_auto_phrase= UdmAutoPhraseChar(lt[0]);
  size_t phrlen= 0;
  int end_of_phrase= 0;
  int final_word_match= UdmWordMatchApplyLength(state, length);

  /*
    Ignore AutoPhraseChar in case when we have a sequence:
    - a word
    - followed by a AutoPhraseChar
    - followed by a separator
    For example: B. Abama
    Notice space character after dot.
  */
  if (&lt[1] < ltend &&
      UdmUniCType(udm_unidata_default, lt[1]) == UDM_UNI_SEPAR)
    lt_auto_phrase= 0;

  if(Res->WWList.nuniq >= UDM_MAXWORDPERQUERY-1)
    return UDM_OK;

  if (state->nphrasecmd % 2) /* in phrase */
  {
    if (((state->auto_phrase && !lt_auto_phrase) || lt[0] == '"'))
    {
      /* End of auto- or non-auto-phrase*/
      phrlen= state->phrpos + 1;
      end_of_phrase= 1;
    }
  }
  else /* not in phrase */
  {
    if (lt_auto_phrase)
    {
      /* Start of auto-phrase */
      int quot= '"';
      state->auto_phrase= 1;
      item.cmd= UDM_STACK_PHRASE;
      item.arg= 0;
      UdmStackItemListAddCmd(Res, state, &quot, 1);
      phrlen= 0;
    }
    else
      phrlen= 1; /* Single word */
  }
  
  item.cmd= UDM_STACK_WORD;
  item.arg= Res->WWList.nuniq;
  rc= UdmStackItemListAdd(&Res->ItemList, &item);


  /*
    Check stopword only when full word.
    Substring searches should not exclude them.
  */
  if(final_word_match == UDM_MATCH_FULL &&
     (UdmStopListListFind(&query->Conf->StopWord, wrd, state->lang) ||
      query->Conf->WordParam.min_word_len > length ||
      query->Conf->WordParam.max_word_len < length))
  {
    origin= UDM_WORD_ORIGIN_STOP;
    Res->ItemList.items[Res->ItemList.nitems - 1].cmd= UDM_STACK_STOP;
  }
  else
  {
    origin= UDM_WORD_ORIGIN_QUERY;
  }

  if (phrlen > 1)
  {
    /* Phrase end found: set phrlen for all words in the same phrase */
    UDM_WIDEWORD *W= &Res->WWList.Word[Res->WWList.nwords-1];
    for (; W >= Res->WWList.Word && W->phrlen == 0; W--)
      W->phrlen= phrlen;
  }

  /* Add the original word form */
  bzero((void*) &OWord, sizeof(OWord));
  OWord.len= strlen(wrd);
  OWord.order= Res->WWList.nuniq;
  OWord.phrpos= state->phrpos;
  OWord.phrlen= phrlen;
  OWord.count= 0;
  OWord.word= wrd;
  OWord.origin = origin;
  OWord.match= final_word_match;
  OWord.secno= state->secno;
  OWord.phrwidth= state->dehyphenate ? 1 : 0;
  OWord.user_weight= state->user_weight;
  
  UdmWideWordListAdd(&Res->WWList, &OWord);
  if (state->nphrasecmd % 2)
    state->phrpos++;
  state->user_weight= UDM_DEFAULT_USER_WORD_WEIGHT;
  
  if (origin == UDM_WORD_ORIGIN_STOP)
    return UDM_OK;
  
  /* Add generated word forms: synonyms, stemming, etc. */
  UdmWideWordListInit(&Forms);
  if (UDM_OK != (rc= UdmAllForms(query,&Forms,&OWord)))
    return rc;
  
  {
    UDM_WIDEWORD FWord;
    size_t frm;
    bzero((void*) &FWord, sizeof(FWord));
    for (frm= 0; frm < Forms.nwords ; frm++)
    {
      FWord.order= Res->WWList.nuniq;
      FWord.phrpos= OWord.phrpos; /* state->phrpos is already changed here */
      FWord.phrlen= phrlen;
      FWord.count= 0;
      FWord.word= Forms.Word[frm].word;
      FWord.len= Forms.Word[frm].len;
      FWord.origin = Forms.Word[frm].origin;
      FWord.match= state->next_word_match_type;
      FWord.secno= state->secno;
      FWord.user_weight= OWord.user_weight;
      UdmWideWordListAdd(&Res->WWList,&FWord);
/*    UdmLog(query, UDM_LOG_DEBUG, "Word form: [%d] %s", FWord.origin, wrd);*/
    }
  }
  UdmWideWordListFree(&Forms);
  if (end_of_phrase)
    state->phrpos= 0;
  Res->WWList.nuniq++;
  return rc;
}


int UdmPrepare(UDM_AGENT * query,UDM_RESULT *Res)
{
  UDM_VARLIST *Vars= &query->Conf->Vars;
  UDM_CHARSET * browser_cs, * local_cs, *sys_int;
  int  ctype, rc;
  int *ustr, *uend, *lt, *lex;
  size_t ulen;
  const char * txt = UdmVarListFindStr(&query->Conf->Vars,"q","");
  const char * qprev = UdmVarListFindStr(&query->Conf->Vars,"qprev","");
  const char   *seg=  UdmVarListFindStr(&query->Conf->Vars, "Segmenter", NULL);
  char *ltxt;
  size_t wlen, llen, obytes;
  char *wrd;
  int *uwrd, segmenter;
  UDM_CONV uni_lc, bc_uni, bc_lc;
  const char *lang;
  UDM_STACK_PARSER_STATE state;
  int *(*UniGetSepToken)(UDM_UNIDATA *unidata,
                         int *str, int *strend, int **last, int *ctype0);
  UDM_UNIDATA *unidata= query->Conf->unidata;
  
  state.secno= 0;
  state.secno_match_type= 0;
  state.use_numeric_operators= UdmVarListFindBool(&query->Conf->Vars, "UseNumericOperators", 0);
  state.dehyphenate= UdmVarListFindBool(&query->Conf->Vars, "Dehyphenate", 0);
  state.strip_accents= UdmVarListFindBool(&query->Conf->Vars, "StripAccents", 0);
  state.nphrasecmd= 0;
  state.word_match= UdmMatchMode(UdmVarListFindStr(&query->Conf->Vars, "wm", "wrd"));
  state.next_word_match_type= state.word_match;
  state.lang= UdmVarListFindStr(&query->Conf->Vars, "g", NULL);
  state.auto_phrase= 0;
  state.phrpos= 0;
  state.user_weight= UDM_DEFAULT_USER_WORD_WEIGHT;
  state.SubstringMatchMinWordLength=
    (size_t) UdmVarListFindInt(&query->Conf->Vars, "SubstringMatchMinWordLength", 0);
  UniGetSepToken= state.dehyphenate ? UdmUniGetSepToken2 :UdmUniGetSepToken;
  
  if ((wrd = (char*)UdmMalloc(query->Conf->WordParam.max_word_len * 12 + 1)) == NULL) return 0;
  if ((uwrd = (int*)UdmMalloc(sizeof(int) * (query->Conf->WordParam.max_word_len + 1))) == NULL) { UDM_FREE(wrd); return 0; }


  if (!(browser_cs = query->Conf->bcs))
    browser_cs=UdmGetCharSet("iso-8859-1");
  
  if(!(local_cs = query->Conf->lcs))
    local_cs=UdmGetCharSet("iso-8859-1");
  
  sys_int= &udm_charset_sys_int;
  
  UdmConvInit(&bc_uni,browser_cs,sys_int,UDM_RECODE_HTML);
  UdmConvInit(&uni_lc,sys_int,local_cs,UDM_RECODE_HTML);
  UdmConvInit(&bc_lc,browser_cs,local_cs,UDM_RECODE_HTML);
  
  ulen= strlen(txt);
  ustr=(int*)(UdmMalloc((sizeof(int))*(ulen+1)));
  obytes= UdmConv(&bc_uni,(char*)ustr,sizeof(ustr[0])*(ulen+1),txt,ulen+1);
  obytes-= sizeof(int);
  
  /* Create copy of query, converted into LocalCharset (for UdmTrack) */
  llen = ulen * 14 + 1;
  ltxt=(char*)UdmMalloc(llen);
  obytes= UdmConv(&uni_lc, ltxt, llen, (char*) ustr, obytes);
  ltxt[obytes]='\0';
  UdmVarListReplaceStr(&query->Conf->Vars,"q",ltxt);  /* "q-lc" was here */
  UDM_FREE(ltxt);
  
  llen = strlen(qprev);
  ltxt=(char*)UdmMalloc(llen*14+1);
  obytes= UdmConv(&bc_lc,ltxt,llen*14+1,qprev,llen);
  ltxt[obytes]='\0';
  UdmVarListReplaceStr(&query->Conf->Vars,"qprev",ltxt);
  UDM_FREE(ltxt);
  
  /* Parse query and build boolean search stack*/
  if (state.strip_accents)
    UdmUniStrStripAccents(unidata, ustr);
  UdmUniStrToLower(unidata, ustr);
  switch(browser_cs->family)
  {
    case UDM_CHARSET_CHINESE_SIMPLIFIED:
    case UDM_CHARSET_CHINESE_TRADITIONAL: lang = "zh"; break;
    case UDM_CHARSET_JAPANESE: lang = "ja"; break;
    case UDM_CHARSET_THAI: lang = "th"; break;
    default: lang = "";
  }

  if (seg && !strcasecmp(seg, "cjk") &&
      UDM_MODE_PHRASE == UdmSearchMode(UdmVarListFindStr(&query->Conf->Vars, "m", NULL)))
    seg= "cjk-phrase";
  
  if ((segmenter= UdmUniSegmenterFind(query, lang, seg)))
  {
    ustr= UdmUniSegmentByType(query, ustr, segmenter, (int) ' ');
    ulen= UdmUniLen(ustr); /* TODO: get rid of it */
    llen= ulen * 14 + 1;
    ltxt= (char*) UdmMalloc(llen);
    obytes= UdmConv(&uni_lc, ltxt, llen, (const char*) ustr, ulen * sizeof(int));
    ltxt[obytes]= '\0';
    UdmVarListReplaceStr(&query->Conf->Vars, "SEGMENTER.QUERY_STRING", ltxt);
    UDM_FREE(ltxt);
  }
  uend= ustr + UdmUniLen(ustr); /* TODO: get rid of it */

  lex= UniGetSepToken(unidata, ustr, uend, &lt , &ctype);
  for ( ;lex; lex= UniGetSepToken(unidata, NULL, uend, &lt, &ctype))
  {
    wlen=lt-lex;
    memcpy(uwrd, lex, (udm_min(wlen, query->Conf->WordParam.max_word_len)) * sizeof(int));
    uwrd[udm_min(wlen, query->Conf->WordParam.max_word_len)] = 0;
    UdmConv(&uni_lc, wrd, query->Conf->WordParam.max_word_len * 12,(char*)uwrd, sizeof(uwrd[0])*(wlen+1));
    UdmTrim(wrd, " \t\r\n");
    
    if (ctype == UDM_UNI_SEPAR)
    {
      UdmStackItemListAddCmd(Res, &state, lex, wlen);
    } 
    else
    {
      UDM_VAR *Section;

      if (lt[0] == ':' || lt[0] == '=')
      {
        if ((Section= UdmVarListFind(&query->Conf->Sections, wrd)))
        {
          state.secno= Section->section;
          state.secno_match_type= lt[0];
          continue;
        }
        if (wlen > 5 && !strncmp(wrd, "secno", 5))
        {
          state.secno= atoi(wrd + 5);
          state.secno_match_type= lt[0];
          continue;
        }
        else if (wlen > 10 && !strncmp(wrd, "importance", 10) &&
                 wrd[10] >= '0' && wrd[10] <= '9')
        {
          state.user_weight= atoi(wrd + 10);
          continue;
        }
      }
      
      if (UDM_OK != (rc= UdmStackItemListAddWord(query, Res, &state,
                                                 wlen, lt, uend, wrd)))
        return rc;
    }
  }
  
  if (state.nphrasecmd & 1)
  {
    UDM_STACK_ITEM item;
    item.cmd= UDM_STACK_PHRASE;
    item.arg= 0;
    UdmStackItemListAdd(&Res->ItemList, &item);
    Res->ItemList.ncmds++;
  }
  UDM_FREE(ustr);
  UDM_FREE(uwrd);
  UDM_FREE(wrd);
  
  if (UdmVarListFindBool(Vars, "ComplexSynonyms", 0))
    UdmComplexSynonyms(query, &Res->WWList);
  
  Res->WWList.wm= state.word_match;
  Res->WWList.strip_noaccents= state.strip_accents;
  return UDM_OK;
}



/******** Convert category string into 32 bit number *************/

void UdmDecodeHex8Str(const char *hex_str, uint4 *hi,
                      uint4 *lo, uint4 *fhi, uint4 *flo)
{
  char str[33],str_hi[17],str_lo[17], *s = str;

  strncpy(str, hex_str, 13);
  str[12] = '\0';
  strcat(str,"000000000000");
  while(*s == '0') *s++ = ' ';
  strncpy(str_hi,&str[0],6); str_hi[6]=0;
  strncpy(str_lo,&str[6],6); str_lo[6]=0;
  
  *hi = (uint4)strtoul(str_hi, (char **)NULL, 36);
  *lo = (uint4)strtoul(str_lo, (char **)NULL, 36);

  if ((fhi != NULL) && (flo != NULL))
  {
    strncpy(str, hex_str, 13);
    str[12] = '\0';
    strcat(str,"ZZZZZZZZZZZZ");
    strncpy(str_hi, &str[0], 6); str_hi[6] = 0;
    strncpy(str_lo, &str[6], 6); str_lo[6] = 0;
  
    *fhi = strtoul(str_hi, (char **)NULL, 36);
    *flo = strtoul(str_lo, (char **)NULL, 36);
  }
}


int UdmParseQueryString(UDM_AGENT * Agent,
                        UDM_VARLIST * vars,
                        const char * query_string)
{
  char * tok, *lt;
  size_t len;
  char *str = (char *)UdmMalloc((len = strlen(query_string)) + 7);
  char *qs = (char*)UdmStrdup(query_string);
  char qname[1024];

  if ((str == NULL) || qs == NULL)
  {
    UDM_FREE(str);
    UDM_FREE(qs);
    return 1;
  }

  UdmSGMLUnescape(qs);
  
  tok= udm_strtok_r(qs, "&", &lt);
  while(tok)
  {
    char empty[]="";
    char * val;
    
    if((val=strchr(tok,'=')))
    {
      *val='\0';
      val++;
    }
    else
    {
      val=empty;
    }
    UdmUnescapeCGIQuery(str,val);
    UdmVarListAddQueryStr(vars,tok,str);
    udm_snprintf(qname, 256, "query.%s", tok);
    UdmVarListAddQueryStr(vars, qname, str);
    
    tok= udm_strtok_r(NULL, "&", &lt);
  }
  UDM_FREE(str);
  UDM_FREE(qs);
  return 0;
}


/*
  Applying fast limit to UDLCRDLIST.
  Used in:
  - UdmFindAlwaysFoundWord()
  - UdmFindMultiWord()
  - UdmFindWordSingle()
  - UdmFindCrossWord()
  - UdmMultiAddCoords()
*/
int
UdmApplyFastLimit(UDM_URLCRDLIST *Coord, UDM_URLID_LIST *urls)
{
  UDM_URL_CRD *dst= Coord->Coords;
  UDM_URL_CRD *src= Coord->Coords;
  UDM_URL_CRD *srcend= Coord->Coords + Coord->ncoords;
  
  if (urls->exclude)
  {
    for ( ; src < srcend; src++)
    {
      if (!UdmBSearch(&src->url_id,
                      urls->urls, urls->nurls, sizeof(urlid_t),
                      (udm_qsort_cmp)UdmCmpURLID))
      {
        *dst= *src;
        dst++;
      }
    }
  }
  else
  {
    for ( ; src < srcend; src++)
    {
      if (UdmBSearch(&src->url_id,
                     urls->urls, urls->nurls, sizeof(urlid_t),
                     (udm_qsort_cmp)UdmCmpURLID))
      {
        *dst= *src;
        dst++;
      }
    }
  }
  Coord->ncoords= dst- Coord->Coords;
  return UDM_OK;
}
