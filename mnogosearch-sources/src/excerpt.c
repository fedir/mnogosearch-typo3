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
#include "udm_services.h"
#include "udm_xmalloc.h"
#include "udm_hash.h"
#include "udm_utils.h"
#include "udm_log.h"
#include "udm_vars.h"
#include "udm_parsehtml.h"
#include "udm_unicode.h"
#include "udm_unidata.h"
#include "udm_searchtool.h"
#include "udm_sgml.h"
#include "udm_sqldbms.h"
#include "udm_mutex.h"
#include "udm_doc.h"
#include "udm_db.h"
#include "udm_guesser.h"
#include "udm_proto.h"
#include "udm_http.h"


static int *
UdmGetExcerptSourceFromContent(UDM_AGENT *Agent,
                               UDM_RESULT *Res,
                               UDM_DOCUMENT *Doc,
                               UDM_CHARSET *bcs,
                               char *Source, size_t SourceLen,
                               size_t *length)
{
  UDM_HTMLTOK tag;
  const char *htok, *last;
  UDM_CHARSET *sys_int= &udm_charset_sys_int;
  UDM_CHARSET *dcs= UdmGetCharSet(UdmVarListFindStr(&Doc->Sections, "Charset",
                                  UdmVarListFindStr(&Doc->Sections, "Server-Charset", "iso-8859-1")));
  int hlstop= UdmVarListFindBool(&Agent->Conf->Vars, "ExcerptStopword", 1);
  const char *content_type= UdmVarListFindStr(&Doc->Sections, "Content-Type", NULL);
  const char *deftype=  UdmVarListFindStr(&Agent->Conf->Vars,"DefaultContentType",NULL);
  const char *seg= UdmVarListFindStr(&Agent->Conf->Vars, "Segmenter", NULL);
  int segmenter= seg ? UdmUniSegmenterFind(Agent, NULL, seg) : 0;
  udm_content_type_t ct;

  if (!content_type || !(ct= UdmContentTypeByName(content_type)))
  {
    content_type= UdmGuessContentType(Source, SourceLen, deftype);
    ct= UdmContentTypeByName(content_type);
  }

  switch (ct)
  {
    case UDM_CONTENT_TYPE_TEXT_XML:
    {
      /* Make sure sizeof(int) alignment */
      size_t dstmaxlen= ((SourceLen * 14 + 10) / sizeof(int)) * sizeof(int);
      char *dst= (char*) UdmMalloc(dstmaxlen);
      UDM_DSTR buf;
      UdmDSTRInit(&buf, 1024);
      UdmHTMLTOKInit(&tag);
      htok= UdmHTMLToken(Source, &last, &tag);
      do
      {
        if (tag.type == UDM_HTML_TXT &&
            !tag.script && !tag.comment && ! tag.style)
          UdmDSTRAppend(&buf, htok, last - htok);
        else
          UdmDSTRAppend(&buf, " ", 1);
      } while ((htok = UdmHTMLToken(NULL, &last, &tag)));
      *length= UdmHlConvertExt(Agent, dst, dstmaxlen, &Res->WWList, bcs,
                               buf.data, buf.size_data, dcs, sys_int,
                               hlstop, segmenter);
      *length /= sizeof(int);
      UdmDSTRFree(&buf);
      return (int*) dst;
    }

    case UDM_CONTENT_TYPE_TEXT_HTML:
    {
      /* Make sure sizeof(int) alignment */
      size_t dstmaxlen= ((SourceLen * 14 + 10) / sizeof(int)) * sizeof(int);
      char *dst= (char*) UdmMalloc(dstmaxlen);
      int i20= 0x20, i0= 0;
      size_t dstlen= 0;
      UDM_HIGHLIGHT_CONV ec;
      UdmHTMLTOKInit(&tag);
      UdmExcerptConvInit(&ec, bcs, dcs, &udm_charset_sys_int);
      for (htok= UdmHTMLToken(Source, &last, &tag);
           htok;
           htok = UdmHTMLToken(NULL, &last, &tag))
      {
        if (tag.type == UDM_HTML_TXT && tag.body &&
            !tag.script && !tag.comment && ! tag.style)
        {
          UDM_ASSERT(dstmaxlen > dstlen);
          dstlen+= UdmHlConvertExtWithConv(Agent,
                                           dst + dstlen, dstmaxlen - dstlen,
                                           &Res->WWList, htok, last - htok,
                                           &ec, hlstop, segmenter);
        }
        else
        {
          /* Put space character */
          memcpy((void*)(dst+dstlen), (void*) &i20, sizeof(i20));
          dstlen+= sizeof(i20);
        }
      } 
      /* Put 0 terminator */
      memcpy((void*)(dst+dstlen), (void*) &i0, sizeof(i0));
      *length= dstlen / sizeof(int);
      return (int*) dst;
    }

    case UDM_CONTENT_TYPE_MESSAGE_RFC822:
    {
      UDM_CONST_STR content;
      UdmConstStrSet(&content, Source, SourceLen);
      return UdmMessageRFC822ExcerptSource(Agent, Res, Doc, bcs, &content, length);
    }

    case UDM_CONTENT_TYPE_DOCX:
    {
      UDM_CONST_STR content;
      UdmConstStrSet(&content, Source, SourceLen);
      return UdmDOCXExcerptSource(Agent, Res, Doc, bcs, &content, length);
    }

    case UDM_CONTENT_TYPE_TEXT_RTF:
    {
      UDM_CONST_STR content;
      UdmConstStrSet(&content, Source, SourceLen);
      return UdmRTFExcerptSource(Agent, Res, Doc, bcs, &content, length);
    }

    case UDM_CONTENT_TYPE_TEXT_PLAIN:
    {
      /* Make sure sizeof(int) alignment */
      size_t dstmaxlen= ((SourceLen * 14 + 10) / sizeof(int)) * sizeof(int);
      char *dst= (char*) UdmMalloc(dstmaxlen);
      *length= UdmHlConvertExt(Agent, dst, dstmaxlen, &Res->WWList, bcs,
                               Source, SourceLen, dcs, sys_int,
                               hlstop, 0);
      *length /= sizeof(int);
      return (int*) dst;
    }

    case UDM_CONTENT_TYPE_HTDB:
    case UDM_CONTENT_TYPE_AUDIO_MPEG:
    case UDM_CONTENT_TYPE_UNKNOWN:
      break;
  }
  return NULL;
}


