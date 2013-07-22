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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "udm_common.h"
#include "udm_parsehtml.h"
#include "udm_doc.h"
#include "udm_textlist.h"
#include "udm_log.h"
#include "udm_utils.h"
#include "udm_vars.h"
#include "udm_proto.h"
#include "udm_searchtool.h"
#include "udm_http.h"


/*
  Check if the sequence pointed by 'tok' is  '\r' or '\r\n'.
  Return 0 if no.
  Return 1 if '\n' found.
  Return 2 if '\r\n' found.
*/
static size_t
UdmIsNewLine(const char *tok, const char *end)
{
  size_t len= end - tok;
  if (!len)
    return 0;
  if (tok[0] == '\n')
    return 1;
  if (len < 2)
    return 0;
  if (tok[0] == '\r' && tok[1] == '\n')
    return 2;
  return 0;
}



static udm_bool_t
UdmIsSpaceOrTab(int ch)
{
  return ch == ' ' || ch == '\t';
}


/************************************/

static const char *
find_rfc1522_start(const char *src, const char *srcend)
{
  for ( ; src < srcend; src++)
  {
    if (src[0] == '=' && src + 1 < srcend && src[1] == '?')
      return src;
  }
  return NULL;
}


static const char *
find_rfc1522_end(const char *src, const char *srcend)
{
  for ( ; src < srcend; src++)
  {
    if (src[0] == '?' && src + 1 < srcend && src[1] == '=')
      return src + 2;
  }
  return NULL;
}


typedef enum rfc1522_chunk_enum
{
  UDM_RFC1522_EOL= 0,
  UDM_RFC1522_TEXT= 1,
  UDM_RFC1522_BASE64= 2,
  UDM_RFC1522_QUOTED_PRINTABLE= 3,
  UDM_RFC1522_LINEAR_WHITESPACE= 4, /* Multi-line header separator */
  UDM_RFC1522_ERROR= 5
} udm_rfc1522_chunk_t;


typedef struct rfc1522_chunk_param_st
{
  UDM_CONST_STR encoded_content;
  UDM_CHARSET *cs;
  udm_rfc1522_chunk_t type;
} UDM_RFC1522_CHUNK_PARAM;


static udm_bool_t
rfc1522_charset(UDM_RFC1522_CHUNK_PARAM *param,
                const char *start, const char *content)
{
  char csname[64];
  udm_snprintf(csname, sizeof(csname), "%.*s", (int) (content - start), start);
  return (param->cs= UdmGetCharSet(csname)) ? UDM_FALSE : UDM_TRUE;
}


static const char *
find_rfc1522_content(UDM_RFC1522_CHUNK_PARAM *param,
                     const char *src, const char *srcend)
{
  const char *src0= src;
  for ( ; src + 2 < srcend; src++)
  {
    if (src[0] == '?' && src[2] == '?')
    {
      switch (src[1])
      {
        case 'Q':
        case 'q':
          param->type= UDM_RFC1522_QUOTED_PRINTABLE;
          if (rfc1522_charset(param, src0, src))
            return NULL;
          return src + 3;
        case 'B':
        case 'b':
          param->type= UDM_RFC1522_BASE64;
          if (rfc1522_charset(param, src0, src))
            return NULL;
          return src + 3;
      }
    }
  }
  return NULL;
}


/*
  "=?" <charset> "?B?"  <text> "?="
  "=?" <charset> "?b?"  <text> "?="
  "=?" <charset> "?Q?"  <text> "?="
  "=?" <charset> "?q?"  <text> "?="
*/
static udm_bool_t
rfc1522_chunk(UDM_CONST_STR *chunk,
              UDM_RFC1522_CHUNK_PARAM *param,
              const char *src, const char *srcend)
{
  const char *rfc1522_start= find_rfc1522_start(src, srcend);
  const char *rfc1522_end, *rfc1522_content;

  if (!rfc1522_start) /* Single text chunk */
  {
    chunk->str= src;
    if (!(chunk->length= srcend - src))
    {
      param->type= UDM_RFC1522_EOL;
      return UDM_TRUE;
    }
    param->type= UDM_RFC1522_TEXT;
    return UDM_FALSE;
  }

  if (rfc1522_start > src) /* Text chunk followed by encoded chunk */
  {
    chunk->str= src;
    if ((chunk->length= UdmIsNewLine(src, rfc1522_start)) &&
        UdmIsSpaceOrTab(src[chunk->length]))
    {
      chunk->length++;
      param->type= UDM_RFC1522_LINEAR_WHITESPACE;
      return UDM_FALSE;
    }
    chunk->length= rfc1522_start - src;
    param->type= UDM_RFC1522_TEXT;
    return UDM_FALSE;
  }

  if (!(rfc1522_content= find_rfc1522_content(param, src + 2, srcend)))
  {
    param->type= UDM_RFC1522_ERROR;
    return UDM_TRUE;
  }

  if ((rfc1522_end= find_rfc1522_end(rfc1522_content, srcend)))
  {
    chunk->str= src;
    chunk->length= rfc1522_end - src;
    param->encoded_content.str= rfc1522_content;
    param->encoded_content.length= rfc1522_end - rfc1522_content - 2;
    return UDM_FALSE;
  }
  param->type= UDM_RFC1522_ERROR;
  return UDM_FALSE;
}


static size_t
UdmConvBase64(UDM_CONV *conv,
              char *dst, size_t dstlen,
              const char *src, size_t srclen)
{
  char *tmp= UdmMalloc(srclen);
  int tmplen;
  size_t dres= 0;
  if (!tmp)
    return 0;
  if ((tmplen= UdmBase64Decode(src, srclen, tmp, NULL, 0)) >= 0)
    dres= UdmConv(conv, dst, dstlen, tmp, (size_t) tmplen);
  UdmFree(tmp);
  return dres;
}


static size_t
UdmConvQuotedPrintable(UDM_CONV *conv,
                       char *dst, size_t dstlen,
                       const char *src, size_t srclen)
{
  char *tmp= UdmMalloc(srclen);
  int tmplen;
  size_t dres= 0;
  if (!tmp)
    return 0;
  if ((tmplen= udm_quoted_printable_decode(src, srclen, tmp, srclen)) > 0)
    dres= UdmConv(conv, dst, dstlen, tmp, (size_t) tmplen);
  UdmFree(tmp);
  return dres;
}


