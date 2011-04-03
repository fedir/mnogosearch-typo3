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


char udm_null_char= 0;

static char udm_hex_digits[]= "0123456789ABCDEF";


size_t
UdmHexEncode(char *dst, const char *src, size_t len)
{
  size_t i;
  for (i=0; i < len; i++)
  {
    unsigned int ch= (unsigned int) (unsigned char) src[i];
    *dst++= udm_hex_digits[(ch >> 4) & 0x0F];
    *dst++= udm_hex_digits[ch & 0x0F];
  }
  *dst= '\0';
  return len * 2;
}


static int
ch2x(int ch)
{
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  
  if (ch >= 'a' && ch <= 'f')
    return 10 + ch - 'a';
  
  if (ch >= 'A' && ch <= 'F')
    return 10 + ch - 'A';
  
  return -1;
}


char*
UdmUnescapeCGIQuery(char *d, const char *s)
{
  char *dd;
  
  for (dd= d; *s; s++)
  {
    if(*s == '%' && ch2x(s[1]) >= 0 && ch2x(s[2]) >= 0)
    {  
      *d++= ch2x(s[1]) * 16 + ch2x(s[2]);
      s+= 2;
    }
    else if(*s=='+')
    {
      *d++=' ';
    }
    else
    {
      *d++=*s;
    }
  }
  *d=0;
  return(dd);
}


char* __UDMCALL
UdmEscapeURL(char *d,const char *s)
{
  char *dd;
  if ( d == NULL || s==NULL)
    return 0 ;
  dd= d;
  while (*s)
  {
    if ((*s & 0x80) || strchr("%&<>+[](){}/?#'\"\\;,",*s))
    {
      sprintf(d,"%%%X",(int) (unsigned char)*s);
      d+=2;
    }
    else if (*s==' ')
    {
      *d='+';
    }
    else
    {
      *d=*s;
    }
    s++;
    d++;
  }
  *d= 0;
  return dd;
}


char *
UdmEscapeURI(char *d,const char *s)
{
  char *dd;
  if ((d==NULL)||(s==NULL))return(0);
  dd=d;
  while (*s)
  {
    if (strchr(" ",*s))
    {
      sprintf(d,"%%%X",(int)*s);
      d+=2;
    }
    else
    {
      *d=*s;
    }
    s++;
    d++;
  }
  *d= 0;
  return(dd);
}


/*
  Remove parent level like /parent/../index.html from path recursively
*/
char * UdmRemove2Dot(char *path)
{
  char *ptr;
  char *tail;

  if (!(ptr=strstr(path,"../"))) return path;
  if (ptr==path) return path; /* How could it be? */
  tail=ptr+2;
  ptr--;
  *ptr=0;
  if (!(ptr=strrchr(path,'/'))) *path=0; else *ptr=0;
  path=strcat(path,tail);
  return UdmRemove2Dot(path);
}



__C_LINK int __UDMCALL UdmInit()
{
  UdmInitTZ();
  return 0;
}


#ifdef WIN32
int UdmBuild(char *path, int omode)
{
  struct stat sb;
  int last, retval, ret_code;
  char *p;

  p = path;
  retval = 0;
  if (p[0] == UDMSLASH)    /* Skip leading slashes. */
    ++p;
  for (last = 0; !last ; ++p) {
    if (p[0] == '\0')
      last = 1;
    else
    if (p[0] != UDMSLASH)
      continue;
    *p = '\0';
    if (p[1] == '\0')
      last = 1;

    if (path[strlen(path)-1]==':'){
      char buf[5*1024];

      sprintf(buf,"%s",path);
      strcat(buf, "\\");
      ret_code=stat(buf, &sb);
    }else{
      ret_code=stat(path, &sb);
    }

    if (ret_code) {
      if (errno != ENOENT || mkdir(path) < 0 ){
        /* warn("%s", path); */
        retval = 1;
        break;
      }
    }
    else if ((sb.st_mode & S_IFMT) != S_IFDIR) {
      if (last)
        errno = EEXIST;
      else
        errno = ENOTDIR;
      /* warn("%s", path); */
      retval = 1;
      break;
    }
    if (!last)
      *p = UDMSLASH;
  }
  return (retval);
}

#else

