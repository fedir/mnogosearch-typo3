#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "../include/udm_config.h"

#define UDM_TEST_PASS 0
#define UDM_TEST_FAIL 1
#define UDM_TEST_SKIP 2
#define UDM_TEST_MASK 3 /* do not match mask */

#define UDM_FREE(x)    {if((x)!=NULL){free(x);x=NULL;}}

/*
  The version with logfile does not mix
  output from msearchtest and from the programs being
  executed well.
*/

/*
#define USE_LOGFILE_NOT_STDERR
*/

#ifdef USE_LOGFILE_NOT_STDERR
FILE *logfile;
#endif

static int
UdmSetEnv(const char * name,const char * value)
{
#ifdef HAVE_SETENV
  return(setenv(name,value,1));
#else
#ifdef HAVE_PUTENV
  int res;
  char * s;
  s=(char*)UdmMalloc(strlen(name)+strlen(value)+3);
  sprintf(s,"%s=%s",name,value);
  res=putenv(s);
  UDM_FREE(s);
  return res;
#else
  return 0;
#endif
#endif
}


static void
UdmUnsetEnv(const char * name)
{
#ifdef HAVE_UNSETENV
  unsetenv(name);
#else
  UdmSetEnv(name,"");
#endif
}


static char *
str_store(char *dest, char *src)
{
  size_t d_size= (dest ? strlen(dest) : 0);
  size_t s_size= strlen(src) + 1;
  char *_= realloc(dest, d_size + s_size);

  if (_) memcpy(_ + d_size, src, s_size);
  else free(dest);
  return(_);
}


static char *
ParseEnvVar (char *str)
{
  char *p1= (char *)str;
  char *p2= (char *)str;
  char *p3;
  char *s;
  char *_= NULL;

  if (! str) return(NULL);
  while ((p1= strchr(p1, '$')))
  {
    if (p1[1] != '(')
    {
      p1++;
      continue;
    }
    *p1= 0;
    _= str_store(_, p2);
    *p1= '$';
    if ((p3= strchr(p1 + 2, ')')))
    {
      *p3= 0;
      s= (char *)getenv(p1 + 2);
      if (s) _= str_store(_, s);
      *p3= ')';
      p2= p1= p3 + 1;
    }
    else
    {
      free(_);
      return(NULL);
    }
  }
  _= str_store(_, p2);
  return(_);
}


static char *
strf(const char *fmt, va_list ap)
{
  int nc;
  char *str= NULL;
  char *p;
  size_t size= 512;

  while (1)
  {
    p= realloc(str, size);
    if (! p)
    {
      free(str);
      str= NULL;
      break;
    }
    str= p;
    nc= vsnprintf(str, size, fmt, ap);
    if (nc < size) break;
    size*= 2;
  }
  return(str);
}


static char *
mem_printf(const char *fmt, ...)
{
  va_list ap;
  char *_;

  va_start(ap, fmt);
  _= strf(fmt, ap);
  va_end(ap);
  return(_);
}


static void
debug(const char *fmt, ...)
{
  va_list list;
  time_t t= time(NULL);
  char *tim= ctime(&t);
  char *str;

  tim += 4;
  tim[strlen(tim) - 6]= 0;

  va_start(list, fmt);
  str= strf(fmt, list);
  va_end(list);

#ifdef USE_LOGFILE_NOT_STDERR
  fseek(logfile, 0, SEEK_END);
  fprintf(logfile, "%s: %s\n", tim, str);
  fflush(logfile);
#else
  fprintf(stderr, "%s: %s\n", tim, str);
#endif
}


static int
testenv(char *key, const char *val)
{
  char *p= getenv(key);
  if (! p) return(1);
  else if (*val && strcmp(p, val)) return(2);
  return(0);
}


static int
substrenv(char *key, char *val)
{
  char *p= getenv(key);
  if (! p) return(1);
  else if (! strstr(p, val)) return(2);
  return(0);
}


