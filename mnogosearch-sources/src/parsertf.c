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
#include "udm_common.h"
#include "udm_utils.h"
#include "udm_parsehtml.h"
#include "udm_textlist.h"
#include "udm_http.h"
#include "udm_vars.h"
#include "udm_searchtool.h"

#include <string.h>


/* Core RTF functionality */
#define CT_FLAG        0
#define CT_DESTINATION 1
#define CT_SYMBOL      2
#define CT_TOGGLE      3
#define CT_VALUE       4

struct st_rtf_parser_state;

typedef struct st_rtf_control
{
  const char *name;
  size_t length;
  unsigned char bits;
  int (*handle_func)(struct st_rtf_parser_state *state,
                     struct st_rtf_control *control, int control_parameter);
  int default_value;
} RTF_CONTROL;


typedef struct st_rtf_parser_state
{
  RTF_CONTROL *valid_controls;
  void *custom_state;
  const char **ptr;
  size_t depth;
  size_t ignorable_depth;
} RTF_PARSER_STATE;


static int rtf_parser_parse(const char *src, size_t src_length,
                            void *custom_state, RTF_CONTROL *valid_controls,
                            int (*start_group_func)(RTF_PARSER_STATE *),
                            int (*end_group_func)(RTF_PARSER_STATE *))
{
  RTF_PARSER_STATE state;
  RTF_CONTROL *c;
  const char *control_word, *p, *e= src + src_length;
  size_t control_length;
  int control_parameter, rc;
  char has_parameter, is_ignorable= 0;

  state.valid_controls= valid_controls;
  state.custom_state= custom_state;
  state.ptr= &p;
  state.depth= 0;
  state.ignorable_depth= (size_t) -1;

  for (p= src; p < e; p++)
  {
    switch (*p)
    {
    case '{':
      state.depth++;
      if (start_group_func && (rc= start_group_func(&state)))
        return rc;
      continue;
    case '}':
      if (state.depth < 1)
        return 1;
      if (end_group_func && (rc= end_group_func(&state)))
        return rc;
      if (state.ignorable_depth == state.depth)
        state.ignorable_depth= (size_t) -1;
      state.depth--;
      continue;
    case '\r':
    case '\n':
      continue;
    case '\\':
      if (++p == e)
        return 1;
      if (*p == '\'')
      {
        if (++p == e)
          return 1;
        if (*p >= 'a' && *p <= 'f')
          control_parameter= (char) (*p - 'a' + 10) << 4;
        else if (*p >= 'A' && *p <= 'F')
          control_parameter= (char) (*p - 'A' + 10) << 4;
        else if (*p >= '0' && *p <= '9')
          control_parameter= (char) (*p - '0') << 4;
        else
          return 1;

        if (++p == e)
          return 1;
        if (*p >= 'a' && *p <= 'f')
          control_parameter+= *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'F')
          control_parameter+= *p - 'A' + 10;
        else if (*p >= '0' && *p <= '9')
          control_parameter+= *p - '0';
        else
          return 1;

        control_length= 0;
        has_parameter= 1;
        goto handle_control;
      }
      else if (*p == '{' || *p == '}' || *p == '\\')
      {
        control_length= 0;
        has_parameter= 1;
        control_parameter= *p;
        goto handle_control;
      }
      else if (*p == '*')
      {
        if (e - p < 3 || p[1] != '\\' ||
            ((p[2] < 'a' || p[2] > 'z') && (p[2] < 'A' || p[2] > 'Z')))
          continue;
        p+= 2;
        is_ignorable= 1;
      }
      control_word= p;
      has_parameter= 0;
      if ((*p < 'a' || *p > 'z') && (*p < 'A' || *p > 'Z'))
      {
        control_length= 1;
        goto handle_control;
      }
      do
      {
        if (++p == e)
        {
          control_length= p - control_word;
          goto handle_control;
        }
      } while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z'));
      control_length= p - control_word;

      if (*p == '-')
      {
        control_parameter= 0;
        has_parameter= 1;
        if (++p == e)
          goto handle_control;
        while (*p >= '0' && *p <= '9')
        {
          control_parameter= control_parameter * 10 - (*p - '0');
          if (++p == e)
            goto handle_control;
        }
      }
      else if (*p >= '0' && *p <= '9')
      {
        control_parameter= 0;
        has_parameter= 1;
        do
        {
          control_parameter= control_parameter * 10 + (*p - '0');
          if (++p == e)
            goto handle_control;
        } while (*p >= '0' && *p <= '9');
      }

      if (*p != ' ')
        p--;
      goto handle_control;
    default:
      control_length= 0;
      has_parameter= 1;
      control_parameter= *p;
    }
  handle_control:
    for (c= state.valid_controls; c->name; c++)
    {
      if (c->length == control_length &&
          memcmp(c->name, control_word, control_length) == 0)
      {
        if (!has_parameter)
        {
          if (c->bits == CT_VALUE)
            return 1;
          else if (c->bits == CT_TOGGLE)
            control_parameter= 1;
        }
        if ((rc= c->handle_func(&state, c, control_parameter)))
          return rc;
        break;
      }
    }
    if (is_ignorable && !c->name)
    {
      if (state.ignorable_depth == (size_t) -1)
        state.ignorable_depth= state.depth;
    }
    is_ignorable= 0;
  }
  return 0;
}
/* End of core RTF functionality */




