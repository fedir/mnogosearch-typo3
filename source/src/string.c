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



char*
UdmStrStore(char *dest, const char *src)
{
  size_t d_size= (dest ? strlen(dest) : 0);
  size_t s_size= strlen(src) + 1;
  char *d= UdmRealloc(dest, d_size + s_size);

  if (d)
    memcpy(d + d_size, src, s_size);
  else
    UDM_FREE(dest);
  return d;
}


#ifndef HAVE_BZERO
__C_LINK void __UDMCALL
bzero(void *b, size_t len)
{
  memset(b,0,len);
}
#endif


#ifndef HAVE_STRCASECMP
__C_LINK int __UDMCALL
strcasecmp(const char *s1, const char *s2)
{
  register const unsigned char
    *us1 = (const unsigned char *)s1,
    *us2 = (const unsigned char *)s2;

  while (tolower(*us1) == tolower(*us2++))
    if (*us1++ == '\0')
      return (0);
  return (tolower(*us1) - tolower(*--us2));
}
#endif

#ifndef HAVE_STRNCASECMP
int strncasecmp(const char *s1, const char *s2, size_t n)
{
  if (n != 0)
  {
    register const unsigned char
        *us1 = (const unsigned char *)s1,
        *us2 = (const unsigned char *)s2;

    do
    {
      if (tolower(*us1) != tolower(*us2++))
        return (tolower(*us1) - tolower(*--us2));
      if (*us1++ == '\0')
        break;
    } while (--n != 0);
  }
  return 0;
}
#endif


#if !defined(HAVE_STRNDUP) || defined(EFENCE)
char *UdmStrndup(const char * str,size_t len)
{
  char *res;
  res= (char*) UdmMalloc(len + 1);
  strncpy(res,str,len);
  res[len]='\0';
  return res;
}
#endif


#ifndef HAVE_STRCASESTR
char *strcasestr(register const char *s, register const char *find)
{
  register char c, sc;
  register size_t len;

  if ((c = *find++) != 0)
  {
    c= tolower((unsigned char)c);
    len= strlen(find);
    do
    {
      do
      {
        if ((sc = *s++) == 0)
          return (NULL);
      } while ((char)tolower((unsigned char)sc) != c);
    } while (strncasecmp(s, find, len) != 0);
    s--;
  }
  return (char *) s;
}
#endif


char *
UdmTrim(char *p, const char *delim)
{
  int len= strlen(p);
  while ((len > 0) && strchr(delim, p[len - 1] ))
  {
    p[len - 1] = '\0';
    len--;
  }
  while ((*p) && (strchr(delim,*p)))
    p++;
  return p;
}


char*
UdmRTrim(char* p, const char *delim)
{
  int len= strlen(p);
  while ((len > 0) && strchr(delim, p[len - 1] ))
  {
    p[len - 1] = '\0';
    len--;
  }
  return p;
}


/* strtok_r clone */
char* udm_strtok_r(char *s, const char *delim, char **last)
{
  const char *spanp;
  int c, sc;
  char *tok;

  if (s == NULL && (s = *last) == NULL)
    return NULL;

cont:
  c= *s++;
  for (spanp= delim; (sc= *spanp++) != 0; )
  {
    if (c == sc)
      goto cont;
  }

  if (c == 0)    /* no non-delimiter characters */
  {
    *last= NULL;
    return NULL;
  }
  
  tok= s - 1;

  for (;;)
  {
    c= *s++;
    spanp = delim;
    do
    {
      if ((sc= *spanp++) == c)
      {
        if (c == 0)
        {
          s= NULL;
        }
        else
        {
          char *w = s - 1;
          *w = '\0';
        }
        *last = s;
        return tok;
      }
    } while (sc != 0);
  }
}


/*
   This function parses string tokens
   It understands: text 'text' "text"
   I.e. Words, tokens enclosed in ' and "
   Behavior is the same with strtok_r()
*/
char*
UdmGetStrToken(char *s, char **last)
{
  char * tbeg,lch;
  if (s == NULL && (s = *last) == NULL)
    return NULL;

  /* Find the beginning of token */
  for( ; (*s) && (strchr(" \r\n\t",*s)); s++);

  if (!*s)
    return NULL;

  lch= *s;
  if ((lch == '\'') || (lch == '"'))
    s++;
  else
    lch=' ';

  tbeg= s;

  while(1)
  {
    switch(*s)
    {
      case '\0': *last= NULL; break;
/*    case '\\': memmove(s,s+1,strlen(s+1)+1);break;*/
      case '"':
      case '\'':
        if(lch == *s)
        {
          *s= '\0';
          *last= s + 1;  
        }
        break;
      case ' ':
      case '\r':
      case '\n':
      case '\t':
        if(lch == ' ')
        {
          *s= '\0';
          *last= s + 1;
        }
        break;
      default:;
    }
    if (*s)
      s++;
    else
      break;
  }
  return tbeg;
}



