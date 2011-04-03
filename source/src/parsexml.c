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
#include "udm_uniconv.h"
#include "udm_vars.h"
#include "udm_textlist.h"
#include "udm_parsexml.h"
#include "udm_hrefs.h"
#include "udm_sgml.h"

typedef struct
{
  UDM_AGENT *Indexer;
  UDM_DOCUMENT *Doc;
  UDM_HREF Href;
  int body_sec;
  const char *XMLDefaultSection;
  char *sec;
  char *secpath;
  size_t pathlen, curlen;
} XML_PARSER_DATA;


/************ MY_XML **************/
#define UDM_XML_EOF		'E'
#define UDM_XML_STRING		'S'
#define UDM_XML_IDENT		'I'
#define UDM_XML_EQ		'='
#define UDM_XML_LT		'<'
#define UDM_XML_GT		'>'
#define UDM_XML_LB		'['
#define UDM_XML_RB		']'
#define UDM_XML_SLASH		'/'
#define UDM_XML_COMMENT		'C'
#define UDM_XML_TEXT		'T'
#define UDM_XML_QUESTION	'?'
#define UDM_XML_EXCLAM		'!'
#define UDM_XML_CDATA		'D'



typedef struct xml_attr_st
{
  const char *beg;
  const char *end;
} UDM_XML_ATTR;


static const char *UdmLex2str (int lex)
{
  switch (lex)
  {
    case UDM_XML_EOF:      return("EOF");
    case UDM_XML_STRING:   return("STRING");
    case UDM_XML_IDENT:    return("IDENT");
    case UDM_XML_CDATA:    return("CDATA");
    case UDM_XML_EQ:       return("'='");
    case UDM_XML_LT:       return("'<'");
    case UDM_XML_GT:       return("'>'");
    case UDM_XML_LB:       return("'['");
    case UDM_XML_RB:       return("']'");
    case UDM_XML_SLASH:    return("'/'");
    case UDM_XML_COMMENT:  return("COMMENT");
    case UDM_XML_TEXT:     return("TEXT");
    case UDM_XML_QUESTION: return("'?'");
    case UDM_XML_EXCLAM:   return("'!'");
  }
  return("UNKNOWN");
}

static void UdmXMLNormText (UDM_XML_ATTR *a)
{
  for (; (a->beg < a->end) && strchr(" \t\r\n", a->beg[0]); a->beg++);
  for (; (a->beg < a->end) && strchr(" \t\r\n", a->end[-1]); a->end--);
}

static int UdmXMLScan (UDM_XML_PARSER *p, UDM_XML_ATTR *a)
{
  int lex;

  for(; (p->cur < p->end) && strchr(" \t\r\n", p->cur[0]); p->cur++);

  if (p->cur >= p->end)
  {
    a->beg= p->end;
    a->end= p->end;
    lex= UDM_XML_EOF;
    goto ret;
  }

  a->beg= p->cur;
  a->end= p->cur;
  
  if (! memcmp(p->cur, "<!--", 4))
  {
    for(; (p->cur < p->end) && memcmp(p->cur, "-->", 3); p->cur++);
    if(! memcmp(p->cur, "-->", 3))
      p->cur += 3;
    a->end= p->cur;
    lex= UDM_XML_COMMENT;
  }
  else if (!memcmp(p->cur, "<![CDATA[",9))
  {
    p->cur+= 9;
    for (; p->cur < p->end ; p->cur++)
    {
      if (p->cur[0] == ']' && p->cur[1] == ']' && p->cur[2] == '>')
      {
        p->cur+= 3;
        a->end= p->cur;
        break;
      }
    }
    lex= UDM_XML_CDATA;
  }
  else if (strchr("?=/<>![]", p->cur[0]))
  {
    /* Simple tokens, consisting on one character */
    p->cur++;
    a->end= p->cur;
    lex= a->beg[0];
  }
  else if ((p->cur[0] == '"') || (p->cur[0] == '\''))
  {
    p->cur++;
    for(; ( p->cur < p->end ) && (p->cur[0] != a->beg[0]); p->cur++) {}
    a->end= p->cur;
    if (a->beg[0] == p->cur[0]) p->cur++;
    a->beg++;
    if (!(p->flags & UDM_XML_SKIP_TEXT_NORMALIZATION))
      UdmXMLNormText(a);
    lex= UDM_XML_STRING;
  }
  else
  {
    /* Identifier */
    for(; (p->cur < p->end) && ! strchr("?'\"=/<>[] \t\r\n", p->cur[0]); p->cur++) {}
    a->end= p->cur;
    if (!(p->flags & UDM_XML_SKIP_TEXT_NORMALIZATION))
      UdmXMLNormText(a);
    lex= UDM_XML_IDENT;
  }

#if 0
  printf("LEX=%s[%d]\n", UdmLex2str(lex), a->end-a->beg);
#endif

ret:
  return(lex);
}

