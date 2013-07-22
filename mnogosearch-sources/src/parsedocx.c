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
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "udm_common.h"
#include "udm_parsexml.h"
#include "udm_parsehtml.h"
#include "udm_vars.h"
#include "udm_textlist.h"
#include "udm_log.h"
#include "udm_searchtool.h"
#include "udm_http.h"
/*
#include "udm_utils.h"
#include "udm_url.h"
#include "udm_match.h"
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
#include "udm_mutex.h"
*/
#include <zlib.h>

/*
#include <udmsearch.h>
*/

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

static size_t
UdmInflate2(char *dst, size_t dstlen, const char *src, size_t srclen)
{
  z_stream z;
  z.zalloc= Z_NULL;
  z.zfree= Z_NULL;
  z.opaque= Z_NULL;
  z.next_in= (Byte *) src;
  z.next_out= (Byte *) dst;
  z.avail_in= srclen;
  z.avail_out= dstlen;

  if (inflateInit2(&z, -MAX_WBITS) != Z_OK)
    return 0;
  
  inflate(&z, Z_FINISH);
  inflateEnd(&z);
  return z.total_out;
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif


static int
get2le(const unsigned char *p)
{
  return p[0] +  (((int) p[1]) << 8);
}


static int
get4le(const unsigned char *p)
{
  return
    p[0] +  (((int) p[1]) << 8) +
    (((int) p[2]) << 16) + (((int) p[3]) << 24);
}


typedef struct
{
  int number_of_this_disk;
  int disk_where_dir_starts;
  int number_of_dir_records_on_this_disk;
  int total_number_of_dir_records;
  int dir_size;
  int dir_offset;
  int comment_length;
} UDM_ZIP_DIR_END;


typedef struct
{
  int signature;
  int version_made_by;
  int version_to_extract;
  int general_purpose_bit_flag;
  int compression_method;
  int last_mod_time;
  int last_mod_date;
  int crc32;
  int compressed_size;
  int uncompressed_size;
  int file_name_length;
  int extra_field_length;
  int file_comment_length;
  int disk_where_file_starts;
  int internal_file_attributes;
  int external_file_attributes;
  int relative_header_offset;
  const char *file_name;
} UDM_ZIP_DIR_ENTRY;


typedef struct
{
  int signature;          /*0 4 Local file header signature = 0x04034b50 */
  int version_to_extract; /*  4  2  Version needed to extract (minimum)  */
  int general_purpose_bit_flag; /* 6  2  General purpose bit flag */
  int compression_method; /* 8  2  Compression method */
  int last_mod_time;      /*10  2  File last modification time */
  int last_mod_date;      /*12  2  File last modification date */
  int crc32;              /*14  4  CRC-32 */
  int compressed_size;    /*18  4  Compressed size */
  int uncompressed_size;  /*22  4  Uncompressed size */
  int file_name_length;   /*26  2  File name length (n) */
  int extra_field_length; /*28  2  Extra field length (m) */
  const char *file_name;     /*30  n  File name */
  const char *extra_field;   /*30+n  m  Extra field */
  const char *data;          /*30+n+m compressed data */
} UDM_ZIP_FILE_HEADER;


static int
udm_zip_get_file_header(UDM_ZIP_FILE_HEADER *h, const char *s)
{
  const unsigned char *u= (const unsigned char *) s;
  h->signature= get4le(u + 0);
  h->version_to_extract= get2le(u + 4);
  h->general_purpose_bit_flag= get2le(u + 6);
  h->compression_method= get2le(u + 8);
  h->last_mod_time= get2le(u + 10);
  h->last_mod_date= get2le(u + 12);
  h->crc32= get4le(u + 14);
  h->compressed_size= get4le(u + 18);
  h->uncompressed_size= get4le(u + 22);
  h->file_name_length= get2le(u + 26);
  h->extra_field_length= get2le(u + 28);
  h->file_name= (const char *) u + 30;
  h->file_name= (const char *) u + 30 + h->file_name_length;
  h->data= (const char *) u + 30 + h->file_name_length + h->extra_field_length;
  return 0;
}


static int
udm_zip_get_dir_entry(UDM_ZIP_DIR_ENTRY *de, const char *s)
{
  const unsigned char *u= (const unsigned char *) s;
  de->signature= get4le(u + 0);
  de->version_made_by= get2le(u + 4);
  de->version_to_extract= get2le(u + 6);
  de->general_purpose_bit_flag= get2le(u + 8);
  de->compression_method= get2le(u + 10);
  de->last_mod_time= get2le(u + 12);
  de->last_mod_date= get2le(u + 14);
  de->crc32= get4le(u + 16);
  de->compressed_size= get4le(u + 20);
  de->uncompressed_size= get4le(u + 24);
  de->file_name_length= get2le(u + 28);
  de->extra_field_length= get2le(u + 30);
  de->file_comment_length= get2le(u + 32);
  de->disk_where_file_starts= get2le(u + 34);
  de->internal_file_attributes= get2le(u + 36);
  de->external_file_attributes= get4le(u + 38);
  de->relative_header_offset= get4le(u + 42);
  de->file_name= (const char *) u + 46;
  return 0;
}


static int
udm_zip_dir_entry_length(UDM_ZIP_DIR_ENTRY *de)
{
  return 46 +
    de->file_name_length +
    de->extra_field_length +
    de->file_comment_length;
}


static int
udm_zip_get_dir_end(UDM_ZIP_DIR_END *zde, const char *buf, size_t len)
{
  const char *s= buf + len - 22;
  if (len < 22)
    return UDM_ERROR;
  for ( ; s > buf; s--)
  {
    if (!memcmp(s, "PK\x05\x06", 4))
    {
      const unsigned char *u= (const unsigned char*) s;
      zde->number_of_this_disk= get4le(u + 4);
      zde->disk_where_dir_starts= get2le(u + 6);
      zde->number_of_dir_records_on_this_disk= get2le(u + 8);
      zde->total_number_of_dir_records= get2le(u + 10);
      zde->dir_size= get4le(u + 12);
      zde->dir_offset= get4le(u + 16);
      zde->comment_length= get2le(u + 20);
      /*
      printf("Found recs=%d/%d ofs=%d size=%d\n",
             zde->number_of_dir_records_on_this_disk,
             zde->total_number_of_dir_records,
             zde->dir_offset,
             zde->dir_size);
      */
      return UDM_OK;
    }
  }
  return UDM_ERROR;
}


typedef struct docx_font_st
{
  udm_bool_t u; /* font: underlined */
  udm_bool_t b; /* font: bold       */
  udm_bool_t i; /* font: italic     */
  udm_bool_t preserve_space;
  int size;
} DOCX_FONT_PR;


static void
UdmDOCXFontPrInit(DOCX_FONT_PR *pr)
{
  bzero(pr, sizeof(*pr));
}

typedef struct docx_para_prop_st
{
  int list_level;
  int span_count;
} DOCX_PARA_PR;


static void
UdmDOCXParaPrInit(DOCX_PARA_PR *pr)
{
  bzero(pr, sizeof(*pr));
}


typedef struct docx_parser_data_st
{
  UDM_AGENT *A;
  UDM_DOCUMENT *D;
  int body_sec;
  int title_sec;
  DOCX_FONT_PR font;
  DOCX_PARA_PR para;
} DOCX_PARSER_DATA;


static udm_bool_t
is_suffix(const char *s1, size_t length1,
          const char *s2, size_t length2)
{
  if (length1 < length2)
    return UDM_FALSE;
  return memcmp(s1 + length1 - length2, s2, length2) == 0;
}


static udm_bool_t
str_to_bool(const char *str, size_t length)
{
  if (udm_strnncasecmp(str, length, UDM_CSTR_WITH_LEN("false")))
    return UDM_TRUE;
  return UDM_FALSE;
}


static void
UdmTextListAddMarkup(UDM_TEXTLIST *TextList, const char *src, size_t length)
{
  UDM_CONST_TEXTITEM Item;
  bzero(&Item, sizeof(Item));
  UdmConstStrSet(&Item.section_name, UDM_CSTR_WITH_LEN("markup"));
  UdmConstStrSet(&Item.text, src, length);
  Item.section= 255;
  UdmTextListAddConst(TextList, &Item);
}


static void
UdmDOCXAddMarkupStartList(DOCX_PARSER_DATA *data)
{
  int i;
  for (i= 0; i < data->para.list_level; i++)
    UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("<ul>"));
  UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("\n<li>\n"));
}


