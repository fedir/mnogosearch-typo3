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
#include <sys/types.h>
#include <string.h>

#include "udm_common.h"
#include "udm_utils.h" /* for bzero */
#include "udm_mutex.h"

/*#define DEBUG_LOCK*/


#ifdef USE_TRACE
#define UDM_TRACE_STREAM(A)		((A)->TR)
#define UDM_TRACE_STREAM_FFLUSH(A)	fflush((A)->TR)
#else
#define UDM_TRACE_STREAM(A)		(stderr)
#define UDM_TRACE_STREAM_FFLUSH(A)
#endif


static UDM_MUTEX MuMu[UDM_LOCK_MAX];


#ifdef WIN32
__C_LINK udm_mutex_t ThreadProcLock;
#endif


__C_LINK void __UDMCALL UdmInitMutexes(void)
{
  int i;
#ifdef WIN32
  InitMutex(&ThreadProcLock);
#endif
  for(i=0;i<UDM_LOCK_MAX;i++)
  {
    bzero((void*)&MuMu[i], sizeof(MuMu[i]));
    InitMutex(&MuMu[i].mutex);
  }
}


__C_LINK  void __UDMCALL UdmDestroyMutexes(void)
{
  int i;
  for(i=0;i<UDM_LOCK_MAX;i++)
  {
    DestroyMutex(&MuMu[i].mutex);
  }
#ifdef WIN32
  DestroyMutex(&ThreadProcLock);
#endif
}


/* CALL-BACK Locking function */
__C_LINK void __UDMCALL UdmLockProc(UDM_AGENT *A,int command,int type,const char *fn,int ln)
{
#ifdef DEBUG_LOCK
  if (type != UDM_LOCK_THREAD)
  fprintf(UDM_TRACE_STREAM(A),
          "[%d]{%02d} %2d Try %s\t%s\t%d\n", (int)getpid(), A ? A->handle : -1, type, (command==UDM_LOCK) ? "lock\t" : "unlock", fn, ln);
  UDM_TRACE_STREAM_FFLUSH(A);
#endif

  switch(command)
  {
    case UDM_LOCK:
      /*fprintf(stderr, "[%d].count=%d\n", type, MuMu[type].count);*/
      {
#ifdef HAVE_DEBUG
        char state_extra[128]= "";
        if (MuMu[type].thread)
        {
          sprintf(state_extra, "Waiting for mutex #%d, %s:%d", type, fn, ln);
          A->State.extra= state_extra;
        }
#endif
        UDM_ASSERT(MuMu[type].thread != UdmThreadSelf());
        UDM_MUTEX_LOCK(&MuMu[type].mutex);
        if (A->State.nmutexes < UDM_AGENT_STATE_MUTEXES)
          A->State.mutex_owned[A->State.nmutexes]= type;
        A->State.nmutexes++;
#ifdef HAVE_DEBUG
        MuMu[type].count++;
        MuMu[type].thread= UdmThreadSelf();
        if (state_extra[0])
        {
          A->State.extra= NULL;
        }
#endif
      }
      break;
    case UDM_UNLOCK:
#ifdef HAVE_DEBUG
      MuMu[type].count--;
      MuMu[type].thread= 0;
#endif
      UDM_MUTEX_UNLOCK(&MuMu[type].mutex);
      A->State.nmutexes--;
      break;
#ifdef HAVE_DEBUG
    case UDM_CKLOCK:
      UDM_ASSERT_MUTEX_OWNER(&MuMu[type]);
      break;
#endif
  }

#ifdef DEBUG_LOCK
  if (type != UDM_LOCK_THREAD)
  fprintf(UDM_TRACE_STREAM(A),
         "[%d]{%02d} %2d %s\t%s\t%d\n", (int)getpid(),
         A ? A->handle : -1, type,
         (command==UDM_LOCK) ? "locked\t" : "unlocked\t", fn, ln);
  UDM_TRACE_STREAM_FFLUSH(A);
#endif

}
