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
#include <ctype.h>

#include "udm_common.h"
#include "udm_textlist.h"
#include "udm_parsehtml.h"
#include "udm_utils.h"
#include "udm_url.h"
#include "udm_match.h"
#include "udm_log.h"
#include "udm_xmalloc.h"
#include "udm_server.h"
#include "udm_hrefs.h"
#include "udm_word.h"
#include "udm_crossword.h"
#include "udm_spell.h"
#include "udm_unicode.h"
#include "udm_unidata.h"
#include "udm_uniconv.h"
#include "udm_sgml.h"
#include "udm_guesser.h"
#include "udm_crc32.h"
#include "udm_vars.h"
#include "udm_mutex.h"
#include "udm_searchtool.h"

/****************************************************************/

static int UdmReallocSection(UDM_AGENT *Indexer, UDM_VAR *Sec)
{
  if(!Sec->val)
  {
    Sec->val=(char*)UdmMalloc(Sec->maxlen+1);
  }
  else
  {
    /* Add separator */
    const char *uspace;
    char *vn = UdmStrStore(NULL, "separator.");
    size_t uspacel;
    size_t space_left;

    vn = UdmStrStore(vn, Sec->name);

    UDM_GETLOCK(Indexer, UDM_LOCK_CONF);
    uspace = UdmVarListFindStr(&Indexer->Conf->Vars, vn, " ");
    UdmFree(vn);
    uspacel = uspace ? strlen(uspace) : 0;
    space_left = Sec->maxlen - Sec->curlen;
    if (space_left > uspacel)
    {
      Sec->curlen += snprintf(Sec->val + Sec->curlen, space_left, "%s", uspace);
    }
    else
    {
      Sec->curlen = Sec->maxlen;
    }
    UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);
  }
  return UDM_OK;
}

