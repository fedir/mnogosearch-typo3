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

#ifndef _UDM_PARSE_XML_H
#define _UDM_PARSE_XML_H


#define UDM_XML_OK	0
#define UDM_XML_ERROR	1

#define UDM_XML_SKIP_TEXT_NORMALIZATION 1

struct xml_stack_st;

typedef struct xml_handler_st
{
  int (*enter_action)(struct xml_stack_st *st, const char *val, size_t len);
  int (*value_action)(struct xml_stack_st *st, const char *val, size_t len);
  int (*leave_action)(struct xml_stack_st *st, const char *val, size_t len);
} UDM_XML_HANDLER;


typedef struct xml_stack_st
{
  char errstr[128];
  char attr[128];
  char *attrend;
  const char *beg;
  const char *cur;
  const char *end;
  char question;
  int flags;
  void *user_data;
  UDM_XML_HANDLER handler;
} UDM_XML_PARSER;


extern void UdmXMLParserCreate(UDM_XML_PARSER *p);
extern void UdmXMLParserFree(UDM_XML_PARSER *p);
extern void UdmXMLSetValueHandler(UDM_XML_PARSER *p, int (*action)(UDM_XML_PARSER *p, const char *s, size_t l));
extern void UdmXMLSetEnterHandler(UDM_XML_PARSER *p, int (*action)(UDM_XML_PARSER *p, const char *s, size_t l));
extern void UdmXMLSetLeaveHandler(UDM_XML_PARSER *p, int (*action)(UDM_XML_PARSER *p, const char *s, size_t l));
extern void UdmXMLSetHandler(UDM_XML_PARSER *p, const UDM_XML_HANDLER *handler);
extern void UdmXMLSetUserData(UDM_XML_PARSER *p, void *user_data);
extern const char *UdmXMLErrorString (UDM_XML_PARSER *p);
extern size_t UdmXMLErrorPos(UDM_XML_PARSER *p);
extern size_t UdmXMLErrorLineno(UDM_XML_PARSER *p);
extern int UdmXMLParser(UDM_XML_PARSER *p, const char *str, size_t len);

extern int    UdmXMLParse(UDM_AGENT*, UDM_DOCUMENT*);

#endif