#ifndef HAVE_VSNPRINTF

#ifndef HAVE_STRNLEN
static int
strnlen(const char *s, int max)
{
  const char *e;
  e= s;
  if (max >= 0)
  {
    while ((*e)&&((e-s) < max))
      e++;
  }
  else
  {
    while (*e)
      e++;
  }  
  return e - s;
}
#endif /* ifndef HAVE_STRNLEN */


/* we use this so that we can do without the ctype library */
#define is_digit(c)  ((c) >= '0' && (c) <= '9')
#define ZEROPAD  1    /* pad with zero */
#define SIGN  2    /* unsigned/signed long */
#define PLUS  4    /* show plus */
#define SPACE  8    /* space if plus */
#define LEFT  16    /* left justified */
#define SPECIAL  32    /* 0x */
#define LARGE  64    /* use 'ABCDEF' instead of 'abcdef' */

#define do_div(n,base) ({ \
int __res; \
__res = ((unsigned long) n) % (unsigned) base; \
n = ((unsigned long) n) / (unsigned) base; \
__res; })


static int
skip_atoi(const char **s)
{
  int i= 0;
  while (is_digit(**s))
    i= i * 10 + *((*s)++) - '0';
  return i;
}


static int
number(long num, int base, int size, int precision ,int type)
{
	int buflen=0;
	char c,sign,tmp[66];
	const char *digits="0123456789abcdefghijklmnopqrstuvwxyz";
	int i;

	if (type & LARGE)
		digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	if (type & LEFT)
		type &= ~ZEROPAD;
	if (base < 2 || base > 36)
		return 0;
	c = (type & ZEROPAD) ? '0' : ' ';
	sign = 0;
	if (type & SIGN) {
		if (num < 0) {
			sign = '-';
			num = -num;
			size--;
		} else if (type & PLUS) {
			sign = '+';
			size--;
		} else if (type & SPACE) {
			sign = ' ';
			size--;
		}
	}
	if (type & SPECIAL) {
		if (base == 16)
			size -= 2;
		else if (base == 8)
			size--;
	}
	i = 0;
	if (num == 0)
		tmp[i++]='0';
	else while (num != 0){
		tmp[i++] = digits[(unsigned long) num % (unsigned) base];
                (unsigned long) num /= (unsigned) base;
             }
	if (i > precision)
		precision = i;
	size -= precision;
	if (!(type&(ZEROPAD+LEFT)))
		while(size-->0)
			buflen++;
	if (sign)
		buflen++;
	if (type & SPECIAL) {
		if (base==8)
			buflen++;
		else if (base==16) {
			buflen++;
			buflen++;
		}
	}
	if (!(type & LEFT))
		while (size-- > 0)
			buflen++;
	while (i < precision--)
		buflen++;
	while (i-- > 0)
		buflen++;
	while (size-- > 0)
		buflen++;
	return buflen;
}