static int
mdiff(char *src, char *dst)
{
  int src_fd;
  int dst_fd;
  char src_buf[1024];
  char dst_buf[sizeof(src_buf)];
  ssize_t src_rs;
  ssize_t dst_rs;
  int _= 0;

  src_fd= open(src, O_RDONLY);
  if (src_fd < 0)
  {
    debug("Unable to open src (%s).", strerror(errno));
    return(1);
  }

  dst_fd= open(dst, O_RDONLY);
  if (dst_fd < 0)
  {
    close(src_fd);
    debug("Unable to open dst (%s).", strerror(errno));
    return(1);
  }

  while (1)
  {
    src_rs= read(src_fd, src_buf, sizeof(src_buf));
    dst_rs= read(dst_fd, dst_buf, sizeof(dst_buf));

    if (src_rs > dst_rs)
    {
      debug("src > dst.");
      _= 1;
      break;
    }
    else if (src_rs < dst_rs)
    {
      debug("src < dst.");
      _= 1;
      break;
    }
    else if (! src_rs)
      break;

    if (memcmp(src_buf, dst_buf, src_rs))
    {
      debug("src and dst differs.");
      _= 1;
      break;
    }
  }
  close(src_fd);
  close(dst_fd);
  return(_);
}


typedef int (*qsort_cmp)(const void*, const void*);


static int
cmpdirent(char **a1, char **a2)
{
  return(strcmp(*a1, *a2));
}


static void
freedir (char **dir)
{
  char **p;

  if (! dir) return;
  for (p= dir; *p; p++) UdmFree(*p);
  UdmFree(dir);
}


static char **
loaddir(const char *path, int sort)
{
  struct dirent *buf;
  char **_, **tmp;
  size_t nrecs= 0, arecs= 32;
  DIR *dir;

  if (! path) return(NULL);

  dir= opendir(path);
  if (! dir) return(NULL);
  _= UdmMalloc(arecs * sizeof(char *));
  if (! _)
  {
    closedir(dir);
    return(NULL);
  }

  while ((buf= readdir(dir)))
  {
    if (arecs < nrecs + 1)
    {
      arecs += 32;
      tmp= UdmRealloc(_, arecs * sizeof(char *));
      if (! tmp)
      {
        closedir(dir);
        _[nrecs]= NULL;
        freedir(_);
        return(NULL);
      }
      _= tmp;
    }

    _[nrecs++]= UdmStrdup(buf->d_name);
  }

  closedir(dir);

  if (sort)
    qsort(_, nrecs, sizeof(char *), (qsort_cmp)cmpdirent);
  _[nrecs]= NULL;
  return(_);
}


static int
UdmTest(char *test)
{
  char *p;
  char *p1;
  FILE *fp;
  char buf[4096];
  int rc= UDM_TEST_FAIL;
  int inverse;
  int expected;
  int c;
  int cr;
  char *str;
  size_t l;
  size_t i;

  fp= fopen(test, "r");
  if (!fp)
  {
    debug("fopen failed: %s", strerror(errno));
    return(UDM_TEST_FAIL);
  }

  while (fgets(buf, sizeof(buf), fp))
  {
    char *end;
    if (buf[0]=='#')continue;
    for (end=buf+strlen(buf)-1; (end>=buf) && strchr(" \r\n\t",end[0]); *end--='\0');
    if (!buf[0])continue;
    
    debug("%s", buf);
    str= ParseEnvVar(buf);
    if (! str)
    {
      debug("ParseEnvVar failed.");
      continue;
    }
    
    p= str;
    while (isspace(*p)) p++;

    if (! strncmp(p, "pass", 4))
    {
      c= UDM_TEST_PASS;
      p += 4;
    }
    else if (! strncmp(p, "fail", 4))
    {
      c= UDM_TEST_FAIL;
      p += 4;
    }
    else if (! strncmp(p, "skip", 4))
    {
      c= UDM_TEST_SKIP;
      p += 4;
    }
    else
    {
      debug("msearchtest: Syntax error: unknown command");
      free(str);
      rc= UDM_TEST_FAIL;
      break;
    }

    while (isspace(*p)) p++;

    if (*p == '!')
    {
      inverse= 1;
      p++;
      while (isspace(*p)) p++;
    } else inverse= 0;

    p1= p;
    while (*p1 && ! isspace(*p1)) p1++;
    if (! *p1)
    {
      free(str);
      rc= UDM_TEST_FAIL;
      break;
    }
    *p1= 0;
    expected= atoi(p);
    p= p1 + 1;
    while (isspace(*p)) p++;
    l= strlen(p);
    for (i= l; i > 0; i--)
    {
      if (isspace(p[l - 1])) p[l - 1]= 0;
      else break;
    }
    if (! strncmp(p, "exec", 4))
    {
      p += 4;
      while (isspace(*p)) p++;
      cr= system(p);
      /*
      TODO
      if (cr && c != UDM_TEST_SKIP && ((cr == expected) ^ inverse))
      {
        printf("failed.\nCommand '%s' failed unexpectedly\n", p);
      }
      */
    }
    else if (! strncmp(p, "mdiff", 5))
    {
      char *f1, *f2;
      p += 5;
      while (isspace(*p)) p++;
      f1= p;
      while (*p && ! isspace(*p)) p++;
      *p= 0;
      p++;
      while (isspace(*p)) p++;
      f2= p;
      cr= mdiff(f1, f2);
    }
    else if (! strncmp(p, "testenv", 7))
    {
      char *f1, *f2;
      p += 8;
      while (isspace(*p)) p++;
      f1= p;
      while (*p && ! isspace(*p)) p++;
      if (*p)
      {
        *p= 0;
        p++;
        while (isspace(*p)) p++;
        f2= p;
        cr= testenv(f1, f2);
      }
      else
        cr= testenv(f1, "");
    }
    else if (! strncmp(p, "substrenv", 9))
    {
      char *f1, *f2;
      p += 10;
      while (isspace(*p)) p++;
      f1= p;
      while (*p && ! isspace(*p)) p++;
      *p= 0;
      p++;
      while (isspace(*p)) p++;
      f2= p;
      cr= substrenv(f1, f2);
    }
    else
    {
      free(str);
      debug("Syntax error.\n");
      rc= UDM_TEST_FAIL;
      break;
    }
    
    if ((cr == expected) ^ inverse)
    {
      free(str);
      rc= c;
      debug("Test condition: true");
      break;
    }
    else
    {
      debug("Test condition: false");
    }
  }
  fclose(fp);
  return rc;
}

