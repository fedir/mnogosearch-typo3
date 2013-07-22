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
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_vars.h"
#include "udm_log.h"
#include "udm_mutex.h"


static int UdmLogLevel= UDM_LOG_INFO;

__C_LINK int __UDMCALL
UdmSetRefProc(UDM_ENV * Conf, 
              void (*_RefProc)(int code,const char *url, const char *ref))
{
  Conf->RefInfo= _RefProc;
  return 0;
}

__C_LINK int __UDMCALL
UdmSetThreadProc(UDM_ENV * Conf,
                 void (*_ThreadInfo)(UDM_AGENT *A,const char *state, const char* str))
{
  Conf->ThreadInfo=_ThreadInfo;
  return(0);
}


__C_LINK void __UDMCALL
UdmShowThreadInfo(UDM_AGENT *A, const char *state, const char *str)
{
  if (A->Conf->ThreadInfo)
    A->Conf->ThreadInfo(A, state, str);
}


typedef struct _code
{
  const char *c_name;
  int          c_val;
} CODE;


#define UDM_LOG_FACILITY_NONE -123 /* Just some magic number */

#if defined HAVE_SYSLOG_H && defined USE_SYSLOG
static const CODE facilitynames[]={
#ifdef LOG_AUTH
  {"auth",     LOG_AUTH},
#endif
#ifdef LOG_AUTHPRIV
  {"authpriv", LOG_AUTHPRIV},
#endif
#ifdef LOG_CRON
  {"cron",     LOG_CRON},
#endif
#ifdef LOG_DAEMON
  {"daemon",   LOG_DAEMON},
#endif
#ifdef LOG_FTP
  {"ftp",      LOG_FTP},
#endif
#ifdef LOG_KERN
  {"kern",     LOG_KERN},
#endif
#ifdef LOG_LPR
  {"lpr",      LOG_LPR},
#endif
#ifdef LOG_MAIL
  {"mail",     LOG_MAIL},
#endif
#ifdef LOG_NEWS
  {"news",     LOG_NEWS},
#endif
#ifdef LOG_SYSLOG
  {"syslog",   LOG_SYSLOG},
#endif
#ifdef LOG_USER
  {"user",     LOG_USER},
#endif
#ifdef LOG_UUCP
  {"uucp",     LOG_UUCP},
#endif
#ifdef LOG_LOCAL0
  {"local0",   LOG_LOCAL0},
#endif
#ifdef LOG_LOCAL1
  {"local1",   LOG_LOCAL1},
#endif
#ifdef LOG_LOCAL2
  {"local2",   LOG_LOCAL2},
#endif
#ifdef LOG_LOCAL3
  {"local3",   LOG_LOCAL3},
#endif
#ifdef LOG_LOCAL4
  {"local4",   LOG_LOCAL4},
#endif
#ifdef LOG_LOCAL5
  {"local5",   LOG_LOCAL5},
#endif
#ifdef LOG_LOCAL6
  {"local6",   LOG_LOCAL6},
#endif
#ifdef LOG_LOCAL7
  {"local7",   LOG_LOCAL7},
#endif
  {"none",     UDM_LOG_FACILITY_NONE},
  {NULL,        0},
};


static int
syslog_facility(const char *f)
{
  const CODE *fn= facilitynames;
  if (!f || !f[0])
    return LOG_FACILITY;
  while (fn->c_name != NULL)
  {
    if (strcasecmp(f, fn->c_name) == 0)
      return fn->c_val;
    fn++;
  }
  fprintf(stderr, "Config file error: unknown facility given: '%s'\n\r", f);
  fprintf(stderr, "Will continue with default facility\n\r");
  return LOG_FACILITY;
}

#endif /* HAVE_SYSLOG_H */


__C_LINK void __UDMCALL
UdmSetLogLevel(UDM_AGENT *A, int level)
{
  UdmLogLevel= level;
}


__C_LINK void __UDMCALL
UdmIncLogLevel(UDM_AGENT *A)
{
  if (UdmLogLevel < UDM_LOG_DEBUG)
    UdmLogLevel++;
}


