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

#ifndef _UDM_UTILS_H
#define _UDM_UTILS_H

#include <stdio.h>

/* for time_t */
#include <time.h>

/* for va_list */
#include <stdarg.h>

#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>

#include "udm_config.h"
#include "udm_uniconv.h" /* for UDM_CHARSET */
/*
  This is used in UdmTime_t2Str and in its callers.
  Note, on AIX strftime() can return time zone in this format:
  'GMT+01:00'. So timezone can be longer than 3,4 or five-letter
  abbreviation.
*/
#define UDM_MAXTIMESTRLEN	64

#if !defined(__i386__) || defined(_WIN64)
#define udm_put_int4(I, C) do { *((char *)(C))= (char)(I);\
                                *(((char *)(C)) + 1)= (char)((I) >> 8);\
                                *(((char *)(C)) + 2)= (char)((I) >> 16);\
                                *(((char *)(C)) + 3)= (char)((I) >> 24); } while (0)
#define udm_get_int4(C) (int4)(((int4)((unsigned char)(C)[0])) +\
                              (((int4)((unsigned char)(C)[1]) << 8)) +\
                              (((int4)((unsigned char)(C)[2]) << 16)) +\
                              (((int4)((unsigned char)(C)[3]) << 24)))
#else
#define udm_put_int4(I, C) *((int4 *)(C))= (int4)(I)
#define udm_get_int4(C) (*((const int4 *)(C)))
#endif

/* Some useful MACROs */
#define UDM_STREND(s)		(s+strlen(s))
#define UDM_FREE(x)		{if((x)!=NULL){UdmFree(x);x=NULL;}}
#define UDM_SKIP(s,set)		while((*s)&&(strchr(set,*s)))s++;
#define UDM_SKIPN(s,set)	while((*s)&&(!strchr(set,*s)))s++;
#define UDM_SQR(x)		((x)*(x))
#define UDM_CSTR_WITH_LEN(x)  (x), ((size_t) (sizeof(x) - 1))

/* NULL safe atoi*/
#define UDM_ATOI(x)		((x)?atoi(x):0)
#define UDM_ATOF(x)		((x)?atof(x):0.0)
#define UDM_ATOU(x)		((x)?(urlid_t)strtoul(x, (char**)NULL,10):0)
#define UDM_NULL2EMPTY(x)	((x)?(x):&udm_null_char)


extern char udm_null_char;

/* Misc functions */
extern __C_LINK int	__UDMCALL	UdmInit(void);
extern char * UdmGetStrToken(char * s,char ** last);
extern char * UdmTrim(char * p, const char * delim);
extern char * UdmRTrim(char* p, const char * delim);
extern char * UdmUnescapeCGIQuery(char *d, const char *s);
extern __C_LINK char * __UDMCALL UdmEscapeURL(char *d,const char *s);
extern char * UdmEscapeURI(char *d,const char *s);
extern char * UdmRemove2Dot(char *path);
extern char * UdmBuildParamStr(char * dst,size_t len,const char * src,char ** argv,size_t argc);
extern char * UdmStrRemoveChars(char * str, const char * sep);
extern char * UdmStrRemoveDoubleChars(char * str, const char * sep);

/* This should convert Last-Modified time returned by webserver
 * to time_t (seconds since the Epoch). -kir
 */
extern time_t UdmHttpDate2Time_t(const char * date);

extern time_t UdmFTPDate2Time_t(char *date);

/***********************************************************
 * converts time_str to time_t (seconds)
 * time_str can be exactly number of seconds
 * or in the form 'xxxA[yyyB[zzzC]]'
 * (Spaces are allowed between xxx and A and yyy and so on)
 *   there xxx, yyy, zzz are numbers (can be negative!)
 *         A, B, C can be one of the following:
 *		s - second
 *		M - minute	
 *		h - hour
 *		d - day
 *		m - month
 *		y - year
 *	(these letters are as in strptime/strftime functions)
 *
 * Examples:
 * 1234 - 1234 seconds
 * 4h30M - 4 hours and 30 minutes (will return 9000 seconds)
 * 1y6m-15d - 1 year and six month minus 15 days (will return 45792000 s)
 * 1h-60M+1s - 1 hour minus 60 minutes plus 1 second (will return 1 s)
 */