int UdmPrepareWords(UDM_AGENT * Indexer,UDM_DOCUMENT * Doc){
  size_t    i;
  const char  *doccset;
  UDM_CHARSET  *doccs;
  UDM_CHARSET  *loccs;
  UDM_CHARSET  *sys_int;
  UDM_CONV  dc_uni;
  UDM_CONV  uni_lc;
  UDM_UNIDATA *unidata= Indexer->Conf->unidata;
  UDM_TEXTLIST  *tlist=&Doc->TextList;
  int    res=UDM_OK;
  int    crc32=0;
  int    crossec;
  int    strip_accents;
  char   *lcsword;  /* Word in LocalCharset */
  size_t uwordlen = UDM_MAXWORDSIZE;
  UDM_VAR *Sec;
  int raw_sections= (UdmVarListFindByPrefix(&Doc->Sections, "Raw.", 4) != NULL);
  const char *content_language= UdmVarListFindStr(&Doc->Sections, "Content-Language", "");
  const char *seg=  UdmVarListFindStr(&Indexer->Conf->Vars, "Segmenter", NULL);

  UdmDebugEnter();
  
  if ((lcsword = (char*)UdmMalloc(12 * uwordlen + 1)) == NULL)
    UdmDebugReturn(UDM_ERROR);


  {
    UDM_VAR *S= UdmVarListFind(&Doc->Sections,"crosswords");
    crossec= S ? S->section : 0;
  }
  
  strip_accents= UdmVarListFindBool(&Indexer->Conf->Vars, "StripAccents", 0);
  doccset=UdmVarListFindStr(&Doc->Sections,"Parser.Charset",NULL);
  if(!doccset)doccset=UdmVarListFindStr(&Doc->Sections,"Charset",NULL);
  if(!doccset||!*doccset)doccset=UdmVarListFindStr(&Doc->Sections,"RemoteCharset","iso-8859-1");
  doccs=UdmGetCharSet(doccset);
  if(!doccs)doccs=UdmGetCharSet("iso-8859-1");
  loccs = Doc->lcs;
  if(!loccs)loccs=UdmGetCharSet("iso-8859-1");
  sys_int= &udm_charset_sys_int;

  UdmConvInit(&dc_uni,doccs,sys_int,UDM_RECODE_HTML);
  UdmConvInit(&uni_lc,sys_int,loccs,UDM_RECODE_HTML);
  
  
  /* Now convert everything to UNICODE format and calculate CRC32 */
  
  for(Sec= NULL, i= 0; i< tlist->nitems; i++)
  {
    size_t srclen, dstlen, cnvlen, ustr_char_len;
    char   *src,*dst;
    int    *lt, *tok, *ustr = NULL;
    UDM_TEXTITEM  *Item=&tlist->Item[i];
    char    secname[128];
    char decimal[128];

    if (!Sec || strcasecmp(Sec->name, Item->section_name))
      Sec= UdmVarListFind(&Doc->Sections, Item->section_name);

    if (Sec && (Sec->flags & UDM_VARFLAG_DECIMAL))
    {
      UdmNormalizeDecimal(decimal, sizeof(decimal), Item->str);
      srclen= strlen(decimal);
      src= decimal;
    }
    else
    {
      srclen= strlen(Item->str);
      src= Item->str;
    }
    
    dstlen= (3 * (srclen + 1))*sizeof(int);  /* with '\0' */
    
    if ((ustr = (int*)UdmMalloc(dstlen)) == NULL)
    {
      UdmLog(Indexer, UDM_LOG_ERROR,
             "%s:%d Can't alloc %u bytes", __FILE__, __LINE__, (int) dstlen);
      res= UDM_ERROR;
      goto ex;
    }
    
    dst=(char*)ustr;
    cnvlen= (Item->flags & UDM_TEXTLIST_FLAG_RFC1522) ?
            UdmConvRFC1522(&dc_uni, dst, dstlen, src, srclen) / 4 :
            UdmConv(&dc_uni, dst, dstlen, src, srclen) / 4;
    ustr[cnvlen]= 0;
    
    ustr_char_len= UdmUniRemoveDoubleSpaces(ustr);

    /*
      Append section text.
      Note, we need to check ustr_char_len,
      because if the original string consisting of entities like "&nbsp;",
      it can turn into an empty string after UdmUniRemoveDoubleSpaces().
      No needs to call UdmReallocSection() in this case,
      to avoid unnecessary extra space delimiter.
    */
    if(Sec && Sec->curlen < Sec->maxlen && ustr_char_len &&
       !(Item->flags & UDM_TEXTLIST_FLAG_SKIP_ADD_SECTION))
    {
      int cnvres;
      UdmReallocSection(Indexer,Sec);
      src= (char*) ustr;
      srclen= ustr_char_len * sizeof(int);
      dstlen= Sec->maxlen - Sec->curlen;
      cnvres= UdmConv(&uni_lc, Sec->val + Sec->curlen, dstlen, src, srclen);
      Sec->curlen+= cnvres;
      Sec->val[Sec->curlen]='\0';
      if (cnvres < 0)
      {
        Sec->curlen= Sec->maxlen;
      }
    }
    
    
    /*
    TODO for clones detection:
    Replace any separators into space to ignore 
    various pseudo-graphics, commas, semicolons
    and so on to improve clone detection quality
    */
    
    if (Doc->Spider.index && (!Sec || !(Sec->flags & UDM_VARFLAG_NOCLONE)))
    {
      crc32= UdmCRC32UpdateUnicode(crc32, ustr, ustr_char_len);
    }
    
    /*
      We need to check Doc->Spider.index, because
      user-defined sections must respect 
      <META NAME="Robots" CONTENT="NOINDEX">
    */
    if(Doc->Spider.index && Item->section)
    {
      if (strip_accents)
        UdmUniStrStripAccents(unidata, ustr);
      UdmUniStrToLower(unidata, ustr);
      ustr = UdmUniSegment(Indexer, ustr, content_language, seg);

      for(tok= UdmUniGetToken(unidata, ustr,&lt);
          tok ;
          tok = UdmUniGetToken(unidata, NULL, &lt))
      {
        size_t  tlen;        /* Word length          */ 
        
        tlen=lt-tok;
        
        
        /*
          Word lengths are checked later in indexer.c.
          We cannot just skip this block here because
          wordpos should be incremented even if we skip
          a word. Otherwise we have phrase search problem.
        */
        /*if (tlen <= max_word_len && tlen >= min_word_len)*/
        {
        
          if (tlen > uwordlen)
          {
            uwordlen= tlen;
            if ((lcsword= (char*) UdmRealloc(lcsword, 12 * uwordlen + 1)) == NULL)
            { 
              res= UDM_ERROR;
              goto ex;
            }
          }

          cnvlen= UdmConv(&uni_lc, lcsword, 12 * uwordlen , (char*) tok, sizeof(*tok) * tlen);
          lcsword[cnvlen]= '\0';
          
          res=UdmWordListAdd(Doc,lcsword,Item->section);
          if(res!=UDM_OK)break;
        
          if(Item->href && crossec)
          {
            UDM_CROSSWORD cw;
            cw.url=Item->href;
            cw.weight = crossec;
            cw.pos=Doc->CrossWords.wordpos[crossec];
            cw.word=lcsword;
            UdmCrossListAdd(Doc,&cw);
          }
        }
      }
    }
    
    if (raw_sections)
    {
      udm_snprintf(secname,sizeof(secname)-1,"Raw.%s",Item->section_name);
      if ((Sec=UdmVarListFind(&Doc->Sections,secname)))
      {
        if(Sec->curlen < Sec->maxlen)
        {
          size_t nbytes;
          
          UdmReallocSection(Indexer,Sec);
          
          dstlen= Sec->maxlen-Sec->curlen;
          nbytes= dstlen < srclen ? dstlen : srclen;
          memcpy(Sec->val+Sec->curlen,Item->str,nbytes);
          Sec->curlen+=nbytes;
          Sec->val[Sec->curlen]='\0';
          
          if (dstlen < srclen)
          {
            Sec->curlen=Sec->maxlen;
          }
        }
      }
    }
    
    UDM_FREE(ustr);
    if(res!=UDM_OK)break;
    
  }

  UdmVarListReplaceInt(&Doc->Sections,"crc32",crc32);

ex:
  UDM_FREE(lcsword);
  UdmDebugReturn(res);
}


