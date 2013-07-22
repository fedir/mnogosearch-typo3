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

#ifndef _UDM_HTTP_H
#define _UDM_HTTP_H

typedef enum 
{
  UDM_CONTENT_TYPE_UNKNOWN= 0,
  UDM_CONTENT_TYPE_TEXT_PLAIN= 1,
  UDM_CONTENT_TYPE_TEXT_HTML= 2,
  UDM_CONTENT_TYPE_TEXT_XML= 3,
  UDM_CONTENT_TYPE_MESSAGE_RFC822= 4,
  UDM_CONTENT_TYPE_AUDIO_MPEG= 5,
  UDM_CONTENT_TYPE_HTDB= 6,
  UDM_CONTENT_TYPE_DOCX= 7,
  UDM_CONTENT_TYPE_TEXT_RTF= 8
} udm_content_type_t;

extern udm_content_type_t UdmContentTypeByName(const char *str);


extern int UdmHTTPConnect(UDM_ENV * Conf,UDM_CONN *connp, char *hostname, int port, int timeout);
extern void UdmParseHTTPResponse(UDM_AGENT * Indexer,UDM_DOCUMENT * Doc);
extern int UdmStartHTTPD(UDM_AGENT *A, int (*routine)(int sock, UDM_AGENT *A));

extern udm_bool_t UdmHTTPBufContentToConstStr(const UDM_HTTPBUF *Buf, UDM_CONST_STR *str);

#endif
