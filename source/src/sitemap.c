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
#include "udm_robots.h"
#include "udm_utils.h"
#include "udm_parsexml.h"
#include "udm_hrefs.h"

/*
#include "udm_vars.h"
#include "udm_log.h"
#include "udm_mutex.h"

#include "udm_conf.h"
#include "udm_sgml.h"
*/


typedef struct
{
  const char *loc;
  size_t loclen;
  time_t lastmod;
  size_t changefreg;
  float priority;
} UDM_SITEMAP_HREF;

static void
UdmSitemapHrefInit(UDM_SITEMAP_HREF *Href)
{
  bzero((void*) Href, sizeof(*Href));
}


typedef struct
{
  UDM_AGENT *Agent;
  UDM_HREFLIST *HrefList;
  UDM_SITEMAP_HREF Href;
} UDM_SITEMAP_XML_PARSER_DATA;


static int
checktag(UDM_XML_PARSER *parser, const char *name, size_t len)
{
  return
    (parser->attrend - parser->attr == len) &&
    !strncasecmp(parser->attr, name, len);
}

static int
SitemapXMLEnter(UDM_XML_PARSER *parser, const char *name, size_t len)
{
  UDM_SITEMAP_XML_PARSER_DATA *Data= parser->user_data;
  if (checktag(parser, UDM_CSTR_WITH_LEN("urlset.url")))
    UdmSitemapHrefInit(&Data->Href);
  return UDM_OK;
}


static int
SitemapXMLLeave(UDM_XML_PARSER *parser, const char *name, size_t len)
{
  UDM_SITEMAP_XML_PARSER_DATA *Data= parser->user_data;
  if (checktag(parser, UDM_CSTR_WITH_LEN("urlset.url")))
  {
    char loc[1024];
    UDM_HREF Href;
    UdmHrefInit(&Href);
    udm_snprintf(loc, sizeof(loc), "%.*s", Data->Href.loclen, Data->Href.loc);
    Href.url= loc;
    UdmHrefListAdd(Data->HrefList, &Href);
    UdmSitemapHrefInit(&Data->Href);
  }
  return UDM_OK;
}


static int
SitemapXMLValue(UDM_XML_PARSER *parser, const char *val, size_t vallen)
{
  UDM_SITEMAP_XML_PARSER_DATA *Data= parser->user_data;
  if (checktag(parser, UDM_CSTR_WITH_LEN("urlset.url.loc")))
  {
    Data->Href.loc= val;
    Data->Href.loclen= vallen;
  }
  else if (checktag(parser, UDM_CSTR_WITH_LEN("urlset.url.changefreq")))
  {
  }
  else if (checktag(parser, UDM_CSTR_WITH_LEN("urlset.url.priority")))
  {
    char str[256];
    udm_snprintf(str, sizeof(str), "%.*s", vallen, val);
    Data->Href.priority= atof(str);
  }
  else if (checktag(parser, UDM_CSTR_WITH_LEN("urlset.url.lastmod")))
  {
    char str[256];
    udm_snprintf(str, sizeof(str), "%.*s", vallen, val);
    Data->Href.lastmod= UdmHttpDate2Time_t(str);
  }
  else
  {
    /*
    const char *attr= parser->attr;
    size_t attrlen= parser->attrend - parser->attr;
    fprintf(stderr, "%.*s=%.*s\n", attrlen, attr, vallen, val);
    */
  }
  return UDM_OK;
}



int
UdmSitemapParse(UDM_AGENT *A, UDM_HREFLIST *HrefList,
                const char *url, char *buf, size_t buflen,
                const char *hostinfo)
{
  int rc= UDM_OK;
  UDM_XML_PARSER parser;
  UDM_SITEMAP_XML_PARSER_DATA Data;
  
  UdmXMLParserCreate(&parser);
  parser.flags |= UDM_XML_SKIP_TEXT_NORMALIZATION;
  bzero(&Data, sizeof(Data));
  Data.Agent= A;
  Data.HrefList= HrefList;

  UdmXMLSetUserData(&parser, &Data);
  UdmXMLSetEnterHandler(&parser, SitemapXMLEnter);
  UdmXMLSetLeaveHandler(&parser, SitemapXMLLeave);
  UdmXMLSetValueHandler(&parser, SitemapXMLValue);

  if (UdmXMLParser(&parser, buf, buflen) == UDM_XML_ERROR)
  {
    char err[256];    
    udm_snprintf(err, sizeof(err), 
                 "XML parsing error: %s at line %d pos %d\n",
                  UdmXMLErrorString(&parser),
                  UdmXMLErrorLineno(&parser),
                  UdmXMLErrorPos(&parser));
    rc= UDM_ERROR;
  }

  UdmXMLParserFree(&parser);
  return rc;
}