size_t
UdmConvRFC1522(UDM_CONV *conv, char *dst, size_t dstlen,
               const char *src, size_t srclen)
{
  char *dst0= dst;
  const char *srcend= src + srclen;
  UDM_CONST_STR chunk;
  UDM_RFC1522_CHUNK_PARAM param;

  for ( ; !rfc1522_chunk(&chunk, &param, src, srcend); src+= chunk.length)
  {
    size_t tmplen;
    /*printf("chunk[%d]: %.*s\n", param.type, (int) chunk.length, chunk.str);*/
    switch (param.type)
    {
      case UDM_RFC1522_TEXT:
        tmplen= UdmConv(conv, dst, dstlen, chunk.str, chunk.length);
        dst+= tmplen;
        dstlen-= tmplen;
        break;
      case UDM_RFC1522_BASE64:
        {
          UDM_CONV conv2;
          UdmConvInit(&conv2, param.cs, conv->to, UDM_RECODE_HTML_NONASCII);
          tmplen= UdmConvBase64(&conv2, dst, dstlen,
                                param.encoded_content.str,
                                param.encoded_content.length);
          dst+= tmplen;
          dstlen-= tmplen;
        }
        break;
      case UDM_RFC1522_QUOTED_PRINTABLE:
        {
          UDM_CONV conv2;
          UdmConvInit(&conv2, param.cs, conv->to, UDM_RECODE_HTML_NONASCII);
          tmplen= UdmConvQuotedPrintable(&conv2, dst, dstlen,
                                         param.encoded_content.str,
                                         param.encoded_content.length);
          dst+= tmplen;
          dstlen-= tmplen;
        }
        break;
      case UDM_RFC1522_LINEAR_WHITESPACE:
        /* Do nothing */
        break;
      case UDM_RFC1522_ERROR:
        break;
      case UDM_RFC1522_EOL:
        UDM_ASSERT(0);
        break;
    }
  }
  return dst - dst0;
}



/************************************/
/*
  Every message consists of:
  1. Headers
  2. Content
  3. Parts (in case of a multi-part messages).
     Every parts start with its own headers followed by content,
     and optionally followed by sub-parts (in case when a multi-part
     message is attached to a multi-part message).

  Message parser executes user-defined handlers as follows:
  - header_handler() is called whenever a header is found.
  - content_handler() is called whenever content is found.
  - part_handler() is called whenever a part of a multi-part message is found.
    
*/
typedef struct udm_message_parser_st
{
  void *data;
  const char *tok;
  const char *end;
  int (*event_handler)(struct udm_message_parser_st *,
                       const UDM_CONST_STR *name);
  int (*header_handler)(struct udm_message_parser_st *,
                        const UDM_CONST_STR *name,
                        const UDM_CONST_STR *val);
  int (*part_handler)(struct udm_message_parser_st *,
                      const UDM_CONST_STR *body);
  int (*content_handler)(struct udm_message_parser_st *,
                         const UDM_CONST_STR *body);
  UDM_CONST_STR boundary;
  char is_multipart;
  char errstr[128];
} UDM_MESSAGE_PARSER;



/********* Initialization API routines *********************/
static void UdmMessageParserInit(UDM_MESSAGE_PARSER *p)
{
  bzero((void*) p, sizeof(*p));
}


static void UdmMessageParserFree(UDM_MESSAGE_PARSER *p)
{
}


static void UdmMessageParserSetUserData(UDM_MESSAGE_PARSER *p, void *data)
{
  p->data= data;
}


static void
UdmMessageParserSetEventHandler(UDM_MESSAGE_PARSER *p,
                                int (*action)(UDM_MESSAGE_PARSER *p,
                                              const UDM_CONST_STR *name))
{
  p->event_handler= action;
}


static void
UdmMessageParserSetHeaderHandler(UDM_MESSAGE_PARSER *p,
                                 int (*action)(UDM_MESSAGE_PARSER *p,
                                               const UDM_CONST_STR *name,
                                               const UDM_CONST_STR *val))
{
  p->header_handler= action;
}


static void
UdmMessageParserSetContentHandler(UDM_MESSAGE_PARSER *p,
                                  int (*action)(UDM_MESSAGE_PARSER *p,
                                  const UDM_CONST_STR *body))
{
  p->content_handler= action;
}


static void
UdmMessageParserSetPartHandler(UDM_MESSAGE_PARSER *p,
                               int (*action)(UDM_MESSAGE_PARSER *p,
                               const UDM_CONST_STR *body))
{
  p->part_handler= action;
}


/*************** Event routines *******************************/
static int
message_event_enter(UDM_MESSAGE_PARSER *p)
{
  static UDM_CONST_STR part_enter= {UDM_CSTR_WITH_LEN("message_enter")};
  return p->event_handler(p, &part_enter);
}


static int
message_event_content(UDM_MESSAGE_PARSER *p)
{
  static UDM_CONST_STR part_content= {UDM_CSTR_WITH_LEN("message_content")};
  return p->event_handler(p, &part_content);
}


static int
message_event_parts(UDM_MESSAGE_PARSER *p)
{
  static UDM_CONST_STR part_content= {UDM_CSTR_WITH_LEN("message_parts")};
  return p->event_handler(p, &part_content);
}


static int
message_event_leave(UDM_MESSAGE_PARSER *p)
{
  static UDM_CONST_STR part_leave= {UDM_CSTR_WITH_LEN("message_leave")};
  return p->event_handler(p, &part_leave);
}


static int
message_event_leave_with_rc(UDM_MESSAGE_PARSER *p, int rc)
{
  static UDM_CONST_STR part_leave= {UDM_CSTR_WITH_LEN("message_leave")};
  p->event_handler(p, &part_leave);
  return rc;
}


/**************** Parsing routines ******************************/
#define UDM_MSG_OK     0
#define UDM_MSG_CRLF   1
#define UDM_MSG_ERROR  2
#define UDM_MSG_EOF    3

#define UDM_MSG_BOUNDARY_MAX_LENGTH 70

static int
UdmMessageGetBoundary(UDM_MESSAGE_PARSER *p, const UDM_CONST_STR *value)
{
  const char *boundary= "boundary=";
  size_t boundary_length= sizeof("boundary=") - 1;
  const char *beg= value->str;
  const char *end= value->str + value->length;
  const char *search_end= end - boundary_length - 1;
  
  for ( ; beg < search_end; beg++)
  {
    if (!strncasecmp(beg, boundary, boundary_length))
    {
      const char *beg0;
      char ch= '\0';
      beg+= boundary_length;
      if (*beg == '"')
        ch= *beg++;
      beg0= beg;
      for ( ; beg < end; beg++)
      {
        if (*beg == ch || (ch == '\0' && (*beg == ' ' || *beg == ';')))
          break;
      }
      if (beg - beg0 <= UDM_MSG_BOUNDARY_MAX_LENGTH)
      {
        p->boundary.str= beg0;
        p->boundary.length= beg - beg0;
      }
      return UDM_OK;
    }
  }
  return UDM_MSG_OK;
}