static void
UdmDOCXAddMarkupEndList(DOCX_PARSER_DATA *data)
{
  int i;
  UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("\n</li>\n"));
  for (i= 0; i < data->para.list_level; i++)
    UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("</ul>"));
  UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("\n"));
}


static int
startElement(UDM_XML_PARSER *parser, const char *src, size_t length)
{
  DOCX_PARSER_DATA *data= parser->user_data;
  if (!udm_strnncasecmp(src, length, UDM_CSTR_WITH_LEN("w:document.w:body.w:p")))
  {
    UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("\n<p>\n"));
    UdmDOCXParaPrInit(&data->para);
    return UDM_OK;
  }
  if (is_suffix(src, length, UDM_CSTR_WITH_LEN("w:p.w:r")))
  {
    if (data->para.list_level && !data->para.span_count)
      UdmDOCXAddMarkupStartList(data);
    data->para.span_count++;
    return UDM_OK;
  }
  if (is_suffix(src, length, UDM_CSTR_WITH_LEN("w:r.w:rPr.w:u")))
  {
    data->font.u= 1;
    return UDM_OK;
  }
  if (is_suffix(src, length, UDM_CSTR_WITH_LEN("w:r.w:rPr.w:i")))
  {
    data->font.i= 1;
    return UDM_OK;
  }
  if (is_suffix(src, length, UDM_CSTR_WITH_LEN("w:r.w:rPr.w:b")))
  {
    data->font.b= 1;
    return UDM_OK;
  }
  if (is_suffix(src, length, UDM_CSTR_WITH_LEN("w:tbl")))
  {
    UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("\n<table border=\"1\">\n"));
    return UDM_OK;
  }
  if (is_suffix(src, length, UDM_CSTR_WITH_LEN("w:tr")))
  {
    UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("<tr>\n"));
    return UDM_OK;
  }
  if (is_suffix(src, length, UDM_CSTR_WITH_LEN("w:tc")))
  {
    UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("<td>\n"));
    return UDM_OK;
  }
  return UDM_OK;
}


