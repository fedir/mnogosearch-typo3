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

#ifndef _UDM_SERVER_H
#define _UDM_SERVER_H

/* Spider follow types */
#define UDM_FOLLOW_UNKNOWN	-1

#define UDM_FOLLOW_NO		0
#define UDM_FOLLOW_PATH		1
#define UDM_FOLLOW_SITE		2
#define UDM_FOLLOW_WORLD	3
#define UDM_FOLLOW_URLLIST	4
#define UDM_FOLLOW_DEFAULT      UDM_FOLLOW_PATH

extern int		UdmSpiderParamInit(UDM_SPIDERPARAM *);
extern __C_LINK int	__UDMCALL UdmServerInit(UDM_SERVER * srv);
extern __C_LINK void __UDMCALL UdmServerFree(UDM_SERVER * srv);
extern __C_LINK int	__UDMCALL UdmServerAdd(UDM_AGENT * A, UDM_SERVER * srv, int flags);
extern UDM_SERVER *	UdmServerFind(UDM_ENV *Conf, UDM_SERVERLIST *, const char * url, char **alias);
extern void		UdmServerListFree(UDM_SERVERLIST *);
extern void		UdmServerListSort(UDM_SERVERLIST *);
extern urlid_t          UdmServerGetSiteId(UDM_AGENT *Indexer, UDM_SERVER *srv, UDM_URL *url);

#endif
