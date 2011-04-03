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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#include "udm_common.h"
#include "udm_url.h"
#include "udm_utils.h"



UDM_URL * __UDMCALL UdmURLInit(UDM_URL *url)
{
  bzero((void*)url, sizeof(UDM_URL));
  return url;
}

void __UDMCALL UdmURLFree(UDM_URL *url)
{
  UDM_FREE(url->schema);
  UDM_FREE(url->specific);
  UDM_FREE(url->hostinfo);
  UDM_FREE(url->auth);
  UDM_FREE(url->hostname);
  UDM_FREE(url->path);
  UDM_FREE(url->filename);
  UDM_FREE(url->anchor);
  url->port= 0;
  url->default_port= 0;
}


static const char *hexd= "0123456789ABCDEF";


static int ch2x(int ch)
{
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  
  if (ch >= 'a' && ch <= 'f')
    return 10 + ch - 'a';
  
  if (ch >= 'A' && ch <= 'F')
    return 10 + ch - 'A';
  
  return -1;
}


static int get_default_port(const char *schema)
{
  if(!strcasecmp(schema,"http"))
    return 80;
  if(!strcasecmp(schema,"https"))
    return 443;
  if(!strcasecmp(schema,"nntp"))
    return 119;
  if(!strcasecmp(schema,"news"))
    return 119;
  if(!strcasecmp(schema,"ftp"))
    return 21;
  return 0;
}


/*
  0 == safe and reserved characters
  1 == control characters
  2 == unsafe characters: space " ; < > ? [ \ ] ^
  3 == non-graph ascii characters
  4 == ampersand
  
  TODO:
  
  &.txt -> &amp;.txt (only in file name)
  :.txt -> ./:.txt   (only in file name)
  #.txt -> %22.txt   (only in file name)
  ?.txt -> %3f.txt   (only in file name)
*/