static int
endElement(UDM_XML_PARSER *parser, const char *src, size_t length)
{
  DOCX_PARSER_DATA *data= parser->user_data;
  if (!udm_strnncasecmp(src, length, UDM_CSTR_WITH_LEN("w:document.w:body.w:p")))
  {
    /*UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("\n</p>\n"));*/
    if (data->para.list_level)
      UdmDOCXAddMarkupEndList(data);
    UdmDOCXParaPrInit(&data->para);
    return UDM_OK;
  }
  if (is_suffix(src, length, UDM_CSTR_WITH_LEN("w:r")))
  {
    UdmDOCXFontPrInit(&data->font);
    return UDM_OK;
  }
  if (is_suffix(src, length, UDM_CSTR_WITH_LEN("w:t")))
  {
    data->font.preserve_space= 0;
    return UDM_OK;
  }
  if (is_suffix(src, length, UDM_CSTR_WITH_LEN("w:tbl")))
  {
    UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("\n</table>\n"));
    return UDM_OK;
  }
  if (is_suffix(src, length, UDM_CSTR_WITH_LEN("w:tr")))
  {
    UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("</tr>\n"));
    return UDM_OK;
  }
  if (is_suffix(src, length, UDM_CSTR_WITH_LEN("w:tc")))
  {
    if (data->D->TextList.nitems &&
        data->D->TextList.Item[data->D->TextList.nitems-1].str &&
        !strcmp(data->D->TextList.Item[data->D->TextList.nitems-1].str, "<td>\n"))
    {
      /* Empty cell: add &nbsp; to enforce borders */
      UDM_CONST_TEXTITEM Item;
      bzero(&Item, sizeof(Item));
      UdmConstStrSet(&Item.section_name, UDM_CSTR_WITH_LEN("body"));
      UdmConstStrSet(&Item.text, UDM_CSTR_WITH_LEN("&nbsp;"));
      Item.section= data->body_sec;
      UdmTextListAddConst(&data->D->TextList, &Item);
    }
    UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("</td>\n"));
    return UDM_OK;
  }
  return UDM_OK;
}