static int
*UdmGetExcerptSourceCachedCopy(UDM_AGENT *Agent,
                               UDM_RESULT *Res,
                               UDM_DOCUMENT *Doc,
                               UDM_CHARSET *bcs,
                               size_t *length)
{
#ifdef HAVE_ZLIB
  size_t l;
  char *Source, *in_buf, *dst;
  z_stream zstream;
  UDM_CHARSET *dcs= UdmGetCharSet(UdmVarListFindStr(&Doc->Sections, "Charset", "iso-8859-1"));
  const char *CachedCopy= UdmVarListFindStr(&Doc->Sections, "CachedCopy", NULL);
#ifdef EXTRA_DEBUG
  udm_timer_t ticks UdmStartTimer();
#endif
  
  if (!CachedCopy || !dcs || !(Source = UdmMalloc(UDM_MAXDOCSIZE)))
    return NULL;

  l= strlen(CachedCopy);
  if (!(in_buf= UdmMalloc(l)))
  {
    UdmFree(Source);
    return(NULL);
  }
  zstream.next_in= (Byte *)in_buf;
  zstream.avail_in= udm_base64_decode((char *)zstream.next_in, CachedCopy, l);
  zstream.next_out= (Byte *)Source;
  zstream.avail_out= UDM_MAXDOCSIZE - 1;
  zstream.zalloc= Z_NULL;
  zstream.zfree= Z_NULL;
  zstream.opaque= Z_NULL;
  
  if (inflateInit2(&zstream, 15) != Z_OK)
  {
    UdmFree(Source);
    UdmFree(in_buf);
    return(NULL);
  }
  
  inflate(&zstream, Z_FINISH);
  inflateEnd(&zstream);
  Source[zstream.total_out] = 0;
  UdmFree(in_buf);

#ifdef EXTRA_DEBUG
  UdmLog(Agent, UDM_LOG_DEBUG, "Inflate: %.5f", UdmStopTimer(&ticks));
#endif

#ifdef EXTRA_DEBUG
  ticks= UdmStartTimer();
#endif



  dst= (char*) UdmGetExcerptSourceFromContent(Agent, Res, Doc, bcs,
                                              Source, zstream.total_out,
                                              length);
  
  UdmFree(Source);

#ifdef EXTRA_DEBUG
  UdmLog(Agent, UDM_LOG_DEBUG, "Convert: %.5f", UdmStopTimer(&ticks));
#endif
  
  return (int*) dst;

#else
  return NULL;
#endif
}