/* rtf2text functionality */
typedef struct st_rtf2text_group_state
{
  int uc;
} RTF2TEXT_GROUP_STATE;


typedef struct st_rtf2text_state
{
  RTF2TEXT_GROUP_STATE g;
  RTF2TEXT_GROUP_STATE *parents;
  UDM_DSTR *dstr;
  int ansicpg;
  int skip_characters;
  size_t allocated_parents;
} RTF2TEXT_STATE;


static int h_chr(RTF_PARSER_STATE *state, RTF_CONTROL *control,
                 int control_parameter)
{
  RTF2TEXT_STATE *custom_state= (RTF2TEXT_STATE *) state->custom_state;
  char c;
  if (state->ignorable_depth <= state->depth)
    return 0;
  if (custom_state->skip_characters)
  {
    custom_state->skip_characters--;
    return 0;
  }
  c= control->default_value ? control->default_value : control_parameter;
  if (!UdmDSTRAppend(custom_state->dstr, &c, 1))
    return 1;
  return 0;
}


static int h_uni(RTF_PARSER_STATE *state, RTF_CONTROL *control,
                 int control_parameter)
{
  RTF2TEXT_STATE *custom_state= (RTF2TEXT_STATE *) state->custom_state;
  char buf[2];

  if (state->ignorable_depth <= state->depth)
    return 0;
  if (control->bits == CT_VALUE)
    custom_state->skip_characters= custom_state->g.uc;
  else
    control_parameter= control->default_value;

  buf[0]= (char) (control_parameter >> 8);
  buf[1]= (char) control_parameter;
  if (!UdmDSTRAppend(custom_state->dstr, "&#x", 3) ||
      !UdmDSTRAppendHex(custom_state->dstr, buf, 2) ||
      !UdmDSTRAppend(custom_state->dstr, ";", 1))
    return 1;
  return 0;
}


static int h_bin(RTF_PARSER_STATE *state, RTF_CONTROL *control,
                 int control_parameter)
{
  if (control_parameter < 0)
    return 1;
  *state->ptr+= control_parameter;
  return 0;
}


static int h_ignorable(RTF_PARSER_STATE *state, RTF_CONTROL *control,
                       int control_parameter)
{
  if (state->ignorable_depth == (size_t) -1)
    state->ignorable_depth= state->depth;
  return 0;
}


static int h_charset(RTF_PARSER_STATE *state, RTF_CONTROL *control,
                     int control_parameter)
{
  RTF2TEXT_STATE *custom_state= (RTF2TEXT_STATE *) state->custom_state;
  custom_state->ansicpg= control->bits == CT_VALUE ? control_parameter :
                                                     control->default_value;
  return 0;
}


static int h_uc(RTF_PARSER_STATE *state, RTF_CONTROL *control,
                int control_parameter)
{
  RTF2TEXT_STATE *custom_state= (RTF2TEXT_STATE *) state->custom_state;
  custom_state->g.uc= control_parameter;
  return 0;
}


static int h_cs(RTF_PARSER_STATE *state, RTF_CONTROL *control,
                int control_parameter)
{
  return 0;
}


