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

#ifndef _UDM_INDEXER_H
#define _UDM_INDEXER_H

/* API fuctions to be called in main() */

extern __C_LINK int  __UDMCALL UdmIndexNextURL(UDM_AGENT *Indexer);
extern __C_LINK int  __UDMCALL UdmURLFile(UDM_AGENT *Indexer,const char *f, int cmd);
extern int UdmConvertHref(UDM_AGENT *Indexer, UDM_URL *CurURL,
                          UDM_SPIDERPARAM *Spider, UDM_HREF *Href);
extern __C_LINK int __UDMCALL UdmStoreHrefs(UDM_AGENT *Indexer);
extern int UdmDocStoreHrefs(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc);
extern int UdmDocProcessContentResponseHeaders(UDM_AGENT *I, UDM_DOCUMENT *D);

#ifdef  WIN32
extern  __C_LINK unsigned int __stdcall thread_main(void *arg);
#else
extern void * thread_main(void *arg);
#endif

#endif