/*
  Find new line ('\n' or '\r\n').
  Return pointer to the next line, or NULL if new line is not found.
  Return length of the new line sequence in "length".
*/
static const char*
UdmFindNewLine(const char *tok, const char *end, size_t *length)
{
  for ( ; tok < end; tok++)
  {
    if ((*length= UdmIsNewLine(tok, end)))
      return tok + *length;
  }
  return NULL;
}

/*
  Check if the sequence pointed by 'tok' is a message header end.
  Note, if the next line starts with SPACE or TAB, then the remainder
  of the current header follows.
*/
static size_t
UdmIsHeaderEnd(const char *tok, const char *end)
{
  size_t length= UdmIsNewLine(tok, end);
  if (!length || tok + length >= end || UdmIsSpaceOrTab(tok[length]))
    return 0;
  return length;
}


static int
UdmMessageScanHeader(UDM_MESSAGE_PARSER *p)
{
  const char *beg= p->tok;
  UDM_CONST_STR name;
  size_t header_end_length;

  if ((header_end_length= UdmIsNewLine(p->tok, p->end)))
  {
    p->tok+= header_end_length;
    return UDM_MSG_CRLF;
  }

  /* Scan name */
  for ( ; p->tok < p->end && p->tok[0] != ':'; p->tok++)
  {
    if (p->tok[0] < 33 || p->tok[0] > 126)
    {
      size_t len= p->tok - beg;
      if (len > 32)
        len= 32;
      udm_snprintf(p->errstr, sizeof(p->errstr),
                   "Unexpected character in header name after '%.*s'",
                   (int) len, p->tok - len);
      return UDM_MSG_ERROR;
    }
  }
  name.str= beg;
  name.length= p->tok - beg;
  p->tok++; /* skip colon */
  /*
     Special case: of a multi-line header:
     "HeaderName:<nl><space><header value>"
  */
  if ((header_end_length= UdmIsNewLine(p->tok, p->end)))
    p->tok+= header_end_length;

  if (!UdmIsSpaceOrTab(p->tok[0]))
  {
    if (header_end_length > 0)
    {
      /*
        Special case: Header with empty value:
        "HeaderName:<nl>"
      */
      UDM_CONST_STR value;
      value.str= name.str + name.length + 1; /* The next character after ':' */
      value.length= 0;
      p->header_handler(p, &name, &value);
      return UDM_MSG_OK;
    }
    /*
      Header name immediately followed by value, with no space between:
      Header:value
    */
  }
  else
    p->tok++; /* skip space */

  /* Scan value */
  beg= p->tok;
  for ( ; p->tok < p->end ; p->tok++)
  {
    header_end_length= UdmIsHeaderEnd(p->tok, p->end);
    if (header_end_length)
    {
      UDM_CONST_STR value;
      value.str= beg;
      value.length= p->tok - beg;
      /* Skip trailing staces and tabs */
      for ( ; value.length ; value.length--)
      {
        int last= value.str[value.length-1];
        if (!UdmIsSpaceOrTab(last))
          break;
      }
      p->header_handler(p, &name, &value);

      if (name.length == sizeof("Content-Type") - 1 &&
          !strncasecmp(name.str, UDM_CSTR_WITH_LEN("Content-Type")))
      {
        p->is_multipart= value.length >= sizeof("multipart/") &&
                         !strncasecmp(value.str, UDM_CSTR_WITH_LEN("multipart/"));
        /*printf("ContentType found is_multipart=%d\n", (int) p->is_multipart);*/
        if (p->is_multipart)
        {
          UdmMessageGetBoundary(p, &value);
          if (p->boundary.length)
          {
            /*printf("Boundary found: '%.*s'\n", p->boundary.length, p->boundary.str);*/
          }
        }
      }
      p->tok+= header_end_length;
      return UDM_MSG_OK;
    }
  }
  udm_snprintf(p->errstr, sizeof(p->errstr), "End-Of-Headers not found");
  return UDM_MSG_ERROR;
}


static const char *
FindBoundary(const UDM_CONST_STR *boundary,
             const char *tok, const char *end)
{
  for ( ; tok < end - boundary->length; tok++)
  {
    if (tok[0] == '-' && tok[1] == '-' &&
        !memcmp(tok + 2, boundary->str, boundary->length))
      return tok;
  }
  return NULL;
}


static int
UdmMessageScanOnePart(UDM_MESSAGE_PARSER *p)
{
  const char *beg0= p->tok, *boundary;
  if ((boundary= FindBoundary(&p->boundary, p->tok, p->end)))
  {
    UDM_CONST_STR part;
    size_t header_end_length;
    p->tok= boundary;
    part.str= beg0;
    part.length= p->tok - beg0;
    p->part_handler(p, &part);
    p->tok+= 2 + p->boundary.length;
    if ((header_end_length= UdmIsHeaderEnd(p->tok, p->end)))
      p->tok+= header_end_length;
    return UDM_MSG_OK;
  }
  /* No boundary found */
  udm_snprintf(p->errstr, sizeof(p->errstr),
               "Could not find boundary while scanning a message part");
  p->tok= p->end;
  return UDM_MSG_ERROR;
}


static int
UdmMessageScanParts(UDM_MESSAGE_PARSER *p)
{
  int rc;
  if (!p->boundary.length)
    return UDM_MSG_ERROR;
  for (rc= UdmMessageScanOnePart(p);
       rc == UDM_MSG_OK;
       rc= UdmMessageScanOnePart(p))
  {
    /*printf("------ NEXT ------\n");*/
    if (p->end - p->tok >= 2 && p->tok[0] == '-' && p->tok[1] == '-')
    {
      /* The final boundary found. Ignore the epilogue.  */
      p->tok= p->end;
      return UDM_MSG_OK;
    }
  }
  
  return rc;
}