static int
Text(UDM_XML_PARSER *parser, const char *src, size_t length)
{
  DOCX_PARSER_DATA *data= parser->user_data;
  UDM_CONST_TEXTITEM Item;
  size_t attrlen= strlen(parser->attr);

  /*  
  if (!strncasecmp(parser->attr, UDM_CSTR_WITH_LEN("xml")) ||
      !strncasecmp(parser->attr, UDM_CSTR_WITH_LEN("w:document.w:body.w:p.w:pPr.w:pStyle"))  ||
      !strncasecmp(parser->attr, UDM_CSTR_WITH_LEN("w:document.w:body.w:p.w:r.w:fldChar"))  ||
      !strncasecmp(parser->attr, UDM_CSTR_WITH_LEN("w:document.w:body.w:p.w:r.w:instrText"))  ||
      !strncasecmp(parser->attr, UDM_CSTR_WITH_LEN("w:document.w:body.w:p.w:pPr.w:spacing"))  ||
      !strncasecmp(parser->attr, UDM_CSTR_WITH_LEN("w:document.w:body.w:p.w:pPr.w:contextualSpacing"))  ||
      !strncasecmp(parser->attr, UDM_CSTR_WITH_LEN("w:document.w:body.w:p.w:pPr.w:numPr"))  ||
      !strncasecmp(parser->attr, UDM_CSTR_WITH_LEN("w:document.w:body.w:p.w:bookmarkStart"))  ||
      !strncasecmp(parser->attr, UDM_CSTR_WITH_LEN("w:document.w:body.w:p.w:bookmarkEnd"))  ||
      !strncasecmp(parser->attr, UDM_CSTR_WITH_LEN("w:document.w:body.w:p.w:pPr.w:sectPr"))  ||
      !strncasecmp(parser->attr, UDM_CSTR_WITH_LEN("w:document.w:body.w:p.w:r.w:t.xml:space"))  ||
      !strncasecmp(parser->attr, UDM_CSTR_WITH_LEN("w:document.w:body.w:sectPr"))  ||
      !strncasecmp(parser->attr, UDM_CSTR_WITH_LEN("w:document.w:body.w:p.w:hyperlink"))  ||
      !strncasecmp(parser->attr, UDM_CSTR_WITH_LEN("w:document.xmlns")))
    return UDM_OK;
  */
  /*printf("ATTR='%s'='%.*s'\n", parser->attr, (int) length, src);*/
  if (is_suffix(parser->attr, attrlen, UDM_CSTR_WITH_LEN("w:t.xml:space")))
  {
    if (length == 8 && !strncasecmp(src, "preserve", 8))
      data->font.preserve_space= 1;
    return UDM_OK;
  }
  if (is_suffix(parser->attr, attrlen, UDM_CSTR_WITH_LEN("w:p.w:pPr.w:numPr.w:ilvl.w:val")))
  {
    char tmp[20];
    udm_snprintf(tmp, sizeof(tmp), "%.*s", (int) length, src);
    data->para.list_level= atoi(tmp) + 1;
    return UDM_OK;
  }
  if (is_suffix(parser->attr, attrlen, UDM_CSTR_WITH_LEN("w:r.w:rPr.w:sz.w:val")))
  {
    char tmp[20];
    udm_snprintf(tmp, sizeof(tmp), "%.*s", (int) length, src);
    data->font.size= atoi(tmp);
    return UDM_OK;
  }
  if (is_suffix(parser->attr, attrlen, UDM_CSTR_WITH_LEN("w:r.w:rPr.w:u.w:val")))
  {
    data->font.u= str_to_bool(src, length);
    return UDM_OK;
  }
  if (is_suffix(parser->attr, attrlen, UDM_CSTR_WITH_LEN("w:r.w:rPr.w:i.w:val")))
  {
    data->font.i= str_to_bool(src, length);
    return UDM_OK;
  }
  if (is_suffix(parser->attr, attrlen, UDM_CSTR_WITH_LEN("w:r.w:rPr.w:b.w:val")))
  {
    data->font.b= str_to_bool(src, length);
    return UDM_OK;
  }

  /* w:document.w:body.w:p.w:r.w:t */
  if (!is_suffix(parser->attr, attrlen, UDM_CSTR_WITH_LEN(".w:t")))
    return UDM_OK;
  if (data->font.b)
    UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("<b>"));
  if (data->font.i)
    UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("<i>"));
  if (data->font.u)
    UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("<u>"));
  if (data->font.size)
  {
    char tmp[64];
    size_t tmplen= udm_snprintf(tmp, sizeof(tmp), "<span style=\"font-size:%d\">", data->font.size / 2);
    UdmTextListAddMarkup(&data->D->TextList, tmp, tmplen);
  }
  if (data->font.preserve_space)
    UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("<span style=\"white-space:pre-wrap\">"));
  bzero(&Item, sizeof(Item));
  UdmConstStrSet(&Item.section_name, UDM_CSTR_WITH_LEN("body"));
  UdmConstStrSet(&Item.text, src, length);
  Item.section= data->body_sec;
  UdmTextListAddConst(&data->D->TextList, &Item);
  if (data->font.preserve_space)
    UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("</span>"));
  if (data->font.size)
    UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("</span>"));
  if (data->font.u)
    UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("</u>"));
  if (data->font.i)
    UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("</i>"));
  if (data->font.b)
    UdmTextListAddMarkup(&data->D->TextList, UDM_CSTR_WITH_LEN("</b>"));
  /*printf("TEXT: %.*s\n", (int) length, src);*/
  return UDM_OK;
}