static int UdmXMLValue (UDM_XML_PARSER *st, const char *str, size_t len)
{
#ifdef DEBUG_XML
  fprintf(stderr, "UdmXMLValue: %.*s\n", len, str);
#endif
  return((st->value) ? (st->value)(st, str, len) : UDM_XML_OK);
}

static int UdmXMLEnter(UDM_XML_PARSER *st, const char *str, size_t len)
{
#ifdef DEBUG_XML
  fprintf(stderr, "UdmXMLEnter: %.*s\n", len, str);
#endif
  if ((st->attrend - st->attr + len + 1) > sizeof(st->attr))
  {
    sprintf(st->errstr, "To deep XML");
    return(UDM_XML_ERROR);
  }
  if (st->attrend > st->attr)
  {
    st->attrend[0]= '.';
    st->attrend++;
  }
  memcpy(st->attrend, str, len);
  st->attrend += len;
  st->attrend[0]= '\0';
  return(st->enter ? st->enter(st, st->attr, st->attrend - st->attr) : UDM_XML_OK);
}

static void mstr (char *s, const char *src, size_t l1, size_t l2)
{
  l1= l1 < l2 ? l1 : l2;
  memcpy(s, src, l1);
  s[l1]= '\0';
}

static int UdmXMLLeave(UDM_XML_PARSER *p, const char *str, size_t slen)
{
  char *e;
  size_t glen;
  char s[32];
  char g[32];
  int rc;

#ifdef DEBUG_XML
  fprintf(stderr, "UdmXMLLeave: %.*s\n", slen, str);
#endif
  /* Find previous '.' or beginning */
  for(e= p->attrend; (e > p->attr) && (e[0] != '.'); e--);
  glen= (e[0] == '.') ? (p->attrend - e - 1) : p->attrend - e;

  if (str && (slen != glen))
  {
    mstr(s, str, sizeof(s) - 1, slen);
    mstr(g, e + 1, sizeof(g) - 1, glen),
    sprintf(p->errstr, "'</%s>' unexpected ('</%s>' wanted)", s, g);
    return(UDM_XML_ERROR);
  }

  rc= p->leave_xml ? p->leave_xml(p, p->attr, p->attrend - p->attr) : UDM_XML_OK;

  *e= '\0';
  p->attrend= e;
  return(rc);
}