static char **
split(const char *sep, const char *str)
{
  char *p;
  char **_= NULL;
  char *buf;
  size_t c= 0;

  buf= strdup(str);
  if (! buf) return(NULL);
  p= strtok(buf, sep);
  while (p) {
    c++;
    _= realloc(_, sizeof(char *) * c);
    _[c - 1]= p;
    p= strtok(NULL, sep);
  }
  c++;
  _= realloc(_, sizeof(char *) * c);
  _[c - 1]= NULL;
  return(_);
}


static void
UdmSetDBAddrs(const char *str)
{
  char **a;
  char **p;
  char *key;
  size_t c= 0;

  a= split(",", str);
  for (p= a; *p; p++)
  {
    key= mem_printf("UDM_TEST_DBADDR%d", c);
    UdmSetEnv(key, *p);
    c++;
    free(key);
  }
  free(a);
}


static void
UdmUnSetDBAddrs (const char *str)
{
  char **a;
  char **p;
  char *key;
  size_t c= 0;

  a= split(",", str);
  for (p= a; *p; p++)
  {
    key= mem_printf("UDM_TEST_DBADDR%d", c);
    UdmUnsetEnv(key);
    c++;
    free(key);
  }
  free(a);
}


static int
about(void)
{
fprintf(stderr,"\n\
In order to run mnoGoSearch test suit please set \n\
UDM_TEST_DBADDR environment variable with a semicolon \n\
separated list of DBAddr you'd like to run tests with. \n\
\n\
You can do it by adding a line into your ~.bashrc using this example:\n\
\n\
export UDM_TEST_DBADDR=\"mysql://localhost/test/;pgsql://root@/root/\"\n\
\n\
Note, tests will destroy all existing data in the given databases,\n\
please use temporary databases for tests purposes.\n\
\n");
  return 1;
}



static char *UDM_TEST_ROOT= NULL;