/**************************** Built-in Parsers ***************************/

static int
UdmTextListAddWithConversion(UDM_DOCUMENT *Doc, 
                             const char *name, const char *src, size_t secno,
                             UDM_CONV *cnv)
{
  size_t tmplen= strlen(src);
  size_t dstlen= tmplen * 8 + 1;
  char *tmp= (char*) UdmMalloc(tmplen + 1);
  char *dst= (char*) UdmMalloc(dstlen + 1);
  
  if (tmp != NULL && dst != NULL)
  {
    UDM_TEXTITEM Item;
    char  sec_name[64];
    UdmUnescapeCGIQuery(tmp, src);
    UdmConv(cnv, dst, dstlen, tmp, strlen(tmp) + 1);
    bzero(&Item, sizeof(Item));
    udm_snprintf(sec_name, sizeof(sec_name), "%s", name);
    Item.str= dst;
    Item.section= secno;
    Item.section_name= sec_name;
    Item.flags= 0;
    UdmTextListAdd(&Doc->TextList, &Item);
  }
  UDM_FREE(tmp);
  UDM_FREE(dst);
  return UDM_OK;  
}


int UdmParseURLText(UDM_AGENT *A,UDM_DOCUMENT *Doc)
{
  UDM_TEXTITEM  Item;
  UDM_VAR       *Sec;
  UDM_CONV      cnv;
  UDM_VARLIST   *V= &Doc->Sections;
  UDM_CHARSET   *l1= UdmGetCharSet("latin1");
  UDM_CHARSET   *rcs= UdmVarListFindCharset(V, "RemoteCharset", l1);
  UDM_CHARSET   *fscs= UdmVarListFindCharset(V, "RemoteFileNameCharset", rcs);
  UDM_CHARSET   *doccs= UdmVarListFindCharset(V, "CharSet", l1);
  
  Item.href=NULL;
  
  if((Sec=UdmVarListFind(&Doc->Sections,"url.proto")))
  {
    char sc[]="url.proto";
    Item.str = UDM_NULL2EMPTY(Doc->CurURL.schema);
    Item.section=Sec->section;
    Item.section_name=sc;
    Item.flags= 0;
    UdmTextListAdd(&Doc->TextList,&Item);
  }
  if((Sec=UdmVarListFind(&Doc->Sections,"url.host")))
  {
    char sc[]="url.host";
    Item.str = UDM_NULL2EMPTY(Doc->CurURL.hostname);
    Item.section=Sec->section;
    Item.section_name=sc;
    Item.flags= 0;
    UdmTextListAdd(&Doc->TextList,&Item);
  }
  
  UdmConvInit(&cnv, fscs, doccs, UDM_RECODE_HTML);
  if((Sec=UdmVarListFind(&Doc->Sections,"url.path")))
  {
    const char *src= UDM_NULL2EMPTY(Doc->CurURL.path);
    UdmTextListAddWithConversion(Doc, Sec->name, src, Sec->section, &cnv);
  }
  if((Sec=UdmVarListFind(&Doc->Sections,"url.file")))
  {
    const char *src= UDM_NULL2EMPTY(Doc->CurURL.filename);
    UdmTextListAddWithConversion(Doc, Sec->name, src, Sec->section,&cnv);
  }
  return UDM_OK;
}

