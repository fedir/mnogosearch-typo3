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
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>


#ifdef WIN32
#include <winsock.h>
#include <time.h>
#include <direct.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_SELECT_H
#include <select.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef USE_HTTPS
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include "udm_dirent.h"
#include "udm_common.h"
#include "udm_proto.h"
#include "udm_utils.h"
#include "udm_mutex.h"
#include "udm_socket.h"
#include "udm_http.h"
#include "udm_ftp.h"
#include "udm_xmalloc.h"
#include "udm_host.h"
#include "udm_execget.h"
#include "udm_id3.h"
#include "udm_log.h"
#include "udm_vars.h"
#include "udm_db.h"
#include "udm_match.h"
#include "udm_agent.h"
#include "udm_contentencoding.h"
#include "udm_url.h"
#include "udm_indexer.h" /* For UdmDocProcessContentResponseHeaders */

/************** connect with timeout **********/

static int
connect_tm(int s,
           const struct sockaddr *name,
           unsigned int namelen,
           unsigned int to)
{
#ifdef WIN32
  return connect(s, name, namelen);
#else
  int flags, res, s_err;
  socklen_t s_err_size= sizeof(int);
  fd_set sfds;
  struct timeval tv;

  if (!to)
    return connect(s,name, namelen);

  flags= fcntl(s, F_GETFL, 0);     /* Set socket to non-blocked */
#ifdef O_NONBLOCK
  fcntl(s, F_SETFL, flags | O_NONBLOCK);  /* and save the flags        */
#endif

  res= connect(s, name, namelen);
  s_err= errno;      /* Save errno */
  fcntl(s, F_SETFL, flags);
  if ((res != 0) && (s_err != EINPROGRESS))
  {
    errno= s_err;    /* Restore errno              */
    return -1;       /* Unknown error, not timeout */
  }
  if (!res)
    return 0;        /* Quickly connected */

  FD_ZERO(&sfds);
  FD_SET(s,&sfds);

  tv.tv_sec= (long) to;
  tv.tv_usec= 0;
  
  while(1)
  {
    res= select(s+1, NULL, &sfds, NULL, &tv);
    if(res == 0)
      return -1;          /* Timeout */
    if(res < 0)
    {
      if (errno == EINTR) /* Signal */
        continue;
      else
        return -1 ;       /* Error */
    }
    break;
  }

  s_err= 0;
  if (getsockopt(s, SOL_SOCKET, SO_ERROR, (char*) &s_err, &s_err_size) != 0)
    return -1;            /* Can't get sock options */

  if (s_err)
  {
    errno= s_err;
    return -1;
  }
  return 0;      /* We have a connection! */
#endif
}


/************** Open TCP Connect **************/


static int
open_host(UDM_AGENT *Agent, UDM_DOCUMENT *Doc)
{
  int net, res;
  
  UdmAgentSetTask(Agent, "Connecting");
  net= socket(AF_INET, SOCK_STREAM, 0);
  Doc->connp.sin.sin_family= AF_INET;
  res= connect_tm(net,
                  (struct sockaddr *)&Doc->connp.sin, sizeof (struct sockaddr_in),
                  (unsigned int)Doc->Spider.read_timeout);
  UdmAgentSetTask(Agent, "Downloading");
  if (res)
  {
    closesocket(net);
    return UDM_NET_CANT_CONNECT;
  }
  return net;
}


/**************** HTTP codes and messages ************/

__C_LINK const char * __UDMCALL UdmHTTPErrMsg(int code)
{
  switch(code){
  case 0:   return("Not indexed yet");
  case 200: return("OK");
  case 206: return("Partial OK");
  case 301: return("Moved Permanently");
  case 302: return("Moved Temporarily");
  case 303: return("See Other");
  case 304: return("Not Modified");
  case 300: return("Multiple Choices");
  case 305: return("Use Proxy (proxy redirect)");
  case 307: return("Temporary Redirect");
  case 400: return("Bad Request");
  case 401: return("Unauthorized");
  case 402: return("Payment Required");
  case 403: return("Forbidden");
  case 404: return("Not found");
  case 405: return("Method Not Allowed");
  case 406: return("Not Acceptable");
  case 407: return("Proxy Authentication Required");
  case 408: return("Request Timeout");
  case 409: return("Conflict");
  case 410: return("Gone");
  case 411: return("Length Required");
  case 412: return("Precondition Failed");
  case 413: return("Request Entity Too Large");
  case 414: return("Request-URI Too Long");
  case 415: return("Unsupported Media Type");
  case 500: return("Internal Server Error");
  case 501: return("Not Implemented");
  case 502: return("Bad Gateway");
  case 505: return("Protocol Version Not Supported");
  case 503: return("Service Unavailable");
  case 504: return("Gateway Timeout");
  default:  return("Unknown status");
  }
}


/*******************************************************/
#ifdef USE_HTTP

static int
UdmHTTPGet(UDM_AGENT *Agent,UDM_DOCUMENT *Doc)
{
  int fd,res= 0;
  fd_set sfds;
  time_t start_time;
  int status;
  size_t buf_size= UDM_NET_BUF_SIZE;
  struct timeval tv;
    
  Doc->Buf.size= 0;

  /* Connect to HTTP or PROXY server */
  if ((fd= open_host(Agent,Doc)) < 0)
   return fd;

  /* Send HTTP request */
  if(UdmSend(fd, Doc->Buf.buf, strlen(Doc->Buf.buf), 0) < 0 )
    return -1;

  /* Retrieve response */
  tv.tv_sec= (long) Doc->Spider.read_timeout;
  tv.tv_usec= 0;

  start_time= time(NULL);
  while (1)
  {
    FD_ZERO(&sfds);
    FD_SET(fd, &sfds);

    status= select(FD_SETSIZE, &sfds, NULL, NULL, &tv);
    if (status == -1)
    {
      res= UDM_NET_ERROR;
      break;
    }
    else if (status == 0)
    {
      res= UDM_NET_TIMEOUT;
      break;
    }
    else
    {
      if (FD_ISSET(fd,&sfds))
      {
        if(Doc->Buf.size + buf_size > Doc->Buf.maxsize)
          buf_size= Doc->Buf.maxsize - Doc->Buf.size;
        else
          buf_size= UDM_NET_BUF_SIZE;

        status= recv(fd, Doc->Buf.buf+Doc->Buf.size, buf_size, 0);
        if (status < 0)
        {
          res= status;
          break;
        }
        else if (status == 0)
        {
          if(Doc->Spider.doc_timeout < (time(NULL) - start_time))
          {
            res= UDM_NET_TIMEOUT;
          }
          break;
        }
        else
        {
          Doc->Buf.size += status;
          start_time= time(NULL);
          if(Doc->Buf.size == Doc->Buf.maxsize)
            break;
        }
      }
      else
      {
        break;
      }
    }
  }
  closesocket(fd);
  return res;
}
#endif

#ifdef USE_HTTPS

#define sslcleanup closesocket(fd); SSL_free (ssl); SSL_CTX_free (ctx)

#ifndef OPENSSL_VERSION_NUMBER
#define OPENSSL_VERSION_NUMBER 0x0000
#endif