time_t Udm_dp2time_t(const char * time_str);


/* This one for printing HTTP Last-Modified: header */
extern void UdmTime_t2HttpStr(time_t t, char * time_str, size_t time_str_size);
/* This one deals with timezone offset */
extern int UdmInitTZ(void);
extern __C_LINK udm_timer_t __UDMCALL UdmStartTimer(void);
extern float UdmStopTimer(udm_timer_t *ticks);

extern char *UdmStrStore (char *dest, const char *src);

/* Probably string missing functions */

#ifndef HAVE_BZERO
extern __C_LINK void __UDMCALL bzero(void *b, size_t len);
#endif

#ifndef HAVE_STRCASECMP
extern __C_LINK int __UDMCALL strcasecmp(const char *s1, const char *s2);
#endif

#ifndef HAVE_STRNCASECMP
extern int strncasecmp(const char *s1, const char *s2, size_t n);
#endif

#ifndef HAVE_STRCASESTR
extern char * strcasestr(register const char *s1, register const char *s2);
#endif

#ifndef HAVE_VSNPRINTF
extern int vsnprintf(char *str, size_t size, const char  *fmt,  va_list ap);
#endif

extern __C_LINK int __UDMCALL udm_snprintf(char *str, size_t size, const char *fmt, ...)
                              __attribute__((format(printf,3,4)));
#ifndef HAVE_SNPRINTF
#define snprintf udm_snprintf
#endif

extern int udm_strnncasecmp(const char *s, size_t slen,
                            const char *t, size_t tlen);

extern char *udm_strtok_r(char *s, const char *delim, char **save_ptr);
#ifndef HAVE_STRTOK_R
#define strtok_r udm_rstok_r
#endif

#if !defined(HAVE_STRNDUP) || defined(EFENCE)
extern char *UdmStrndup(const char *str, size_t len);
#endif
#ifndef HAVE_STRNDUP
#define strndup UdmStrndup
#endif

extern __C_LINK int __UDMCALL UdmHex2Int(int h);
extern __C_LINK int __UDMCALL UdmInt2Hex(int i);


size_t UdmHexEncode(char *dst, const char *src, size_t len);
size_t UdmHexDecode(char *dst, const char *src, size_t len);

#define BASE64_LEN(len) (4 * (((len) + 2) / 3) +2)
extern __C_LINK size_t __UDMCALL udm_base64_encode (const char *s, char *store, size_t length);
extern __C_LINK size_t __UDMCALL udm_base64_decode (char * dst, const char * src, size_t len);
extern char * udm_rfc1522_decode(char * dst, const char *src);

/* Whether to allow multuple chunks 'AAA= BB== CC==' */
#define UDM_BASE64_ALLOW_MULTIPLE_CHUNKS 1
int UdmBase64Decode(const char *src, size_t len,
                    void *dst0, const char **end, int flags);

size_t udm_quoted_printable_decode(const char *src, size_t srclen,
                                   char *dst, size_t dstlen);

/* Build directory */
extern int   UdmBuild(char * path, int mode);

/* SetEnv */
extern int UdmSetEnv(const char * name,const char * value);
extern void UdmUnsetEnv(const char * name);

extern size_t UdmUniRemoveDoubleSpaces(int *ustr);
extern void UdmUniPrint(int * ustr);

extern ssize_t UdmRecvall(int s, void *buf, size_t len);
extern ssize_t UdmSend(int s, const void *msg, size_t len, int flags);

extern void UdmWriteLock(int fd);
extern void UdmUnLock(int fd);
extern void UdmReadLock(int fd);
extern void UdmReadLockFILE(FILE *f);
extern __C_LINK void __UDMCALL UdmWriteLockFILE(FILE *f);
extern __C_LINK void __UDMCALL UdmUnLockFILE(FILE *f);