int UdmParseHeaders(UDM_AGENT *Indexer,UDM_DOCUMENT *Doc)
{
  size_t i;
  UDM_TEXTITEM Item;
  
  Item.href=NULL;
  for(i=0;i<Doc->Sections.nvars;i++)
  {
    char  secname[128];
    UDM_VAR  *Sec;
    udm_snprintf(secname,sizeof(secname),"header.%s",Doc->Sections.Var[i].name);
    secname[sizeof(secname)-1]='\0';
    if((Sec=UdmVarListFind(&Doc->Sections,secname)))
    {
      Item.str=Doc->Sections.Var[i].val;
      Item.section=Sec->section;
      Item.section_name=secname;
      Item.flags= 0;
      UdmTextListAdd(&Doc->TextList,&Item);
    }
  }
  return UDM_OK;
}

int UdmParseText(UDM_AGENT * Indexer,UDM_DOCUMENT * Doc)
{
  UDM_TEXTITEM  Item;
  UDM_VAR       *BSec=UdmVarListFind(&Doc->Sections,"body");
  char          sc[]="body";
  
  Item.href=NULL;
  
  if(BSec && Doc->Buf.content && Doc->Spider.index)
  {
    char *lt;
    Item.section=BSec->section;
    Item.str = udm_strtok_r(Doc->Buf.content, "\r\n", &lt);
    Item.section_name=sc;
    Item.flags= 0;
    while(Item.str)
    {
      UdmTextListAdd(&Doc->TextList,&Item);
      Item.str = udm_strtok_r(NULL, "\r\n", &lt);
    }
  }
  return(UDM_OK);
}


int UdmHTMLTOKInit(UDM_HTMLTOK *tag)
{
  bzero((void*)tag, sizeof(*tag));
  return UDM_OK;
}

static char spacemap[256]=
{
  0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  
};

static inline int IsSpaceChar(int x)
{
  return (int) spacemap[x];
}


/*
  TODO: extend UdmHTMLToken() to unserstand ISO-2022-XX sequences, which
  can have '<' and '>' as part of the text when in multi-byte mode, and
  which don't mean the beginning and the end of the tag corrspondingly.
  See uconv-eucjp.c for more details.
*/
const char * UdmHTMLToken(const char *src, const char ** lt,UDM_HTMLTOK *t)
{
  const char *b, *e;
  t->ntoks= 0;
  
  if (!src && !(src= *lt))
    return NULL;

  if (*src == '<')
  {
    const char *tmp= src + 1;
    t->type= (*tmp++ == '!' && *tmp++ == '-' && *tmp++== '-') ?
             UDM_HTML_COM : UDM_HTML_TAG;
  }
  else
  {
    if(!*src)
      return NULL;
    t->type= UDM_HTML_TXT;
  }
  
  switch(t->type)
  {
    case UDM_HTML_TAG:

      for(*lt= b= src + 1; *b; )
      {
        const char *valbeg= NULL;
        const char *valend= NULL;
        size_t nt= t->ntoks;
        
        /* Skip leading spaces */
        for ( ; IsSpaceChar((unsigned char)*b) ; b++);

        if(*b == '>')
        {
          *lt= b + 1;
          return src;
        }

        if(*b == '<')
        { /* Probably broken tag occure */
          *lt= b;
          return src;
        }

        /* Skip non-spaces, i.e. name */
        for (e= b; *e && !strchr(" =>\t\r\n", *e); e++);
        
        if(t->ntoks < UDM_MAXTAGVAL)
          t->ntoks++;
        
        t->toks[nt].val= 0;
        t->toks[nt].vlen= 0;
        t->toks[nt].name= b;
        t->toks[nt].nlen= e - b;

        if (nt == 0)
        {
          const char *beg;
          int val;
          if (b[0] == '/')
          {
            val= 0;
            beg= b + 1;
          }
          else
          {
            val= 1;
            beg= b;
          }
          if (!strncasecmp(beg,"script",6))
            t->script= val;
          else if (!strncasecmp(beg, "noindex", 7))
            t->comment= val;
          else if (!strncasecmp(beg, "style", 5))
            t->style= val;
          else if (!strncasecmp(beg, "body", 4))
            t->body= val;
        }

        if(*e == '>')
        {
          *lt= e + 1;
          return src;
        }

        if(!*e)
        {
          *lt= e;
          return src;
        }
        
        /* Skip spaces */
        for( ; IsSpaceChar((unsigned char) *e) ; e++);
        
        if(*e != '=')
        {
          b= e;
          *lt= b;        /* bug when hang on broken inside tag pages fix */
          continue;
        }
        
        /* Skip spaces */
        for(b= e + 1; IsSpaceChar((unsigned char) *b); b++);
        
        if(*b == '"')
        {
          b++;
          valbeg= b;
          for (e= b; *e && *e != '"'; e++);
          valend= e;
          b= e;
          if (*b == '"') b++;
        }
        else if (*b == '\'')
        {
          b++;
          valbeg= b;
          for (e= b; *e && *e != '\''; e++);
          valend= e;
          b= e;
          if (*b == '\'')b++;
        }
        else
        {
          valbeg= b;
          for (e= b; *e && !strchr(" >\t\r\n", *e); e++);
          valend= e;
          b= e;
        }
        *lt= b;
        t->toks[nt].val= valbeg;
        t->toks[nt].vlen= valend - valbeg;
      }
      break;

    case UDM_HTML_COM: /* comment */
      
      if(!strncasecmp(src, "<!--UdmComment-->",17) ||
         !strncasecmp(src, "<!--noindex-->", 14) ||
         !strncasecmp(src, "<!--X-BotPNI-->",15))
        t->comment=1;
      else if(!strncasecmp(src, "<!--/UdmComment-->",18) ||
              !strncasecmp(src, "<!--/noindex-->", 15) ||
              !strncasecmp(src, "<!--X-BotPNI-End-->", 19))
        t->comment=0;

      for (e= src; *e ; e++)
      {
        if (e[0] == '-' && e[1] == '-' && e[2] == '>')
        {
          *lt= e + 3;
          return src;
        }
      }
      *lt= e;
      break;

    case UDM_HTML_TXT: /* text */
    default:
      /* Special case when script  */
      /* body is not commented:    */
      /* <script> x="<"; </script> */
      /* We should find </script>  */
      
      for (e= src; *e; e++)
      {
        if (*e == '<')
        {
          if(t->script)
          {
            if (!strncasecmp(e, "</script>",9))
            {
              /* This is when script body  */
              /* is not hidden using <!--  */
              break;
            }
            else if (!strncmp(e, "<!--",4))
            {
              /* This is when script body  */
              /* is hidden but there are   */
              /* several spaces between    */
              /* <SCRIPT> and <!--         */
              break;
            }
          }
          else
          {
            break;
          }
        }
      }
      
      *lt= e;
      break;
  }
  return src;
}


