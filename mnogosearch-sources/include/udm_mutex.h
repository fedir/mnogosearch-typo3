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

#ifndef _UDM_MUTEX_H
#define _UDM_MUTEX_H


#ifdef WIN32
#define udm_thread_t               HANDLE
#define udm_mutex_t                CRITICAL_SECTION
#define InitMutex(x)               InitializeCriticalSection(x)
#define DestroyMutex(x)            DeleteCriticalSection(x)
#define UDM_MUTEX_LOCK(x)          EnterCriticalSection(x)
#define UDM_MUTEX_UNLOCK(x)        LeaveCriticalSection(x)
#else   /* not WIN32 */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* WIN32 */

#ifdef HAVE_PTHREAD
#include <pthread.h>
#define udm_thread_t               pthread_t
#define udm_mutex_t                pthread_mutex_t
#define InitMutex(x)               pthread_mutex_init(x,NULL)
#define DestroyMutex(x)            pthread_mutex_destroy(x)
#define UDM_MUTEX_LOCK(x)          pthread_mutex_lock(x)
#define UDM_MUTEX_UNLOCK(x)        pthread_mutex_unlock(x)
#else
#define udm_thread_t               int
#define udm_mutex_t                int
#define InitMutex(x)               *(x)=0
#define DestroyMutex(x)
#define UDM_MUTEX_LOCK(x)
#define UDM_MUTEX_UNLOCK(x)
#endif
#endif

typedef struct
{
  udm_mutex_t mutex;
#ifdef HAVE_DEBUG
  int handle;
  int count;
  pthread_t thread;
#endif
} UDM_MUTEX;


/* MUTEX stuff to lock dangerous places in multi-threaded mode */

extern __C_LINK int __UDMCALL UdmSetLockProc(UDM_ENV * Conf,
  void (*proc)(UDM_AGENT *A,int command,int type,const char *fname,int lineno));
extern __C_LINK void __UDMCALL UdmLockProc(UDM_AGENT *A, int command, int type, const char *fn, int ln);

extern __C_LINK void __UDMCALL UdmInitMutexes(void);
extern __C_LINK void __UDMCALL UdmDestroyMutexes(void);

#ifdef WIN32
extern __C_LINK udm_mutex_t ThreadProcLock;
#endif

#define UDM_GETLOCK(A,mutex)           if((A)->Conf->LockProc)(A)->Conf->LockProc((A),UDM_LOCK,(mutex),__FILE__,__LINE__)
#define UDM_RELEASELOCK(A,mutex)       if((A)->Conf->LockProc)(A)->Conf->LockProc((A),UDM_UNLOCK,(mutex),__FILE__,__LINE__)
#define UDM_LOCK_CHECK_OWNER(A,mutex)  if((A)->Conf->LockProc)(A)->Conf->LockProc((A),UDM_CKLOCK,(mutex),__FILE__,__LINE__)

/* Locking commands */
#define UDM_LOCK                  1
#define UDM_UNLOCK                2
#define UDM_CKLOCK                3

#endif /* _UDM_MUTEX_H */