/*
  Process word/document.xml
*/
static UDM_XML_HANDLER docx_document_handler=
{
  startElement,
  Text,
  endElement
};


static int
core_startElement(UDM_XML_PARSER *parser, const char *src, size_t length)
{
  return UDM_OK;
}


static int
core_Text(UDM_XML_PARSER *parser, const char *src, size_t length)
{
  DOCX_PARSER_DATA *data= parser->user_data;
  size_t attrlen= strlen(parser->attr);
  /*printf("%s=%.*s\n", parser->attr, (int) length, src);*/
  if (!udm_strnncasecmp(parser->attr, attrlen, UDM_CSTR_WITH_LEN("cp:coreProperties.dc:title")))
  {
    UDM_CONST_TEXTITEM Item;
    bzero(&Item, sizeof(Item));
    UdmConstStrSet(&Item.section_name, UDM_CSTR_WITH_LEN("title"));
    UdmConstStrSet(&Item.text, src, length);
    Item.section= data->title_sec;
    UdmTextListAddConst(&data->D->TextList, &Item);
    /*printf("HERE sec=%d\n", data->title_sec);*/
  }
  else if (!udm_strnncasecmp(parser->attr, attrlen, UDM_CSTR_WITH_LEN("cp:coreProperties.dcterms:modified")))
  {
    time_t modified;
    char tmp[64];
    udm_snprintf(tmp, sizeof(tmp), "%.*s", (int) length, src);
    if ((modified= UdmHttpDate2Time_t(tmp)))
    {
      UdmTime_t2HttpStr(modified, tmp, sizeof(tmp));
      /*
      printf("DATE='%s' %d olddate=%s\n", tmp, (int) modified, 
           UdmVarListFindStr(&data->D->Sections, "Last-Modified", NULL));*/
      UdmVarListReplaceStr(&data->D->Sections, "Last-Modified", tmp);
    }
  }
  return UDM_OK;
}


static int
core_endElement(UDM_XML_PARSER *parser, const char *src, size_t length)
{
  return UDM_OK;
}


/*
  Process docProps/core.xml
*/
static UDM_XML_HANDLER docx_core_handler=
{
  core_startElement,
  core_Text,
  core_endElement
};


