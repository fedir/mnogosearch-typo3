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

#ifdef USE_FTP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>

#ifdef HAVE_NETDB_H
#include <netdb.h>
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


#include "udm_common.h"
#include "udm_socket.h"
#include "udm_host.h"
#include "udm_utils.h"
#include "udm_ftp.h"
#include "udm_log.h"
#include "udm_proto.h"
#include "udm_xmalloc.h"
#include "udm_conf.h"
#include "udm_vars.h"

/* #define DEBUG_FTP */

int Udm_ftp_get_reply(UDM_CONN *connp)
{
  if (!connp->buf)
    return -1;
  return (atoi(connp->buf)/100);
}

int Udm_ftp_read_line(UDM_CONN *connp)
{
  if (socket_select(connp, UDM_NET_READ_TIMEOUT, 'r'))
  {
#ifdef DEBUG_FTP
    fprintf(stderr, "ftp://%s (ftp_read_line-timeout-err): ", connp->hostname);
    /*UdmLog(connp->indexer, UDM_LOG_DEBUG, "ftp://%s (ftp_read_line-timeout-err): ", connp->hostname);*/
#endif
    return -1;
  }
  
  do
  {
    if (socket_read_line(connp) < 0)
      return -1;
    if (((connp->buf[0] =='1')||(connp->buf[0] =='2')||
        (connp->buf[0] =='3')||(connp->buf[0] =='4')||
        (connp->buf[0] =='5')) && (connp->buf[3] == ' '))
      break;
  }
  while( 1 );
  return 0;
}


int Udm_ftp_connect(UDM_ENV * Conf,UDM_CONN *connp,
                    const char *hostname, int port,
                    char *user, char *passwd, int timeout)
{
  size_t len;
  
  if (!connp)
    return -1;

  if (connp->connected == UDM_NET_CONNECTED)
    Udm_ftp_close(connp);  
  connp->connected= UDM_NET_NOTCONNECTED;

  if (!port)
    connp->port= 21;
  else    
    connp->port= port;

  connp->timeout= timeout;
  
  if (!hostname)
    return -1;
  len= strlen(hostname);
  connp->hostname= UdmXrealloc(connp->hostname, len+1);
  udm_snprintf(connp->hostname, len+1, "%s", hostname);
  
  if (Udm_ftp_open_control_port(Conf,connp))
    return -1;
  if (Udm_ftp_login(connp, user, passwd))
    return -1;
  Udm_ftp_set_binary(connp);
  connp->connected= UDM_NET_CONNECTED;
  return 0;
}


int Udm_ftp_open_control_port(UDM_ENV * Conf,UDM_CONN *connp)
{
  int code;

  if (UdmHostLookup(&Conf->Hosts,connp)) /* FIXME: not reentrant */
    return -1;
  if (socket_open(connp))
    return -1;
  if (socket_connect(connp))
    return -1;
  /* Read server response */
  Udm_ftp_read_line(connp);
  code= Udm_ftp_get_reply(connp);
  if (code != 2)
    return -1;
  return 0;
}


int Udm_ftp_open_data_port( UDM_CONN *c, UDM_CONN *d)
{
  char buf[64];  
  unsigned char *a, *p;
  
  if (!d)
    return -1;
  if (socket_getname(c, &d->sin) == -1)
    return -1;
  if (socket_open(d))
    return -1;
  if (socket_listen(d))
    return -1;
  if (socket_getname(d, &d->sin) == -1)
    return -1;

  a= (unsigned char *)&d->sin.sin_addr;
  p= (unsigned char *)&d->sin.sin_port;

  udm_snprintf(buf, 64, "PORT %d,%d,%d,%d,%d,%d",
                a[0], a[1], a[2], a[3], p[0], p[1]);
  Udm_ftp_send_cmd(c, buf);
  if (strncasecmp(c->buf, "200 ", 4))
    return -1;
  d->user= c->user;
  d->pass= c->pass;
  return 0;
}


int Udm_ftp_send_cmd(UDM_CONN *connp, const char *cmd)
{
  char *buf;
  size_t len;

  connp->err= 0;
  len= strlen(cmd)+2;    
        buf= UdmXmalloc(len+1);
  udm_snprintf(buf, len+1, "%s\r\n", cmd);
  socket_buf_clear(connp);
  if (socket_write(connp, buf))
  {
    UDM_FREE( buf);
    return -1;
  }
#ifdef DEBUG_FTP
  fprintf(stderr, "ftp://%s (cmd) : %s", connp->hostname, buf);
  /*UdmLog(connp->indexer, UDM_LOG_DEBUG, "ftp://%s (cmd) : %s", connp->hostname, buf);*/
#endif
  UDM_FREE(buf);
  if (Udm_ftp_read_line(connp))
    return -1;
#ifdef DEBUG_FTP
  fprintf(stderr, "ftp://%s (reply): %s", connp->hostname, connp->buf);
  /*UdmLog(connp->indexer, UDM_LOG_DEBUG, "ftp://%s (reply): %s", connp->hostname, connp->buf);*/
#endif
  return(Udm_ftp_get_reply(connp));
}