static int
UdmMessageParserExec(UDM_MESSAGE_PARSER *p, const char *str, size_t length)
{
  int rc;
  if (UDM_OK != (rc= message_event_enter(p)))
    return rc;
  if (length < 5)
  {
    udm_snprintf(p->errstr, sizeof(p->errstr), "Message is too short");
    return message_event_leave_with_rc(p, UDM_ERROR);
  }
  p->tok= str;
  p->end= str + length;
  if (!memcmp(str, UDM_CSTR_WITH_LEN("From ")))
  {
    const char *tmp;
    size_t new_line_length;
    UDM_CONST_STR name, value;
    /*
       From line found: this is not really a message/rfc822 file.
       It's a mixture of mbox and message format.
       The reminder of the From line is not important.
    */
    if (!(tmp= UdmFindNewLine(p->tok, p->end, &new_line_length)))
    {
      udm_snprintf(p->errstr, sizeof(p->errstr), "End of From line not found");
      return message_event_leave_with_rc(p, UDM_ERROR);
    }
    UdmConstStrSet(&name, UDM_CSTR_WITH_LEN("From-Line"));
    UdmConstStrSet(&value, p->tok, tmp - p->tok - new_line_length);
    if (UDM_OK != (rc= p->header_handler(p, &name, &value)))
      return message_event_leave_with_rc(p, rc);
    p->tok= tmp;
  }
  for (rc= UdmMessageScanHeader(p);
       rc == UDM_MSG_OK;
       rc= UdmMessageScanHeader(p))
  {
  }

  if (rc != UDM_MSG_CRLF)
    return message_event_leave_with_rc(p, UDM_ERROR);

  if (UDM_OK != (rc= message_event_content(p)))
    return message_event_leave_with_rc(p, rc);

  if (p->is_multipart)
  {
    const char *boundary= FindBoundary(&p->boundary, p->tok, p->end);
    size_t header_end_length;
    UDM_CONST_STR content;
    if (!boundary)
    {
      udm_snprintf(p->errstr, sizeof(p->errstr),
                   "Could not find boundary while scanning multipart hidden body");
      return message_event_leave_with_rc(p, UDM_ERROR);
    }
    /*
      Execute content_handler to with 0-length content,
      to avoid parting of the content.
    */
    content.str= p->tok;
    content.length= 0;
    p->content_handler(p, &content);

    /* Continue parsing parts */
    message_event_parts(p);
    p->tok= boundary +  p->boundary.length + 2 /* leading -- */;
    if ((header_end_length= UdmIsHeaderEnd(p->tok, p->end)))
      p->tok+= header_end_length;
    if (UDM_MSG_OK != (rc= UdmMessageScanParts(p)))
    {
      if (rc == UDM_MSG_EOF)
        return message_event_leave(p);
      return message_event_leave_with_rc(p, UDM_ERROR);
    }
  }
  else
  {
    UDM_CONST_STR val;
    val.str= p->tok;
    val.length= p->end - p->tok;
    p->content_handler(p, &val);
    if (UDM_OK != (rc= message_event_parts(p)))
      return message_event_leave_with_rc(p, rc);
  }

  return message_event_leave(p);
}



/******************* User routines ******************/


typedef struct udm_message_data_st
{
  UDM_AGENT *A;
  UDM_DOCUMENT *D;
  struct udm_message_data_st *Parent;
  size_t level;
} UDM_MESSAGE_DATA;


static UDM_MESSAGE_DATA *UdmTopMessageData(UDM_MESSAGE_DATA *data)
{
  while (data->Parent)
    data= data->Parent;
  return data;
}


extern int UdmDocParseContent(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc);


/*
  TODO: merge with a similar function in parsehtml.c
*/
static int
UdmTextListAddWithConversion(UDM_TEXTLIST *TextList, 
                             const char *name, const char *src, size_t secno,
                             int flags, UDM_CONV *cnv)
{
  size_t srclen= strlen(src);
  size_t dstlen= srclen * 8 + 1;
  char *dst= (char*) UdmMalloc(dstlen + 1);

  if (dst != NULL)
  {
    UDM_TEXTITEM Item;
    char  sec_name[64];
    UdmConv(cnv, dst, dstlen, src, srclen + 1);
    bzero(&Item, sizeof(Item));
    udm_snprintf(sec_name, sizeof(sec_name), "%s", name);
    Item.str= dst;
    Item.section= secno;
    Item.section_name= sec_name;
    Item.flags= flags;
    UdmTextListAdd(TextList, &Item);
    /*printf("DST=%s\n", dst);*/
  }
  UDM_FREE(dst);
  return UDM_OK;  
}


static void
UdmTextListJoinWithConversion(UDM_DOCUMENT *Doc, UDM_TEXTLIST *b, UDM_CONV *cnv)
{
  size_t i;
  UDM_TEXTLIST *a= &Doc->TextList;
  for (i= 0; i < b->nitems; i++)
  {
    UDM_TEXTITEM *Item= &b->Item[i];
    if (Item->section == 255)
    {
      UdmTextListAdd(a, Item);
    }
    else
    {
      UDM_VAR *Sec= UdmVarListFindBySecno(&Doc->Sections, Item->section);
      if (Sec)
        UdmTextListAddWithConversion(a, Sec->name, Item->str,
                                     Item->section, Item->flags, cnv);
    }
  }
}


static void
UdmDocBase64Decode(UDM_DOCUMENT *Doc)
{
  int res;
  char *tmp= UdmMalloc(Doc->Buf.content_length + 1);
  size_t header_size= Doc->Buf.content - Doc->Buf.buf;
  memcpy(tmp, Doc->Buf.content, Doc->Buf.content_length);
  Doc->Buf.content[Doc->Buf.content_length]= '\0';
  res= UdmBase64Decode(tmp, Doc->Buf.content_length, Doc->Buf.content, NULL, 0);
  if (res < 0)
  {
    /* Cut content */
    Doc->Buf.content_length= 0;
    Doc->Buf.size= header_size;
  }
  else
  {
    Doc->Buf.content_length= (size_t) res;
    Doc->Buf.size= header_size + (size_t) res;
  }
  /*printf("DECODED len=%d CONTENT'%.*s'", Doc->Buf.content_length, Doc->Buf.content_length, Doc->Buf.content);*/
  Doc->Buf.content[Doc->Buf.content_length]= '\0';
  UdmFree(tmp);
}


static void
UdmDocQuotedPrintableDecode(UDM_DOCUMENT *Doc)
{
  size_t res= udm_quoted_printable_decode(Doc->Buf.content, Doc->Buf.content_length,
                                          Doc->Buf.content, Doc->Buf.content_length);
  Doc->Buf.content[res]= '\0';
  Doc->Buf.size= Doc->Buf.content_length= res;
}


static int
UdmContentTransferEncodingDecode(UDM_AGENT *A, UDM_DOCUMENT *Doc)
{
  const char *ce= UdmVarListFindStr(&Doc->Sections,"Content-Transfer-Encoding", "");
  if (!strcasecmp(ce, "base64"))
  {
    UdmDocBase64Decode(Doc);
    return UDM_OK;
  }
  else if (!strcasecmp(ce, "quoted-printable"))
  {
    UdmDocQuotedPrintableDecode(Doc);
    return UDM_OK;
  }
  else if (!ce[0] || !strcasecmp(ce, "8bit") || !strcasecmp(ce, "7bit") ||
           !strcasecmp(ce, "binary"))
  {
    /* Do nothing */
    return UDM_OK;
  }
  else
  {
    UdmLog(A, UDM_LOG_WARN, "Unsupported Content-Transfer-Encoding: %s", ce);
    return UDM_ERROR;
  }
}


