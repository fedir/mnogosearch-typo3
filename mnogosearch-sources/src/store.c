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

#include <udm_config.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#ifdef   HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#include "udm_store.h"
#include "udm_utils.h"
#include "udm_vars.h"
#include "udm_parsehtml.h"
#include "udm_unicode.h"
#include "udm_unidata.h"
#include "udm_searchtool.h"
#include "udm_doc.h"
#include "udm_db.h"
#include "udm_result.h"
#include "udm_parser.h"
#include "udm_http.h"
#include "udm_guesser.h"


int UdmCachedCopyUnpack(UDM_DOCUMENT *Doc, const char *sval, size_t l)
{
#ifdef HAVE_ZLIB
  z_stream zstream;
  char *in_buf;

  in_buf= UdmMalloc(l);
  zstream.next_in= (Byte *)in_buf;
  zstream.avail_in= udm_base64_decode((char *)zstream.next_in, sval, l);
  zstream.next_out= (Byte *)Doc->Buf.buf;
  zstream.avail_out= UDM_MAXDOCSIZE-1;
  zstream.zalloc= Z_NULL;
  zstream.zfree= Z_NULL;
  zstream.opaque= Z_NULL;
  
  if (inflateInit2(&zstream, 15) != Z_OK) 
  {
    UdmFree(Doc->Buf.buf);
    UdmFree(in_buf);
    Doc->Buf.buf= NULL;
    return UDM_ERROR;
  }
  
  inflate(&zstream, Z_FINISH);
  inflateEnd(&zstream);
  Doc->Buf.size = zstream.total_out;
  Doc->Buf.content = Doc->Buf.buf;
  Doc->Buf.buf[Doc->Buf.size]= '\0';
  UdmFree(in_buf);
  return UDM_OK;
#endif
  return UDM_ERROR;
}



/******************* Cached Copy routines ***************************/

typedef struct udm_cached_copy_helper_st
{
  UDM_DOCUMENT Doc;
  UDM_RESULT   Res;
  UDM_DSTR     dstr;
  UDM_CHARSET  *cs;
} UDM_CACHED_COPY_HELPER;


static void
UdmParseCachedCopy(UDM_CACHED_COPY_HELPER *param)
{
  UDM_HTMLTOK  tag;
  const char *htok;
  char *last= NULL;
  UdmHTMLTOKInit(&tag);
  for(htok= UdmHTMLToken(param->Doc.Buf.content, (const char **) &last, &tag) ;
      htok ;)
  {
    switch(tag.type)
    {
      case UDM_HTML_COM:
      case UDM_HTML_TAG:
        UdmDSTRAppend(&param->dstr, htok, (size_t) (last - htok));
        UdmHTMLParseTag(&tag, &param->Doc);
        break;
      case UDM_HTML_TXT:
      {
        UDM_WIDEWORDLIST *WWL= (tag.title || tag.script ||
                                tag.comment || tag.style) ?
                                NULL : &param->Res.WWList;         
        char ch= *last, *tmp;
        *last= '\0';
        if ((tmp= UdmHlConvert(WWL, htok, param->cs, param->cs)))
        {
          UdmDSTRAppend(&param->dstr, tmp, strlen(tmp));
          UdmFree(tmp);
        }
        *last= ch;
        break;
      }
    }
    htok= UdmHTMLToken(NULL, (const char **) &last, &tag);
  }
}


static void
UdmPrepareQueryString(UDM_ENV *Env, UDM_DOCUMENT *Doc)
{
  const char *qstring;

  if ((qstring= UdmVarListFindStr(&Env->Vars, "ENV.QUERY_STRING", NULL)))
  {
    /*
      Remove dbnum from query string.
      Note: dbnum is passed when a cached copy is displayed in cluster.
    */
    if (!strncmp(qstring, "dbnum=", 6))
    {
      char tmp[1024];
      for (qstring+= 6 ; (qstring[0] >= '0' && qstring[0] <= '9') || qstring[0]=='&' ; qstring++);
      udm_snprintf(tmp, sizeof(tmp), "%s", qstring);
      UdmVarListReplaceStr(&Doc->Sections, "ENV.QUERY_STRING", tmp);
      UdmVarListReplaceStr(&Env->Vars, "ENV.QUERY_STRING", tmp);
    }
    else
    {
      UdmVarListReplaceStr(&Doc->Sections, "ENV.QUERY_STRING", qstring);
    }
  }
}


static void
UdmCachedCopyFromTextSource(UDM_CACHED_COPY_HELPER *param)
{
  char *tmp= UdmHlConvert(&param->Res.WWList, param->Doc.Buf.content,
                          param->cs, param->cs);
  if (tmp)
  {
    UdmDSTRAppend(&param->dstr, tmp, strlen(tmp));
    UdmFree(tmp);
  }
}