static int UdmHTTPSGet(UDM_AGENT *Indexer,UDM_DOCUMENT *Doc)
{
  int fd;
  int res= 0, status;
  SSL_CTX* ctx;
  SSL*     ssl=NULL;
  SSL_METHOD *meth;
  time_t start_time;
  int buf_size= UDM_NET_BUF_SIZE;
  pid_t pid;

#if OPENSSL_VERSION_NUMBER >= 0x00905100
  while (RAND_status() != 1)
  {
#endif
    /* seed in the current time and process id
     * these are normally 4 bytes each, which should be enough
     * for our necessary 128-bit minimum seed for the PRNG */
    start_time= time(NULL);
    RAND_seed((unsigned char *)&start_time, sizeof(time_t));
    pid= getpid();
    RAND_seed((unsigned char *)&pid, sizeof(pid_t));
#if OPENSSL_VERSION_NUMBER >= 0x00905100
  }
#endif

  /* Connect to HTTPS server */
  if((fd= open_host(Indexer,Doc)) < 0)
     return(fd);

  SSLeay_add_ssl_algorithms();
  meth= SSLv23_client_method();
  SSL_load_error_strings();

  if ((ctx= SSL_CTX_new (meth))==NULL)
  {
    sslcleanup;
    return UDM_NET_ERROR;
  }

  /* -------------------------------------------------- */
  /* Now we have TCP connection. Start SSL negotiation. */

  if ((ssl= SSL_new(ctx)) == NULL)
  {
    sslcleanup;
    closesocket(fd);
    return UDM_NET_ERROR;
  }

  SSL_set_fd (ssl, fd);

  if (SSL_connect(ssl) < 0)
  {
    sslcleanup;
    return UDM_NET_ERROR;
  }

  /* Send HTTP request */
  if ((SSL_write(ssl, Doc->Buf.buf, (int)strlen(Doc->Buf.buf))) < 0)
  {
    sslcleanup;
    return UDM_NET_ERROR;
  }

  /* Get result */
  Doc->Buf.size= 0;
  start_time= time(NULL);
  while(1)
  {
    if(Doc->Buf.size+buf_size > Doc->Buf.maxsize)
      buf_size= Doc->Buf.maxsize - Doc->Buf.size;
    else
      buf_size= UDM_NET_BUF_SIZE;
    
    status= SSL_read(ssl, Doc->Buf.buf + Doc->Buf.size, buf_size);
    if( status < 0 )
    {
      res= status;
      break;
    }
    else if( status == 0 )
    {
      break;
    }
    else
    {
      Doc->Buf.size += status;
      if(Doc->Spider.doc_timeout<(time(NULL)-start_time))
      {
        res= UDM_NET_TIMEOUT;
        break;
      }
      if( Doc->Buf.size == Doc->Buf.maxsize )
        break;
    }
  }
  
/*    nread= SSL_read(ssl, Doc->buf, (int)(Doc->Buf.maxsize - 1));*/
/*
    if(nread <= 0)
    {
      sslcleanup;
      return UDM_NET_ERROR;
    }
*/    
  SSL_shutdown (ssl);  /* send SSL/TLS close_notify */

/*  Doc->Buf.size=nread;*/
  Doc->Buf.buf[Doc->Buf.size]= '\0';

  /* Clean up. */
  sslcleanup;
  return res;
}

#endif

#ifdef USE_FTP
static int UdmFTPGet(UDM_AGENT * Indexer,UDM_DOCUMENT * Doc)
{
  int  last_mod_tm, code, res=0;
  char  buf[256], *full_path=NULL;
  size_t  len,buf_size;
  time_t  last_mod_time=UdmHttpDate2Time_t(UdmVarListFindStr(&Doc->Sections,"Last-Modified",""));
  
  Doc->Buf.size=0;
  
  /* Check for changing host */
  if (!Doc->connp.hostname || 
      strcmp(Doc->connp.hostname, UDM_NULL2EMPTY(Doc->CurURL.hostname)) || 
      (Doc->connp.connected == UDM_NET_NOTCONNECTED))
  {
    char *authstr= NULL;
    char * user=NULL;
    char * pass=NULL;
    
    if (Doc->CurURL.auth != NULL)
    {
      authstr= (char*)UdmStrdup(Doc->CurURL.auth);
      user=authstr;
      if((pass=strchr(authstr,':')))
      {
        *pass='\0';
        pass++;
      }
    }
    
    UDM_GETLOCK(Indexer,UDM_LOCK_CONF);
    code=Udm_ftp_connect(Indexer->Conf,&Doc->connp, UDM_NULL2EMPTY(Doc->CurURL.hostname),
             Doc->CurURL.port?Doc->CurURL.port:Doc->CurURL.default_port, user, pass,
             Doc->Spider.read_timeout); /* locked */
    UDM_RELEASELOCK(Indexer,UDM_LOCK_CONF);
    
    if (code == -1)
    {
      if (Doc->connp.err >0)
      {
        sprintf(Doc->Buf.buf, "HTTP/1.1 401 OK\r\n\r\n  ");
        Doc->Buf.size= strlen(Doc->Buf.buf);
      }
      else
      {
        res= Doc->connp.err;
      }
    }
    UDM_FREE(authstr);
  }
  
  if (Doc->connp.connected == UDM_NET_CONNECTED)
  {
    /* Make listing */
    if (Doc->CurURL.filename == NULL)
    {
      code= Udm_ftp_cwd(&Doc->connp, UDM_NULL2EMPTY(Doc->CurURL.path));
      if (code != -1)
      {
        code= Udm_ftp_list(Doc);
        if (code == -1)
        {
          if (Doc->connp.err >0)
          {
            sprintf(Doc->Buf.buf, "HTTP/1.1 403 OK\r\n\r\n");
            Doc->Buf.size= strlen(Doc->Buf.buf);
          }
          else
          {
            res= Doc->connp.err;
          }
        }
        else
        {
          udm_snprintf(Doc->Buf.buf, Doc->Buf.maxsize, 
                       "HTTP/1.1 200 OK\r\n"
                       "Content-Type: text/html\r\n\r\n"
                       "<html><body>%s</body></html>", 
                       Doc->connp.connp->buf);
          Doc->Buf.size= strlen(Doc->Buf.buf);
        }
      }
      else
      {
        if (Doc->connp.err >0)
        {
          sprintf(Doc->Buf.buf, "HTTP/1.1 403 OK\r\n\r\n");
          Doc->Buf.size= strlen(Doc->Buf.buf);
        }
        else
        {
          res= Doc->connp.err;
        }
      }
    }
    else
    {
      len= strlen(UDM_NULL2EMPTY(Doc->CurURL.path)) + strlen(UDM_NULL2EMPTY(Doc->CurURL.filename));
      full_path= UdmMalloc(len+1);
      udm_snprintf(full_path, len + 1, "%s%s", UDM_NULL2EMPTY(Doc->CurURL.path), UDM_NULL2EMPTY(Doc->CurURL.filename));
      full_path[len]='\0';
      last_mod_tm= Udm_ftp_mdtm(&Doc->connp,full_path);
      if(last_mod_tm == -1 && Doc->connp.err)
      {
        if (Doc->connp.err >0)
        {
          sprintf(Doc->Buf.buf, "HTTP/1.1 404 OK\r\n\r\n");
          Doc->Buf.size= strlen(Doc->Buf.buf);
        }
        else
          res= Doc->connp.err;

      }
      else if (last_mod_tm == last_mod_time)
      {
        sprintf(Doc->Buf.buf, "HTTP/1.1 304 OK\r\n\r\n");
        Doc->Buf.size= strlen(Doc->Buf.buf);
      }
      else
      {
        UdmTime_t2HttpStr(last_mod_tm, buf);
        if (Doc->method!=UDM_METHOD_HEAD)
        {
          int s, f= -1;
          size_t fsize= Doc->Buf.maxsize;
          sscanf(UdmVarListFindStr(&Doc->RequestHeaders, "Range", "bytes=0-0"), "bytes=%d-%d", &s, &f);
          if (f != 0)
          {
            if (s < 0)
            {
              s += Udm_ftp_size(&Doc->connp, full_path);
            }
            else if (f > 0)
            {
              fsize= f - s;
            }
            if (s > 0)
              Udm_ftp_rest(&Doc->connp, (size_t)s);
          }
          if (!Udm_ftp_get(&Doc->connp, Doc->connp.connp, full_path, fsize))
          {
            udm_snprintf(Doc->Buf.buf, Doc->Buf.maxsize, 
                         "HTTP/1.1 20%c OK\r\nLast-Modified: %s\r\n\r\n", 
                         (Doc->connp.connp->err == UDM_NET_FILE_TL) ? '6' : '0',
                          buf);
            Doc->Buf.size= strlen(Doc->Buf.buf);
            if (Doc->connp.connp->buf_len+Doc->Buf.size >= Doc->Buf.maxsize)
              buf_size= Doc->Buf.maxsize - Doc->Buf.size;
            else
              buf_size= Doc->connp.connp->buf_len;
            
            memcpy(Doc->Buf.buf+Doc->Buf.size, Doc->connp.connp->buf, buf_size);
            Doc->Buf.size +=buf_size; 
          }
          else
          {
            if (Doc->connp.err > 0)
            {
              sprintf(Doc->Buf.buf, "HTTP/1.1 403 OK\r\n\r\n");
              Doc->Buf.size= strlen(Doc->Buf.buf);
            }
            else
            {
              res= Doc->connp.err;
            }
          }
        }
        else
        {
          size_t size= Udm_ftp_size(&Doc->connp, full_path);
          sprintf(Doc->Buf.buf,
                  "HTTP/1.1 200 OK\r\n"
                  "Content-Type: text/html\r\n"
                  "Content-Length: %d\r\n"
                  "Last-Modified: %s\r\n\r\n", size, buf);
          Doc->Buf.size= strlen(Doc->Buf.buf);
        }
      }
    }
    Udm_ftp_close(&Doc->connp);
  }
  
  UDM_FREE(full_path);
  UDM_FREE(Doc->connp.buf);
  if (Doc->connp.connp && Doc->connp.connp->buf)
  {
    UDM_FREE(Doc->connp.connp->buf);
    Doc->connp.connp->buf=NULL;
  }
  return res;
}
#endif  