int UdmBuild(char *path, int omode)
{
  struct stat sb;
  mode_t numask, oumask;
  int first, last, retval;
  char *p;

  p = path;
  oumask = 0;
  retval = 0;
  if (p[0] == UDMSLASH)    /* Skip leading slashes. */
    ++p;
  for (first = 1, last = 0; !last ; ++p) {
    if (p[0] == '\0')
      last = 1;
    else if (p[0] != UDMSLASH)
      continue;
    *p = '\0';
    if (p[1] == '\0')
      last = 1;
    if (first) {
      /*
       * POSIX 1003.2:
       * For each dir operand that does not name an existing
       * directory, effects equivalent to those cased by the
       * following command shall occcur:
       *
       * mkdir -p -m $(umask -S),u+wx $(dirname dir) &&
       *    mkdir [-m mode] dir
       *
       * We change the user's umask and then restore it,
       * instead of doing chmod's.
       */
      oumask = umask(0);
      numask = oumask & ~(S_IWUSR | S_IXUSR);
      (void)umask(numask);
      first = 0;
    }
    if (last)
      (void)umask(oumask);
    if (stat(path, &sb)) {
      if (errno != ENOENT ||
          mkdir(path, last ? omode :
          S_IRWXU | S_IRWXG | S_IRWXO) < 0
         ){
        /* warn("%s", path); */
        retval = 1;
        break;
      }
    }
    else if ((sb.st_mode & S_IFMT) != S_IFDIR) {
      if (last)
        errno = EEXIST;
      else
        errno = ENOTDIR;
      /* warn("%s", path); */
      retval = 1;
      break;
    }
    if (!last)
      *p = UDMSLASH;
  }
  if (!first && !last)
    (void)umask(oumask);
  return (retval);
}

#endif


char *
UdmBuildParamStr(char * dst,size_t len,const char * src,
                 char ** argv,size_t argc)
{
  const char * s;
  char * d;
  size_t argn;
  size_t curlen;

  *dst='\0';
  s=src;
  d=dst;
  curlen=0;
  
  while (*s)
  {
    if (*s=='$')
    {
      argn=atoi(s+1);
      if ((argn<=argc)&&(argn>0))
      {
        size_t arglen;
        arglen=strlen(argv[argn-1]);
        if (arglen+curlen+1<len)
        {
          strcpy(d,argv[argn-1]);
          d+=strlen(d);
          curlen+=arglen;
        }else{
          break;
        }
      }
      s++;
      while (*s>='0'&&*s<='9')
        s++;
    }else
#ifdef WIN32
#else
    if (*s=='\\')
    {
      s++;
      if (*s)
      {
        if (curlen+2<len)
        {
          *d=*s;
          s++;
          d++;
          *d='\0';
          curlen++;
        }
        else
        {
          break;
        }
      }
    }
    else
#endif
    {
      if (curlen+2<len)
      {
        *d=*s;
        s++;
        d++;
        *d='\0';
        curlen++;
      }
      else
      {
        break;
      }
    }
  }
  return dst;
}

/*********** Environment variable routines *******/

int
UdmSetEnv(const char * name,const char * value)
{
#ifdef HAVE_SETENV
  return(setenv(name,value,1));
#else
#ifdef HAVE_PUTENV
  int res;
  char * s;
  s= (char*) UdmMalloc(strlen(name)+strlen(value)+3);
  sprintf(s, "%s=%s", name, value);
  res= putenv(s);
  UDM_FREE(s);
  return res;
#else
  return 0;
#endif
#endif

}


void
UdmUnsetEnv(const char * name)
{
#ifdef HAVE_UNSETENV
  unsetenv(name);
#else
  UdmSetEnv(name,"");
#endif
}


/******************* String routines ******************/

char*
UdmStrRemoveChars(char * str, const char * sep)
{
  char *s, *e;
  int has_sep= 0;

  e= s= str;  
  while (*s)
  {
    if (strchr(sep,*s))
    {
      if (!has_sep)
      {
        e= s;
        has_sep= 1;
      }
    }
    else
    {
      if (has_sep)
      {
        memmove(e,s,strlen(s)+1);
        s= e;
        has_sep= 0;
      }
    }
    s++;
  }
  /* End spaces */
  if (has_sep)
    *e= '\0';
  
  return str;
}