static int ftp_expect_bytes(char *buf)
{
  char *ch, *ch1;
  int bytes;
  
  ch= strstr(buf, " bytes");
  ch1= strrchr(buf, '(');
  if (ch==0 || ch1 ==0)
    return -1;
  bytes= atol(ch1+1);
  return bytes;
}


int Udm_ftp_send_data_cmd(UDM_CONN *c, UDM_CONN *d, char *cmd, size_t max_doc_size)
{
  int code, bytes=0;
  
  if (!d)
    return -1;

  d->timeout= c->timeout;
  d->hostname= c->hostname;
  c->err= 0;
  
  if (Udm_ftp_open_data_port(c, d))
  {
    socket_close(d);
    return -1;
  }

  code= Udm_ftp_send_cmd(c, cmd);
  if (code == -1)
  {
    socket_close(d);
    return -1;
  }
  else if ( code >3 )
  {
    c->err= code;
    socket_close(d);
    return -1;
  }
  bytes= ftp_expect_bytes(c->buf);  
  if (socket_accept(d))
  {
    socket_close(d);
    return -1;
  }

  if (socket_read(d, max_doc_size) < 0)
  {
    /*UdmLog(c->indexer, UDM_LOG_DEBUG, "ftp://%s (socket_read-err):", c->hostname);*/
    socket_close(d);
    Udm_ftp_read_line(c);
    return -1;
  }
  socket_close(d);
 
  /* Check if file too lage ABORT */
  if (d->err == UDM_NET_FILE_TL)
  {
    if (Udm_ftp_abort(c))
    {
      socket_buf_clear(d);
      return -1;
    }
  } 
  

  /* 226 Transfer complete. */
  if (Udm_ftp_read_line(c))
  {
    /* Fixme: What to do if not receive "226 Transfer complete."? */
    /*UdmLog(c->indexer, UDM_LOG_DEBUG, "ftp://%s (data-end-err): %d", d->hostname, d->buf_len);*/
    Udm_ftp_close(c);
    if (bytes == d->buf_len)
      return 0;
    return -1;
  }
  code= Udm_ftp_get_reply(c);
  if (code == -1)
  {
    return -1;
  }
  else if ( code >3 )
  {
    c->err= code;
    return -1;
  }
  return 0;    
}


int Udm_ftp_login(UDM_CONN *connp, char *user, char *passwd)
{
  char *buf;
  char user_tmp[32], passwd_tmp[64];
  int code;
  size_t len;
  
  UDM_FREE(connp->user);
  UDM_FREE(connp->pass);
  if (!user)
    udm_snprintf(user_tmp, 32, "anonymous");
  else
  {
    udm_snprintf(user_tmp, 32, "%s", user);
    connp->user= (char*)UdmStrdup(user);
  }
      
  if (!passwd)
    udm_snprintf(passwd_tmp, 64, "%s-%s@mnogosearch.org", PACKAGE, VERSION);
  else
  {
    udm_snprintf(passwd_tmp, 32, "%s", passwd);
    connp->pass= (char*)UdmStrdup(passwd);
  }
  
  len= strlen(user_tmp)+strlen("USER ");
  buf= (char *) UdmXmalloc(len+1);
  udm_snprintf(buf, len+1, "USER %s", user_tmp);
  code= Udm_ftp_send_cmd(connp, buf);
  UDM_FREE(buf);
  if (code == -1)
    return -1;
  else if (code == 2) /* Don't need password */
    return 0;
    
  len= strlen(passwd_tmp)+strlen("PASS ");
  buf= (char *) UdmXmalloc(len+1);
  udm_snprintf(buf, len+1, "PASS %s", passwd_tmp);
  code= Udm_ftp_send_cmd( connp, buf);
  UDM_FREE(buf);
  if (code > 3)
    return -1;
  return 0;
}