static void
UdmCachedCopyMake(UDM_AGENT *Agent,
                  UDM_CACHED_COPY_HELPER *param, const char *content_type)
{
  udm_content_type_t ct;
#ifdef USE_PARSER
  if (content_type)
  {
    UDM_PARSER   *Parser;
    if ((Parser= UdmParserFind(&Agent->Conf->Parsers, content_type)))
    {
      content_type= Parser->to_mime ? Parser->to_mime : "text/html";
    }
  }
#endif
  if (!param->Doc.Buf.content)
    return;

  param->cs= UdmVarListFindCharset(&param->Doc.Sections, "Parser.Charset", UdmGetCharSet("latin1"));
  if (!content_type || !(ct= UdmContentTypeByName(content_type)))
  {
    UDM_CONST_STR content;
    if (UdmHTTPBufContentToConstStr(&param->Doc.Buf, &content))
      return;
    content_type= UdmGuessContentType(content.str, content.length, "text/plain");
    ct= UdmContentTypeByName(content_type);
  }
  switch (ct)
  {
    case UDM_CONTENT_TYPE_TEXT_HTML:
    case UDM_CONTENT_TYPE_TEXT_XML:
      UdmParseCachedCopy(param);
      break;

    case UDM_CONTENT_TYPE_MESSAGE_RFC822:
      UdmMessageRFC822CachedCopy(Agent, &param->Res, &param->Doc, &param->dstr);
      break;

    case UDM_CONTENT_TYPE_TEXT_PLAIN:
      UdmDSTRAppend(&param->dstr, UDM_CSTR_WITH_LEN("<pre>\n"));
      UdmCachedCopyFromTextSource(param);
      UdmDSTRAppend(&param->dstr, UDM_CSTR_WITH_LEN("</pre>\n"));
      break;

    case UDM_CONTENT_TYPE_DOCX:
      UdmVarListReplaceStr(&param->Doc.Sections, "Parser.Charset", "utf-8");
      UdmVarListReplaceStr(&Agent->Conf->Vars, "Charset", "utf-8");
      param->cs= UdmGetCharSet("utf-8");
      UdmDOCXCachedCopy(Agent, &param->Res, &param->Doc, &param->dstr);
      break;

    case UDM_CONTENT_TYPE_TEXT_RTF:
      UdmDSTRAppend(&param->dstr, UDM_CSTR_WITH_LEN("<span style=\"white-space:pre-wrap\">"));
      if (UDM_OK == UdmRTFCachedCopy(Agent, &param->Res, &param->Doc, &param->dstr))
      {
        param->cs= UdmVarListFindCharset(&param->Doc.Sections, "Meta-Charset",
                                         UdmGetCharSet("cp1252"));
        UdmVarListReplaceStr(&Agent->Conf->Vars, "Charset", param->cs->name);
        UdmDSTRAppend(&param->dstr, UDM_CSTR_WITH_LEN("</span>\n"));
      }
      else
      {
        UdmDSTRReset(&param->dstr);
      }
      break;

    case UDM_CONTENT_TYPE_AUDIO_MPEG:
    case UDM_CONTENT_TYPE_HTDB:
    case UDM_CONTENT_TYPE_UNKNOWN:
      break;
  }
}


/*
  Fetch cached copy from the database,
  highlight and store into the "document" variable
  of Agent->Env->Vars;
  The URL of the needed document must be stored in Agent->Env-Vars.
*/
int
UdmCachedCopyGet(UDM_AGENT *Agent)
{
  UDM_CACHED_COPY_HELPER param;
  UDM_DOCUMENT *Doc= &param.Doc;
  UDM_ENV *Env= Agent->Conf;
  const char   *content_type;

  bzero(&param, sizeof(param));
  UdmDocInit(&param.Doc);
  UdmResultInit(&param.Res);
  UdmDSTRInit(&param.dstr, 1024);

  UdmPrepare(Agent, &param.Res);
  UdmVarListReplaceStr(&Doc->Sections, "URL", UdmVarListFindStr(&Env->Vars, "URL", "0"));
  UdmVarListReplaceStr(&Doc->Sections, "dbnum", UdmVarListFindStr(&Env->Vars, "dbnum", "0"));
  UdmPrepareQueryString(Agent->Conf, &param.Doc);
  UdmURLAction(Agent, Doc, UDM_URL_ACTION_GET_CACHED_COPY);
  UdmVarListReplaceLst(&Env->Vars, &Doc->Sections, NULL, "*");

  /* Highlight and Display */
  content_type= UdmVarListFindStr(&Env->Vars, "Content-Type", NULL);
  UdmCachedCopyMake(Agent, &param, content_type);
  UdmVarListReplaceStrn(&Env->Vars, "document", param.dstr.data, param.dstr.size_data);

  UdmResultFree(&param.Res);
  UdmDocFree(&param.Doc);
  UdmDSTRFree(&param.dstr);
  return UDM_OK;
}