__C_LINK void __UDMCALL
UdmDecLogLevel(UDM_AGENT *A)
{
  if (UdmLogLevel > 0)
    UdmLogLevel--;
}


int
UdmOpenLog(const char * appname,UDM_ENV *Env, int log2stderr)
{
#if defined HAVE_SYSLOG_H && defined USE_SYSLOG
  int openlog_flag= LOG_PID;
  
  Env->Log.flags= UdmVarListFindInt(&Env->Vars, "LogFlags", 0);
  Env->Log.facility= syslog_facility(UdmVarListFindStr(&Env->Vars,"SyslogFacility",""));
  
#ifdef LOG_PERROR
  /* LOG_PERROR supported by 4.3BSD Reno releases and later */
  if (log2stderr)
  {
    if (Env->Log.facility != UDM_LOG_FACILITY_NONE)
    {
      openlog_flag|= LOG_PERROR;
      log2stderr= 0;
    }
  }
#endif

  if (Env->Log.facility != UDM_LOG_FACILITY_NONE)
    openlog(appname ? appname : "<NULL>", openlog_flag, Env->Log.facility);


#endif /* HAVE_SYSLOG_H */

  if (log2stderr)
    Env->Log.logFD= stderr;

  Env->Log.is_log_open= 1;
  return 0;
}


__C_LINK int __UDMCALL
UdmNeedLog(int level)
{
  if (UdmLogLevel < level)
    return 0;
  return 1;
}


static int
udm_logger(UDM_ENV *Env, int handle, int level, const char *fmt, va_list ap)
{
  char buf[UDM_LOG_BUF_LEN+1];
  int i= 0;

  if (handle && !(Env->Log.flags & UDM_LOG_FLAG_SKIP_PID))
    i= snprintf(buf,UDM_LOG_BUF_LEN,"[%d]{%02d} ", (int)getpid(), handle);
#ifdef WIN32
  _vsnprintf(buf + i, UDM_LOG_BUF_LEN, fmt, ap);
  if(Env->ThreadInfo)
    Env->ThreadInfo(0,"",buf);
  else
    printf("%s\n",buf);
#else
  vsnprintf(buf + i, (size_t)(UDM_LOG_BUF_LEN - i), fmt, ap);
#endif

#if defined HAVE_SYSLOG_H && defined USE_SYSLOG
  if (Env->Log.facility != UDM_LOG_FACILITY_NONE)
    syslog((level != UDM_LOG_ERROR) ? LOG_INFO : LOG_ERR, "%s", buf);
#endif
  if (Env->Log.logFD)
    fprintf(Env->Log.logFD,"%s\n",buf); 
  return 1;
}


void
UdmLog(UDM_AGENT *Agent, int level, const char *fmt, ...)
{
  va_list ap;

  if (!Agent)
  {
    fprintf(stderr, "BUG IN LOG - blame Kir\n");
    return;
  }

  if (!UdmNeedLog(level))
    return;

  UDM_GETLOCK(Agent, UDM_LOCK_LOG);

#ifdef WIN32
#else
  if (Agent->Conf->Log.is_log_open)
  {
#endif
    va_start(ap,fmt);
    udm_logger(Agent->Conf, Agent->handle,level,fmt,ap);
    va_end(ap);
#ifdef WIN32
#else
  }
  else
  {
    fprintf(stderr,"Log has not been opened\n");
  }
#endif

  UDM_RELEASELOCK(Agent, UDM_LOCK_LOG);
  return;
}


void
UdmLog_noagent(UDM_ENV * Env, int level, const char *fmt, ...)
{
  va_list ap;

#ifdef WIN32
#else
  if(Env->Log.is_log_open)
  {
#endif
    if (UdmNeedLog(level))
    {
      va_start(ap,fmt);
      udm_logger(Env, 0, level, fmt, ap);
      va_end(ap);
    }
#ifdef WIN32
#else
  }
  else
  {
    fprintf(stderr,"Log has not been opened\n");
  }
#endif
  return;
}
