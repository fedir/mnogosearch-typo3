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

#ifndef _UDM_CONF_H
#define _UDM_CONF_H

#include <sys/types.h>

extern __C_LINK int __UDMCALL UdmEnvLoad(UDM_AGENT *Indexer, const char * name, int flags);
extern __C_LINK int __UDMCALL UdmEnvSave(UDM_AGENT *Indexer, const char * name, int flags);
extern __C_LINK int __UDMCALL UdmEnvAddLine(UDM_CFG *C, char *str);
extern __C_LINK int __UDMCALL UdmAgentAddLine(UDM_AGENT *A, const char *str);

extern char *UdmParseEnvVar(UDM_ENV *Conf, const char *str);
extern int  UdmSearchMode(const char * mode);
extern int  UdmMatchMode(const char * mode);
extern int  UdmFollowType(const char * follow);
extern int  UdmMethod(const char * method);
extern void UdmWeightFactorsInit(char *res, const char *wf, size_t numsections);
extern size_t UdmWeightFactorsInit2(char *res, UDM_VARLIST *V1, UDM_VARLIST *V2, const char *name);

extern const char	*UdmMethodStr(int method);
extern __C_LINK const char * __UDMCALL UdmFollowStr(int follow);

extern size_t UdmGetArgs(char *str, char **av, size_t max);

#endif
