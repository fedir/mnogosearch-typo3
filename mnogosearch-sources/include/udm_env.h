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

#ifndef _UDM_ENV_H
#define _UDM_ENV_H

extern __C_LINK UDM_ENV*	__UDMCALL UdmEnvInit(UDM_ENV *);
extern __C_LINK void __UDMCALL UdmEnvFree(UDM_ENV *);
extern __C_LINK char* __UDMCALL UdmEnvErrMsg(UDM_ENV *);
extern void UdmEnvSetDirs(UDM_ENV *Env);
extern  int UdmEnvPrepare(UDM_ENV *Env);
extern int UdmEnvDBListAdd(UDM_ENV *Env, const char *dbaddr, int mode);
#endif