static int
*UdmGetExcerptSourceBody(UDM_AGENT *Agent,
                         UDM_RESULT *Res,
                         UDM_DOCUMENT *Doc,
                         UDM_CHARSET *bcs,
                         size_t *length)
{
  int *res;
  size_t l;
  const char *Source= UdmVarListFindStr(&Doc->Sections, "body", NULL);
  UDM_CHARSET *sys_int= &udm_charset_sys_int;

#ifndef NO_ADVANCE_CONV
  size_t  ul, charlen;
  UDM_CONV conv;
  
  if (!Source || !sys_int || !bcs)
    return NULL;
  
  l= strlen(Source);
  if (!(res = UdmMalloc(sizeof(int) * (l + 1))))
    return NULL;
  
  UdmConvInit(&conv, bcs, sys_int, UDM_RECODE_HTML);
  if ((ul= UdmConv(&conv, (char *)res, sizeof(int) * (l + 1), Source, l)) < 0)
  {
    UdmFree(res);
    return(NULL);
  }
  charlen= ul / sizeof(int);
  res[charlen]= 0;
  *length= charlen;
#else
  {
    int hlstop= UdmVarListFindBool(&Agent->Conf->Vars, "ExcerptStopword", 1);
  
    if (!Source || !(l= strlen(Source)))
      return NULL;
  
    res= (int*) UdmHlConvertExt(&Res->WWList, Agent->Conf->lcs,
                                Source, l, Agent->Conf->lcs, sys_int, hlstop);
  }
#endif
  return res;
}

/*
 Space characters:
 
 0009 = 0000.0000.0000.1001
 000A = 0000.0000.0000.1010
 000D = 0000.0000.0000.1101
 0020 = 0000.0000.0010.0000
 00A0 = 0000.0000.1010.0000
 
 2000 = 0010.0000.0000.0000
 2001 = 0010.0000.0000.0001
 2002 = 0010.0000.0000.0010
 2003 = 0010.0000.0000.0011
 2004 = 0010.0000.0000.0100
 2005 = 0010.0000.0000.0101
 2006 = 0010.0000.0000.0110
 2007 = 0010.0000.0000.0111
 2008 = 0010.0000.0000.1000
 2009 = 0010.0000.0000.1001
 200A = 0010.0000.0000.1010
 200B = 0010.0000.0000.1011
 202F = 0010.0000.0010.1111
 3000 = 0011.0000.0000.0000
 ----   -------------------
 CF50 - 11??.1111.?1?1.???? - not space characters

  if (X & 0xCF50)
  {
    then X is not a space character
  }
*/