#ifdef USE_NEWS
static ssize_t fdgets(char * str,size_t size,int fd){
  size_t nbytes= 0;  /* Bytes read from fd and stored to str */
  int done=0;    /* Finish flag */

  while(!done)
  {
    if(nbytes+1>size)
    {
      break;
    }
    if(!recv(fd,str+nbytes,1,0))
    {
      break;
    }
    else
    {
      if(str[nbytes]=='\n')
      {
        done=1;
      }
      nbytes++;
    }
  }
  if(!nbytes)
  {
    return 0;
  }
  else
  {
    str[nbytes]='\0';
    return nbytes;
  }
}


/* #define DEBUG_NNTP */

static ssize_t
NNTPRecv(char * str,size_t sz, int fd)
{
  ssize_t res;
  res=fdgets(str,sz,fd);
#ifdef DEBUG_NNTP
  {
    char * s, ch;
    for(s=str; *s && *s!='\r' && *s!='\n' && (s-str<sz) ; s++);
    ch=*s;*s='\0';
    fprintf(stderr,"NNTPGet : NNTPRecv: '%s'\n",str);
    *s=ch;
  }
#endif
  return res;
}

static ssize_t
NNTPSend(char * str,size_t sz, int fd)
{
  ssize_t res;
  res= UdmSend(fd, str, sz, 0);
#ifdef DEBUG_NNTP
  {
    char * s, ch;
    for(s=str; *s && *s!='\r' && *s!='\n' && (s-str<sz) ; s++);
    ch=*s;*s='\0';
    fprintf(stderr,"NNTPGet : NNTPSend: '%s'\n",str);
    *s=ch;
  }
#endif
  return res;
}


#define UDM_NNTP_SEND    10
#define UDM_NNTP_RECV    11
#define UDM_NNTP_RECVCMD  12

#define UDM_NNTP_CONNECT  21
#define UDM_NNTP_HELLO    22
#define UDM_NNTP_READER    23
#define UDM_NNTP_BREAK    24
#define UDM_NNTP_QUIT    25

#define UDM_NNTP_USER    31
#define UDM_NNTP_USERRES  32
#define UDM_NNTP_PASS    33
#define UDM_NNTP_PASSRES  34

#define UDM_NNTP_LIST    41
#define UDM_NNTP_LISTRES  42
#define UDM_NNTP_LISTDAT  43

#define UDM_NNTP_STAT    51
#define UDM_NNTP_STATRES  52

#define UDM_NNTP_GROUP    61
#define UDM_NNTP_GROUPRES  62

#define UDM_NNTP_XOVER    71
#define UDM_NNTP_XOVERRES  72
#define UDM_NNTP_XOVERDAT  73

#define UDM_NNTP_ARTICLE  81
#define UDM_NNTP_ARTICLERES  82
#define UDM_NNTP_ARTICLEDAT  83



#define UDM_TERM(x)    (x)[sizeof(x)-1]='\0'

#define UDM_NNTP_MAXCMD    100

#ifdef DEBUG_NNTP
static const char * nntpcmd(int cmd)
{
  switch(cmd)
  {
    case UDM_NNTP_CONNECT:    return("connect");
    case UDM_NNTP_QUIT:    return("quit");
    case UDM_NNTP_BREAK:    return("break");
    case UDM_NNTP_HELLO:    return("hello");
    case UDM_NNTP_READER:    return("reader");
    case UDM_NNTP_RECV:    return("recv");
    case UDM_NNTP_RECVCMD:    return("recvcmd");
    case UDM_NNTP_SEND:    return("send");
    
    case UDM_NNTP_USER:    return("user");
    case UDM_NNTP_USERRES:    return("userres");
    
    case UDM_NNTP_PASS:    return("pass");
    case UDM_NNTP_PASSRES:    return("passres");
    
    case UDM_NNTP_LIST:    return("list");
    case UDM_NNTP_LISTRES:    return("listres");
    case UDM_NNTP_LISTDAT:    return("listdat");
    
    case UDM_NNTP_GROUP:    return("group");
    case UDM_NNTP_GROUPRES:    return("groupres");
    
    case UDM_NNTP_XOVER:    return("xover");
    case UDM_NNTP_XOVERRES:    return("xoverres");
    case UDM_NNTP_XOVERDAT:    return("xoverdat");
  
    case UDM_NNTP_STAT:    return("stat");
    case UDM_NNTP_STATRES:    return("statres");
  
    case UDM_NNTP_ARTICLE:    return("article");
    case UDM_NNTP_ARTICLERES:  return("articleres");
    case UDM_NNTP_ARTICLEDAT:  return("articledat");
  }
  return("UNKNOWN");
}
#endif
static void inscmd(int * commands,size_t * n,int command)
{
  if(((*n)+1)<UDM_NNTP_MAXCMD)
  {
    commands[*n]=command;
    (*n)++;
  }
}
static int delcmd(int * cmd,size_t * n)
{
  if(*n>0)
  {
    (*n)--;
    return cmd[*n];
  }
  else
  {
    return UDM_NNTP_BREAK;
  }
}