static int event_handler(UDM_MESSAGE_PARSER *p,
                         const UDM_CONST_STR *name)
{
  UDM_CONV cnv;
  UDM_CHARSET *l1= UdmGetCharSet("latin1");
  UDM_TEXTITEM Item;
  static UDM_CONST_STR value= {UDM_CSTR_WITH_LEN("")};
  char header_name[128];
  UDM_MESSAGE_DATA *data= (UDM_MESSAGE_DATA *) p->data;
  UDM_DOCUMENT *Doc= data->D;
  UDM_DOCUMENT *TopDoc= UdmTopMessageData(data)->D;
  udm_bool_t is_top_doc= data->Parent->D == TopDoc;
  udm_snprintf(header_name, sizeof(header_name), "%smsg.%.*s",
               is_top_doc ? "" : "part.", (int) name->length, name->str);

  bzero(&Item, sizeof(Item));
  Item.str= UdmConstStrDup(&value);
  Item.section= 255;
  Item.section_name= header_name;
  Item.flags= UDM_TEXTLIST_FLAG_RFC1522;
  if (!strcasecmp(header_name, "part.msg.message_content") ||
      !strcasecmp(header_name, "part.msg.message_parts"))
  {
    udm_content_type_t ct= UdmContentTypeByName(UdmVarListFindStr(&Doc->Sections, "Content-Type", "text/plain"));
    Item.flags= (ct == UDM_CONTENT_TYPE_MESSAGE_RFC822) ?
                 UDM_TEXTLIST_FLAG_MESSAGE_RFC822 : 0;
  }
  UdmTextListAdd(&Doc->TextList, &Item);
  UdmFree(Item.str);

  UdmConvInit(&cnv, l1, l1, UDM_RECODE_HTML);
  UdmTextListJoinWithConversion(TopDoc, &Doc->TextList, &cnv);
  UdmTextListFree(&Doc->TextList);

  return UDM_OK;
}


static int content_handler(UDM_MESSAGE_PARSER *p,
                          const UDM_CONST_STR *val)
{
  UDM_CONV cnv;
  UDM_CHARSET *partcs, *topcs;
  UDM_CHARSET *l1= UdmGetCharSet("latin1");
  UDM_MESSAGE_DATA *data= (UDM_MESSAGE_DATA *) p->data;
  UDM_DOCUMENT *Doc= data->D;
  UDM_DOCUMENT *TopDoc= UdmTopMessageData(data)->D;

  UDM_ASSERT(Doc != TopDoc);

  if (!val->length) /* e.g. the ignored content of a multi-part message */
    goto join;

  /*UdmVarListPrint(stdout, &Doc->Sections);*/

  /*
  printf("TopDoc: url=%s cs=%s ct=%s\n",
         UdmVarListFindStr(&TopDoc->Sections, "URL", "???"),
         UdmVarListFindStr(&TopDoc->Sections, "Charset", "???"),
         UdmVarListFindStr(&TopDoc->Sections, "Content-Type", "???"));
  printf("Single part content:\n'%.*s'\n", val->length, val->str);
  */

  /*
  fprintf(stderr, "buf=%p/%d content=%p/%d bufend=%p val=%p\n",
        Doc->Buf.buf, Doc->Buf.size, Doc->Buf.content, Doc->Buf.content_length,
        Doc->Buf.buf + Doc->Buf.size, val->str);
  */
  if (Doc->Buf.buf) /* Content of the top level message */
  {
    size_t content_offset= val->str - Doc->Buf.buf;
    /* val must be inside Doc->Buf */
    UDM_ASSERT(Doc->Buf.buf < val->str);
    UDM_ASSERT(val->str + val->length <= Doc->Buf.buf + Doc->Buf.size);
    Doc->Buf.content= Doc->Buf.buf + content_offset;
    Doc->Buf.content_length= val->length;
  }
  else /* Content of a part of a multipart message */
  {
    Doc->Buf.maxsize= val->length;
    Doc->Buf.buf= Doc->Buf.content= (char*)UdmMalloc(Doc->Buf.maxsize + 1);
    Doc->Buf.size= Doc->Buf.maxsize= Doc->Buf.content_length= val->length;
    memcpy(Doc->Buf.buf, val->str, val->length);
    Doc->Buf.buf[val->length]= '\0';
  }

  if (UDM_ERROR == UdmContentTransferEncodingDecode(data->A, Doc))
    return UDM_OK;

  UdmVarListReplaceInt(&Doc->Sections, "Status", UDM_HTTP_STATUS_OK);
  if (!UdmVarListFindStr(&Doc->Sections, "Content-Type", NULL))
    UdmVarListReplaceStr(&Doc->Sections, "Content-Type", "text/plain");
  UdmDocParseContent(data->A, Doc);

join:
  /* TODO: join this code with parsehtml.c */
  partcs= UdmVarListFindCharset(&Doc->Sections, "Parser.Charset", NULL);
  if (!partcs) partcs= UdmVarListFindCharset(&Doc->Sections,"Charset", NULL);
  if (!partcs) partcs= UdmVarListFindCharset(&Doc->Sections,"RemoteCharset", NULL);
  if (!partcs) partcs= UdmVarListFindCharset(&Doc->Sections, "Meta-Charset", l1);
  topcs= UdmVarListFindCharset(&TopDoc->Sections, "Charset", l1);
  /*printf("partcs=%s topcs=%s\n", partcs->name, topcs->name);*/
  /*UdmVarListPrint(stdout, &Doc->Sections);*/

  UdmConvInit(&cnv, partcs, topcs, UDM_RECODE_HTML);
  UdmTextListJoinWithConversion(TopDoc, &Doc->TextList, &cnv);
  UdmTextListFree(&Doc->TextList);

  return UDM_OK;
}


#define MAX_LEVEL_PREFIX 5
static const char *level_prefix[MAX_LEVEL_PREFIX]=
{
  "", "> ", ">> ", ">>> ", ">>>> "
};


static void
add_header_to_textlist(UDM_DOCUMENT *Doc,
                       const UDM_CONST_STR *name,
                       const UDM_CONST_STR *value,
                       udm_bool_t is_top_doc)
{
  const UDM_VAR *Sec;
  char header_name[128];
  udm_snprintf(header_name, sizeof(header_name), "%smsg.%.*s",
               is_top_doc ? "" : "part.", (int) name->length, name->str);
  if ((Sec= UdmVarListFind(&Doc->Sections, header_name)))
  {
    UDM_TEXTITEM Item;
    bzero(&Item, sizeof(Item));
    Item.str= UdmConstStrDup(value);
    Item.section= Sec->section;
    Item.section_name= Sec->name;
    Item.flags= UDM_TEXTLIST_FLAG_RFC1522;
    UdmTextListAdd(&Doc->TextList, &Item);
    UdmFree(Item.str);
  }
}