char*
UdmStrRemoveDoubleChars(char * str, const char * sep)
{
  char * s, *e;
  int has_sep=0;
  
  /* Initial spaces */
  for(s=str;(*s)&&(strchr(sep,*s));s++);
  if (s!=str)memmove(str,s,strlen(s)+1);
  e=s=str;
  
  /* Middle spaces */
  while (*s)
  {
    if (strchr(sep,*s))
    {
      if (!has_sep)
      {
        e=s;
        has_sep=1;
      }
    }
    else
    {
      if (has_sep)
      {
        *e=' ';
        memmove(e+1,s,strlen(s)+1);
        s=e+1;
        has_sep=0;
      }
    }
    s++;
  }
  
  /* End spaces */
  if (has_sep)*e='\0';
  
  return str;
}


size_t
UdmUniRemoveDoubleSpaces(int *ustr)
{
  int *u, *e, addspace=0;
  
  for(u= e= ustr; *u ;)
  {
    switch(*u)
    {
      case ' ':
      case '\t':
      case '\r':
      case '\n':
      case 0xA0: /* nbsp */
        addspace=1;
        u++;
        break;
      
      default:
        if (addspace)
        {
          if (e > ustr)
          {
            *e=' ';
            e++;
          }
          addspace= 0;
        }
        *e= *u;
        e++;
        u++;
        break;
    }
  }
  *e= 0;
  return e - ustr;
}


void
UdmUniPrint(int *ustr)
{
  int * lt;
  for(lt=ustr;*lt;lt++)
  {
    fprintf(stderr,"%04X ",*lt);
  }
  fprintf(stderr,"\n");
}


/**************** recv and send ***********/

ssize_t
UdmRecvall(int s, void *buf, size_t len)
{
#ifdef MSG_WAITALL
  if (len == 0)
    return 0;
  return recv(s, buf, len, MSG_WAITALL);
#else
  ssize_t received= 0, r= 0;
  char *b= buf;
  time_t start= time(NULL);
  if (len == 0)
     return received;
  while ( ((size_t)received < len) && ((r = recv(s, b+received, len-received,0)) >= 0))
  {
    received+= r;
    if (have_sigint || have_sigterm 
#ifndef WIN32
        || have_sigpipe
#endif
        ) break;
    if (time(NULL) - start > 300)
      break;
  }
  return (r < 0) ? r : received;
#endif
}


/* To bust performance you may increase this value, but be sure, that kernel can handle it */
#define UDM_BLKLEN 8196

ssize_t
UdmSend(int s, const void *msg, size_t len, int flags)
{
  ssize_t o = 0;
  size_t olen, slen;
  const char *p = msg;

  while (len)
  {
    slen = (len < UDM_BLKLEN) ? len : UDM_BLKLEN;
    olen = send(s, p, slen, flags);
    if (olen == (size_t)-1) return olen;
    len -= olen;
    p += olen;
    o += olen;
  }
  return o;
}


/********** File lock routines *************** */

#ifndef WIN32
static struct flock*
file_lock(struct flock *ret, int type, int whence)
{
  ret->l_type= type ;
  ret->l_start= 0 ;
  ret->l_whence= whence ;
  ret->l_len= 0 ;
  ret->l_pid= getpid() ;
  return ret;
}
#endif


void
UdmWriteLock(int fd)
{  /* an exclusive lock on an entire file */
#ifndef WIN32
  static struct flock ret ;
  fcntl(fd, F_SETLKW, file_lock(&ret, F_WRLCK, SEEK_SET));
#else
/* Windows routine */
  DWORD filelen_high;
  DWORD filelen_low;
  OVERLAPPED overlap_str;
  int lock_res;

  overlap_str.Offset= 0;
  overlap_str.OffsetHigh= 0;
  overlap_str.hEvent= 0;

  filelen_low= GetFileSize((HANDLE)fd, &filelen_high);
  lock_res= LockFileEx((HANDLE)fd, LOCKFILE_EXCLUSIVE_LOCK, 0, filelen_low, filelen_high, &overlap_str);
  UDM_ASSERT(lock_res);
#endif
}