static int UdmNNTPGet(UDM_AGENT * Indexer,UDM_DOCUMENT *Doc)
{
  char str[2048]="";
  char grp[256]="";
  char msg[256]="";
  char user[256]="";
  char pass[256]="";
  char *tok,*lt,*end;
  int  cmd[UDM_NNTP_MAXCMD];
  size_t ncmd=0;
  
  int  fd=0;
  int  status=0;
  int  lastcmd=0;
  int  has_if_modified=0;
  int  a=0;
  int  b=0;
  int  from=0;
  int  to=0;
  int  headers=1;
  int  has_content=0;
  ssize_t nbytes;
  
  memset(cmd,0,sizeof(cmd));
  end=Doc->Buf.buf;
  Doc->Buf.size=0;
  *end='\0';

  /* We'll send user/password given in URL first         */
  /* Only after that we'll use them from "header" field  */
  if (Doc->CurURL.auth != NULL)
  {
    strncpy(user,Doc->CurURL.auth,sizeof(user)-1);
    UDM_TERM(user);
    if((tok=strchr(user,':')))
    {
      *tok++='\0';
      strncpy(pass,tok,sizeof(pass)-1);
      UDM_TERM(pass);
    }
  }
  
  
  inscmd(cmd,&ncmd,UDM_NNTP_QUIT);
  
  /* Parse request headers */
  tok= udm_strtok_r(Doc->Buf.buf,"\r\n",&lt);
  while(tok){
#ifdef DEBUG_NNTP
    fprintf(stderr,"HEADER: '%s'\n",tok);
#endif
    if(!strncasecmp(tok,"If-Modified-Since: ",19))has_if_modified=1;
    if(!strncasecmp(tok,"Authorization: ",15))
    {
      size_t l;
      char * auth=UdmTrim(tok+15," \t\r\n");
      strncpy(str,auth,sizeof(str)-1);
      UDM_TERM(str);
      l=udm_base64_decode(user,str,sizeof(user)-1);
      if((auth=strchr(user,':')))
      {
        *auth++='\0';
        strcpy(pass,auth);
      }
    }
    tok= udm_strtok_r(NULL,"\r\n",&lt);
  }


  /* Find in which "url" field group and message are */
  /* It depends on protocol being used               */

  /* First bad case, make redirect */
  if(!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "news") &&
      strcmp(UDM_NULL2EMPTY(Doc->CurURL.path), "/"))
  {
    /* news://news.server.com/group/ */
    /* In news:// group should not   */
    /* have trailing slash !!!!!!!!  */
    /* Let's    redirect to the same */
    /* URL without slashes           */
    char *s;
    strncpy(grp,Doc->CurURL.path,sizeof(grp)-1);
    UDM_TERM(grp);
    /* Remove trailing slash */
    if((s=strrchr(grp+1,'/')))*s='\0';
    status=301;
    udm_snprintf(str, sizeof(str)-1, "%s://%s%s", Doc->CurURL.schema,Doc->CurURL.hostinfo,grp);
    UDM_TERM(str);
    sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nLocation: %s\r\n\r\n",status,UdmHTTPErrMsg(status),str);
    Doc->Buf.size=strlen(Doc->Buf.buf);
    return 0;
  }
  
  
  /* Second bad case, make redirect */
  if(!strcasecmp(Doc->CurURL.schema,"nntp") &&
     !strcmp(Doc->CurURL.path,"/") &&
     Doc->CurURL.filename != NULL)
  {
    /* nntp://news.server.com/something        */
    /* This is wrong URL, assume that          */
    /* "something" is a group name, redirect   */
    /* to the same URL but with trailing slash */
    status=301;
    udm_snprintf(str,sizeof(str)-1,
                 "%s://%s/%s/",
                 Doc->CurURL.schema,
                 UDM_NULL2EMPTY(Doc->CurURL.hostinfo),
                 UDM_NULL2EMPTY(Doc->CurURL.filename));
    UDM_TERM(str);
    sprintf(Doc->Buf.buf,
            "HTTP/1.0 %d %s\r\nLocation: %s\r\n\r\n",
            status, UdmHTTPErrMsg(status),str);
    Doc->Buf.size=strlen(Doc->Buf.buf);
    return 0;
  }


  if(!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "news") &&
     (Doc->CurURL.filename != NULL))
  {
    /* url->path here is always "/"     */
    /* So there are two possible cases: */
    /* news://news.server.com/group     */
    /* news://news.server.com/messg     */
    /* url->filename contains either    */
    /* message ID  or group name        */
    /* and doesn't have any slashes     */
    /* Check whether @ does present     */
    if(strchr(Doc->CurURL.filename,'@'))
    {
      /* We have a message */
      strncpy(msg,Doc->CurURL.filename,sizeof(grp)-1);
      UDM_TERM(msg);
      if(has_if_modified)
      {
        inscmd(cmd,&ncmd,UDM_NNTP_STAT);
      }
      else
      {
        inscmd(cmd,&ncmd,UDM_NNTP_ARTICLE);
      }
    }
    else
    {
      /* We have a news group */
      strncpy(grp,Doc->CurURL.filename,sizeof(grp)-1);
      UDM_TERM(grp);
      inscmd(cmd,&ncmd,UDM_NNTP_XOVER);
      inscmd(cmd,&ncmd,UDM_NNTP_GROUP);
    }
  }
  else if(!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "nntp") &&
          strcmp(UDM_NULL2EMPTY(Doc->CurURL.path), "/"))
  {
    /* There are two possible cases:         */
    /*   nntp://news.server.com/group/       */
    /*   nntp://news.server.com/group/msg    */
    /* Group is in url->path and contains    */
    /* both leading and trailing slashes     */ 
    char * s;
    
    /* Copy without leading slash */
    strncpy(grp,Doc->CurURL.path+1,sizeof(grp)-1);
    UDM_TERM(grp);
    
    /* Remove trailing slash */
    if((s=strchr(grp,'/')))*s='\0';
    if(Doc->CurURL.filename != NULL)
    {
      strncpy(msg,Doc->CurURL.filename,sizeof(msg)-1);
      UDM_TERM(msg);
      if(has_if_modified)
      {
        inscmd(cmd,&ncmd,UDM_NNTP_STAT);
        inscmd(cmd,&ncmd,UDM_NNTP_GROUP);
      }
      else
      {
        inscmd(cmd,&ncmd,UDM_NNTP_ARTICLE);
        inscmd(cmd,&ncmd,UDM_NNTP_GROUP);
      }
    }
    else
    {
      inscmd(cmd,&ncmd,UDM_NNTP_XOVER);
      inscmd(cmd,&ncmd,UDM_NNTP_GROUP);
    }
  }
  else
  {
    inscmd(cmd,&ncmd,UDM_NNTP_LIST);
  }
  inscmd(cmd,&ncmd,UDM_NNTP_READER);
  inscmd(cmd,&ncmd,UDM_NNTP_CONNECT);


#ifdef DEBUG_NNTP
  fprintf(stderr,"NNTPGet : Enter with proto='%s' host='%s' port=%d path='%s' filename='%s'\n",url->schema,url->hostname,url->port?url->port:url->default_port,url->path,url->filename);
  fprintf(stderr,"NNTPGet : Assume grp='%s' msg='%s'\n",grp,msg);