/************************ Dynamic strings stuff ***********/
typedef struct dstr_struct
{
	size_t size_total; /* Bytes allocated */
	size_t size_data;  /* Bytes used by data */
	size_t size_page;  /* Bytes to allocate on overflow */
	char free;         /* Need free flag */
	char *data;        /* null-terminated string */
} UDM_DSTR;

extern UDM_DSTR *UdmDSTRInit (UDM_DSTR *dstr, size_t size_page);
extern void UdmDSTRFree (UDM_DSTR *dstr);
extern size_t UdmDSTRAppend (UDM_DSTR *dstr, const char *data, size_t size_data);
extern size_t UdmDSTRAppendHex(UDM_DSTR *dstr, const char *s, size_t slen);
extern size_t UdmDSTRAppendSTR (UDM_DSTR *dstr, const char *data);
extern size_t UdmDSTRAppendINT4(UDM_DSTR *s, int i);
extern size_t UdmDSTRAppendINT2(UDM_DSTR *s, int i);
extern int UdmDSTRAppendf (UDM_DSTR *dstr, const char *fmt, ...)
           __attribute__((format(printf,2,3)));
extern void UdmDSTRReset (UDM_DSTR *dstr);
extern int UdmDSTRAlloc (UDM_DSTR *dstr, size_t size_data);
extern int UdmDSTRRealloc (UDM_DSTR *dstr, size_t size_data);
/**************************** Constant string routines ***********************/
typedef struct udm_const_string_st
{
  const char *str;
  size_t length;
} UDM_CONST_STR;
extern void UdmConstStrSet(UDM_CONST_STR *str, const char *s, size_t len);
extern int UdmConstStrNCaseCmp(const UDM_CONST_STR *str, const char *s, size_t length);
extern char *UdmConstStrDup(const UDM_CONST_STR *str);
extern void UdmConstStrTrim(UDM_CONST_STR *str, const char *sep);
/********************************** zint4 stuff ******************************/
typedef struct st_udm_zint4_state
{
  int prev;
  unsigned char *buf, *end;
  unsigned char bits_left;
} UDM_ZINT4_STATE;

extern void udm_zint4_init(UDM_ZINT4_STATE *state, char *buf);
extern void udm_zint4(UDM_ZINT4_STATE *state, int next);
extern void udm_zint4_finalize(UDM_ZINT4_STATE *state);
extern int udm_dezint4(const char *buf, int4 *array, int buf_len);
/**************************** end of zint4 stuff *****************************/

#ifdef HAVE_ZLIB
size_t UdmInflate(char *dst, size_t dstlen, const char *src, size_t srclen);
size_t UdmDeflate(char *dst, size_t dstlen, const char *src, size_t srclen);
#endif

/*****************************************************************************/

extern int udm_l1tolower[256];

#ifdef HAVE_DEBUG
extern void *UdmBSearch(const void *key, const void *base, size_t nmemb,
                         size_t size,
                         int (*compar)(const void *, const void *));
extern void UdmSort(void *base, size_t nmemb, size_t size,
                    int(*compar)(const void *, const void *));
#else
#define UdmBSearch bsearch
#define UdmSort   qsort
#endif

#define UDM_MIN(x,y)  ((x) < (y) ? (x) : (y))
#define UDM_MAX(x,y)  ((x) > (y) ? (x) : (y))

enum udm_dirtype_t
{
  UDM_DIRTYPE_CONF,
  UDM_DIRTYPE_SHARE,
  UDM_DIRTYPE_VAR,
  UDM_DIRTYPE_TMP
};

extern size_t UdmGetDir(char *d, size_t dlen, enum udm_dirtype_t type);
extern size_t UdmGetFileName(char *d, size_t dlen, enum udm_dirtype_t type,
                             const char *fname);

extern int UdmNormalizeDecimal(char *dst, size_t dstsize, const char *src);

#endif /* _UDM_UTILS_H */
