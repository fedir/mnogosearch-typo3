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

#ifndef _UDM_CONFIG_H
#define _UDM_CONFIG_H

#ifdef WIN32
#include "udm_winautoconf.h"
#else
#include "udm_autoconf.h"
#endif

/* ODBC Library */
#if (HAVE_IODBC|| HAVE_UNIXODBC||HAVE_SOLID||HAVE_VIRT||HAVE_EASYSOFT||HAVE_SAPDB||HAVE_DB2)
#define HAVE_ODBC 1
#endif

/* Backend */
#if defined (HAVE_SQLITE3_AMALGAMATION)
#define HAVE_SQLITE3 1
#endif

#if (HAVE_ODBC||HAVE_MYSQL||HAVE_PGSQL||HAVE_IBASE||HAVE_ORACLE8||HAVE_CTLIB||HAVE_SQLITE||HAVE_SQLITE3)
#define HAVE_SQL 1
#endif

/* Unix/Windows specific stuff */

#ifdef WIN32

#include <windows.h>
#define HAVE_WINDOWS_H
#define HAVE_IO_H
#define HAVE_DIRECT_H
#define HAVE_PROCESS_H
#define HAVE_WINSOCK_H
#define __INDLIB__		__declspec(dllexport) /* export functions for DLL  */
#define UDMSLASH		'\\'
#define UDMSLASHSTR		"\\"
#define UDM_TMP_DIR		"."
#define UDMSLEEP(x)		_sleep(x*1000)
#define getpid()		_getpid()
#define UDM_IWRITE		S_IWRITE
#define UDM_BINARY		O_BINARY

#define snprintf	_snprintf
#define vsnprintf	_vsnprintf
#define lseek		_lseek
#define open		_open
#define popen		_popen
#define pclose	    	_pclose
#define read		_read
#define write		_write
#define strdup		_strdup

typedef __int32 udmhash32_t;
typedef unsigned __int64 udmhash64_t;
typedef unsigned __int64 udm_uint8;
typedef unsigned __int32 uint4;
typedef __int32 int4;
typedef __int64 udm_timer_t;

#define SDPALIGN __int64

#ifdef __cplusplus
  #define __C_LINK "C"
#else
  #define __C_LINK
#endif

#define __UDMCALL __cdecl

#else /* WIN32 */

#define __C_LINK
#define __UDMCALL

#define UDMSLASH		'/'
#define UDMSLASHSTR		"/"
#define UDM_TMP_DIR		"/tmp"
#define UDMSLEEP(x)		sleep(x)
#define closesocket(x)		close(x)
#define UDM_IWRITE		0644
#define UDM_BINARY		0

#if SIZEOF_SHORT == 4
typedef short udmhash32_t;
typedef unsigned short uint4;
typedef short int4;
#elif SIZEOF_INT == 4
typedef int udmhash32_t;
typedef unsigned int uint4;
typedef int int4;
#else
typedef long udmhash32_t;
typedef unsigned long uint4;
typedef long int4;
#endif

#if SIZEOF_INT == 8
typedef unsigned int udmhash64_t;
typedef unsigned int udm_uint8;
#elif SIZEOF_LONG == 8
typedef unsigned long udmhash64_t;
typedef unsigned long udm_uint8;
#else
typedef unsigned long long udmhash64_t;
typedef unsigned long long udm_uint8;
#endif

#if (SIZEOF_CHARP == SIZEOF_INT)
#define SDPALIGN int
#elif (SIZEOF_CHARP == SIZEOF_LONG)
#define SDPALIGN long
#elif (SIZEOF_CHARP == SIZEOF_SHORT)
#define SDPALIGN short
#else
#define SDPALIGN long long
#endif

typedef unsigned long long udm_timer_t;


#endif /* WIN32 */

typedef int4 urlid_t;
typedef uint4 udmcrc32_t;

#define UDM_MAX_URLID_T 0x7FFFFFFF

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef __USE_GNU
#define __USE_GNU
#endif

/* For strtok_r() */
#ifndef _REENTRANT
#define _REENTRANT
#endif

#ifndef EFENCE

#define UdmFree free
#define UdmRealloc realloc
#define UdmMalloc malloc
#define UdmCalloc calloc
#define UdmValloc valloc
#define UdmStrdup strdup
#ifdef HAVE_STRNDUP
#define UdmStrndup strndup
#endif

#else
#include "udm_efence.h"
#endif

#ifdef USE_PARANOIA
extern void *UdmViolationEnter(void);
extern void UdmViolationExit(void *);
#define UdmDebugEnter()            void *paran= UdmViolationEnter();
#define UdmDebugReturn(rc)         {UdmViolationExit(paran);return rc;}
#define UdmDebugReturnVoid()       {UdmViolationExit(paran);return;}
#else
#define UdmViolationExit(x)
#define UdmDebugEnter()           /* UdmDebugEnter() */
#define UdmDebugReturn(rc)        {return rc;}
#define UdmDebugReturnVoid()      {return;}
#endif

#ifndef PATH_MAX
#define PATH_MAX 512
#endif

#ifdef HAVE_DEBUG
  #define UDM_ASSERT(x)  assert(x)
  #ifdef HAVE_PTHREAD
    #ifdef WIN32
    #define UDM_ASSERT_MUTEX_OWNER(x) UDM_ASSERT((x)->count >0 && (UdmThreadSelf() == (x)->thread))
    #else
    #define UDM_ASSERT_MUTEX_OWNER(x) UDM_ASSERT((x)->count > 0 && pthread_equal(pthread_self(),(x)->thread))
    #endif
  #else
    #define UDM_ASSERT_MUTEX_OWNER(x)
  #endif
#else
  #define UDM_ASSERT(x)
  #define UDM_ASSERT_MUTEX_OWNER(x) 
#endif

#ifndef __GNUC__
#define __attribute__(A)
#endif

#ifdef __GNUC__
#define GCC_VERSION (__GNUC__ * 10000  + __GNUC_MINOR__ * 100  + __GNUC_PATCHLEVEL__)
#else
#define GCC_VERSION 0
#endif

#if GCC_VERSION > 40600
#define HAVE_GCC_PRAGMA_PUSH
#endif

#endif /* _UDM_CONFIG_H */