int UdmXMLParser (UDM_XML_PARSER *p, const char *str, size_t len)
{
  p->attrend= p->attr;
  p->beg= str;
  p->cur= str;
  p->end= str + len;

  while (p->cur < p->end)
  {
    UDM_XML_ATTR a;
    if (p->cur[0] == '<')
    {
      int lex;
      int exclam= 0;
      p->question= 0;

      lex= UdmXMLScan(p, &a);

      if (UDM_XML_COMMENT==lex)
        continue;

      if (UDM_XML_CDATA == lex)
      {
        a.beg+= 9;
        a.end-= 3;
        UdmXMLValue(p, a.beg, a.end - a.beg);
        continue;
      }
      
      lex= UdmXMLScan(p, &a);

      if (UDM_XML_SLASH == lex)
      {
        if (UDM_XML_IDENT != (lex= UdmXMLScan(p, &a)))
        {
          sprintf(p->errstr, "1: %s unexpected (ident wanted)", UdmLex2str(lex));
          return(UDM_XML_ERROR);
        }
        if (UDM_XML_OK != UdmXMLLeave(p, a.beg, a.end-a.beg))
          return(UDM_XML_ERROR);
        lex= UdmXMLScan(p, &a);
        goto gt;
      }

      if (UDM_XML_EXCLAM == lex)
      {
        lex= UdmXMLScan(p, &a);
        exclam= 1;
      }
      else if (UDM_XML_QUESTION == lex)
      {
        lex= UdmXMLScan(p, &a);
        p->question= 1;
      }

      if (UDM_XML_IDENT == lex)
      {
        if (UDM_XML_OK != UdmXMLEnter(p, a.beg, a.end-a.beg))
          return(UDM_XML_ERROR);
      }
      else
      {
        sprintf(p->errstr, "3: %s unexpected (ident or '/' wanted)", UdmLex2str(lex));
        return(UDM_XML_ERROR);
      }

      /* Scan attributes */
      while ((UDM_XML_IDENT == (lex= UdmXMLScan(p, &a))) ||
             (UDM_XML_STRING == lex))
      {
        UDM_XML_ATTR b;
        if (UDM_XML_EQ == (lex= UdmXMLScan(p, &b)))
        {
          /* Scan attr=value */
          lex= UdmXMLScan(p, &b);
          if ((lex == UDM_XML_IDENT) || (lex == UDM_XML_STRING))
          {
            if ((UDM_XML_OK != UdmXMLEnter(p, a.beg, a.end-a.beg)) ||
                (UDM_XML_OK != UdmXMLValue(p, b.beg, b.end - b.beg)) ||
                (UDM_XML_OK != UdmXMLLeave(p, a.beg, a.end - a.beg)))
              return(UDM_XML_ERROR);
          }
          else
          {
            sprintf(p->errstr, "4: %s unexpected (ident or string wanted)", UdmLex2str(lex));
            return(UDM_XML_ERROR);
          }
        }
        else if ((UDM_XML_STRING == lex) || (UDM_XML_IDENT == lex))
        {
          if (UDM_XML_IDENT == lex)
          {
            if ((UDM_XML_OK != UdmXMLEnter(p, a.beg, a.end - a.beg)) ||
                (UDM_XML_OK != UdmXMLLeave(p, a.beg, a.end - a.beg)))
              return(UDM_XML_ERROR);
          }
          else
          {
            /* Do nothing */
          }
        }
        else
          break;
      }
      
      /* Skip intSubset list in DOCTYPE declaration */
      if (exclam && lex == UDM_XML_LB)
      {
        for(lex= UdmXMLScan(p, &a); ; lex= UdmXMLScan(p, &a))
        {
          if (lex == UDM_XML_EOF)
          {
            sprintf(p->errstr, "7: %s unexpected (']' wanted)", UdmLex2str(lex));
            return UDM_ERROR;
          }
          if (lex == UDM_XML_RB)
          {
            lex= UdmXMLScan(p, &a);
            break;
          }
        }
      }

      if (lex == UDM_XML_SLASH)
      {
        if (UDM_XML_OK != UdmXMLLeave(p, NULL, 0))
          return(UDM_XML_ERROR);
        lex= UdmXMLScan(p, &a);
      }
gt:

      if (p->question)
      {
        if (lex != UDM_XML_QUESTION)
        {
          sprintf(p->errstr, "6: %s unexpected ('?' wanted)", UdmLex2str(lex));
          return(UDM_XML_ERROR);
        }
        if (UDM_XML_OK != UdmXMLLeave(p, NULL, 0))
          return(UDM_XML_ERROR);
        lex= UdmXMLScan(p, &a);
      }

      if (exclam)
      {
        if (UDM_XML_OK != UdmXMLLeave(p, NULL, 0))
          return(UDM_XML_ERROR);
      }

      if (lex != UDM_XML_GT)
      {
        sprintf(p->errstr, "5: %s unexpected ('>' wanted)", UdmLex2str(lex));
        return(UDM_XML_ERROR);
      }
    }
    else
    {
      a.beg= p->cur;
      for (; (p->cur < p->end) && (p->cur[0] != '<'); p->cur++);
      a.end= p->cur;

      if (!(p->flags & UDM_XML_SKIP_TEXT_NORMALIZATION))
        UdmXMLNormText(&a);
      if (a.beg!=a.end)
        UdmXMLValue(p, a.beg, a.end - a.beg);
    }
  }
  return(UDM_XML_OK);
}


void UdmXMLParserCreate (UDM_XML_PARSER *p)
{
  bzero((void*)p, sizeof(p[0]));
}


void UdmXMLParserFree (UDM_XML_PARSER *p)
{
}


void UdmXMLSetValueHandler (UDM_XML_PARSER *p, int (*action)(UDM_XML_PARSER *p, const char *s, size_t l))
{
  p->value= action;
}


void UdmXMLSetEnterHandler (UDM_XML_PARSER *p, int (*action)(UDM_XML_PARSER *p, const char *s, size_t l))
{
  p->enter= action;
}


void UdmXMLSetLeaveHandler (UDM_XML_PARSER *p, int (*action)(UDM_XML_PARSER *p, const char *s, size_t l))
{
  p->leave_xml= action;
}


void UdmXMLSetUserData (UDM_XML_PARSER *p, void *user_data)
{
  p->user_data= user_data;
}


