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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <errno.h>

#ifdef HAVE_IO_H
#include <io.h>  /* for Win */
#endif

#ifdef HAVE_DIRECT_H
#include <direct.h> /* for Win */
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>

#include "udm_signals.h"
#include "udm_utils.h"



#ifdef USE_PARANOIA

/* NOTE: gcc optimization should be switched off */

#define NSAVE 10
/*#define PRINTF_DEBUGINFO*/

#ifdef USE_SYSLOG
extern char * __progname;
#endif

typedef struct
{
  unsigned long save[NSAVE];
  unsigned long saveip[NSAVE];
  unsigned invflag;

#ifdef PARANOIDAL_ROOT
  int paranoidal_check;
#endif

} UDM_PARANOIA_PARAM;


static unsigned 
getbp()
{ 
	__asm__("movl %ebp,%eax");
	__asm__("movl (%eax),%eax");
}


#ifdef USE_SYSLOG
#define SUICIDE \
  openlog(__progname,LOG_NDELAY|LOG_PERROR|LOG_PID|LOG_CONS,LOG_USER);\
  syslog(LOG_ERR,"Stack violation - exiting");\
  closelog();\
  kill(SIGSEGV,getpid());\
  exit(1) ;
#else
#define SUICIDE \
  kill(SIGSEGV,getpid());\
  exit(1) ;
#endif




void *UdmViolationEnter(void)
{
  unsigned bp= getbp();
  int i= 0;
  UDM_PARANOIA_PARAM *p= (UDM_PARANOIA_PARAM*)UdmMalloc(sizeof(UDM_PARANOIA_PARAM));

#if defined(PRINTF_DEBUGINFO)
  printf("\nEnter p: %lx\n", p);
#endif
  if (p == NULL) exit(2);

  bzero(p, sizeof(UDM_PARANOIA_PARAM));
#ifdef PARANOIDAL_ROOT
  p->paranoidal_check = -1;
#endif

#ifdef PARANOIDAL_ROOT
  if (p->paranoidal_check == -1) 
    p->paranoidal_check= issetugid() || (!geteuid()) || (!getuid());
  if (!p->paranoidal_check)
    return (void*)p; 
#endif

  p->invflag++;
  if (p->invflag > 1)
    return (void*) p;
  
  bzero(p->save, sizeof(p->save));
  bzero(p->saveip, sizeof(p->saveip));
  bp= *((unsigned*)bp);
  
  for(i= 0; i < NSAVE; i++)
  { 
#if defined(PRINTF_DEBUGINFO)
    printf("saving %lx (%i,%d)\n", bp, p->invflag, i);
#endif
    p->save[i]= bp;
    if(!bp)
      break;
    /* this is bogus, but... Sometimes we got stack entries
     * points to low addresses. 0x20000000 - is a shared 
     * library maping start */ 
    if (bp < 0x20000000)
    {
      p->save[i]= 0;
      break;
    };
    p->saveip[i]= *((long unsigned*)bp + 1);
#if defined(PRINTF_DEBUGINFO)
    printf("storing   ip %lx : %x \n", p->saveip[i], *((unsigned*)bp + 1));
#endif
    bp= *(long unsigned*)bp;
  }
  return (void *) p;
};


void UdmViolationExit(void *v)
{ 
  unsigned bp= getbp();
  int i= 2;
  UDM_PARANOIA_PARAM *p= (UDM_PARANOIA_PARAM*)v;

#if defined(PRINTF_DEBUGINFO)
  printf("Exit p: %lx\n", p);
#endif
#ifdef PARANOIDAL_ROOT
  /* at exit_violation we always have paranoidal_check set to 0 or 1 */
  if (!p->paranoidal_check)
  {
    UDM_FREE(p);
    return; 
  }
#endif

  if(p->invflag > 1)
  {
    p->invflag--;
    return;
  };
  bp= *((unsigned*)bp);
  
  for (i= 0; i < NSAVE; i++)
  {
#if defined(PRINTF_DEBUGINFO)
    printf("processing %lx (%i, %d)\n", bp, p->invflag, i);
#endif
    if(!p->save[i]) break;
    if(bp != p->save[i]) { 
      SUICIDE;
    };
    if(!bp) break;
#if defined(PRINTF_DEBUGINFO)
    printf("restoring ip %lx : %x \n", p->saveip[i], *((unsigned*)bp + 1));
#endif
    if(p->saveip[i] != *((unsigned*)bp+1)) { 
      SUICIDE;
    };
    bp = *(unsigned*)bp;
  };
  p->invflag--;
  if (p->invflag == 0)
    UDM_FREE(p);
  return;
};


#endif /* USE_PARANOIA */
