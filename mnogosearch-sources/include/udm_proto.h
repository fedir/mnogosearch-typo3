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

#ifndef _UDM_PROTO_H
#define _UDM_PROTO_H

/* some of NNTP status codes */
#define NNTP_GROUP_OK                             211
#define NNTP_XOVER_OK                             224

/* HTTP status codes */
#define UDM_HTTP_STATUS_UNKNOWN                   0
#define UDM_HTTP_STATUS_OK                   200
#define UDM_HTTP_STATUS_PARTIAL_OK           206

#define UDM_HTTP_STATUS_MULTIPLE_CHOICES          300
#define UDM_HTTP_STATUS_MOVED_PARMANENTLY         301
#define UDM_HTTP_STATUS_MOVED_TEMPORARILY         302
#define UDM_HTTP_STATUS_SEE_OTHER            303
#define UDM_HTTP_STATUS_NOT_MODIFIED              304
#define UDM_HTTP_STATUS_USE_PROXY            305

#define UDM_HTTP_STATUS_BAD_REQUEST               400
#define UDM_HTTP_STATUS_UNAUTHORIZED              401
#define UDM_HTTP_STATUS_PAYMENT_REQUIRED          402
#define UDM_HTTP_STATUS_FORBIDDEN            403
#define UDM_HTTP_STATUS_NOT_FOUND            404
#define UDM_HTTP_STATUS_METHOD_NOT_ALLOWED        405
#define UDM_HTTP_STATUS_NOT_ACCEPTABLE            406
#define UDM_HTTP_STATUS_PROXY_AUTHORIZATION_REQUIRED   407
#define UDM_HTTP_STATUS_REQUEST_TIMEOUT           408
#define UDM_HTTP_STATUS_CONFLICT             409
#define UDM_HTTP_STATUS_GONE                 410
#define UDM_HTTP_STATUS_LENGTH_REQUIRED           411
#define UDM_HTTP_STATUS_PRECONDITION_FAILED       412
#define UDM_HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE  413
#define UDM_HTTP_STATUS_REQUEST_URI_TOO_LONG      414
#define UDM_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE         415

#define UDM_HTTP_STATUS_INTERNAL_SERVER_ERROR          500
#define UDM_HTTP_STATUS_NOT_IMPLEMENTED           501
#define UDM_HTTP_STATUS_BAD_GATEWAY               502
#define UDM_HTTP_STATUS_SERVICE_UNAVAILABLE       503
#define UDM_HTTP_STATUS_GATEWAY_TIMEOUT           504
#define UDM_HTTP_STATUS_NOT_SUPPORTED             505


/* UdmSearch network error codes */
#define UDM_NET_ERROR              -1
#define UDM_NET_TIMEOUT            -2
#define UDM_NET_CANT_CONNECT       -3
#define UDM_NET_CANT_RESOLVE       -4
#define UDM_NET_UNKNOWN            -5
#define UDM_NET_FILE_TL                 -6

/* Mirror parameters and error codes */
#define UDM_MIRROR_NO              -1
#define UDM_MIRROR_YES             0
#define UDM_MIRROR_NOT_FOUND       -1
#define UDM_MIRROR_EXPIRED         -2
#define UDM_MIRROR_CANT_BUILD      -3
#define UDM_MIRROR_CANT_OPEN       -4

/* HTTP codes */
extern __C_LINK const char * __UDMCALL UdmHTTPErrMsg(int code);

/* Single routine for various protocols */
extern int UdmGetURL(UDM_AGENT * Indexer,UDM_DOCUMENT * Doc);
extern int UdmGetURLSimple(UDM_AGENT *A, UDM_DOCUMENT *D, const char *url);

/* Mirroring features */
extern __C_LINK int __UDMCALL UdmMirrorPUT(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc, UDM_URL *url);
extern __C_LINK int __UDMCALL UdmMirrorGET(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc, UDM_URL *url);

#endif