const char *UdmXMLErrorString (UDM_XML_PARSER *p)
{
  return(p->errstr);
}


size_t UdmXMLErrorPos (UDM_XML_PARSER *p)
{
  const char *beg= p->beg;
  const char *s;
  for (s= p->beg; s < p->cur; s++)
  {
    if (s[0] == '\n')
      beg=s;
  }
  return(p->cur-beg);
}


size_t UdmXMLErrorLineno (UDM_XML_PARSER *p)
{
  size_t res= 0;
  const char *s;
  for (s=p->beg; s<p->cur; s++)
  {
    if (s[0]=='\n')
      res++;
  }
  return(res);
}

/************ /MY_XML **************/

static int startElement(UDM_XML_PARSER *parser, const char *name, size_t l)
{
  XML_PARSER_DATA *D= parser->user_data;
  UDM_VARLIST *Vars= &D->Indexer->Conf->XMLEnterHooks;
  const char *val;
  
  UDM_FREE(D->sec);
  D->sec= (char*)UdmStrndup(name, l);
  UDM_FREE(D->secpath);
  D->secpath= UdmStrndup(name, l);
  if (Vars->nvars && (val= UdmVarListFindStr(Vars, D->secpath, NULL)))
  {
    if (!strcasecmp(val, "HrefVarInit"))
    {
      UdmVarListFree(&D->Href.Vars);
    }
    else if (!strcasecmp(val, "HrefInit"))
    {
      UdmHrefFree(&D->Href);
      UdmHrefInit(&D->Href);
    }
  }
  return(UDM_XML_OK);
}

static int endElement(UDM_XML_PARSER *parser, const char *name, size_t l)
{
  XML_PARSER_DATA *D= parser->user_data;
  size_t i= l;

  if (D->Indexer->Conf->XMLLeaveHooks.nvars)
  {
    char *prevname= UdmStrndup(name, l);
    const char *val;
    if (D->Href.url &&
        (val= UdmVarListFindStr(&D->Indexer->Conf->XMLLeaveHooks,
                                prevname, NULL)))
    {
      UDM_DOCUMENT *Doc= D->Doc;
      D->Href.referrer= UdmVarListFindInt(&Doc->Sections, "Referrer-ID", 0);
      D->Href.hops= 1 + UdmVarListFindInt(&Doc->Sections, "Hops", 0);
      D->Href.site_id= UdmVarListFindInt(&Doc->Sections, "Site_id", 0);
      D->Href.method= UDM_METHOD_GET;
      UdmHrefListAdd(&Doc->Hrefs, &D->Href);
    }
    UdmFree(prevname);
  }

  while (--i && name[i] != '.');

  UDM_FREE(D->sec);
  D->sec= (char *)UdmStrndup(name, i);
  UDM_FREE(D->secpath);
  D->secpath= UdmStrndup(name, i);
  return(UDM_XML_OK);
}

