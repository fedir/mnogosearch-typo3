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

#ifndef _UDM_LOG_H
#define _UDM_LOG_H

#include "udm_config.h"
#include "udm_common.h" /* for UDM_ENV etc. */
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

/* Define default log facility */
#ifdef HAVE_SYSLOG_H
#ifndef LOG_FACILITY
#define LOG_FACILITY LOG_LOCAL7
#endif /* LOG_FACILITY */
#else
#define LOG_FACILITY 0
#endif

/* According to some recommendations, try not to exceed about 800 bytes 
   or you might have problems with old syslog implementations */
#define UDM_LOG_BUF_LEN 255

/* Verbose levels */
#define UDM_LOG_NONE	0
#define UDM_LOG_ERROR	1
#define UDM_LOG_WARN	2
#define UDM_LOG_INFO	3
#define UDM_LOG_EXTRA	4
#define UDM_LOG_DEBUG	5

extern int UdmOpenLog(const char * appname,UDM_ENV *Env, int log2stderr);
extern void UdmLog(UDM_AGENT *Agent, int level, const char *fmt, ...);
/* if you do not have UDM_AGENT struct yet, use UdmLog_noagent */
extern void UdmLog_noagent(UDM_ENV *Env, int level, const char *fmt, ...);

extern __C_LINK void __UDMCALL UdmSetLogLevel(UDM_AGENT *A, int level);
extern __C_LINK void __UDMCALL UdmIncLogLevel(UDM_AGENT *A);
extern __C_LINK void __UDMCALL UdmDecLogLevel(UDM_AGENT *A);
extern __C_LINK int  __UDMCALL UdmSetThreadProc(UDM_ENV * Conf,void (*_ThreadInfo)(UDM_AGENT* A,const char *state, const char* str));
extern __C_LINK int  __UDMCALL UdmSetRefProc(UDM_ENV * Conf,void (*_RefProc)(int code,const char *url, const char *ref));
extern __C_LINK int  __UDMCALL UdmNeedLog(int level);
extern __C_LINK void __UDMCALL UdmShowThreadInfo(UDM_AGENT* A,const char *state, const char* str);
#endif /* _UDM_LOG_H */