#endif


  while(ncmd)
  {
#ifdef DEBUG_NNTP
    fprintf(stderr,"NCMD=%d\n",ncmd);
#endif
    lastcmd=delcmd(cmd,&ncmd);
#ifdef DEBUG_NNTP
    fprintf(stderr,"CMD=%s\n",nntpcmd(lastcmd));
#endif
    switch(lastcmd){
    case UDM_NNTP_CONNECT:
      /* Connect to NNTP server */
      fd=open_host(Indexer,Doc);
      if(fd<0)return(fd);    /* Could not connect */
      inscmd(cmd,&ncmd,UDM_NNTP_HELLO);
      inscmd(cmd,&ncmd,UDM_NNTP_RECVCMD);
      break;
      
    case UDM_NNTP_RECV:
      nbytes=NNTPRecv(str,sizeof(str),fd);
      break;
      
    case UDM_NNTP_RECVCMD:
      nbytes=NNTPRecv(str,sizeof(str),fd);
      status=atoi(str);
      break;
      
    case UDM_NNTP_SEND:
      nbytes=NNTPSend(str,strlen(str),fd);
      break;
      
    case UDM_NNTP_READER:
      inscmd(cmd,&ncmd,UDM_NNTP_HELLO);
      inscmd(cmd,&ncmd,UDM_NNTP_RECVCMD);
      inscmd(cmd,&ncmd,UDM_NNTP_SEND);
      sprintf(str,"mode reader\r\n");
      break;
      
    case UDM_NNTP_HELLO:
      if(status!=200)
      {
        status=500;
        sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nNNTP-Response: %s\r\n\r\n",status,UdmHTTPErrMsg(status),str);
        inscmd(cmd,&ncmd,UDM_NNTP_QUIT);
      }
      break;
      
    case UDM_NNTP_USER:
      if(user[0])
      {
        inscmd(cmd,&ncmd,UDM_NNTP_USERRES);
        inscmd(cmd,&ncmd,UDM_NNTP_RECVCMD);
        inscmd(cmd,&ncmd,UDM_NNTP_SEND);
        sprintf(str,"authinfo user %s\r\n",user);
      }
      else
      {
        status=401;
        sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nNNTP-Response: %s\r\n\r\n",status,UdmHTTPErrMsg(status),str);
        inscmd(cmd,&ncmd,UDM_NNTP_QUIT);
      }
      break;
      
    case UDM_NNTP_USERRES:
      if(status==381)
      {
        inscmd(cmd,&ncmd,UDM_NNTP_PASS);
      }
      else
      {
        status=500;
        sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nNNTP-Response: %s\r\n\r\n",status,UdmHTTPErrMsg(status),str);
        inscmd(cmd,&ncmd,UDM_NNTP_QUIT);
      }
      break;
      
    case UDM_NNTP_PASS:
      if(pass[0])
      {
        inscmd(cmd,&ncmd,UDM_NNTP_PASSRES);
        inscmd(cmd,&ncmd,UDM_NNTP_RECVCMD);
        inscmd(cmd,&ncmd,UDM_NNTP_SEND);
        sprintf(str,"authinfo pass %s\r\n",pass);
      }
      else
      {
        status=401;
        sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nNNTP-Response: %s\r\n\r\n",status,UdmHTTPErrMsg(status),str);
        inscmd(cmd,&ncmd,UDM_NNTP_QUIT);
      }
      break;
      
    case UDM_NNTP_PASSRES:
      if((status!=200) && (status!=281))
      {
        if(status==502)status=401; /* Unauthorzed */
        else  status=500;
        sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nNNTP-Response: %s\r\n\r\n",status,UdmHTTPErrMsg(status),str);
        inscmd(cmd,&ncmd,UDM_NNTP_QUIT);
        break;
      }
      break;
      
    case UDM_NNTP_LIST:
      inscmd(cmd,&ncmd,UDM_NNTP_LISTRES);
      inscmd(cmd,&ncmd,UDM_NNTP_RECVCMD);
      inscmd(cmd,&ncmd,UDM_NNTP_SEND);
      sprintf(str,"list\r\n");
      break;
      
    case UDM_NNTP_LISTRES:
      if(status==480){
        inscmd(cmd,&ncmd,UDM_NNTP_LIST);
        inscmd(cmd,&ncmd,UDM_NNTP_USER);
        break;
      }
      if(status!=215){
        status=500;
        sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nNNTP-Response: %s\r\n\r\n",status,UdmHTTPErrMsg(status),str);
        inscmd(cmd,&ncmd,UDM_NNTP_QUIT);
        break;
      }
      sprintf(Doc->Buf.buf,"HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n<HTML><BODY>\n");
      end=Doc->Buf.buf+strlen(Doc->Buf.buf);
      inscmd(cmd,&ncmd,UDM_NNTP_LISTDAT);
      inscmd(cmd,&ncmd,UDM_NNTP_RECV);
      break;
      
    case UDM_NNTP_LISTDAT:
      if(str[0]=='.')
      {
        udm_snprintf(end, Doc->Buf.maxsize - strlen(Doc->Buf.buf), "</BODY></HTML>\n");
        break;
      }
      if((tok=strchr(str,' ')))*tok=0;
      if(!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "nntp"))
        udm_snprintf(end, Doc->Buf.maxsize - strlen(Doc->Buf.buf), "<A HREF=\"/%s/\"></A>\n", str);
      else
        udm_snprintf(end, Doc->Buf.maxsize - strlen(Doc->Buf.buf), "<A HREF=\"/%s\"></A>\n", str);
      end=end+strlen(end);
      inscmd(cmd,&ncmd,UDM_NNTP_LISTDAT);
      inscmd(cmd,&ncmd,UDM_NNTP_RECV);
      break;
      
    case UDM_NNTP_XOVER:
      /* List of the articles in one news group */
      inscmd(cmd,&ncmd,UDM_NNTP_XOVERRES);
      inscmd(cmd,&ncmd,UDM_NNTP_RECVCMD);
      inscmd(cmd,&ncmd,UDM_NNTP_SEND);
      sprintf(str,"xover %d-%d\r\n",from,to);
      break;
      
    case UDM_NNTP_XOVERRES:
      if(status==224)
      {/* 224 data follows */
        sprintf(Doc->Buf.buf,"HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n<HTML><BODY>\n");
        end=Doc->Buf.buf+strlen(Doc->Buf.buf);
        inscmd(cmd,&ncmd,UDM_NNTP_XOVERDAT);
        inscmd(cmd,&ncmd,UDM_NNTP_RECV);
      }
      else
      {
        sprintf(Doc->Buf.buf,"HTTP/1.0 404 Not found\r\nNNTP-Server-Response: %s\r\n\r\n",str);
        inscmd(cmd,&ncmd,UDM_NNTP_QUIT);
      }
      break;
      
    case UDM_NNTP_XOVERDAT:
      if(str[0]=='.')
      {
        udm_snprintf(end, Doc->Buf.maxsize - strlen(Doc->Buf.buf), "</BODY></HTML>\n");
        break;
      }
      else if(!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "nntp"))
      {
        char *e;
        if((e= udm_strtok_r(str,"\t\r\n",&lt)))
          udm_snprintf(end, Doc->Buf.maxsize - strlen(Doc->Buf.buf), 
                 "<A HREF=\"%s%s\"></A>\n", UDM_NULL2EMPTY(Doc->CurURL.path), e);
        end=end+strlen(end);
        inscmd(cmd,&ncmd,UDM_NNTP_XOVERDAT);
        inscmd(cmd,&ncmd,UDM_NNTP_RECV);
      }
      else
      {
        int n=0;
        char *field[10];
        memset(field, 0, sizeof(field));
        
        tok= udm_strtok_r(str,"\t\r\n",&lt);
        while(tok)
        {
#ifdef DEBUG_NNTP
          fprintf(stderr,"NNTPGet : field[%d]: '%s'\n",n,tok);
#endif
          if(n<10)field[n]=tok;
          tok= udm_strtok_r(NULL,"\t\r\n",&lt);
          n++;
        }
        if(field[4])
        {
          char * msgb=field[4];
          char * msge;
          if(*msgb=='<')msgb++;
          for(msge=msgb ; *msge!='\0' && *msge!='>' ; msge++);
          if(*msge=='>')*msge='\0';
#ifdef DEBUG_NNTP
          fprintf(stderr,"NNTPGet : messg_id: '%s'\n",msgb);
#endif
          if( ((end-Doc->Buf.buf) + strlen(msgb) + 50) < Doc->Buf.maxsize){
            udm_snprintf(end, Doc->Buf.maxsize - strlen(Doc->Buf.buf), "<A HREF=\"/%s\"></A>\n", msgb);
            end+=strlen(end);
          }
        }
        inscmd(cmd,&ncmd,UDM_NNTP_XOVERDAT);
        inscmd(cmd,&ncmd,UDM_NNTP_RECV);
      }
      break;
      
    case UDM_NNTP_GROUP:
      inscmd(cmd,&ncmd,UDM_NNTP_GROUPRES);
      inscmd(cmd,&ncmd,UDM_NNTP_RECVCMD);
      inscmd(cmd,&ncmd,UDM_NNTP_SEND);
      sprintf(str, "group %s\r\n",grp);
      break;
      
    case UDM_NNTP_GROUPRES:
      if(status==480)
      { /* Authorization required */
        inscmd(cmd,&ncmd,UDM_NNTP_GROUP);
        inscmd(cmd,&ncmd,UDM_NNTP_USER);
      }
      else if(status==211)
      { /* Group selected */
        sscanf(str,"%d%d%d%d\n",&a,&b,&from,&to);
      }
      else
      {
        status=404;
        sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nNNTP-Response: %s\r\n\r\n",status,UdmHTTPErrMsg(status),str);
        inscmd(cmd,&ncmd,UDM_NNTP_QUIT);
        break;
      }
      break;
      
    case UDM_NNTP_STAT:
      inscmd(cmd,&ncmd,UDM_NNTP_STATRES);
      inscmd(cmd,&ncmd,UDM_NNTP_RECVCMD);
      inscmd(cmd,&ncmd,UDM_NNTP_SEND);
      if(!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "nntp"))
      {
        udm_snprintf(str,sizeof(str)-1,"stat %s\r\n",msg);
      }
      else
      {
        udm_snprintf(str,sizeof(str)-1,"stat <%s>\r\n",msg);
      }
      break;
      
    case UDM_NNTP_STATRES:
      if(status==480)
      { /* Authorization required */
        inscmd(cmd,&ncmd,UDM_NNTP_STAT);
        inscmd(cmd,&ncmd,UDM_NNTP_USER);
      }
      else if(status==223)
      {  /* Article exists             */
        status=304;  /* return 304 Not modified    */
      }
      else if(status==423)
      {  /* No such article            */
        status=404;  /* return 404 Not found       */
      }
      else
      {      /* Unknown status code        */
        status=500;  /* Return 505 Server error    */
      }
      sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nNNTP-Response: %s\r\n\r\n",status,UdmHTTPErrMsg(status),str);
      inscmd(cmd,&ncmd,UDM_NNTP_QUIT);
      break;
      
    case UDM_NNTP_ARTICLE:
      inscmd(cmd,&ncmd,UDM_NNTP_ARTICLERES);
      inscmd(cmd,&ncmd,UDM_NNTP_RECVCMD);
      inscmd(cmd,&ncmd,UDM_NNTP_SEND);
      if(!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "nntp"))
      {
        udm_snprintf(str,sizeof(str),"article %s\r\n",msg);
      }
      else
      {
        udm_snprintf(str,sizeof(str),"article <%s>\r\n",msg);
      }
      break;
      
    case UDM_NNTP_ARTICLERES:
      if(status==480)
      { /* Authorization required */
        inscmd(cmd,&ncmd,UDM_NNTP_ARTICLE);
        inscmd(cmd,&ncmd,UDM_NNTP_USER);
      }
      else if(status==220){ /* Article follows */
        sprintf(Doc->Buf.buf,"HTTP/1.0 200 OK\r\n");
        end=Doc->Buf.buf+strlen(Doc->Buf.buf);
        inscmd(cmd,&ncmd,UDM_NNTP_ARTICLEDAT);
        inscmd(cmd,&ncmd,UDM_NNTP_RECV);
      }
      else
      {
        status=404;
        sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nNNTP-Response: %s\r\n\r\n",status,UdmHTTPErrMsg(status),str);
        inscmd(cmd,&ncmd,UDM_NNTP_QUIT);
      }
      break;
      
    case UDM_NNTP_ARTICLEDAT:
      if(str[0]=='.')break;
      if(headers)
      {
        if(!strncasecmp(str,"Content-Type:",13))has_content=1;
        if((!strcmp(str,"\r\n"))||(!strcmp(str,"\n")))
        {
          headers=0;
          if(!has_content)strcat(Doc->Buf.buf,"Content-Type: text/plain\r\n");
        }
      }
      if((strlen(Doc->Buf.buf)+strlen(str)) < Doc->Buf.maxsize)
      {
        strcat(end,str);
        end=end+strlen(end);
      }
      inscmd(cmd,&ncmd,UDM_NNTP_ARTICLEDAT);
      inscmd(cmd,&ncmd,UDM_NNTP_RECV);
      break;
      
    case UDM_NNTP_QUIT:
      if(fd>0){
        sprintf(str,"quit\r\n");
        NNTPSend(str,strlen(str),fd);
        closesocket(fd);
        fd=0;
      }
      inscmd(cmd,&ncmd,UDM_NNTP_BREAK);
      break;
    
    case UDM_NNTP_BREAK:  
    default:
      ncmd=0;
      break;
    }
  }
  if(fd>0){
    /* This never should happen */
    /* Error in state machine   */
    closesocket(fd);
  }
  /* Return length of buffer */
  Doc->Buf.size=strlen(Doc->Buf.buf);
  return(0);
}