static RTF_CONTROL rtf2text_controls[]=
{
  { UDM_CSTR_WITH_LEN(""),           CT_SYMBOL, h_chr, 0      }, /* ANSI character */
  { UDM_CSTR_WITH_LEN("-"),          CT_SYMBOL, h_uni, 0x00AD }, /* SOFT HYPHEN */
  { UDM_CSTR_WITH_LEN("_"),          CT_SYMBOL, h_uni, 0x2011 }, /* NON-BREAKING HYPHEN */
  { UDM_CSTR_WITH_LEN("~"),          CT_SYMBOL, h_uni, 0x00A0 }, /* NO-BREAK SPACE */
  { UDM_CSTR_WITH_LEN("bullet"),     CT_SYMBOL, h_uni, 0x2022 }, /* BULLET */
  { UDM_CSTR_WITH_LEN("cell"),       CT_SYMBOL, h_chr, 0x0020 }, /* end of cell */
  { UDM_CSTR_WITH_LEN("column"),     CT_SYMBOL, h_chr, 0x000A }, /* column break */
  { UDM_CSTR_WITH_LEN("emdash"),     CT_SYMBOL, h_uni, 0x2014 }, /* EM DASH */
  { UDM_CSTR_WITH_LEN("emspace"),    CT_SYMBOL, h_uni, 0x2003 }, /* EM SPACE */
  { UDM_CSTR_WITH_LEN("endash"),     CT_SYMBOL, h_uni, 0x2013 }, /* EN DASH */
  { UDM_CSTR_WITH_LEN("enspace"),    CT_SYMBOL, h_uni, 0x2002 }, /* EN SPACE */
  { UDM_CSTR_WITH_LEN("ldblquote"),  CT_SYMBOL, h_uni, 0x201C }, /* LEFT DOUBLE QUOTATION MARK */
  { UDM_CSTR_WITH_LEN("line"),       CT_SYMBOL, h_chr, 0x000A }, /* LINE SEPARATOR */
  { UDM_CSTR_WITH_LEN("lquote"),     CT_SYMBOL, h_uni, 0x2018 }, /* LEFT SINGLE QUOTATION MARK */
  { UDM_CSTR_WITH_LEN("nestcell"),   CT_SYMBOL, h_chr, 0x0020 }, /* end of nested table cell */
  { UDM_CSTR_WITH_LEN("nestrow"),    CT_SYMBOL, h_chr, 0x000A }, /* end of nested table row */
  { UDM_CSTR_WITH_LEN("page"),       CT_SYMBOL, h_chr, 0x000A }, /* page break */
  { UDM_CSTR_WITH_LEN("par"),        CT_SYMBOL, h_chr, 0x000A }, /* PARAGRAPH SEPARATOR */
  { UDM_CSTR_WITH_LEN("qmspace"),    CT_SYMBOL, h_chr, 0x0020 }, /* one-quarter em space */
  { UDM_CSTR_WITH_LEN("row"),        CT_SYMBOL, h_chr, 0x000A }, /* end of table row */
  { UDM_CSTR_WITH_LEN("rdblquote"),  CT_SYMBOL, h_uni, 0x201D }, /* RIGHT DOUBLE QUOTATION MARK */
  { UDM_CSTR_WITH_LEN("rquote"),     CT_SYMBOL, h_uni, 0x2019 }, /* RIGHT SINGLE QUOTATION MARK */
  { UDM_CSTR_WITH_LEN("sect"),       CT_SYMBOL, h_chr, 0x000A }, /* end of section and paragraph */
  { UDM_CSTR_WITH_LEN("tab"),        CT_SYMBOL, h_chr, 0x0009 }, /* HT */

  { UDM_CSTR_WITH_LEN("u"),          CT_VALUE,       h_uni,       0      }, /* Unicode character */
  { UDM_CSTR_WITH_LEN("uc"),         CT_VALUE,       h_uc,        0      },
  { UDM_CSTR_WITH_LEN("bin"),        CT_VALUE,       h_bin,       0      },
  { UDM_CSTR_WITH_LEN("cs"),         CT_VALUE,       h_cs,        0      },

  { UDM_CSTR_WITH_LEN("author"),     CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("buptim"),     CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("colortbl"),   CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("comment"),    CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("creatim"),    CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("doccomm"),    CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("fonttbl"),    CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("footer"),     CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("footerf"),    CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("footerr"),    CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("footerl"),    CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("footnote"),   CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("ftncn"),      CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("ftnsep"),     CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("ftnsepc"),    CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("g"),          CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("header"),     CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("headerf"),    CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("headerr"),    CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("headerl"),    CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("info"),       CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("keywords"),   CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("operator"),   CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("pict"),       CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("printim"),    CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("privatel"),   CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("revtim"),     CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("rxe"),        CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("stylesheet"), CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("subject"),    CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("tc"),         CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("title"),      CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("txe"),        CT_DESTINATION, h_ignorable, 0      },
  { UDM_CSTR_WITH_LEN("xe"),         CT_DESTINATION, h_ignorable, 0      },

  { UDM_CSTR_WITH_LEN("ansi"),       CT_FLAG,        h_charset,   1252   },
  { UDM_CSTR_WITH_LEN("ansicpg"),    CT_VALUE,       h_charset,   0      },
  { UDM_CSTR_WITH_LEN("mac"),        CT_FLAG,        h_charset,   10000  },
  { UDM_CSTR_WITH_LEN("pc"),         CT_FLAG,        h_charset,   437    },
  { UDM_CSTR_WITH_LEN("pca"),        CT_FLAG,        h_charset,   850    },

  { 0, 0, 0, 0, 0 }
};