static int ftp_parse_list(UDM_DOCUMENT *Doc)
{
  UDM_CONN *connp= Doc->connp.connp;
  const char *path= UDM_NULL2EMPTY(Doc->CurURL.path);
  char *line, *buf_in, *ch, *buf_out, *tok, *fname;
  int len_h, len_f,len_p, i;
  char *dir;
  size_t len,buf_len,cur_len;

  if (!connp->buf || !connp->buf_len)
    return 0;
  buf_in= connp->buf;
  /* 22 = strlen(<a href=\"ftp://%s%s%s/\"></a>)*/
  len_h= strlen(connp->hostname) + ((connp->user) ? strlen(connp->user) : 0) + ((connp->pass) ? strlen(connp->pass) : 0) + 2 + 22;
  len_p= strlen(path);
  cur_len= 0;
  buf_len= connp->buf_len;
  buf_out= UdmXmalloc(buf_len);
  buf_out[0]= '\0';
  line= udm_strtok_r(buf_in,"\r\n",&tok);
  do
  {
    if (!(fname= strtok(line, " ")))
      continue;
    /* drwxrwxrwx x user group size month date time file_name */
    for(i=0; i<7; i++)
      if (!(fname= strtok(NULL, " ")))
        break;
    if (!(fname= strtok(NULL, "")))
      continue;
    len= 0 ;
    len_f= len_h + len_p + strlen(fname);
    if ((cur_len+len_f) >= buf_len){
    buf_len+= UDM_NET_BUF_SIZE;
    buf_out= UdmXrealloc(buf_out, buf_len);
  }
          
  switch (line[0])
  {
    case 'd':
      if (!fname || !strcmp(fname, ".") || !strcmp(fname, ".."))
        break;
      len= len_f;
      udm_snprintf(UDM_STREND(buf_out) /*buf_out+cur_len*/, len+1, "<a href=\"ftp://%s%s%s%s%s%s%s/\"></a>\n",
           (connp->user) ? connp->user : "", (connp->user) ? ":" : "",
           (connp->pass) ? connp->pass : "", (connp->user || connp->pass) ? "@" : "",
              connp->hostname, path, fname);
      break;
    case 'l':
      if (! UdmVarListFindInt(&Doc->Sections, "FollowSymLinks", 0)) break;
      ch= strstr (fname, " -> ");
      if (!ch)
        break;
      len= ch - fname;
      dir= UdmMalloc(len+1);
      udm_snprintf(dir, len+1, "%s", fname);
      len= len_h + len_p + strlen(dir);
      udm_snprintf(UDM_STREND(buf_out)/*buf_out+cur_len*/, len+1, "<a href=\"ftp://%s%s%s%s%s%s%s/\"></a>\n", 
           (connp->user) ? connp->user : "", (connp->user) ? ":" : "",
           (connp->pass) ? connp->pass : "", (connp->user || connp->pass) ? "@" : "",
           connp->hostname, path, dir);
      UDM_FREE(dir);
        /*ch +=4;*/
        /* Check if it is absolute link */
/*        if ((ch[0] == '/') || (ch[0] == '\\') ||
           ((isalpha(ch[0]) && (ch[1]==':')))){
          len= len_h+strlen(ch);
          udm_snprintf(buf_out+cur_len, len+1, "<a href=\"ftp://%s%s/\"></a>", 
                connp->hostname, ch);
        }else{
          len= len_h+len_p+strlen(ch);
              udm_snprintf(buf_out+cur_len, len+1, "<a href=\"ftp://%s%s%s/\"></a>", 
                connp->hostname, path, ch);
        }
*/
      break;
    case '-':
      len= len_f; 
      udm_snprintf(UDM_STREND(buf_out)/*buf_out+cur_len*/, len+1, "<a  href=\"ftp://%s%s%s%s%s%s%s\"></a>\n", 
           (connp->user) ? connp->user : "", (connp->user) ? ":" : "",
           (connp->pass) ? connp->pass : "", (connp->user || connp->pass) ? "@" : "",
           connp->hostname, path, fname);
      break;
    }
    cur_len+= len;
  }
  while( (line= udm_strtok_r(NULL, "\r\n", &tok)));

  if (cur_len+1 > connp->buf_len_total)
  {
    connp->buf_len_total= cur_len;  
    connp->buf= UdmXrealloc(connp->buf, (size_t)connp->buf_len_total+1);
  }
  bzero(connp->buf, ((size_t)connp->buf_len_total+1));
  memcpy(connp->buf, buf_out, cur_len);
  UDM_FREE(buf_out);
  return 0;
}


static char *Udm_alloc_cmd_with_path_unescaped(const char *cmd, const char *arg)
{
  size_t cmd_len= strlen(cmd);
  size_t arg_len= strlen(arg);
  size_t len= cmd_len + arg_len + 2; /* 2 extra bytes: for space and for \0 */
  char *res= UdmXmalloc(len);
  if (res)
  {
    sprintf(res, "%s ", cmd);
    UdmUnescapeCGIQuery(res + cmd_len + 1, arg);
  }
  return res;
}