static int
UdmTest2(const char *pdb, const char *rec, const char *mask)
{
  struct stat st;
  int res;
  size_t i;
  char *p;
  UdmSetDBAddrs(pdb);
  if (*rec == '.')
    return UDM_TEST_MASK;
  if (strncmp(rec, "test", 4))
    return UDM_TEST_MASK;
  if (mask)
  {
    char maskbuf[128];
    size_t masklen= snprintf(maskbuf, sizeof(maskbuf), "%s", mask);
    if (masklen && maskbuf[masklen - 1] == '/')
    {
      maskbuf[masklen - 1]= '\0';
    }
    if (mask && strcmp(rec, maskbuf))
      return UDM_TEST_MASK;
  }
  p= mem_printf("%s/%s", UDM_TEST_ROOT, rec);
  if (!p)
  {
    debug("mem_printf failed.");
    return UDM_TEST_FAIL;
  }
  if (lstat(p, &st) < 0)
  {
    debug("stat %s failed (%s).", rec, strerror(errno));
    return UDM_TEST_FAIL;
  }
  if (!S_ISDIR(st.st_mode))
    return UDM_TEST_FAIL;
  debug("Starting test %s", rec);
  printf("%s", rec);
  for (i= 0; i < (int)(35 - strlen(rec)); i++) putchar('.');
  if (UdmSetEnv("UDM_ETC_DIR", p) < 0)
  {
    debug("setenv failed (%s).", strerror(errno));
    free(p);
    return UDM_TEST_FAIL;
  }
  if (UdmSetEnv("UDM_TEST_DIR", p) < 0)
  {
    debug("setenv failed (%s).", strerror(errno));
    free(p);
    return UDM_TEST_FAIL;
  }
  free(p);
  p= mem_printf("%s/%s/test.cmd", UDM_TEST_ROOT, rec);
  if (! p)
  {
    debug("mem_printf failed.");
    return UDM_TEST_FAIL;
  }
  
  debug("Running: %s", p);
  res= UdmTest(p);
  free(p);
  
  debug("----------------------------------");
  debug("------------- END OF %s", rec);
  debug("----------------------------------");
  UdmUnSetDBAddrs(pdb);
  return res;
}


int main (int argc, char **argv)
{
  int res;
  char *UDM_TEST_LOG;
  char *UDM_TEST_DBADDR;
  char **DBAddrs;
  char **pdb;
  char *mask;
  size_t rpos;
  char **recs;
  int test_pass= 0;
  int test_fail= 0;
  int test_skip= 0;
  
  mask= (argc == 2) ? argv[1] : NULL;
  
  if (!(UDM_TEST_DBADDR= getenv("UDM_TEST_DBADDR")))
  {
    fprintf(stderr, "\nEnvironment variable 'UDM_TEST_DBADDR' is not set.\n");
    return about();
  }

  if (!(UDM_TEST_LOG= getenv("UDM_TEST_LOG")))
  {
    fprintf(stderr, "Environment variable 'UDM_TEST_LOG' is not set.\n");
    return(1);
  }

  if (!(UDM_TEST_ROOT= getenv("UDM_TEST_ROOT")))
  {
    fprintf(stderr, "Environment variable 'UDM_TEST_ROOT' is not set.\n");
    return(1);
  }

  if (!(recs= loaddir(UDM_TEST_ROOT, 1)))
  {
    fprintf(stderr, "loaddir '%s' failed (%s).", UDM_TEST_ROOT, strerror(errno));
    return(1);
  }

#ifdef USE_LOGFILE_NOT_STDERR
  if (!(logfile= fopen(UDM_TEST_LOG, "w")))
  {
    fprintf(stderr, "fopen '%s' failed: %s.\n", UDM_TEST_LOG, strerror(errno));
    return(1);
  }
#endif

  printf("Starting tests from '%s'.\n", UDM_TEST_ROOT);
  debug("Starting test from '%s'.", UDM_TEST_ROOT);

  
  for (DBAddrs= split(";", UDM_TEST_DBADDR), pdb= DBAddrs; *pdb; pdb++)
  {
    printf("DBAddr: %s\n", *pdb);
    debug("DBAddr: %s", *pdb);

    for (rpos= 0; recs[rpos]; rpos++)
    {
      res= UdmTest2(*pdb, recs[rpos], mask);

      switch (res)
      {
        case UDM_TEST_PASS: printf("passed.\n"); test_pass++; break;
        case UDM_TEST_FAIL: printf("failed.\n"); test_fail++; break;
        case UDM_TEST_SKIP: printf("skipped.\n"); test_skip++; break;
        case UDM_TEST_MASK:
        default: /* test skipped because of mask do nothing*/ ;
      }
    }
  }

  printf("Statistic: total %d, passed %d, skipped %d, failed %d.\n",
         test_pass + test_fail + test_skip, test_pass, test_skip, test_fail);
  
  if (test_fail)
  {
    printf("Some tests failed. Please check '%s' for further detailes.\n",
           UDM_TEST_LOG);
  }
  return test_fail ? 1 : 0;
}