static size_t
udm_vsnprintf_len(const char *fmt, va_list args)
{
  size_t buflen=0;
  int len;
  unsigned long num;
  int i, base;
  const char *s;
  int ichar;
  double ifloat;

  int flags;    /* flags to number() */

  int field_width;  /* width of output field */
  int precision;    /* min. # of digits for integers; max
           number of chars for from string */
  int qualifier;    /* 'h', 'l', or 'L' for integer fields */

  for (; *fmt ; ++fmt) {
    if (*fmt != '%') {
      buflen++;
      continue;
    }
      
    /* process flags */
    flags = 0;
    repeat:
      ++fmt;    /* this also skips first '%' */
      switch (*fmt) {
        case '-': flags |= LEFT; goto repeat;
        case '+': flags |= PLUS; goto repeat;
        case ' ': flags |= SPACE; goto repeat;
        case '#': flags |= SPECIAL; goto repeat;
        case '0': flags |= ZEROPAD; goto repeat;
        }
    
    /* get field width */
    field_width = -1;
    if (is_digit(*fmt))
      field_width = skip_atoi(&fmt);
    else if (*fmt == '*') {
      ++fmt;
      /* it's the next argument */
      field_width = va_arg(args, int);
      if (field_width < 0) {
        field_width = -field_width;
        flags |= LEFT;
      }
    }

    /* get the precision */
    precision = -1;
    if (*fmt == '.') {
      ++fmt;  
      if (is_digit(*fmt))
        precision = skip_atoi(&fmt);
      else if (*fmt == '*') {
        ++fmt;
        /* it's the next argument */
        precision = va_arg(args, int);
      }
      if (precision < 0)
        precision = 0;
    }

    /* get the conversion qualifier */
    qualifier = -1;
    if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
      qualifier = *fmt;
      ++fmt;
    }

    /* default base */
    base = 10;

    switch (*fmt) {
    case 'f':
                        if (precision < 0)
                                precision = 6;
                        buflen += precision;
                        if ((flags & SPECIAL) && (precision == 0))
                                buflen++;
                        if (flags & (PLUS || SPACE))
                                buflen++;
                        ifloat = va_arg(args, double);
                        while ((ifloat >= 1.) || (field_width-- > 0)) {
                                ifloat /= base;
                                buflen++;
                        };
 
                        continue;
    case 'c':
      if (!(flags & LEFT))
        while (--field_width > 0)
          buflen++;
      ichar = va_arg(args, int);
      buflen+= sizeof((unsigned char) ichar);
      while (--field_width > 0)
        buflen++;
      continue;

    case 's':
      s = va_arg(args, char *);
      if (!s)
        s = "<NULL>";

      len = strnlen(s, precision);

      if (!(flags & LEFT))
        while (len < field_width--)
          buflen++;
      for (i = 0; i < len; ++i){
        buflen++;
        s++;
      }
      while (len < field_width--)
        buflen++;
      continue;

    case 'p':
      if (field_width == -1) {
        field_width = 2*sizeof(void *);
        flags |= ZEROPAD;
      }
      buflen += number(
        (long) va_arg(args, void *), 16,
        field_width, precision, flags);
      continue;


    case '%':
      buflen++;
      continue;

    /* integer number formats - set up the flags and "break" */
    case 'o':
      base = 8;
      break;

    case 'X':
      flags |= LARGE;
    case 'x':
      base = 16;
      break;

    case 'd':
    case 'i':
      flags |= SIGN;
    case 'u':
      break;

    default:
      buflen++;
      if (*fmt)
        buflen++;
      else
        --fmt;
      continue;
    }
    if (qualifier == 'l')
      num = va_arg(args, unsigned long);
    else if (qualifier == 'h') {
      num = (unsigned short) va_arg(args, int);
      if (flags & SIGN)
        num = (short) num;
    } else if (flags & SIGN)
      num = va_arg(args, int);
    else
      num = va_arg(args, unsigned int);
    buflen += number((long)num, base, field_width, precision, flags);
  }
  return buflen;
}


int
vsnprintf(char *str, size_t size, const char  *fmt,  va_list ap)
{
  if (udm_vsnprintf_len(fmt, ap) < size)
  {
    return vsprintf(str, fmt, ap);
  }
  else
  {
    strncpy(str, "Oops! Buffer overflow in vsnprintf()", size);
    return -1;
  }
}
#endif /* ifndef HAVE_VSNPRINTF */



/*
  This snprintf function was written
  by Murray Jensen <Murray.Jensen@cmst.csiro.au>
*/


__C_LINK int __UDMCALL
udm_snprintf(char *buf, size_t len, const char *fmt, ...)
{
#ifndef HAVE_VSNPRINTF
  char *tmpbuf;
  size_t need;
#endif /* ifndef HAVE_VSNPRINTF */
  va_list ap;
  int ret;

  va_start(ap, fmt);

#ifdef HAVE_VSNPRINTF
  ret= vsnprintf(buf, len, fmt, ap);
  buf[len-1]= '\0';
#else

  need= udm_vsnprintf_len(fmt, ap);

  if ((tmpbuf = (char *) UdmMalloc(need + 1)) == NULL)
    strncpy(buf, "Oops! Out of memory in snprintf", len-1);
  else
  {
    vsprintf(tmpbuf, fmt, ap);
    strncpy(buf, tmpbuf, len-1);
    UDM_FREE(tmpbuf);
  }

  ret= strlen(buf);
#endif /* HAVE_VSNPRINTF */

  va_end(ap);
  return ret;
}