static int header_handler(UDM_MESSAGE_PARSER *p,
                          const UDM_CONST_STR *name,
                          const UDM_CONST_STR *value_arg)
{
  UDM_MESSAGE_DATA *data= (UDM_MESSAGE_DATA *) p->data;
  UDM_DOCUMENT *Doc= data->D;
  UDM_DOCUMENT *TopDoc= UdmTopMessageData(data)->D;
  UDM_CONST_STR value_buf= *value_arg, *value= &value_buf;

  /* Skip leading space-alike characters */
  UdmConstStrTrim(value, " \t\r\n");
  UdmLog(data->A, UDM_LOG_DEBUG, "%s%.*s: %.*s", 
         level_prefix[data->level < MAX_LEVEL_PREFIX ? data->level : MAX_LEVEL_PREFIX - 1],
         (int) name->length, name->str,
         (int) value->length, value->str);
  if (!UdmConstStrNCaseCmp(name, UDM_CSTR_WITH_LEN("Content-Type")))
  {
    static UDM_CONST_STR ctname= {UDM_CSTR_WITH_LEN("Content-Type")};
    static UDM_CONST_STR csname= {UDM_CSTR_WITH_LEN("Charset")};
    UDM_CONST_STR val;
    char buf[128], *semicolon, *charset_found= NULL;
    udm_bool_t is_multipart;
    udm_snprintf(buf, sizeof(buf), "%.*s", (int) value->length, value->str);
    is_multipart= !strncasecmp(buf, "multipart/", 10);
    /*
      Content-Type: application/msword; name="test.doc"
      Content-Type: text/html; charset=KOI8-R
      Content-Type: text/plain; charset="koi8-r"
      Content-Type: text/html; charset=WINDOWS-1251; name="test.html"
    */
    if ((semicolon= strchr(buf, ';')))
    {
      *semicolon++= '\0';
      if ((charset_found= strstr(semicolon, "charset=")) ||
          (charset_found= strstr(semicolon, "CHARSET=")))
      {
        const char *cs;
        char *end;
        charset_found+= 8;
        if ((end= strchr(charset_found, ';')))
          *end= '\0';
        charset_found= UdmTrim(charset_found, "\"");
        if (!(cs= UdmCharsetCanonicalName(charset_found)))
          cs= charset_found;
        UdmVarListReplaceStr(&Doc->Sections, "Charset", cs);
        UdmConstStrSet(&val, cs, strlen(cs));
        add_header_to_textlist(Doc, &csname, &val,
                               (!is_multipart && data->Parent->D == TopDoc));
        if (!is_multipart && data->Parent->D == TopDoc)
        {
          UdmVarListReplaceStr(&TopDoc->Sections, "Charset", cs);
          UdmVarListReplaceStr(&TopDoc->Sections, "Server-Charset", cs);
        }
      }
    }
    UdmVarListReplaceStr(&Doc->Sections, "Content-Type", buf);
    UdmConstStrSet(&val, buf, strlen(buf));
    add_header_to_textlist(Doc, &ctname, &val, data->Parent->D == TopDoc);
    /*
      Set charset of a multipart message to "utf-8", to collect
      all parts without data loss and excessive space usage.
      Need "Server-Charset" to make UdmGuessCharSet() work correctly later.
      Need "Charset" to make UdmTextListJoinWithConversion() work correctly.
    */
    if (!charset_found && is_multipart && data->Parent->D == TopDoc)
    {
      UdmVarListReplaceStr(&TopDoc->Sections, "Server-Charset", "utf-8");
      UdmVarListReplaceStr(&TopDoc->Sections, "Charset", "utf-8");
    }
  }
  else
  {
    if (!UdmConstStrNCaseCmp(name,
                             UDM_CSTR_WITH_LEN("Content-Transfer-Encoding")))
    {
      char header_name[32];
      udm_snprintf(header_name, sizeof(header_name),
                   "%.*s", (int) name->length, name->str);
      UdmVarListReplaceStrn(&Doc->Sections, header_name,
                            value->str, value->length);
    }
    else if (!UdmConstStrNCaseCmp(name, UDM_CSTR_WITH_LEN("Date")))
    {
      char tmp[64];
      udm_snprintf(tmp, sizeof(tmp), "%.*s", (int) value->length, value->str);
      UdmVarListReplaceStr(&TopDoc->Sections, "Last-Modified", tmp);
    }
    /*
      Sections like msg.Subject will be collected only for the top level headers.
      Parts of a multi-part message will be concatenated to part.msg.Subject.
    */
    add_header_to_textlist(Doc, name, value, data->Parent->D == TopDoc);
  }
  return UDM_OK;
}


static int part_handler(UDM_MESSAGE_PARSER *multipart,
                        const UDM_CONST_STR *val)
{
  UDM_MESSAGE_PARSER p;
  UDM_MESSAGE_DATA *multipart_data= (UDM_MESSAGE_DATA *) multipart->data;
  UDM_DOCUMENT *TopDoc= UdmTopMessageData(multipart_data)->D;
  UDM_MESSAGE_DATA data;
  UDM_DOCUMENT Doc;
  int rc;

/*  printf("Found part: '%.*s'\n", val->length, val->str);*/

  UdmLog(multipart_data->A, UDM_LOG_DEBUG, "Part found, length=%d", (int) val->length);
  data.A= multipart_data->A;
  UdmDocInit(&Doc);
  Doc.Spider= TopDoc->Spider;
  UdmVarListReplaceLst(&Doc.Sections, &data.A->Conf->Sections, NULL, "*");
  data.D= &Doc;
  data.Parent= multipart_data;
  data.level= multipart_data->level + 1;

  UdmMessageParserInit(&p);
  UdmMessageParserSetUserData(&p, &data);
  UdmMessageParserSetEventHandler(&p, event_handler);
  UdmMessageParserSetHeaderHandler(&p, header_handler);
  UdmMessageParserSetPartHandler(&p, part_handler);
  UdmMessageParserSetContentHandler(&p, content_handler);
  if (UDM_OK != (rc= UdmMessageParserExec(&p, val->str, val->length)))
    UdmLog(data.A, UDM_LOG_WARN, "%s", p.errstr);
  
  UdmMessageParserFree(&p);

  UdmDocFree(&Doc);
  return rc;
}