static void
UdmDocAddHref(UDM_DOCUMENT *Doc, char *href, const char *crosstext)
{
  UDM_HREF Href;
  UdmSGMLUnescape(href);
  UdmHrefInit(&Href);
  /*
    referrer, hops and site_id will be set later,
    in the end of UdmHTMLParse()
  */
  Href.url= href;
  Href.method= UDM_METHOD_GET;
  if (crosstext)
  {
    urlid_t url_id= UdmVarListFindInt(&Doc->Sections, "ID", 0);
    UdmVarListAddStr(&Href.Vars, "CrossText", crosstext);
    UdmVarListAddInt(&Href.Vars, "Referrer-ID", url_id);
  }
  UdmHrefListAdd(&Doc->Hrefs, &Href);
}


int UdmHTMLParseTag(UDM_HTMLTOK * tag,UDM_DOCUMENT * Doc)
{
  UDM_TEXTITEM Item;
  UDM_VAR  *Sec;
  int opening, rel_nofollow= 0;
  char *metaname=NULL;
  char *metacont=NULL;
  char *href=NULL;
  char *lang = NULL;
  size_t i;
  const char *nameptr= tag->toks[0].name;
  size_t namelen= tag->toks[0].nlen;

  UdmDebugEnter();

  if(!tag->ntoks || !nameptr)
    goto ex;
    
  for(i=0;i<tag->ntoks;i++)
  {
    const char *aname= tag->toks[i].name;
    size_t alen= tag->toks[i].nlen;
    if(!udm_strnncasecmp(aname, alen, UDM_CSTR_WITH_LEN("name")))
    {
      metaname= UdmStrndup(tag->toks[i].val, tag->toks[i].vlen);
    }
    else if(!udm_strnncasecmp(aname, alen, UDM_CSTR_WITH_LEN("http-equiv")))
    {
      metaname= UdmStrndup(tag->toks[i].val, tag->toks[i].vlen);
    }
    else if(!udm_strnncasecmp(aname, alen, UDM_CSTR_WITH_LEN("content")))
    {
      metacont= UdmStrndup(tag->toks[i].val, tag->toks[i].vlen);
    }
    else if(!udm_strnncasecmp(aname, alen, UDM_CSTR_WITH_LEN("href")))
    {
      /* A, LINK, AREA*/
      char *y= UdmStrndup(tag->toks[i].val,tag->toks[i].vlen);
      href= (char*)UdmStrdup(UdmTrim(y, " \t\r\n"));
      UDM_FREE(y);
    }
    else if(!udm_strnncasecmp(aname, alen, UDM_CSTR_WITH_LEN("rel")))
    {
      char param[64], *y;
      size_t paramlen= udm_min(tag->toks[i].vlen, sizeof(param) - 1);
      memcpy(param, tag->toks[i].val, paramlen);
      param[paramlen]= '\0';
      y= UdmTrim(param, " \t\r\n");
      rel_nofollow= !strcasecmp(y, "nofollow");
    }
    else if(!udm_strnncasecmp(aname, alen, UDM_CSTR_WITH_LEN("src")))
    {
      /* IMG, FRAME */
      char *y= UdmStrndup(tag->toks[i].val,tag->toks[i].vlen);
      href= (char*)UdmStrdup(UdmTrim(y, " \t\r\n"));
      UDM_FREE(y);
    }
    else if (!udm_strnncasecmp(aname, alen, UDM_CSTR_WITH_LEN("lang")))
    {
      char *n;
      char *y= UdmStrndup(tag->toks[i].val,tag->toks[i].vlen);
      lang= (char*)UdmStrdup(UdmTrim(y, " \t\r\n"));
      for(n = lang; *n; *n = tolower(*n),n++);
      UDM_FREE(y);
    }
    else if (!tag->skip_attribute_sections)
    {
      char secname[128];
      udm_snprintf(secname, sizeof(secname), "attribute.%.*s",
                   (int) tag->toks[i].nlen, tag->toks[i].name);

      if ((Sec= UdmVarListFind(&Doc->Sections, secname)) && Doc->Spider.index)
      {
        char *y= UdmStrndup(tag->toks[i].val,tag->toks[i].vlen);
        Item.str= y;
        Item.section= Sec->section;
        Item.section_name= secname;
        Item.href= NULL;
        Item.flags= 0;
        UdmTextListAdd(&Doc->TextList, &Item);
        UDM_FREE(y);
      }
    }
  }
  
  if(nameptr[0] == '/')
  {
    opening= 0;
    nameptr++;
    namelen--;
  }
  else
  {
    opening=1;
  }

  tag->nonbreaking= 0;
  /* Let's find tag name in order of frequency */

  if(!udm_strnncasecmp(nameptr, namelen, "a", 1))
  {
    UDM_FREE(tag->lasthref);      /*117941*/
  }
  else if(!udm_strnncasecmp(nameptr, namelen, UDM_CSTR_WITH_LEN("title")))
    tag->title=opening;  /*6192*/
  else if(!udm_strnncasecmp(nameptr, namelen, UDM_CSTR_WITH_LEN("html")))
  {
    if (opening && lang != NULL)
      UdmVarListReplaceStr(&Doc->Sections, "Meta-Language", lang);
  }
  else if(!udm_strnncasecmp(nameptr, namelen, UDM_CSTR_WITH_LEN("body")))
  {
    tag->body=opening;  /*5146*/
    if (opening && (lang != NULL))
    {
      UdmVarListReplaceStr(&Doc->Sections, "Meta-Language", lang);
    }
  }
  else if(!udm_strnncasecmp(nameptr, namelen, UDM_CSTR_WITH_LEN("meta")) &&
          metaname && metacont)
  {
    char secname[128];
    udm_snprintf(secname, sizeof(secname), "meta.%s", metaname);
    if((!tag->comment) && (Sec=UdmVarListFind(&Doc->Sections,secname)) && 
       Doc->Spider.index)
    {
      /*
        We can't call UdmSGMLUnescape(metacont); here,
        because it works only with dcs=lati1.
        Un-SGML will happen later, in Unicode format.
      */
      Item.str=metacont;
      Item.section=Sec->section;
      Item.section_name=secname;
      Item.href = NULL;
      Item.flags= 0;
      UdmTextListAdd(&Doc->TextList,&Item);
    }
    
    if(!strcasecmp(metaname,"Content-Type"))
    {
      char *p;
      if((p=strstr(metacont,"charset=")))
      {
        const char *cs = UdmCharsetCanonicalName(UdmTrim(p + 8, "' \t"));
        UdmVarListReplaceStr(&Doc->Sections, "Meta-Charset", cs ? cs : p + 8);
      }
    }
    else if(!strcasecmp(metaname, "Content-Language") ||
            !strcasecmp(metaname, "DC.Language"))
    {
      char *l, *n ;
      l = (char*)UdmStrdup(metacont);
      for(n = l; *n; *n = tolower(*n),n++);
      UdmVarListReplaceStr(&Doc->Sections, "Meta-Language", l);
      UDM_FREE(l);
    }
    else if(!strcasecmp(metaname,"refresh"))
    {
      /* Format: "10; Url=http://something/" */
      /* URL can be written in different     */
      /* forms: URL, url, Url and so on      */
      char *p;
      
      if((p = strchr(metacont, '=')))
      {
        if((p >= metacont + 3) && (!strncasecmp(p-3,"URL=",4)))
        {
          href = (char*)UdmStrdup(p + 1);
        }
        else
        {
          UDM_FREE(href);
        }
      }
    }
    else if(!strcasecmp(metaname,"robots") &&
            (Doc->Spider.use_robots)&&(metacont))
    {
      char * lt;
      char * rtok;
          
      rtok = udm_strtok_r(metacont," ,\r\n\t",&lt);
      while(rtok)
      {
        if(!strcasecmp(rtok,"ALL"))
        {
          /* Set Server parameters */
          tag->follow=Doc->Spider.follow;
          tag->index=Doc->Spider.index;
        }
        else if(!strcasecmp(rtok,"NONE"))
        {
          tag->follow=UDM_FOLLOW_NO;
          tag->index=0;
          Doc->Spider.follow = UDM_FOLLOW_NO;
          Doc->Spider.index = 0;
        }
        else if(!strcasecmp(rtok,"NOINDEX"))
        {
          tag->index=0;
          Doc->Spider.index = 0;
          UdmTextListFree(&Doc->TextList);
          /* Doc->method = UDM_METHOD_DISALLOW;*/
        }
        else if(!strcasecmp(rtok,"NOFOLLOW"))
        {
          tag->follow=UDM_FOLLOW_NO;
          Doc->Spider.follow = UDM_FOLLOW_NO;
        }
        else if(!strcasecmp(rtok,"NOARCHIVE"))
        {
          UdmVarListReplaceStr(&Doc->Sections, "Z", "");
        }
        else if(!strcasecmp(rtok,"INDEX")) {
          tag->index = Doc->Spider.index;
        }
        else if(!strcasecmp(rtok,"FOLLOW")) 
          tag->follow=Doc->Spider.follow;
        rtok = udm_strtok_r(NULL," \r\n\t",&lt);
      }
    }
  }
  else if(!udm_strnncasecmp(nameptr, namelen, UDM_CSTR_WITH_LEN("script")))
    tag->script=opening;
  else if(!udm_strnncasecmp(nameptr, namelen, UDM_CSTR_WITH_LEN("style")))
    tag->style=opening;
  else if(!udm_strnncasecmp(nameptr, namelen, UDM_CSTR_WITH_LEN("noindex")))
    tag->comment=opening;
  else if(!udm_strnncasecmp(nameptr, namelen, UDM_CSTR_WITH_LEN("base")))
  {
    if (href)
    {
      UdmVarListReplaceStr(&Doc->Sections,"base.href",href);
      /*
        Do not add BASE HREF itself into database.
        It will be used only to compose relative links.
      */
      UDM_FREE(href);
    }
  }
  else if (!udm_strnncasecmp(nameptr, namelen, UDM_CSTR_WITH_LEN("font")))
  {
    tag->nonbreaking= 1;
  }

  if((href)&&(tag->follow!=UDM_FOLLOW_NO))
  {
    if (udm_strnncasecmp(nameptr, namelen, "a", 1))
    {
      /*Add the link only if there's no 'nofollow'*/
      if(!rel_nofollow)
      {
        UdmDocAddHref(Doc, href, NULL);
        /* ...No crosstext, add the link right now*/
      }
    }
    else
    {
      /* TODO: Postpone adding, for crosswords and SQLExportHref */
      UDM_FREE(tag->lasthref);
      /*Add the link only if there's no 'nofollow'*/
      if(!rel_nofollow)
      {
        tag->lasthref= (char*) UdmStrdup(href);
        UdmDocAddHref(Doc, href, NULL);
      }
    }
  }
  UDM_FREE(metaname);
  UDM_FREE(metacont);
  UDM_FREE(href);
  UDM_FREE(lang);

ex:
  
  UdmDebugReturn(0);
}