static char path_enc_type[256]=
{
/*00*/  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/*10*/  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/*20*/  2,0,2,2,0,2,4,0,0,0,0,0,0,0,0,0,  /*  !"#$%&'()*+,-./ */
/*30*/  0,0,0,0,0,0,0,0,0,0,0,2,2,0,2,2,  /* 0123456789:;<=>? */
/*40*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* @ABCDEFGHIJKLMNO */
/*50*/  0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,0,  /* PQRSTUVWXYZ[\]^_ */
/*60*/  2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* `abcdefghijklmno */
/*70*/  0,0,0,0,0,0,0,0,0,0,0,2,2,2,0,1,  /* pqrstuvwxyz{|}~  */
/*80*/  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
/*90*/  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
/*A0*/  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
/*B0*/  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
/*C0*/  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
/*D0*/  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
/*E0*/  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
/*F0*/  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3
};


/* 
  Convert escaped characters to upper case: 0xea -> 0xEA
  Unescape safe characters
  Escape unsafe characters
  dst should be (UDM_URL_CANONIZE_MULTIPLY*strlen(src) + 1) bytes long
*/
size_t UdmURLCanonizePath(char *dst, size_t dstsize, const char *src)
{
  char *d0= dst;
  char *de= dst + dstsize;
  int query = 0;
  
  for ( ; src[0] && dst < de ; src++)
  {
    int code;
    
    if (src[0] == '%' && ch2x(src[1]) >= 0 && ch2x(src[2]) >= 0)
    {
      code= ch2x(src[1])*16 + ch2x(src[2]);
      if (path_enc_type[code])
      {
        if (dst+3 < de)
        {
          *dst++= '%';
          *dst++= hexd[ch2x(src[1])];
          *dst++= hexd[ch2x(src[2])];
          src+=2;
          continue;
        }
        else
          break;
      }
      else
      {
        /* Remove escaping from a safe character */
        *dst++= code;
        src+=2;
        continue;
      }
    }
    
    if (src[0] == '?' && ! query)
    {
      query = 1;
      *dst++= src[0];
      continue;
    }
    
    code= (int)(unsigned char)src[0];
    if (path_enc_type[code] && ! (path_enc_type[code] == 4 && query))
    {
      if (dst+3 < de)
      {
        *dst++= '%';
        *dst++= hexd[code >> 4];
        *dst++= hexd[code & 0x0F];
        continue;
      }
      else
        break;
    }
    *dst++= src[0];
  }
  if (dst < de)
    dst[0]= '\0';
  return dst-d0;
}


/*
  Convert query string into its canonical form.

  SYNOPSIS
    UdmURLQueryStringCanonize(char *dst, const char *src)
    dst        destination
    src        source

  DESCRIPTION
    Convert query string into canonical form by
    URL-encoding of special characters.
    Safe characters are not unescaped, for example,
    %41 is not unescaped to "A" (unlike in UdmURLCanonizePath).

    dst must be big enough, at least 3*strlen(src)+1 bytes.
   
  NOTE
  
    After click on a link having a query string like

      <a href="?a=xxx">here</a>

    the URL which appear in the Location bar depends
    on the browser type - various browsers escape
    different sets of special characters:

    Explorer:
      %20 space

    Konquerror:
      %20 space
      %25 % (when not followed by two hex digits)

    Opera:
      %20 space
      %22 "
      %3C <
      %3E >
      %60 `
      
    Mozilla:
      %20 space
      %22 "
      %3C <
      %3E >
      %60 `
      %7F <del>
      
    Let's go Mozilla'a way.

  RETURN VALUES
    N/A
*/

static char query_enc_type[256]=
{
/*00*/  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/*10*/  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/*20*/  1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,  /*  !"#$%&'()*+,-./ */
/*30*/  0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,  /* 0123456789:;<=>? */
/*40*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* @ABCDEFGHIJKLMNO */
/*50*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* PQRSTUVWXYZ[\]^_ */
/*60*/  1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* `abcdefghijklmno */
/*70*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,  /* pqrstuvwxyz{|}~  */
/*80*/  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/*90*/  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/*A0*/  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/*B0*/  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/*C0*/  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/*D0*/  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/*E0*/  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/*F0*/  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
};


static void
UdmURLQueryStringCanonize(char *dst, const char *src)
{
  for (; *src; src++)
  {
    if (query_enc_type[(unsigned char) *src])
    {
      *dst++= '%';
      *dst++= hexd[((unsigned char)*src) >> 4];
      *dst++= hexd[((unsigned char)*src) & 0x0F];
    }
    else
      *dst++= *src;
  }
  *dst= '\0';
}


static void
UdmURLQueryStringAppend(char *dst, const char *query)
{
  if (query)
  {
    char *end= dst + strlen(dst);
    *end++= '?';
    UdmURLQueryStringCanonize(end, query);
  }
}