void
UdmUnLock(int fd) /* ulock an entire file */
{ 
#ifndef WIN32
  static struct flock ret ;
  fcntl(fd, F_SETLKW, file_lock(&ret, F_UNLCK, SEEK_SET));
#else
  /* Windows routine */
  DWORD filelen_high;
  DWORD filelen_low;
  int lock_res;

  filelen_low= GetFileSize((HANDLE)fd, &filelen_high);
  lock_res= UnlockFile((HANDLE)fd, 0, 0, filelen_low, filelen_high);
  UDM_ASSERT(lock_res);
#endif

}

void
UdmReadLock(int fd)
{   /* a shared lock on an entire file */
#ifndef WIN32
  static struct flock ret ;
  fcntl(fd, F_SETLKW, file_lock(&ret, F_RDLCK, SEEK_SET));
#else
  /* Windows routine */
  DWORD filelen_high;
  DWORD filelen_low;
  OVERLAPPED overlap_str;
  int lock_res;

  overlap_str.Offset= 0;
  overlap_str.OffsetHigh= 0;
  overlap_str.hEvent= 0;

  filelen_low= GetFileSize((HANDLE)fd, &filelen_high);
  lock_res= LockFileEx((HANDLE)fd, 0, 0, filelen_low, filelen_high, &overlap_str);
  UDM_ASSERT(lock_res);
#endif
}


void
UdmReadLockFILE(FILE *f)
{   /* a shared lock on an entire file */
#ifndef WIN32
  static struct flock ret ;
  fcntl(fileno(f), F_SETLKW, file_lock(&ret, F_RDLCK, SEEK_SET));
#else
  /* Windows routine */
  int file_desc = fileno(f);
  UdmReadLock(file_desc);
#endif
}


void __UDMCALL
UdmWriteLockFILE(FILE *f)
{  /* an exclusive lock on an entire file */
#ifndef WIN32
  static struct flock ret ;
  fcntl(fileno(f), F_SETLKW, file_lock(&ret, F_WRLCK, SEEK_SET));
#else
  /* Windows routine */
  int file_desc = fileno(f);
  UdmWriteLock(file_desc);
#endif
}


void __UDMCALL
UdmUnLockFILE(FILE *f)
{ /* ulock an entire file */
#ifndef WIN32
  static struct flock ret ;
  fcntl(fileno(f), F_SETLKW, file_lock(&ret, F_UNLCK, SEEK_SET));
#else
  /* Windows routine */
  int file_desc = fileno(f);
  UdmUnLock(file_desc);
#endif
}




/******************** ZINT4 routines ********************/

/*
   Note that buf must be at least X * 5 + 5 bytes.
   Where X is the number of integers in array.
*/
void
udm_zint4_init(UDM_ZINT4_STATE *state, char *buf)
{
  state->bits_left= 8;
  state->prev= 0;
  state->end= state->buf= (unsigned char *)buf;
}


static inline void
udm_zint4_write_bits(UDM_ZINT4_STATE *state,
                     unsigned char byte, unsigned char nbits)
{
  if (state->bits_left == 8)
    *state->end= 0;
  if (state->bits_left >= nbits)
  {
    *state->end+= byte << (state->bits_left - nbits);
    if (nbits == state->bits_left)
    {
      state->end++;
      state->bits_left= 8;
    }
    else
      state->bits_left-= nbits;
    return;
  }
  *state->end+= byte >> (nbits - state->bits_left);
  state->end++;
  state->bits_left+= 8 - nbits;
  *state->end= byte << state->bits_left;
}


/*
  Encoding components:
  Bits Bit mask                                         Range
  ==== ================================================ ==========
  5           0 xxxx                                    0x0000000F
  10         10 xxxx xxxx                               0x000000FF
  15        110 xxxx xxxx xxxx                          0x00000FFF
  20       1110 xxxx xxxx xxxx xxxx                     0x0000FFFF
  25      11110 xxxx xxxx xxxx xxxx xxxx                0x000FFFFF
  30     111110 xxxx xxxx xxxx xxxx xxxx xxxx           0x00FFFFFF
  35    1111110 xxxx xxxx xxxx xxxx xxxx xxxx xxxx      0x0FFFFFFF
  40   11111110 xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx 0xFFFFFFFF
*/