#endif


#ifdef USE_FILE
static int
cmpdirent (char **a1, char **a2)
{
  return(strcmp(*a1, *a2));
}

static void
freedir(char **dir)
{
  char **p;

  if (! dir) return;
  for (p= dir; *p; p++) UdmFree(*p);
  UdmFree(dir);
}

static char**
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
    if (arecs <= nrecs + 1)
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

  if (sort) UdmSort(_, nrecs, sizeof(char *), (udm_qsort_cmp)cmpdirent);
  _[nrecs]= NULL;
  return(_);
}

static int
UdmFILEGet(UDM_AGENT *Indexer,UDM_DOCUMENT *Doc)
{
  int fd,size,status=0,is_dir;
  struct stat sb,sb1;
  char filename[4096]= "";
  char newfilename[4096];
  char openname[4096];
  char mystatname[4096];
  char *lt,*s, *e;
  const char *hostname;
  time_t ims=0;
  int  cvs_ignore= Indexer->Conf->CVS_ignore;

  Doc->Buf.size=0;
  hostname= UdmVarListFindStr(&Doc->RequestHeaders,"Host",NULL);
  if (hostname && (hostname[0] == '\0' || !strcasecmp(hostname, "localhost")))
    hostname= NULL;

  for (s= Doc->Buf.buf; *s && *s != ' '; s++);
  
  if (*s == ' ')
  {
    s++;
    for (e= s; *e && *e != ' '; e++);
    if (*e == ' ' && e - s < sizeof(filename))
    {
      memcpy(filename, s, e - s);
      filename[e - s]= 0;
    }
  }

  if (! *filename)
  {
    sprintf(Doc->Buf.buf,"HTTP/1.0 500 %s\r\nX-Reason: bad file\r\n\r\n",UdmHTTPErrMsg(500));
    Doc->Buf.size=strlen(Doc->Buf.buf);
    return 0;
  }

  udm_snprintf(newfilename, sizeof(newfilename),
               "%s%s%s",
               hostname ? "//" : "",
               hostname ? hostname : "",
               filename);
  UdmUnescapeCGIQuery(openname,newfilename);

#ifdef WIN32
  /* Remove leading "/" for windows path */
  s= openname;
  if (*s == '/' && (s[2] == ':' || s[2] == '\\'))
  {
    memmove(s, s + 1, strlen(s));
  }
#endif

  /* Remember If-Modified-Since timestamp */
  s= udm_strtok_r(Doc->Buf.buf,"\r\n",&lt);
  while(s)
  {
    if(!strncasecmp(s,"If-Modified-Since: ",19))
    {
      ims=UdmHttpDate2Time_t(s+19);
    }
    s= udm_strtok_r(NULL,"\r\n",&lt);
  }

  strcpy(mystatname,openname);

#ifdef WIN32
  /*
    Remove traling slash sign stat("c:\a\") doesn't work
    under win. We'll pass stat("c:\a") instead
    In other hand when only drive name is checked it must
    have traling slash:  stat("c:\");
    For a network resouce we should execute WITH trailing
    slash, i.e. stat("\\servername\resoucename\");
  */
  {
    int len;
    
    s= mystatname;
    len=strlen(s);
    while(*s)
    {
      if(*s=='/')*s='\\';
      s++;
    }
    if ((len>3) && (mystatname[len-1]=='\\') && !hostname)
      mystatname[len-1]='\0';
  }
#endif
  
  if(stat(mystatname,&sb))
  {
    switch(errno)
    {
      case ENOENT: status=404;break; /* Not found */
      case EACCES: status=403;break; /* Forbidden*/
      default: status=500;
    }
    sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\n"
                         "X-Reason: %d:%s\r\n"
                         "X-Statname: '%s'\r\n"
                         "\r\n",
                         status,UdmHTTPErrMsg(status),errno,
                         strerror(errno), mystatname);
    Doc->Buf.size=strlen(Doc->Buf.buf);
    return 0;
  }

  /* If directory is given without ending "/"   */
  /* we must redirect to the same URL with "/"  */
  if((sb.st_mode&S_IFDIR)&&(filename[strlen(filename)-1]!='/'))
  {
    status=301;
    sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\nLocation: file:%s/\r\n\r\n",status,UdmHTTPErrMsg(status),filename);
    Doc->Buf.size=strlen(Doc->Buf.buf);
    return 0;
  }
  
  if(sb.st_mode&S_IFDIR)
  {
    char **recs= loaddir(openname, 1);
    size_t rpos;

/*    if((dir=opendir(openname))){ */
    if (recs)
    {
      char *stre;
      strcpy(Doc->Buf.buf,"HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n<HTML><BODY>\n");
      
      stre=UDM_STREND(Doc->Buf.buf);
/*      while((rec= readdir (dir))){ */
      for (rpos= 0; recs[rpos]; rpos++)
      {
        char escaped_name[1024]="";
        char *src;

        /*
        This does not work on Solaris
        is_dir=(rec->d_type==DT_DIR);
        */
        if (cvs_ignore && !strcmp(recs[rpos],"CVS"))
          continue;
        
        sprintf(newfilename, "%s%s", openname, recs[rpos]);

#ifndef WIN32
        if (! lstat(newfilename, &sb1) &&
          S_ISLNK(sb1.st_mode) &&
          ! UdmVarListFindInt(&Doc->Sections, "FollowSymLinks", 0))
        {
          continue;
        }
#endif
        if(stat(newfilename,&sb1))
        {
          UdmLog(Indexer, UDM_LOG_EXTRA, "Can't stat '%s'", newfilename);
          is_dir= 0;
        }
        else
        {
          is_dir= ((sb1.st_mode&S_IFDIR) > 0);
        }

        e=escaped_name;
        for(src=recs[rpos];*src;src++)
        {
          if(strchr(" %&<>+[](){}/?#'\"\\;,`",*src))
          {
            sprintf(e,"%%%X",(int)*src);
            e+=3;
          }
          else
          {
            *e=*src;
            e++;
          }
        }
        *e=0;
        sprintf(stre,"<A HREF=\"%s%s\">\n",escaped_name,is_dir?"/":"");
        stre=UDM_STREND(stre);
        if ((size_t)(stre - Doc->Buf.buf) > (Doc->Buf.maxsize - 2048))
        {
          e= (char*) UdmRealloc(Doc->Buf.buf, (Doc->Buf.maxsize + 65536) * sizeof(char));
          if (e == NULL) break;
          Doc->Buf.buf= e;
          Doc->Buf.maxsize += 65536;
          stre=UDM_STREND(e);
        }
      }
/*      closedir(dir); */
      freedir(recs);
      strcpy(UDM_STREND(Doc->Buf.buf),"</BODY><HTML>\n");
      Doc->Buf.size=strlen(Doc->Buf.buf);
      return 0;
    }
    else
    {
      switch(errno)
      {
        case ENOENT: status=404;break; /* Not found */
        case EACCES: status=403;break; /* Forbidden*/
        default: status=500;
      }
      sprintf(Doc->Buf.buf,
              "HTTP/1.0 %d %s\r\n"
              "X-Reason: Loaddir returned no results for '%s'\r\n"
              "\r\n",
              status, UdmHTTPErrMsg(status), openname);
      Doc->Buf.size=strlen(Doc->Buf.buf);
      return 0;
    }
  }
  else
  {
    size_t len= 0;
    UDM_MATCH  *M;
    UDM_MATCH_PART P[10];
    const char *fn= (Doc->CurURL.filename && Doc->CurURL.filename[0]) ?
                    Doc->CurURL.filename : "index.html";
    char content_type[80]= "";
    
    /* Lets compare last modification date */
    if(ims>=sb.st_mtime)
    {
      /* Document seems to be unchanged */
      status=304;
      sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\n\r\n",status,UdmHTTPErrMsg(status));
      Doc->Buf.size=strlen(Doc->Buf.buf);
      return 0;
    }

    if((fd=open(openname,O_RDONLY|UDM_BINARY))<0)
    {
      switch(errno)
      {
        case ENOENT: status=404;break;
        case EACCES: status=403;break;
        default: status=1;
      }
      sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\n\r\n",status,UdmHTTPErrMsg(status));
      Doc->Buf.size=strlen(Doc->Buf.buf);
      return 0;
    }
    
    UDM_GETLOCK(Indexer, UDM_LOCK_CONF);
    if((M= UdmMatchListFind(&Indexer->Conf->MimeTypes, fn, 10, P)))
    {
      udm_snprintf(content_type, sizeof(content_type), "%s", M->arg);
    }
    UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);
    
    if (strcasecmp(content_type, "application/http"))
    {
      sprintf(Doc->Buf.buf,"HTTP/1.0 200 OK\r\n");
      strcpy(UDM_STREND(Doc->Buf.buf),"Last-Modified: ");
      UdmTime_t2HttpStr(sb.st_mtime, UDM_STREND(Doc->Buf.buf));
      strcpy(UDM_STREND(Doc->Buf.buf),"\r\n");
      sprintf(UDM_STREND(Doc->Buf.buf),"\r\n");
      len= strlen(Doc->Buf.buf);
    }

    size= read(fd, Doc->Buf.buf + len, Doc->Buf.maxsize - len);
    close(fd);
    if(size>0)
    {
      Doc->Buf.size= len + size;
      return 0;
    }
    else
    {
      return(size);
    }
  }
}
#endif