static int
UdmDOCXParseContentXML(DOCX_PARSER_DATA *data,
                       const UDM_XML_HANDLER *xml_handler,
                       const UDM_CONST_STR *content)
{
  int rc= UDM_OK;
  UDM_XML_PARSER parser;

  /*printf("%.*s\n", (int) length, src);*/
  UdmXMLParserCreate(&parser);
  UdmXMLSetUserData(&parser, data);
  UdmXMLSetHandler(&parser, xml_handler);
  parser.flags|= UDM_XML_SKIP_TEXT_NORMALIZATION;

  if (UdmXMLParser(&parser, content->str, (int) content->length) == UDM_XML_ERROR)
  {
    char err[256];    
    udm_snprintf(err, sizeof(err), 
                 "XML parsing error: %s at line %d pos %d\n",
                  UdmXMLErrorString(&parser),
                  (int) UdmXMLErrorLineno(&parser),
                  (int) UdmXMLErrorPos(&parser));
    UdmVarListReplaceStr(&data->D->Sections, "X-Reason", err);
    rc= UDM_ERROR;
  }

  UdmXMLParserFree(&parser);
  return rc;
}


static int
UdmZIPScanDirectory(DOCX_PARSER_DATA *data,
                    const UDM_CONST_STR *content,
                    const UDM_XML_HANDLER *xml_handler,
                    const char *fname, size_t fname_length)
{
  int rc;
  size_t i;
  UDM_ZIP_DIR_END zde;
  UDM_ZIP_DIR_ENTRY dir_entry;
  UDM_ZIP_FILE_HEADER fh;
  const char *s;

  if ((rc= udm_zip_get_dir_end(&zde, content->str, content->length)))
    return UDM_ERROR;

  for (s= content->str + zde.dir_offset, i= 0;
       i < zde.number_of_dir_records_on_this_disk;
       i++, s+= udm_zip_dir_entry_length(&dir_entry))
  {
    int len;
    udm_zip_get_dir_entry(&dir_entry, s);
    udm_zip_get_file_header(&fh, content->str + dir_entry.relative_header_offset);
    /*
    printf("comp=%d uncomp=%d method=%d ofs=%d name='%.*s'\n",
           dir_entry.compressed_size,
           dir_entry.uncompressed_size,
           dir_entry.compression_method,
           dir_entry.relative_header_offset,
           dir_entry.file_name_length,
           dir_entry.file_name);
    printf(" sig=%04X met=%d namelen=%d compresses_size=%d bits=%d\n",
           fh.signature, (int) fh.compression_method,
           fh.file_name_length,
           fh.compressed_size,
           fh.general_purpose_bit_flag);
    */
    if (dir_entry.file_name_length == fname_length &&
        !memcmp(dir_entry.file_name, fname, fname_length))
    {
      char *file_content;
      if (dir_entry.uncompressed_size > UDM_MAXDOCSIZE)
      {
        UdmLog(data->A, UDM_LOG_WARN,
               "%.*s is too large: %d",
               (int) fname_length, fname, dir_entry.compressed_size);
        return UDM_ERROR;
      }
      if (!(file_content= UdmMalloc(dir_entry.uncompressed_size + 1)))
      {
        UdmLog(data->A, UDM_LOG_WARN,
               "UdmZIPScanDirectory: UdmMalloc(%d) failed",
               dir_entry.compressed_size);
        return UDM_ERROR;
      }
      if (!(len= UdmInflate2(file_content, dir_entry.uncompressed_size,
                             fh.data, dir_entry.compressed_size)))
        rc= UDM_ERROR;
      else
      {
        UDM_CONST_STR fcontent;
        UdmConstStrSet(&fcontent, file_content, len);
        rc= UdmDOCXParseContentXML(data, xml_handler, &fcontent);
      }
      UdmFree(file_content);
      return rc;
    }
  }
  return UDM_ERROR;
}