static int rtf2text_start_group(RTF_PARSER_STATE *state)
{
  RTF2TEXT_STATE *custom_state= (RTF2TEXT_STATE *) state->custom_state;
  if (state->depth > custom_state->allocated_parents)
  {
    RTF2TEXT_GROUP_STATE *tmp;
    if ((tmp= realloc(custom_state->parents,
                      (custom_state->allocated_parents + 32) *
                      sizeof(RTF2TEXT_GROUP_STATE))) == NULL)
      return 1;
    custom_state->parents= tmp;
    custom_state->allocated_parents+= 32;
  }
  memcpy(&custom_state->parents[state->depth - 1], &custom_state->g,
         sizeof(RTF2TEXT_GROUP_STATE));
  return 0;
}


static int rtf2text_end_group(RTF_PARSER_STATE *state)
{
  RTF2TEXT_STATE *custom_state= (RTF2TEXT_STATE *) state->custom_state;
  memcpy(&custom_state->g, &custom_state->parents[state->depth - 1],
         sizeof(RTF2TEXT_GROUP_STATE));
  return 0;
}


static int rtf2text(const char *src, size_t src_length, UDM_DSTR *out, int *cp)
{
  RTF2TEXT_STATE custom_state;

  custom_state.ansicpg= 1252;
  custom_state.skip_characters= 0;
  custom_state.dstr= out;
  custom_state.g.uc= 1;
  custom_state.allocated_parents= 32;
  if ((custom_state.parents= malloc(custom_state.allocated_parents *
                                    sizeof(RTF2TEXT_GROUP_STATE))) == NULL)
    return 1;

  rtf_parser_parse(src, src_length, &custom_state, rtf2text_controls,
                   rtf2text_start_group, rtf2text_end_group);

  free(custom_state.parents);
  *cp= custom_state.ansicpg;
  return 0;
}
/* End of rtf2text functionality */



int
UdmRTFParse(UDM_AGENT *Agent, UDM_DOCUMENT *Doc)
{
  UDM_CONST_STR content;
  UDM_DSTR dstr;
  int cp;
  if (UdmHTTPBufContentToConstStr(&Doc->Buf, &content))
      return UDM_ERROR;
  if (!UdmDSTRInit(&dstr, 64*1024))
    return UDM_ERROR;
  if (!rtf2text(content.str, content.length, &dstr, &cp))
  {
    char charset[16];
    UDM_CONST_TEXTITEM Item;
    bzero(&Item, sizeof(Item));
    UdmConstStrSet(&Item.section_name, UDM_CSTR_WITH_LEN("body"));
    UdmConstStrSet(&Item.text, dstr.data, dstr.size_data);
    Item.section= 1; /*data->body_sec;*/
    UdmTextListAddConst(&Doc->TextList, &Item);
    udm_snprintf(charset, sizeof(charset), "cp%d", cp);
    UdmVarListReplaceStr(&Doc->Sections, "Meta-Charset", charset);
  }
  UdmDSTRFree(&dstr);
  return UDM_OK;
}