static int
UdmBuildHTTPRequest(UDM_DOCUMENT *Doc)
{
  const char  *method=(Doc->method==UDM_METHOD_HEAD)?"HEAD":"GET";
  const char  *proxy=UdmVarListFindStr(&Doc->RequestHeaders,"Proxy",NULL);
  size_t      i= strlen(UDM_NULL2EMPTY(Doc->CurURL.path)) + 
                 strlen(UDM_NULL2EMPTY(Doc->CurURL.filename)) + 1;
  char *url= (char*)UdmMalloc(i);
  char *eurl= (char*)UdmMalloc(3 * i);

  if (url == NULL || eurl == NULL) return UDM_ERROR;

  sprintf(url, "%s%s", UDM_NULL2EMPTY(Doc->CurURL.path),
                       UDM_NULL2EMPTY(Doc->CurURL.filename)); 
  UdmEscapeURI(eurl, url);
  
  if(!Doc->Buf.buf)Doc->Buf.buf=(char*)UdmMalloc(Doc->Buf.maxsize + 1);
  
  if(proxy && strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "file"))
  {
    sprintf(Doc->Buf.buf,"%s %s://%s%s HTTP/1.0\r\n",
            method, UDM_NULL2EMPTY(Doc->CurURL.schema), 
            UDM_NULL2EMPTY(Doc->CurURL.hostinfo), eurl);
  }
  else
  {
    sprintf(Doc->Buf.buf,"%s %s HTTP/1.0\r\n", method, eurl);
  }
  UDM_FREE(eurl);UDM_FREE(url);
  
  /* Add user defined headers */
  for( i=0 ; i<Doc->RequestHeaders.nvars ; i++)
  {
    UDM_VAR *Hdr=&Doc->RequestHeaders.Var[i];
    sprintf(UDM_STREND(Doc->Buf.buf),"%s: %s\r\n",Hdr->name,Hdr->val);
  }
  
  /* Empty line is the end of HTTP header */
  strcat(Doc->Buf.buf,"\r\n");