void
udm_zint4(UDM_ZINT4_STATE *state, int next)
{
  unsigned int n= (unsigned int)(next - state->prev);
  state->prev= next;
  if (n < 0x10) /* compressed to 5 bits, 1 block */
  {
    udm_zint4_write_bits(state, n, 5);
  }
  else if (n < 0x110) /* compressed to 10 bits, 2 blocks */
  {
    n-= 0x10;
    udm_zint4_write_bits(state, 2, 2);
    udm_zint4_write_bits(state, n, 8);
  }
  else if (n < 0x1110) /* compressed to 15 bits, 3 blocks */
  {
    n-= 0x110;
    udm_zint4_write_bits(state, 6, 3);
    udm_zint4_write_bits(state, n >> 8, 4);
    udm_zint4_write_bits(state, n & 0xff, 8);
  }
  else if (n < 0x11110) /* compressed to 20 bits, 4 blocks */
  {
    n-= 0x1110;
    udm_zint4_write_bits(state, 0xe, 4);
    udm_zint4_write_bits(state, n >> 8, 8);
    udm_zint4_write_bits(state, n & 0xff, 8);
  }
  else if (n < 0x111110) /* compressed to 25 bits, 5 blocks */
  {
    n-= 0x11110;
    udm_zint4_write_bits(state, 0x1e, 5);
    udm_zint4_write_bits(state, n >> 16, 4);
    udm_zint4_write_bits(state, (n >> 8) & 0xff, 8);
    udm_zint4_write_bits(state, n & 0xff, 8);
  }
  else if (n < 0x1111110) /* compressed to 30 bits, 6 blocks */
  {
    n-= 0x111110;
    udm_zint4_write_bits(state, 0x3e, 6);
    udm_zint4_write_bits(state, n >> 16, 8);
    udm_zint4_write_bits(state, (n >> 8) & 0xff, 8);
    udm_zint4_write_bits(state, n & 0xff, 8);
  }
  else if (n < 0x11111110) /* compressed to 35 bits */
  {
    n-= 0x1111110;
    udm_zint4_write_bits(state, 0x7e, 7);
    udm_zint4_write_bits(state, n >> 24, 4);
    udm_zint4_write_bits(state, (n >> 16) & 0xff, 8);
    udm_zint4_write_bits(state, (n >> 8) & 0xff, 8);
    udm_zint4_write_bits(state, n & 0xff, 8);
  }
  else /* compressed to 40 bits */
  {
    n-= 0x11111110;
    udm_zint4_write_bits(state, 0xfe, 8);
    udm_zint4_write_bits(state, n >> 24, 8);
    udm_zint4_write_bits(state, (n >> 16) & 0xff, 8);
    udm_zint4_write_bits(state, (n >> 8) & 0xff, 8);
    udm_zint4_write_bits(state, n & 0xff, 8);
 }
}


void
udm_zint4_finalize(UDM_ZINT4_STATE *state)
{
  if (state->bits_left < 8)
    udm_zint4_write_bits(state, 0xff, 8);
  *state->end++= 0xff;
  *state->end++= 0xff;
  *state->end++= 0xff;
  *state->end++= 0xff;
  *state->end++= 0xff;
}