/*
  Boundary is up to 70 characters.

  - Single part messages:
    text/plain
    text/html
    Types supported by parsers.

  - Multipart messages:
    multipart/mixed
    multipart/alternative
    multipart/digest
    multipart/related
    Multipart/report
    multipart/signed
    multipart/encrypted
    multipart/form-data
    multipart/x-mixed-replace

  - Multipart messages with messages.
*/
static int
UdmMessageRFC822ParseInternal(UDM_AGENT *A, UDM_DOCUMENT *TopDoc,
                              const UDM_CONST_STR *content)
{
  UDM_MESSAGE_PARSER p;
  UDM_MESSAGE_DATA topdata, data;
  UDM_DOCUMENT Doc;
  int rc;
  size_t size;

  topdata.A= A;
  topdata.D= TopDoc;
  topdata.Parent= NULL;
  topdata.level= 0;

  UdmDocInit(&Doc);
  Doc.Spider= TopDoc->Spider;
  UdmVarListReplaceLst(&Doc.Sections, &A->Conf->Sections, NULL, "*");
  Doc.Buf.buf= Doc.Buf.content= UdmMalloc((size= content->length) + 1);
  Doc.Buf.content_length= size;
  Doc.Buf.size= size;
  Doc.Buf.maxsize= size + 1;
  memcpy(Doc.Buf.buf, content->str, content->length);
  Doc.Buf.buf[size]= '\0';

  data.A= A;
  data.D= &Doc;
  data.Parent= &topdata;
  data.level= 0;

  UdmMessageParserInit(&p);
  UdmMessageParserSetUserData(&p, &data);
  UdmMessageParserSetEventHandler(&p, event_handler);
  UdmMessageParserSetHeaderHandler(&p, header_handler);
  UdmMessageParserSetPartHandler(&p, part_handler);
  UdmMessageParserSetContentHandler(&p, content_handler);
  if (UDM_OK != (rc= UdmMessageParserExec(&p, Doc.Buf.content, size)))
  {
    if (p.errstr[0])
    {
      UDM_CONST_STR name, value;
      UdmConstStrSet(&name, UDM_CSTR_WITH_LEN("parser.error"));
      UdmConstStrSet(&value, p.errstr, strlen(p.errstr));
      add_header_to_textlist(TopDoc, &name, &value, UDM_TRUE);
    }
    UdmLog(data.A, UDM_LOG_WARN, "%s", p.errstr);
  }

  UdmMessageParserFree(&p);
  UdmDocFree(&Doc);

  return rc;
}


int
UdmMessageRFC822Parse(UDM_AGENT *A, UDM_DOCUMENT *TopDoc)
{
  UDM_CONST_STR content;
  content.str= TopDoc->Buf.content;
  content.length= strlen(TopDoc->Buf.content); /* TODO34 */
  return UdmMessageRFC822ParseInternal(A, TopDoc, &content);
}



int *
UdmMessageRFC822ExcerptSource(UDM_AGENT *Agent,
                              UDM_RESULT *Res,
                              UDM_DOCUMENT *Doc,
                              UDM_CHARSET *bcs,
                              const UDM_CONST_STR *content,
                              size_t *length)
{
  UDM_VAR Sec;
  UDM_CHARSET *dcs;
  const char *dcsname;
  size_t i, dstmaxlen;
  int *dst;
  char body[]= "body";
  char value[]= "";
  UDM_CONV cnv;
  UDM_DSTR dstr;
  int hlstop= UdmVarListFindBool(&Agent->Conf->Vars, "ExcerptStopword", 1);
  const char *seg= UdmVarListFindStr(&Agent->Conf->Vars, "Segmenter", NULL);
  int segmenter= seg ? UdmUniSegmenterFind(Agent, NULL, seg) : 0;

  UdmDSTRInit(&dstr, 512);

  /* TODO: move to a better place */
  bzero((void*)&Sec, sizeof(Sec));
  Sec.name= body;
  Sec.section= 1;
  Sec.val= value;
  UdmVarListReplace(&Agent->Conf->Sections, &Sec);
  UdmVarListReplace(&Doc->Sections, &Sec);

  Doc->Spider.index= 1;
  UdmMessageRFC822ParseInternal(Agent, Doc, content);

  /* Collect body chunks from Doc->TextList into dstr */
  for (i= 0; i < Doc->TextList.nitems; i++)
  {
    UDM_TEXTITEM *Item= &Doc->TextList.Item[i];
    if (!strcmp(Item->section_name, "body"))
    {
      if (dstr.size_data)
        UdmDSTRAppend(&dstr, " ", 1);
      UdmDSTRAppend(&dstr, Item->str, strlen(Item->str));
    }
  }

  /* HlConvert to Unicode */
  dcsname= UdmVarListFindStr(&Doc->Sections, "Charset", "latin1");
  if (!(dcs= UdmGetCharSet(dcsname)))
    dcs= UdmGetCharSet("latin1");
  UdmConvInit(&cnv, dcs, &udm_charset_sys_int, UDM_RECODE_HTML_SPECIAL);
  dstmaxlen= UdmConvSizeNeeded(&cnv, dstr.size_data, UDM_RECODE_HTML_SPECIAL);
  if (!(dst= (int*) UdmMalloc(dstmaxlen)))
  {
    *length= 0;
    return NULL;
  }
  *length= UdmHlConvertExt(Agent, (char *) dst, dstmaxlen, &Res->WWList, bcs,
                           dstr.data, dstr.size_data,
                           dcs, &udm_charset_sys_int,
                           hlstop, segmenter) / sizeof(int);
  UdmDSTRFree(&dstr);
  return dst;
}


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


static void
UdmMessageRFC822CachedCopyAppendHeader(UDM_AGENT *Agent,
                                       UDM_DSTR *dstr,
                                       UDM_WIDEWORDLIST *WWList,
                                       UDM_TEXTITEM *Item,
                                       UDM_HIGHLIGHT_CONV *ec,
                                       int hlstop, int seg,
                                       const char *name,
                                       size_t namelen)
{
  size_t len, itemlen= strlen(Item->str), tmplen= itemlen + 1;
  UDM_CONV cnv;
  char *tmp= UdmMalloc(tmplen);
  UdmConvInit(&cnv, ec->uni_dst.to, ec->uni_dst.to, 0);
  len= UdmConvRFC1522(&cnv, tmp, tmplen, Item->str, itemlen);

  UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("<code><b>"));
  UdmDSTRAppend(dstr, name, namelen);
  UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN(":</b>&nbsp;"));
  UdmHlConvertDSTRWithConv(Agent, dstr, WWList, tmp, len, ec, hlstop, seg);
  UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("</code><br>"));
  UdmFree(tmp);
}


static void
UdmMessageRFC822EnableSection(UDM_AGENT *Agent, UDM_DOCUMENT *Doc,
                              const char *name, udm_secno_t secno)
{
  UDM_VAR Sec;
  char namebuf[128];
  char valbuf[]= "";
  udm_snprintf(namebuf, sizeof(namebuf), "%s", name);
  bzero((void*)&Sec, sizeof(Sec));
  Sec.name= namebuf;
  Sec.section= secno;
  Sec.val= valbuf;
  UdmVarListReplace(&Agent->Conf->Sections, &Sec);
  UdmVarListReplace(&Doc->Sections, &Sec);
}