static int Text (UDM_XML_PARSER *parser, const char *s, size_t len)
{
  XML_PARSER_DATA *D= parser->user_data;
  UDM_DOCUMENT *Doc= D->Doc;
  UDM_TEXTITEM  Item;
  UDM_VARLIST  *Vars= &D->Indexer->Conf->XMLDataHooks;
  UDM_VAR    *Sec;
  const char *val;
  size_t slen;

  if (D->sec == NULL) return(UDM_XML_OK);

  /* Process XMLDataHook commands */
  if ((val= UdmVarListFindStr(Vars, D->sec, NULL)))
  {
    if (!strcasecmp(val, "HrefSet"))
    {
      UDM_FREE(D->Href.url);
      D->Href.url= UdmStrndup(s, (size_t)len);
      UdmSGMLUnescape(D->Href.url);
    }
    else if (!strcasecmp(val, "HrefVarAdd"))
    {
      UdmVarListReplaceStrn(&D->Href.Vars, D->sec, s, len);
    }
    else if (!strcasecmp(val, "HrefVarAppend"))
    {
      UDM_VAR *var;
      if ((var= UdmVarListFind(&D->Href.Vars, D->sec)))
      {
        UdmVarAppendStrn(var, " ", 1);
        UdmVarAppendStrn(var, s, len);
      }
      else
      {
        UdmVarListReplaceStrn(&D->Href.Vars, D->sec, s, len);
      }
    }
  }
  
  bzero((void*)&Item, sizeof(Item));
  Item.str= UdmStrndup(s, (size_t)len);
  if((Sec= UdmVarListFind(&Doc->Sections, D->sec)))
  {
    Item.section= Sec->section;
    Item.section_name= D->sec;
  }
  else if(D->XMLDefaultSection)
  {
    Item.section= D->body_sec;
    Item.section_name= (char*) D->XMLDefaultSection;
  }
  else
  {
    Item.section= 0;
    Item.section_name= D->sec;
  }
  UdmTextListAdd(&Doc->TextList, &Item);
  UdmFree(Item.str);

  if (D->secpath &&
      (slen= strlen(D->secpath)) >= 5 &&
      !strncasecmp(&D->secpath[slen - 5], ".href", 5))
  {
    UdmHrefFree(&D->Href);
    UdmHrefInit(&D->Href);
    D->Href.url= UdmStrndup(s, (size_t)len);
    UdmSGMLUnescape(D->Href.url);
    D->Href.referrer= UdmVarListFindInt(&Doc->Sections, "Referrer-ID", 0);
    D->Href.hops= 1 + UdmVarListFindInt(&Doc->Sections, "Hops", 0);
    D->Href.site_id= UdmVarListFindInt(&Doc->Sections, "Site_id", 0);
    D->Href.method= UDM_METHOD_GET;
    UdmHrefListAdd(&Doc->Hrefs, &D->Href);
  }


  if ((slen == 12 && !strcasecmp(D->secpath, "rss.encoding")) ||
      (parser->question && slen == 12 &&
       !strcasecmp(D->secpath, "xml.encoding")))
  {
    char buf[64];
    if (len > 0 && len < sizeof(buf))
    {
      const char *csname;
      memcpy(buf, s, len);
      buf[len]= '\0';
      csname= UdmCharsetCanonicalName(buf);
      if (csname)
        UdmVarListReplaceStr(&Doc->Sections, "Meta-Charset", csname);
    }
  }
  return(UDM_XML_OK);
}

#if 0
static void Decl(void *userData, const XML_Char *version, const XML_Char *encoding, int standalone)
{
  XML_PARSER_DATA *D= userData;
  UDM_DOCUMENT *Doc= D->Doc;

  if (encoding != NULL)
    UdmVarListReplaceStr(&Doc->Sections,
                         "Meta-Charset", UdmCharsetCanonicalName(encoding));

}

static int EncHandler(void *encodingHandlerData, const XML_Char *name, XML_Encoding *info)
{
  UDM_AGENT *indexer= encodingHandlerData;
  UDM_CHARSET  *cs;
  size_t i;

  if (!(cs= UdmGetCharSet(name)))
  {
    return 0;
  }
  
  /* FIXME: rewrite this for multibytes encodings */
  if (cs->tab_to_uni == NULL) return 0; 

  info->convert= NULL;
  info->release= NULL;
  info->data= NULL;
  for(i= 0; i < 256; i++)
    info->map[i]= cs->tab_to_uni[i];

  return 1;
}
#endif

int UdmXMLParse(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc)
{
  int res= UDM_OK;
  XML_PARSER_DATA Data;
  UDM_XML_PARSER parser;
  const char *XMLDefaultSection= UdmVarListFindStr(&Indexer->Conf->Vars, "XMLDefaultSection", NULL);
  UDM_VAR  *BSec= XMLDefaultSection ? UdmVarListFind(&Doc->Sections,XMLDefaultSection) : NULL;
  int    body_sec= BSec ? BSec->section : 0;

  UdmXMLParserCreate(&parser);
  bzero(&Data, sizeof(Data));
  Data.Indexer= Indexer;
  Data.Doc= Doc;
  Data.body_sec= body_sec;
  Data.XMLDefaultSection= XMLDefaultSection;

  UdmXMLSetUserData(&parser, &Data);
  UdmXMLSetEnterHandler(&parser, startElement);
  UdmXMLSetLeaveHandler(&parser, endElement);
  UdmXMLSetValueHandler(&parser, Text);

  if (UdmXMLParser(&parser, Doc->Buf.content, 
                   (int)strlen(Doc->Buf.content)) == UDM_XML_ERROR)
  {
    char err[256];    
    udm_snprintf(err, sizeof(err), 
                 "XML parsing error: %s at line %d pos %d\n",
                  UdmXMLErrorString(&parser),
                  UdmXMLErrorLineno(&parser),
                  UdmXMLErrorPos(&parser));
    UdmVarListReplaceStr(&Doc->Sections, "X-Reason", err);
    res= UDM_ERROR;
  }

  UdmXMLParserFree(&parser);
  UDM_FREE(Data.sec);
  UDM_FREE(Data.secpath);
  UdmHrefFree(&Data.Href);
  return res;
}