int
udm_dezint4(const char *buf, int4 *array, int buf_len)
{
  unsigned char next_byte= *buf;
  unsigned char bit_idx= 8;
  char nblocks= 1;
  int4 prev= 0, *array_end;
  if ((unsigned char)buf[buf_len - 1] != 0xff ||
      (unsigned char)buf[buf_len - 2] != 0xff ||
      (unsigned char)buf[buf_len - 3] != 0xff ||
      (unsigned char)buf[buf_len - 4] != 0xff ||
      (unsigned char)buf[buf_len - 5] != 0xff)
    return(0);
  for (array_end= array;; array_end++)
  {
    uint4 next= 0;
    /* Extract number of blocks */
    while (next_byte & (1 << --bit_idx))
    {
      if (++nblocks == 9)
        return(array_end - array);
      if (! bit_idx)
      {
        bit_idx= 8;
        next_byte= *++buf;
      }
    }
    if (! bit_idx)
    {
      bit_idx= 8;
      next_byte= *++buf;
    }

    for (;; nblocks--)
    {
      switch (bit_idx) {
        case 1:
          next+= (next_byte & 1) << 3;
          bit_idx= 5;
          next_byte= *++buf;
          next+= (next_byte >> 5) & 0xf;
          break;
        case 2:
          next+= (next_byte & 3) << 2;
          bit_idx= 6;
          next_byte= *++buf;
          next+= (next_byte >> 6) & 7;
          break;
        case 3:
          next+= (next_byte & 7) << 1;
          bit_idx= 7;
          next_byte= *++buf;
          next+= (next_byte >> 7) & 3;
          break;
        case 4:
          next+= next_byte & 0xf;
          bit_idx= 8;
          next_byte= *++buf;
          break;
        case 5:
          bit_idx= 1;
          next+= (next_byte >> 1) & 0xf;
          break;
        case 6:
          bit_idx= 2;
          next+= (next_byte >> 2) & 0xf;
          break;
        case 7:
          bit_idx= 3;
          next+= (next_byte >> 3) & 0xf;
          break;
        case 8:
          bit_idx= 4;
          next+= (next_byte >> 4) & 0xf;
          break;
      }
      if (nblocks > 1)
        next= (next + 1) << 4;
      else
        break;
    }
    prev+= next;
    *array_end= prev;
  }
  return(array_end - array);
}


/****************** Sort and Search routines ***************/

#ifdef HAVE_DEBUG
void UdmSort(void *base, size_t nmemb, size_t size,
             int(*compar)(const void *, const void *))
{
  UDM_ASSERT(nmemb > 0);
  qsort(base, nmemb, size, compar);
}

void *UdmBSearch(const void *key, const void *base, size_t nmemb,
                 size_t size, int (*compar)(const void *, const void *))
{
  UDM_ASSERT(nmemb > 0);
  return bsearch(key, base, nmemb, size, compar);
}
#endif


/*************** Directory routines **************/

#ifdef WIN32
#include <winreg.h>
#endif

size_t
UdmGetDir(char *d, size_t dlen, enum udm_dirtype_t type)
{
#ifdef WIN32
  HKEY hKey;
  LONG res= RegOpenKeyEx(HKEY_LOCAL_MACHINE, UDM_REG_DIRS_KEY, 0, KEY_READ, &hKey);
  size_t ret= 0;
  if (res == ERROR_SUCCESS)
  {
    char buf[MAX_PATH];
    DWORD dwType;
    DWORD cbData= MAX_PATH;
    if (ERROR_SUCCESS == RegQueryValueEx(hKey, UDM_REG_INSTALLDIR_VALUE,
                                         NULL, &dwType, (LPBYTE)buf, &cbData))
      ret= udm_snprintf(d, dlen, "%s%s",
                        buf, type == UDM_DIRTYPE_SHARE ? "\\create" : "");
    RegCloseKey(hKey);
  }
  return ret ?  ret : GetCurrentDirectory(dlen, d);
#else
  char *env;
  switch (type)
  {
    case UDM_DIRTYPE_CONF:
      if (!(env= getenv("UDM_CONF_DIR")))
        env= getenv("UDM_ETC_DIR");
      return udm_snprintf(d, dlen, "%s", env ? env : UDM_CONF_DIR);

    case UDM_DIRTYPE_SHARE:
      env= getenv("UDM_SHARE_DIR");
      return udm_snprintf(d, dlen, "%s", env ? env : UDM_SHARE_DIR);

    case UDM_DIRTYPE_VAR:
      env= getenv("UDM_VAR_DIR");
      return udm_snprintf(d, dlen, "%s", env ? env : UDM_VAR_DIR);

    case UDM_DIRTYPE_TMP:
      if (!(env= getenv("UDM_TMP_DIR")))
        env= getenv("TMPDIR");
      return udm_snprintf(d, dlen, "%s", env ? env : UDM_TMP_DIR);
    default:
      *d= '\0';
  }
#endif
  return 0;
}


size_t
UdmGetFileName(char *d, size_t dlen, enum udm_dirtype_t type, const char *fname)
{
  size_t len= UdmGetDir(d, dlen, type);
  if (len < dlen)
    len+= udm_snprintf(d + len, dlen - len, "%s%s", UDMSLASHSTR, fname);
  return len;
}