int Udm_ftp_list (UDM_DOCUMENT *Doc)
{
  char *filename= NULL;
  char *cmd;
  size_t len;

  if (!filename)
  {
    cmd= UdmXmalloc(5);
    sprintf(cmd, "LIST");
  }
  else
  {
    len= strlen(filename)+10;
    cmd= UdmXmalloc(len+1);
    udm_snprintf(cmd, len+1, "LIST %s", filename);
  }

  if (Udm_ftp_send_data_cmd(&Doc->connp, Doc->connp.connp, cmd, Doc->Buf.maxsize)== -1)
  {
    UDM_FREE(cmd);
    /*UdmLog(c->indexer, UDM_LOG_DEBUG, "(ftp_list-err)->%s", d->buf);*/
    return -1;
  }
  UDM_FREE(cmd);
  ftp_parse_list(Doc);
  return 0;
}


int Udm_ftp_get(UDM_CONN *c, UDM_CONN *d, char *path, size_t max_doc_size)
{
  char *cmd;
  
  if (!path || !(cmd= Udm_alloc_cmd_with_path_unescaped("RETR", path)))
    return -1;

  if (Udm_ftp_send_data_cmd(c, d, cmd, max_doc_size) == -1 && d->err != UDM_NET_FILE_TL)
  {
    UDM_FREE(cmd);
    return -1;
  }
  UDM_FREE(cmd);
  return 0;
}


int Udm_ftp_mdtm(UDM_CONN *c, char *path)
{
  char *cmd;
  int code;
  
  if (!path || !(cmd= Udm_alloc_cmd_with_path_unescaped("MDTM", path)))
    return -1;
    
  code= Udm_ftp_send_cmd(c, cmd);
  UDM_FREE(cmd);
  if (code == -1)
  {
    return -1;
  }
  else if ( code >3 )
  {
    c->err= code;
    return -1;
  }
  return (UdmFTPDate2Time_t(c->buf));
}


size_t Udm_ftp_size(UDM_CONN *c, char *path)
{
  char *cmd;
  int code;
  unsigned int len; 
  
  if (!path || !(cmd= Udm_alloc_cmd_with_path_unescaped("SIZE", path)))
    return -1;
    
  code= Udm_ftp_send_cmd(c, cmd);
  UDM_FREE(cmd);
  if (code == -1)
  {
    return -1;
  }
  else if ( code >3 )
  {
    c->err= code;
    return -1;
  }
  sscanf(c->buf, "213 %u", &len);
  return (size_t) len;
}


int Udm_ftp_rest(UDM_CONN *c, size_t rest)
{
  char cmd[64];
  int code;
  
  udm_snprintf(cmd, 63, "REST %u", (int) rest);

  code= Udm_ftp_send_cmd(c, cmd);
  if (code == -1)
  {
    return -1;
  }
  else if ( code >3 )
  {
    c->err= code;
    return -1;
  }
  return 0;
}


int Udm_ftp_set_binary(UDM_CONN *c)
{
  char *cmd;
  int code;
  
  cmd= UdmXmalloc(7);
  sprintf(cmd, "TYPE I");

  code= Udm_ftp_send_cmd(c, cmd);
  UDM_FREE(cmd);
  if (code == -1){
                return -1;
  }
  else if ( code >3 )
  {
    c->err= code;
    return -1;
  }
  return 0;
}


int Udm_ftp_cwd(UDM_CONN *c, const char *path)
{
  char *cmd;
  int code;
  
  if (!path || !(cmd= Udm_alloc_cmd_with_path_unescaped("CWD", path)))
    return -1;
    
  code= Udm_ftp_send_cmd(c, cmd);
  UDM_FREE(cmd);
  if (code == -1)
  {
    return -1;
  }
  else if ( code >3 )
  {
    c->err= code;
    return -1;
  }
  return 0;
}


int Udm_ftp_close(UDM_CONN *connp)
{
  if (connp->connected == UDM_NET_CONNECTED)
    Udm_ftp_send_cmd(connp, "QUIT");
  connp->connected= UDM_NET_NOTCONNECTED;
  socket_close(connp);
  if (connp->connp)
    socket_close(connp->connp);
  return 0;
}


int Udm_ftp_abort(UDM_CONN *connp)
{
  int code;

  socket_buf_clear(connp->connp);
  if (send(connp->conn_fd, "\xFF\xF4\xFF", 3, MSG_OOB)==-1)
    return -1;
  
  if (socket_write(connp, "\xF2"))
    return -1;
  
  code= Udm_ftp_send_cmd(connp, "ABOR");
  socket_buf_clear(connp->connp);

  if (code !=4)
    return -1;
  return 0;
}

#endif