__C_LINK char * __UDMCALL
UdmExcerptDoc (UDM_AGENT *Agent,
               UDM_RESULT *Res, UDM_DOCUMENT *Doc,
               size_t ExcerptSize, size_t ExcerptPadding)
{
  char *res;
  size_t ul,l, i, j, left, right;
  size_t prev_right= 0;
  int *Source, prev_is_letter;
  UDM_CONV conv;
  UDM_CHARSET *sys_int= &udm_charset_sys_int;
  UDM_CHARSET *bcs= Agent->Conf->bcs;
  UDM_DSTR buf;
  int dots[]= { 0x2E, 0x2E, 0x2E }; /* "..." */
#ifdef EXTRA_DEBUG
  unsigned long ticks;
#endif
  
  if (!sys_int || !bcs)
    return NULL;

#ifdef EXTRA_DEBUG
  ticks= UdmStartTimer();
#endif

  if (UdmVarListFindBool(&Agent->Conf->Vars, "UseLocalCachedCopy", 0))
  {
    const char *url= UdmVarListFindStr(&Doc->Sections, "url", NULL);
    int rc= url ? UdmGetURLSimple(Agent, Doc, url) : UDM_ERROR;
    size_t csize= Doc->Buf.size - (Doc->Buf.content-Doc->Buf.buf);
    
    if (0)
    {
      const char *cl= UdmVarListFindStr(&Doc->Sections, "Content-Length", "?");
      const char *ct= UdmVarListFindStr(&Doc->Sections, "Content-Type", "?");
      const char *ce= UdmVarListFindStr(&Doc->Sections, "Content-Encoding", "?");
      fprintf(stderr, "rc=%d url=%s size=%d cl=%s ce=%s ct=%s b.ct=%p b.cl=%d csize=%d\n",
              rc, url, (int) Doc->Buf.size, cl, ce, ct, Doc->Buf.content,
              (int) Doc->Buf.content_length, (int) csize);
    }
    
    Source= UdmGetExcerptSourceFromContent(Agent, Res, Doc, bcs,
                                           Doc->Buf.content, csize, &ul);
  }
  else
    Source= NULL;

  if (!Source &&
      !(Source= UdmGetExcerptSourceCachedCopy(Agent, Res, Doc, bcs, &ul)) &&
      !(Source= UdmGetExcerptSourceBody(Agent, Res, Doc, bcs, &ul)))
    return(NULL);

  /* Strip whitespaces */
  for (prev_is_letter= 0, i= 0, j= 0; i < ul; i++)
  {
    if ((Source[i] & 0xCF50) || !UdmUniIsSpace(Source[i]))
    {
      Source[j++]= Source[i];
      prev_is_letter= 1;
    }
    else if (prev_is_letter)
    {
      Source[j++]= 0x20;
      prev_is_letter= 0;
    }
  }
  /* Remove trailing space if exists */
  if (j && UdmUniIsSpace(Source[j-1]))
    j--;
  Source[j] = 0;
  ul = j;

#ifdef EXTRA_DEBUG
  UdmLog(Agent, UDM_LOG_ERROR, "GetSour: %.5f ul=%d", UdmStopTimer(&ticks), ul);
#endif  

#ifdef EXTRA_DEBUG
  ticks= UdmStartTimer();
#endif

  /* Get excerpt */
  UdmDSTRInit(&buf, 1024);
  for (i= 0; i < ul; i++)
  {
    if (Source[i] == 2) /* Beginning of a highlighted area found */
    {
      for (j= i + 1; j < ul; j++) /* Find the end of highighted area */
        if (Source[j] == 3)
          break;
      
      /*
        Move towards the beginning of the document
        and find where to break excerpt:
        - either on the closest space character
        - or put all "ExcerptPadding" characters if no space found.
          This is very likely in case of Asian texts with no spaces.
      */
      left= ExcerptPadding < i ? i - ExcerptPadding : 0;
      if (left < prev_right)
      {
        left= prev_right;
      }
      if (left)
      {
        size_t left0= left;
        for ( ; Source[left] != 0x20; left++)
        {
          if (left >= i) /* no space found */
          {
            left= left0;
            break;
          }
        }
      }
      
      /*
        Now move towards the end of the document and
        find the right excerpt border:
        - either on the closest space character
        - or after ExcerptSize characters
      */
      right= ExcerptPadding + j;
      if (right >= ul)
      {
        right= ul - 1;
      }
      else
      {
        size_t right0= right;
        for ( ; Source[right] != 0x20 ;  right--)
        {
          if (right <= j) /* No space found */
          {
            right= right0;
            /*
              Do not break excerpt in the middle of a highlighted word.
            */
            for ( ; j <  right; j++)
            {
              if (Source[j] == 0x02)
              {
                right= j - 1;
                break;
              }
            }
            break;
          }
        }
      }
      
      /*
        Check if there is a space for the new excerpt.
        We must stay under ExcerptSize limit.
      */
      if (ExcerptSize < buf.size_data / sizeof(int) + right - left + 1)
        break;
      if (left != prev_right)
        UdmDSTRAppend(&buf, (char *)dots, sizeof(dots));
      UdmDSTRAppend(&buf, (char *)&Source[left], (right - left + 1) * sizeof(int));
      i= right;

       /*
         On the next step we should not add dots if the new highlighted word
         immediately follows this word: <02>W1<03><02><W2>
         This is possible in case of "Segmenter cjk".
       */
      if (Source[right] == 0x03)
        right++;
      prev_right= right;
    }
  }
  if (!buf.size_data)
  {
    ul= ExcerptSize > ul ? ul : ExcerptSize;
    UdmDSTRAppend(&buf, (char *)Source, ul * sizeof(int));
  }
  UdmFree(Source);
  
  ul= buf.size_data / sizeof(int) * 20;
  if (!ul || !(res= UdmMalloc(ul))) 
  {
    UdmDSTRFree(&buf);
    return NULL;
  }

  /* Convert Unicode to BrowserCharset */
  UdmConvInit(&conv, sys_int, bcs, UDM_RECODE_HTML);
  l= UdmConv(&conv, res, ul, buf.data, buf.size_data);
  UdmDSTRFree(&buf);
  res[l]= 0;

#ifdef EXTRA_DEBUG
  UdmLog(Agent, UDM_LOG_ERROR, "Excerpt: %.5f l=%d", UdmStopTimer(&ticks), l);
#endif  

  return res;
}