udm_bool_t
UdmHTTPBufContentToConstStr(const UDM_HTTPBUF *Buf, UDM_CONST_STR *str)
{
  if (!Buf->content || Buf->content > Buf->buf + Buf->size)
    return UDM_TRUE;
  str->str= Buf->content;
  str->length= Buf->size - (Buf->content - Buf->buf);
  return UDM_FALSE;
}


static int
UdmDOCXParseInternal(UDM_AGENT *A, UDM_DOCUMENT *Doc,
                     const UDM_CONST_STR *content)
{
  DOCX_PARSER_DATA data;
  UDM_VAR *BSec= UdmVarListFind(&Doc->Sections, "body");
  UDM_VAR *TSec= UdmVarListFind(&Doc->Sections, "title");
  data.A= A;
  data.D= Doc;
  data.body_sec= BSec ? BSec->section : 0;
  data.title_sec= TSec ? TSec->section : 0;
  UdmDOCXFontPrInit(&data.font);
  UdmDOCXParaPrInit(&data.para);
  UdmVarListReplaceStr(&Doc->Sections, "Meta-Charset", "utf-8");
  UdmZIPScanDirectory(&data, content, &docx_core_handler,
                      UDM_CSTR_WITH_LEN("docProps/core.xml"));
  return UdmZIPScanDirectory(&data, content, &docx_document_handler,
                             UDM_CSTR_WITH_LEN("word/document.xml"));
}


int
UdmDOCXParse(UDM_AGENT *A, UDM_DOCUMENT *Doc)
{
  UDM_CONST_STR content;
  if (UdmHTTPBufContentToConstStr(&Doc->Buf, &content))
    return UDM_ERROR;
  return UdmDOCXParseInternal(A, Doc, &content);
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


int
UdmDOCXCachedCopy(UDM_AGENT *Agent,
                  UDM_RESULT *Res,
                  UDM_DOCUMENT *Doc,
                  UDM_DSTR *dstr)
{
  int rc;
  size_t i;
  int hlstop= UdmVarListFindBool(&Agent->Conf->Vars, "ExcerptStopword", 1);
  const char *seg= UdmVarListFindStr(&Agent->Conf->Vars, "Segmenter", NULL);
  int segmenter= seg ? UdmUniSegmenterFind(Agent, NULL, seg) : 0;
  UDM_CHARSET *topcs= UdmGetCharSet("utf-8");
  UDM_HIGHLIGHT_CONV ec;

  if (UDM_OK != (rc= UdmDOCXParse(Agent, Doc)))
    return rc;

  UdmExcerptConvInit(&ec, Agent->Conf->bcs, topcs, topcs);

  /* Collect body chunks from Doc->TextList into dstr */
  for (i= 0; i < Doc->TextList.nitems; i++)
  {
    UDM_TEXTITEM *Item= &Doc->TextList.Item[i];
    if (Item->section == 255)
    {
      UdmDSTRAppend(dstr, Item->str, strlen(Item->str));
      continue;
    }
    if (!strcmp(Item->section_name, "body"))
    {
      UdmHlConvertDSTRWithConv(Agent, dstr,
                               &Res->WWList,
                               Item->str, strlen(Item->str),
                               &ec, hlstop, segmenter);
    }
  }
  return UDM_OK;
}


int *
UdmDOCXExcerptSource(UDM_AGENT *Agent,
                     UDM_RESULT *Res,
                     UDM_DOCUMENT *Doc,
                     UDM_CHARSET *bcs,
                     const UDM_CONST_STR *content,
                     size_t *length)
{
  UDM_CHARSET *dcs;
  size_t i, dstmaxlen;
  int *dst;
  UDM_CONV cnv;
  UDM_DSTR dstr;
  int hlstop= UdmVarListFindBool(&Agent->Conf->Vars, "ExcerptStopword", 1);
  const char *seg= UdmVarListFindStr(&Agent->Conf->Vars, "Segmenter", NULL);
  int segmenter= seg ? UdmUniSegmenterFind(Agent, NULL, seg) : 0;

  if (UDM_OK != UdmDOCXParseInternal(Agent, Doc, content))
    return NULL;

  UdmDSTRInit(&dstr, 512);

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
  dcs= UdmGetCharSet("utf-8");
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