/*
  TODO34: remove this
*/
static size_t
UdmHlConvertDSTRWithConv(UDM_AGENT *Agent, UDM_DSTR *dstr,
                         UDM_WIDEWORDLIST *WWList,
                         const char *src, size_t srclen,
                         UDM_HIGHLIGHT_CONV *ec,
                         int hlstop, int segmenter)
{
  size_t tmplen= srclen * 3 + 1;
  char *tmp= UdmMalloc(tmplen);
  tmplen= UdmHlConvertExtWithConv(Agent, tmp, tmplen,
                                  WWList, src, srclen,
                                  ec, hlstop, segmenter);
  UdmDSTRAppend(dstr, tmp, tmplen);
  UdmFree(tmp);
  return tmplen;
}


int
UdmRTFCachedCopy(UDM_AGENT *Agent,
                 UDM_RESULT *Res,
                 UDM_DOCUMENT *Doc,
                 UDM_DSTR *dstr)
{
  UDM_CONST_STR content;
  int cp;
  UDM_DSTR dstr2;
  int rc= UDM_OK;

  if (UdmHTTPBufContentToConstStr(&Doc->Buf, &content))
    return UDM_ERROR;
  if (!UdmDSTRInit(&dstr2, 64 * 1024))
    return UDM_ERROR;

  if (!rtf2text(content.str, content.length, &dstr2, &cp))
  {
    int hlstop= UdmVarListFindBool(&Agent->Conf->Vars, "ExcerptStopword", 1);
    const char *seg= UdmVarListFindStr(&Agent->Conf->Vars, "Segmenter", NULL);
    int segmenter= seg ? UdmUniSegmenterFind(Agent, NULL, seg) : 0;
    UDM_HIGHLIGHT_CONV ec;
    UDM_CHARSET *dcs;
    char charset[16];
    udm_snprintf(charset, sizeof(charset), "cp%d", cp);
    if (!(dcs= UdmGetCharSet(charset)))
    {
      rc= UDM_ERROR;
      goto ex;
    }
    UdmVarListReplaceStr(&Doc->Sections, "Parser.Charset", dcs->name);
    UdmVarListReplaceStr(&Doc->Sections, "Charset", dcs->name);
    UdmVarListReplaceStr(&Doc->Sections, "Meta-Charset", dcs->name);
    UdmExcerptConvInit(&ec, Agent->Conf->bcs, dcs, dcs);
    UdmHlConvertDSTRWithConv(Agent, dstr, &Res->WWList,
                             dstr2.data, dstr2.size_data,
                             &ec, hlstop, segmenter);
  }
ex:
  UdmDSTRFree(&dstr2);
  return rc;
}


int *
UdmRTFExcerptSource(UDM_AGENT *Agent,
                    UDM_RESULT *Res,
                    UDM_DOCUMENT *Doc,
                    UDM_CHARSET *bcs,
                    const UDM_CONST_STR *content,
                    size_t *length)
{
  UDM_DSTR dstr;
  int *dst= NULL, cp;

  if (!UdmDSTRInit(&dstr, 64*1024))
    return 0;

  if (!rtf2text(content->str, content->length, &dstr, &cp))
  {
    UDM_CHARSET *dcs;
    char charset[16];
    udm_snprintf(charset, sizeof(charset), "cp%d", cp);
    UdmVarListReplaceStr(&Doc->Sections, "Meta-Charset", charset);
    if ((dcs= UdmGetCharSet(charset)))
    {
      int hlstop= UdmVarListFindBool(&Agent->Conf->Vars, "ExcerptStopword", 1);
      const char *seg= UdmVarListFindStr(&Agent->Conf->Vars, "Segmenter", NULL);
      int segmenter= seg ? UdmUniSegmenterFind(Agent, NULL, seg) : 0;
      size_t dstmaxlen;
      UDM_CONV cnv;
      UdmConvInit(&cnv, dcs, &udm_charset_sys_int, UDM_RECODE_HTML_SPECIAL);
      dstmaxlen= UdmConvSizeNeeded(&cnv, dstr.size_data, UDM_RECODE_HTML_SPECIAL);
      if (!(dst= (int*) UdmMalloc(dstmaxlen)))
      {
        *length= 0;
        goto ex;
      }
      *length= UdmHlConvertExt(Agent, (char *) dst, dstmaxlen, &Res->WWList, bcs,
                               dstr.data, dstr.size_data,
                               dcs, &udm_charset_sys_int,
                               hlstop, segmenter) / sizeof(int);
    }
  }
ex:
  UdmDSTRFree(&dstr);
  return dst;
}