#define MAXSTACK  1024

typedef struct
{
  size_t len;
  char * ofs;
} UDM_TAGSTACK;


int UdmHTMLParse(UDM_AGENT *Indexer,UDM_DOCUMENT *Doc){
  UDM_HTMLTOK  tag;
  UDM_TEXTITEM Item;
  const char   *htok;
  const char   *last;
  UDM_VAR      *BSec=UdmVarListFind(&Doc->Sections,"body");
  UDM_VAR      *NBSec=UdmVarListFind(&Doc->Sections,"nobody");
  UDM_VAR      *TSec=UdmVarListFind(&Doc->Sections,"title");
  int          body_sec  = BSec ? BSec->section : 0;
  int          nobody_sec= NBSec ? NBSec->section : 0;
  int          title_sec = TSec ? TSec->section : 0;
  int          body_sec_flags= BSec ? BSec->flags : 0;
  int          nobody_sec_flags= NBSec ? NBSec->flags : 0;
  int          title_sec_flags= TSec ? TSec->flags : 0;
  char         scb[]="body";
  char         scnb[]="nobody";
  char         sct[]="title";
  /*
    We cannot use BSec and TSec later, as they are
    modified by UdmVarListReplace from UdmHTMLParseTag
  */

  UdmDebugEnter();
  
  bzero((void*)&Item, sizeof(Item));
  UdmHTMLTOKInit(&tag);
  tag.follow=Doc->Spider.follow;
  tag.index=Doc->Spider.index;
  tag.skip_attribute_sections=
    (UdmVarListFindByPrefix(&Doc->Sections, "attribute.", 10) == NULL);

  htok=UdmHTMLToken(Doc->Buf.content,&last,&tag);
  
  while(htok)
  {
    const char *tmpbeg;
    const char *tmpend;
    
    switch(tag.type)
    {
      case UDM_HTML_COM:
        break;

      case UDM_HTML_TXT:
        for( tmpbeg=htok;   tmpbeg<last && strchr(" \r\n\t",tmpbeg[0]) ; tmpbeg++);
        for( tmpend=last;   tmpbeg<tmpend && strchr(" \r\n\t",tmpend[-1]) ; tmpend--);
        if (tmpbeg >= tmpend || tag.comment || tag.style ||
                                tag.script  || !tag.index)
          break;
        
        Item.str= UdmStrndup(tmpbeg, (size_t) (tmpend - tmpbeg));
        
        if (tag.body)
        {
          if (body_sec && !(body_sec_flags & UDM_VARFLAG_USERDEF))
          {
            Item.href=tag.lasthref;
            Item.section=body_sec;
            Item.section_name=scb;
            Item.flags= 0;
#if NOT_READY_YET
            if (tag.nonbreaking)
              UdmTextListAppend(&Doc->TextList, &Item);
            else
#endif
              UdmTextListAdd(&Doc->TextList,&Item);
          }
        }
        else if (tag.title)
        {
          if (title_sec && !(title_sec_flags & UDM_VARFLAG_USERDEF))
          {
            Item.href=NULL;
            Item.section=title_sec;
            Item.section_name=sct;
            Item.flags= 0;
            UdmTextListAdd(&Doc->TextList,&Item);
          }
        }
        else /* Text outside body and title */
        {
          if (nobody_sec && !(nobody_sec_flags & UDM_VARFLAG_USERDEF))
          {
            Item.href= tag.lasthref;
            Item.section= nobody_sec;
            Item.section_name= (nobody_sec == body_sec) ? scb : scnb;
            Item.flags= 0;
            UdmTextListAdd(&Doc->TextList, &Item);
          }
        }
        UDM_FREE(Item.str);
        break;

      case UDM_HTML_TAG:  
        UdmHTMLParseTag(&tag,Doc);
        break;
    }
    htok=UdmHTMLToken(NULL,&last,&tag);
  }
  
  /* Set size_id, referrer and hops */
  {
    size_t i;
    UDM_HREF Tmp;
    Tmp.referrer= UdmVarListFindInt(&Doc->Sections, "Referrer-ID", 0);
    Tmp.hops= 1 + UdmVarListFindInt(&Doc->Sections,"Hops",0);
    Tmp.site_id= UdmVarListFindInt(&Doc->Sections, "Site_id", 0);

    for (i= 0; i < Doc->Hrefs.nhrefs; i++)
    {
      UDM_HREF *Href= &Doc->Hrefs.Href[i];
      Href->referrer= Tmp.referrer;
      Href->hops= Tmp.hops;
      Href->site_id= Tmp.site_id;
    }
  }
  
  UDM_FREE(tag.lasthref);  
  UdmDebugReturn(UDM_OK);
}