int UdmURLParse(UDM_URL *url,const char *src)
{
  char *anchor, *query;
  const char *schema;
  size_t oldlen, newlen;
  
  UdmURLFree(url);
  
  /* 
    Find possible schema end then check that it is really schema
    According to RFC, it consists of alphas, digits, + and -  signs.
    Otherwise, it is not a schema, for example:
    "mod/index.html#a:1" is an anchor not a schema.
  */
  
  if((schema= strchr(src,':')))
  {
    const char * ch;
    for(ch= src; ch < schema; ch++)
    {
      if(!isalnum(*ch) && !strchr("+-." ,ch[0]))
      {
        /* Bad character, not a schema */
        schema= 0;
        break;
      }
    }
  }
  
  
  if(schema)
  {
    char *s;
	
    /* Have scheme - absolute path */
    url->schema= (char*)UdmStrndup(src, schema-src);
    for (s= url->schema; *s; *s=tolower(*s), s++);
    url->specific= (char*)UdmStrdup(schema + 1);
    url->default_port= get_default_port(url->schema);
    
    if(url->specific[0] == '/' && url->specific[1] == '/')
    {
      const char *end, *beg= url->specific+2;
      const char *hostname;
      
      /* Have hostinfo */
      if((end= strchr(beg,'/')))
      {
        /* Have hostname with path */
        url->path= (char*)UdmStrdup(end);
        url->hostinfo= (char*)UdmStrndup(beg, end-beg);
      }
      else
      {
        /* Hostname without path */
        url->hostinfo= (char*)UdmStrdup(url->specific + 2);
        url->path= (char*)UdmStrdup("/");
      }
      
      if((hostname= strchr(url->hostinfo,'@')))
      {
        /* Username and password is given  */
        /* Store auth string user:password */
        url->auth= (char*)UdmStrndup(url->hostinfo, hostname - url->hostinfo);
        hostname++;
      }
      else
      {
        hostname= url->hostinfo;
      }
      
      if((end= strchr(hostname,':')))
      {
        url->hostname= (char*)UdmStrndup(hostname, end - hostname);
        url->port= atoi(end+1);
      }
      else
      {
        url->hostname= (char*)UdmStrdup(hostname);
        url->port= 0;
      }
      
      /* Convert hostname to lowercase */
      for (s= url->hostname; *s; *s= tolower(*s), s++);
    }
    else
    {
      /*
        Have no host but have schema.
        This is possible for:
        file: exec: cgi: mailto: htdb: news:
        As far as we do not need mailto: just ignore it
      */
      
      if(!strcasecmp(url->schema,"mailto") ||
         !strcasecmp(url->schema,"javascript"))
        return(UDM_URL_BAD);
      else if(!strcasecmp(url->schema,"file") ||
              !strcasecmp(url->schema,"exec") ||
              !strcasecmp(url->schema,"cgi")  ||
              !strcasecmp(url->schema,"htdb"))
        url->path= (char*)UdmStrdup(url->specific);
      else if(!strcasecmp(url->schema,"news"))
      {
        /* 
          We will use localhost as NNTP server
          if it is not indicated in the URL 
        */
        url->hostname= (char*)UdmStrdup("localhost");
        url->path= (char*)UdmMalloc(strlen(url->specific) + 2);
        sprintf(url->path,"/%s",url->specific);
        url->default_port= 119;
      }
      else
      {
        /* Unknown schema */
        return UDM_URL_BAD;
      }
    }
  }
  else
  {
    /* No schema */
    url->path= (char*)UdmStrdup(src);
  }
  
  /* Cut the anchor if exists */
  if((anchor= strchr(url->path,'#')))
    *anchor=0;
  
  
  oldlen= strlen(url->path);
  newlen= 3 * oldlen + 1;

  if ((query= strchr(url->path, '?')))
  {
    *query++= '\0';
    if (!*query)
      query= NULL;
  }
  
  /*
    If path is relative then just copy it to filename
    i.e. neither  /usr/local/ nor c:/windows/temp/
  */
  
  if ((url->path[0] != '/') && url->path[0] && (url->path[1] != ':'))
  { 
    /* Relative path */
    url->filename= (char*) UdmMalloc(newlen);
    strcpy(url->filename, url->path + (strncmp(url->path,"./",2) ? 0 : 2));
    UdmURLQueryStringAppend(url->filename, query);
    url->path[0]= 0;
  }
  else
  {
    /* Absolute path */
    char *file, *path;

    if (!(path= UdmMalloc(newlen)))
      return UDM_ERROR;
    
    UdmURLCanonizePath(path, newlen, url->path);
    UdmURLNormalizePath(path);

    if ((file= strrchr(path, '/')) && file[1])
    {
      file++;
      url->filename= (char*) UdmMalloc(newlen);
      strcpy(url->filename, file);
      *file= '\0';  /* Remove file part from path */
    }
    
    if (query && !url->filename)
    {
      url->filename= (char*) UdmMalloc(newlen);
      url->filename[0]= '\0';
    }
    
    UdmURLQueryStringAppend(url->filename, query);
    
    /*
      We cannot free url->path earlier
      because query points to its part
    */
    UdmFree(url->path);
    url->path= path;
  }    
  return UDM_OK;
}