#ifdef DEBUG_REQUEST
  fprintf(stderr,"Request:'%s'\n",Doc->Buf.buf);
#endif
  return UDM_OK;
}


static void
UdmCreateErrorResponse(UDM_DOCUMENT *Doc, int code)
{
  Doc->Buf.size= udm_snprintf(Doc->Buf.buf, Doc->Buf.maxsize,
                              "HTTP/1.0 %d %s\r\n\r\n",
                              code, UdmHTTPErrMsg(code));
  UdmVarListReplaceInt(&Doc->Sections, "Status", code);
}


int UdmGetURL(UDM_AGENT * Indexer,UDM_DOCUMENT * Doc)
{
  const char  *proxy;
  int mirror_period, found_in_mirror= 0, res= 0;

  UDM_GETLOCK(Indexer, UDM_LOCK_CONF);
  proxy= UdmVarListFindStr(&Doc->RequestHeaders,"Proxy",NULL);
  mirror_period= UdmVarListFindInt(&Doc->Sections, "MirrorPeriod", -1);
  UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);

  UdmBuildHTTPRequest(Doc);
  
  /* If mirroring is enabled */
  if (mirror_period >= 0)
  {
    /* 
      on u_m==0 it returned by mtime from mirrorget
      but we knew that it should be put in mirror
    */
    res= UdmMirrorGET(Indexer, Doc, &Doc->CurURL);
    if(!res)
    {
      UdmLog(Indexer, UDM_LOG_DEBUG, "Taken from mirror");
      found_in_mirror= 1;
    }
  }

  if (!found_in_mirror)
  {
    if(!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "exec"))
    {
      res= UdmExecGet(Indexer,Doc);
    }
    else if(!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "cgi"))
    {
      res= UdmExecGet(Indexer,Doc);
    }
#ifdef HAVE_SQL
    else if(!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "htdb"))
    {
      res= UdmHTDBGet(Indexer, Doc);
    }
#endif
#ifdef USE_FILE
    else if(!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "file"))
    {
      res= UdmFILEGet(Indexer,Doc);
    }
#endif
#ifdef USE_NEWS
    else if((!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "news")))
    {
      res= UdmNNTPGet(Indexer,Doc);
    }
    else if((!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "nntp")))
    {
      res= UdmNNTPGet(Indexer,Doc);
    }
#endif
#ifdef USE_HTTPS
    else if((!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "https")))
    {
      res= UdmHTTPSGet(Indexer,Doc);
    }
#endif
#ifdef USE_HTTP
    else if((!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "http")) ||
            (!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "ftp") && (proxy)))
    {
      res= UdmHTTPGet(Indexer,Doc);
    }
#endif
#ifdef USE_FTP
    else if ((!strcasecmp(UDM_NULL2EMPTY(Doc->CurURL.schema), "ftp")) && (!proxy))
    {
      res= UdmFTPGet(Indexer,Doc);
    }
#endif
  }

  /* Add NULL terminator */
  Doc->Buf.buf[Doc->Buf.size]= '\0';
  
  switch(res)
  {
    case UDM_NET_UNKNOWN:
      UdmLog(Indexer,UDM_LOG_WARN,"Protocol not supported");
      UdmCreateErrorResponse(Doc, UDM_HTTP_STATUS_NOT_SUPPORTED);
      break;
    case UDM_NET_ERROR:
      UdmLog(Indexer,UDM_LOG_WARN,"Network error");
      UdmCreateErrorResponse(Doc, UDM_HTTP_STATUS_SERVICE_UNAVAILABLE);
      break;
    case UDM_NET_TIMEOUT:
      UdmLog(Indexer,UDM_LOG_WARN,"Download timeout");
      UdmCreateErrorResponse(Doc, UDM_HTTP_STATUS_GATEWAY_TIMEOUT);
      break;
    case UDM_NET_CANT_CONNECT:
      UdmLog(Indexer,UDM_LOG_WARN,"Can't connect to host %s:%d",Doc->connp.hostname,Doc->connp.port);
      UdmCreateErrorResponse(Doc, UDM_HTTP_STATUS_SERVICE_UNAVAILABLE);
      break;
    case UDM_NET_CANT_RESOLVE:
      UdmLog(Indexer,UDM_LOG_WARN,"Unknown %shost '%s'",proxy?"proxy ":"",Doc->connp.hostname);
      UdmCreateErrorResponse(Doc, UDM_HTTP_STATUS_SERVICE_UNAVAILABLE);
      break;
    default:
      if(res<0)
      {  /* No connection */
        UdmLog(Indexer,UDM_LOG_WARN,"Can't connect to host %s:%d",Doc->connp.hostname,Doc->connp.port);
        UdmCreateErrorResponse(Doc, UDM_HTTP_STATUS_SERVICE_UNAVAILABLE);
        break;
      }
  }
  
  /* Put into mirror if required */
  if ((mirror_period >= 0) && (!found_in_mirror))
  {
    if(UdmMirrorPUT(Indexer, Doc, &Doc->CurURL))
    {
      return UDM_ERROR;
    }
  }
  return UDM_OK;
}

/*
  UdmDocInit() must be called of Doc before calling this function
*/
int
UdmGetURLSimple(UDM_AGENT *Agent, UDM_DOCUMENT *Doc, const char *vurl)
{
  UDM_VARLIST *vars= &Agent->Conf->Vars;
  size_t max_doc_size= (size_t)UdmVarListFindInt(vars,"MaxDocSize",UDM_MAXDOCSIZE);
  if(!Doc->Buf.buf) Doc->Buf.buf = (char*)UdmMalloc(max_doc_size);
  Doc->Buf.maxsize= max_doc_size;

  UdmURLParse(&Doc->CurURL,vurl);
  UdmVarListReplaceStr(&Doc->RequestHeaders, "Host", UDM_NULL2EMPTY(Doc->CurURL.hostname));
  Doc->connp.hostname= (char*)UdmStrdup(UDM_NULL2EMPTY(Doc->CurURL.hostname));
  Doc->connp.port= Doc->CurURL.port ? Doc->CurURL.port : Doc->CurURL.default_port;
    
  if(UdmHostLookup(&Agent->Conf->Hosts, &Doc->connp))
  {
  }

  if  (UdmGetURL(Agent, Doc) != UDM_OK)
    return UDM_ERROR;

  UdmParseHTTPResponse(Agent, Doc);
  UdmDocProcessContentResponseHeaders(Agent, Doc);

  if(Doc->Buf.content)
  {
    const char *ce=UdmVarListFindStr(&Doc->Sections,"Content-Encoding","");
#ifdef HAVE_ZLIB
    if(!strcasecmp(ce,"gzip") || !strcasecmp(ce,"x-gzip"))
      UdmDocUnGzip(Doc);
    else if(!strcasecmp(ce,"deflate"))
      UdmDocInflate(Doc);
    else if(!strcasecmp(ce,"compress") || !strcasecmp(ce,"x-compress"))
      UdmDocUncompress(Doc);
#endif
  }
  return UDM_OK;
}