typedef struct message_section_param_st
{
  UDM_CONST_STR  header_name;
  const char *section_name;
  udm_secno_t secno;
} UDM_MESSAGE_HEADER_PARAM;


static const UDM_MESSAGE_HEADER_PARAM message_header_param[]=
{
 {{UDM_CSTR_WITH_LEN("From")},               "msg.from",                    2},
 {{UDM_CSTR_WITH_LEN("To")},                 "msg.to",                      3},
 {{UDM_CSTR_WITH_LEN("Subject")},            "msg.subject",                 4},
 {{UDM_CSTR_WITH_LEN("Content-Type")},       "msg.content-type",            5},
 {{UDM_CSTR_WITH_LEN("Date")},               "msg.date",                    6},
 {{UDM_CSTR_WITH_LEN("Content-Type")},       "part.msg.content-type",       7},
 {{UDM_CSTR_WITH_LEN("Content-Disposition")},"part.msg.content-disposition",8},
 {{0,0},NULL,0}
};


static const UDM_MESSAGE_HEADER_PARAM *
UdmMessageHeaderParamFind(const char *section_name)
{
  const UDM_MESSAGE_HEADER_PARAM *p;
  for (p= message_header_param; p->section_name; p++)
  {
    if (!strcmp(p->section_name, section_name))
      return p;
  }
  return NULL;
}


static void
UdmMessageRFC822EnableSections(UDM_AGENT *Agent, UDM_DOCUMENT *Doc)
{
  const UDM_MESSAGE_HEADER_PARAM *p;
  UdmMessageRFC822EnableSection(Agent, Doc, "body", 1);
  UdmMessageRFC822EnableSection(Agent, Doc, "nobody", 1);
  for (p= message_header_param; p->section_name; p++)
  {
    UdmMessageRFC822EnableSection(Agent, Doc, p->section_name, p->secno);
  }
}


static int
UdmMessageRFC822EventToMarkup(UDM_DSTR *dstr, UDM_TEXTITEM *Item)
{
    /* Top level message events */
    if (!strncasecmp(Item->section_name, UDM_CSTR_WITH_LEN("msg.message_enter")))
    {
      UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("<div style=\"background-color:#EEEEEE;\">"));
      UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("<!-- Message Enter -->\n"));
      return UDM_OK;
    }
    if (!strncasecmp(Item->section_name, UDM_CSTR_WITH_LEN("msg.message_content")))
    {
      UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("</div>\n"));
      UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("<pre>"));
      UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("<!-- Message Content -->\n"));
      return UDM_OK;
    }
    if (!strncasecmp(Item->section_name, UDM_CSTR_WITH_LEN("msg.message_parts")))
    {
      UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("</pre>"));
      UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("<!-- Message Content End -->\n"));
      return UDM_OK;
    }
    if (!strncasecmp(Item->section_name, UDM_CSTR_WITH_LEN("msg.message_leave")))
    {
      UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("<!-- Message Leave -->\n"));
      return UDM_OK;
    }

    /* Part event */
    if (!strncasecmp(Item->section_name, UDM_CSTR_WITH_LEN("part.msg.message_enter")))
    {
      UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("<blockquote>"));
      UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("<!-- Part Enter -->\n"));
      UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("<div style=\"background-color:#EEEEEE;margin-top:1\">"));
      UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("<!-- Part Headers -->\n"));
      return UDM_OK;
    }
    if (!strncasecmp(Item->section_name, UDM_CSTR_WITH_LEN("part.msg.message_content")))
    {
      UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("</div>"));
      UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("<!-- Part Header End -->\n"));
      if (Item->flags != UDM_TEXTLIST_FLAG_MESSAGE_RFC822)
        UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("<pre>"));
      else
        UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("<blockquote>"));
      UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("<!-- Part Content -->\n"));
      return UDM_OK;
    }
    if (!strncasecmp(Item->section_name, UDM_CSTR_WITH_LEN("part.msg.message_parts")))
    {
      if (Item->flags != UDM_TEXTLIST_FLAG_MESSAGE_RFC822)
        UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("</pre>"));
      else
        UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("</blockquote>"));
      UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("<!-- Part Content end -->\n"));
      UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("<!-- Part Sub-parts -->\n"));
      return UDM_OK;
    }
    if (!strncasecmp(Item->section_name, UDM_CSTR_WITH_LEN("part.msg.message_leave")))
    {
      UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("<!-- Part Sub-parts End -->\n"));
      UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("</blockquote>"));
      UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("<!-- Part Leave -->\n"));
      return UDM_OK;
    }
  return UDM_ERROR;
}


int
UdmMessageRFC822CachedCopy(UDM_AGENT *Agent,    
                           UDM_RESULT *Res,
                           UDM_DOCUMENT *Doc,
                           UDM_DSTR *dstr)
{
  size_t i;
  int hlstop= UdmVarListFindBool(&Agent->Conf->Vars, "ExcerptStopword", 1);
  const char *seg= UdmVarListFindStr(&Agent->Conf->Vars, "Segmenter", NULL);
  int segmenter= seg ? UdmUniSegmenterFind(Agent, NULL, seg) : 0;
  UDM_CONST_STR content;
  UDM_CHARSET *topcs;
  UDM_HIGHLIGHT_CONV ec;

  UdmConstStrSet(&content, Doc->Buf.content, strlen(Doc->Buf.content));

  /* TODO: move to a better place */
  UdmMessageRFC822EnableSections(Agent, Doc);

  Doc->Spider.index= 1;
  UdmMessageRFC822ParseInternal(Agent, Doc, &content);
  topcs= UdmVarListFindCharset(&Doc->Sections, "Charset", UdmGetCharSet("latin1"));
  UdmExcerptConvInit(&ec, Agent->Conf->bcs, topcs, topcs);

  /* Collect body chunks from Doc->TextList into dstr */
  for (i= 0; i < Doc->TextList.nitems; i++)
  {
    UDM_TEXTITEM *Item= &Doc->TextList.Item[i];
    const UDM_MESSAGE_HEADER_PARAM *hp;

    if (Item->section == 255)
    {
      UdmMessageRFC822EventToMarkup(dstr, Item);
      continue;
    }
    if (!strcmp(Item->section_name, "body"))
    {
      UdmHlConvertDSTRWithConv(Agent, dstr,
                               &Res->WWList,
                               Item->str, strlen(Item->str),
                               &ec, hlstop, segmenter);
    }
    else if ((hp= UdmMessageHeaderParamFind(Item->section_name)))
    {
      UdmMessageRFC822CachedCopyAppendHeader(Agent, dstr, &Res->WWList, Item,
                                             &ec, hlstop, segmenter,
                                             hp->header_name.str,
                                             hp->header_name.length);
    }
    UdmDSTRAppend(dstr, UDM_CSTR_WITH_LEN("\n"));
  }
  return UDM_OK;
}