char * UdmURLNormalizePath(char * str)
{
  char *q, *d, *s=str;

  /* Temporarily hide query string */

  if((q= strchr(s,'?')))
  {
    *q++='\0';
    if(!*q)
      q=NULL;
  }

  /* Remove all "/../" entries */

  while((d= strstr(str,"/../")))
  {
    char * p;
    
    if(d > str)
    {
      /* Skip non-slashes */
      for (p= d-1; (*p != '/') && (p > str); p--);
      
      /* Skip slashes */
      for( ; (p>(str+1)) && (*(p-1)=='/'); p--);
    }
    else
    {
      /*
        We are at the top level and have ../
        Remove it too to avoid loops          
      */
      p=str;
    }
    memmove(p,d+3,strlen(d)-2);
  }

  /* Remove remove trailig "/.." */

  d=str+strlen(str);
  if((d-str> 2)&&(!strcmp(d-3,"/..")))
  {
    d-=4;
    while((d>str)&&(*d!='/'))d--;
    if(*d=='/')
      d[1]='\0';
    else
      strcpy(str,"/");
  }

  /* Remove all "/./" entries */
  
  while((d=strstr(str,"/./")))
    memmove(d,d+2,strlen(d)-1);
  
  /* Remove the trailing "/."  */
  
  if((d=str+strlen(str))>(str+2))
    if(!strcmp(d-2,"/."))
      *(d-1)='\0';
  
  /* Remove all "//" entries */
  while((d=strstr(str,"//")))
    memmove(d,d+1,strlen(d));
  
  
  /*
    Replace "%7E" with "~"
    Actually it is to be done
    for all valid characters
    which do not require escaping
    However I'm lazy, do it for 7E
    as the most often "abused"     
  */
  
  while((d=strstr(str,"%7E")))
  {
    *d='~';
    memmove(d+1,d+3,strlen(d+3)+1);
  }

  /* Restore query string */

  if(q)
  {
    char *e= str + strlen(str);
    *e='?';
    memmove(e+1,q,strlen(q)+1);
  }

  return str;
}


size_t UdmURLCanonize(const char *src, char *dst, size_t dstsize)
{
  UDM_URL url;
  size_t res;
  
  UdmURLInit(&url);
  if (UdmURLParse(&url, src) || !url.schema)
  {
    res= udm_snprintf(dst, dstsize, "%s", src);
  }
  else if (!strcmp(url.schema, "mailto") ||
           !strcmp(url.schema, "javascript"))
  {
    const char *specific= url.specific ? url.specific : "";
    res= udm_snprintf(dst, dstsize, "%s:%s", url.schema, specific);
  }
  else if (!strcmp(url.schema, "htdb"))
  {
    const char *path= url.path ? url.path : "/";
    const char *file= url.filename ? url.filename : "";
    res= udm_snprintf(dst, dstsize, "%s:%s%s", url.schema, path, file);
  }
  else
  {
    const char *path= url.path ? url.path : "/";
    const char *file= url.filename ? url.filename : "";
    const char *host= url.hostname ? url.hostname : "";
    const char *auth= url.auth ? url.auth : "";
    const char *at= url.auth ? "@" : "";
    const char *colon= "";
    char port[10]= "";
    
    if (url.port && url.port != url.default_port)
    {
      sprintf(port, "%d", url.port);
      colon= ":";
    }
    res= udm_snprintf(dst, dstsize, "%s://%s%s%s%s%s%s%s", 
                      url.schema, auth, at, host, colon, port, path, file);
  }
  UdmURLFree(&url);
  return res;
}
