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
#include <string.h>
#include <sys/types.h>

#ifdef HAVE_IO_H
#include <io.h>	/* for Win */
#endif
#ifdef HAVE_DIRECT_H
#include <direct.h> /* for Win */
#endif
#ifdef  HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_parser.h"
#include "udm_xmalloc.h"
#include "udm_log.h"
#include "udm_vars.h"
#include "udm_wild.h"
#include "udm_signals.h"

/* Bug 625: these macroses are not defined on some rare platforms */
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif


__C_LINK int __UDMCALL UdmParserAdd(UDM_PARSERLIST *List,UDM_PARSER *P)
{
  List->Parser= (UDM_PARSER*)(UdmRealloc(List->Parser,(List->nparsers+1)*sizeof(UDM_PARSER)));
  List->Parser[List->nparsers].from_mime= (char*)UdmStrdup(P->from_mime);
  List->Parser[List->nparsers].to_mime= (char*)UdmStrdup(P->to_mime);
  List->Parser[List->nparsers].cmd= (char*)UdmStrdup(P->cmd);
  List->Parser[List->nparsers].src= P->src ? UdmStrdup(P->src) : NULL;
  List->nparsers++;
  return 0;
}

__C_LINK void __UDMCALL UdmParserListFree(UDM_PARSERLIST *List)
{
  size_t i;
  for(i=0;i<List->nparsers;i++)
  {
    UDM_FREE(List->Parser[i].from_mime);
    UDM_FREE(List->Parser[i].to_mime);
    UDM_FREE(List->Parser[i].cmd);
    UDM_FREE(List->Parser[i].src);
  }
  UDM_FREE(List->Parser)
  List->nparsers=0;
}


#ifdef USE_PARSER

/* Parser1: from STDIN to STDOUT */

#ifdef WIN32
static char *parse1(UDM_AGENT * Agent, char *buf, size_t buflen, char *cmd, size_t maxlen)
{
  UdmLog(Agent,UDM_LOG_ERROR,"This type of parser isn't supported");
  return NULL;
}
#else

static void sighandler(int sign);
static void init_signals(void)
{
  /* Set up signals handler*/
  UdmSignal(SIGALRM, sighandler);
}


static void sighandler(int sign)
{
  switch(sign)
  {
   case SIGALRM:
      _exit(0);
      break;
    default:
      break;
  }
  init_signals();
}

static char *parse1(UDM_AGENT * Agent, char *buf, size_t buflen, char *cmd, size_t maxlen)
{
  char *result = NULL;
  int wr[2];
  int rd[2];    
  pid_t pid;    

  /* Create write and read pipes */
  if (pipe(wr) == -1)
  {
    UdmLog(Agent,UDM_LOG_ERROR,"Cannot make a pipe for a write");
    return NULL;
  }
  if (pipe(rd) == -1)
  {
    UdmLog(Agent,UDM_LOG_ERROR,"Cannot make a pipe for a read");
    return NULL;
  }    

  /* Fork a clild */
  if ((pid = fork())==-1)
  {
    UdmLog(Agent,UDM_LOG_ERROR,"Cannot spawn a child");
    return NULL;
  }

  if (pid>0)
  {
    /* Parent process */
    char string[1024];
    ssize_t nbytes;

    /* Close other pipe ends */
    close(wr[0]);
    close(wr[1]);
    close(rd[1]);

    result=buf;
    memset(result,0,maxlen);
    memset(string,0,sizeof(string));
    while ((nbytes= read(rd[0],string,sizeof(string)-1)) > 0)
    {
      string[nbytes]= '\0';
      strncat(result,string,maxlen-strlen(result));
      memset(string,0,sizeof(string));
    }
    close(rd[0]);
    wait(NULL);
  }
  else
  {
    /* Child process */
    /* Fork a clild */
    if ((pid = fork())==-1)
    {
      UdmLog(Agent,UDM_LOG_ERROR,"Cannot spawn a child");
      return NULL;
    }

    if (pid>0)
    {
    /* Parent process */
      /* Close other pipe ends */
      close(wr[0]);
      close(rd[0]);
      close(rd[1]);

      /* Send string to be parsed */
      write(wr[1], buf, buflen);
      close(wr[1]);

      exit(0);
    }
    else
    {
      /* Child process */
      /* Close other pipe ends */
      close (wr[1]);
      close (rd[0]);

      /* Connect pipe to stdout */
      dup2(rd[1],STDOUT_FILENO);

      /* Connect pipe to stdin */
      dup2(wr[0],STDIN_FILENO);

      alarm((unsigned int) UdmVarListFindInt(&Agent->Conf->Vars, "ParserTimeOut", 300) );
      init_signals();

      system(cmd);
      exit(0);
    }
  }
  return result;
}
#endif

/* Parser2: from FILE to STDOUT */
static char *parse2(UDM_AGENT * Agent, char *buf, char *cmd, size_t maxlen)
{
  FILE *f;
  char *result = NULL;

  result=buf;
  memset(result,0,maxlen);

#ifdef WIN32
  f=_popen(cmd,"rb");
#else
  f=popen(cmd,"r");
#endif
  if(f){
    int fd;
    char string[1024];
    ssize_t nbytes;
    
    fd=fileno(f);
    memset(string,0,sizeof(string));
    while ((nbytes= read(fd,string,sizeof(string)-1)) > 0)
    {
      string[nbytes]= '\0';
      strncat(result,string,maxlen-strlen(result));
      memset(string,0,sizeof(string));
    }
#ifdef WIN32
    _pclose(f);
#else
    pclose(f);
#endif
  }else{
    UdmLog(Agent,UDM_LOG_ERROR,"Error in popen() (parse2)");
    return NULL;
  }
  return result;
}


/* Parser3: from FILE to FILE */
static char *parse3(UDM_AGENT * Agent, char *buf, char *cmd, size_t maxlen, char *to_file)
{
  char *result = NULL;
  int fd;

  result=buf;
  memset(result,0,maxlen);

  system(cmd);
  
  if((fd=open(to_file,O_RDONLY|UDM_BINARY))){
    read(fd,result,maxlen);
    close(fd);
  }else{
    UdmLog(Agent,UDM_LOG_ERROR,"Can't open output file (parse3)");
    return NULL;
  }
  return result;
}


/* Parser4: from STDIN to FILE */
static char *parse4(UDM_AGENT * Agent, char *buf, size_t buflen, char *cmd, size_t maxlen, char *to_file)
{
  FILE *f;
  char *result = NULL;

#ifdef WIN32
  f=_popen(cmd,"wb");
#else
  f=popen(cmd,"w");
#endif

  if(f)
  {
    int fd;
    
    fd=fileno(f);
    write(fd, buf, buflen);
#ifdef WIN32
    _pclose(f);
#else
    pclose(f);
#endif
    result=buf;
    memset(result,0,maxlen);

    if((fd=open(to_file,O_RDONLY|UDM_BINARY))){
      read(fd,result,maxlen);
      close(fd);
    }else{
      UdmLog(Agent,UDM_LOG_ERROR,"Can't open output file (parse4)");
      return NULL;
    }
  }
  else
  {
    UdmLog(Agent,UDM_LOG_ERROR,"Error in popen() (parse4)");
    return NULL;
  }
  return result;
}


static char *parse_file(UDM_AGENT *Agent,
                        UDM_PARSER *parser,
                        UDM_DOCUMENT *Doc,
                        char *buf, size_t length, size_t maxlen)
{
  char cmd[1024]="";
  char *result=NULL;
  char *arg1pos,*arg2pos;
  int parser_type;
  char fn0[1024]="";
  char fn1[1024]="";
  char * fnames[2];
  const char *url= UdmVarListFindStr(&Doc->Sections,"URL","");
  const char *tmpdir= UdmVarListFindStr(&Agent->Conf->Vars, "TmpDir", UDM_TMP_DIR);

  arg1pos=strstr(parser->cmd,"$1");
  arg2pos=strstr(parser->cmd,"$2");

  /* Build temp file names and command line */
#ifdef WIN32
  /*
   Windows generates something strange in tmpnam()
   So create the name ourself. Because some parsers
   might be compiled under DOS, the name should be
   DOS compatible.
  */
  sprintf(fn0, ".\\prstmp%d",Agent->handle);
#else
  sprintf(fn0,"%s/ind.%d.%d", tmpdir, Agent->handle, getpid());
#endif
  strcpy(fn1,fn0);
  fnames[0]=strcat(fn0,".in");
  fnames[1]=strcat(fn1,".out");
  if (strstr(parser->cmd, "${"))
  {
    UDM_DSTR dstr;
    UdmDSTRInit(&dstr, 1024);
    UdmDSTRParse(&dstr, parser->cmd, &Doc->Sections);
    UdmBuildParamStr(cmd, sizeof(cmd), dstr.data, fnames, 2);
    UdmDSTRFree(&dstr);
  }
  else
  {
    UdmBuildParamStr(cmd, sizeof(cmd), parser->cmd, fnames, 2);
  }

  if(arg1pos)
  {
    int fd;

    /* Create temporary file */
#ifdef WIN32
    umask((int)022);
#else
    umask((mode_t)022);
#endif
    fd=open(fnames[0],O_RDWR|O_CREAT|O_TRUNC|UDM_BINARY,UDM_IWRITE);
    if (fd < 0)
    {
      UdmLog(Agent,UDM_LOG_ERROR,"Can't open output file '%s'", fnames[0]);
      return 0;
    }
    /* Write to the temporary file */
    write(fd,buf,length);
    close(fd);
  }

  if(arg1pos&&arg2pos)
    parser_type= 3;
  else
  {
    if(arg1pos)
      parser_type= 2;
    else if(arg2pos)
      parser_type= 4; 
    else
      parser_type= 1;
  }
  
  /*fprintf(stderr,"cmd='%s' parser_type=%d\n",cmd,parser_type);*/
  UdmLog(Agent,UDM_LOG_EXTRA,"Starting external parser: '%s'",cmd);
  UdmSetEnv("UDM_URL",url);
  switch(parser_type)
  {
    case 1: result= parse1(Agent, buf, length, cmd, maxlen); break;
    case 2: result= parse2(Agent, buf, cmd, maxlen); break;
    case 3: result= parse3(Agent, buf, cmd, maxlen, fnames[1]); break;
    case 4: result= parse4(Agent, buf, length, cmd, maxlen, fnames[1]); break;
  }
  UdmUnsetEnv("UDM_URL");

  /* Remove temporary file */
  if(arg1pos)unlink(fnames[0]);
  if(arg2pos)unlink(fnames[1]);

  return result;
}

__C_LINK UDM_PARSER * __UDMCALL UdmParserFind(UDM_PARSERLIST *List,const char *mime_type)
{
  size_t i;
  UDM_ASSERT(mime_type != NULL);
  for(i=0;i<List->nparsers;i++)
  {
    if(!UdmWildCaseCmp(mime_type,List->Parser[i].from_mime))
      return &List->Parser[i];
  }
  return NULL;
}

char *UdmParserExec(UDM_AGENT * Agent,UDM_PARSER *P,UDM_DOCUMENT *Doc)
{
  size_t maxlen= (size_t)(Doc->Buf.maxsize-(Doc->Buf.content-Doc->Buf.buf));
  char *result;
  if (P->src)
  {
    size_t srclen;
    UDM_DSTR dstr;
    UdmDSTRInit(&dstr, 1024);
    UdmDSTRParse(&dstr, P->src, &Doc->Sections);
    srclen= dstr.size_data < maxlen ? dstr.size_data : maxlen;
    memcpy(Doc->Buf.content, dstr.data, srclen);
    UdmDSTRFree(&dstr);
    result= parse_file(Agent, P, Doc, Doc->Buf.content, srclen, maxlen);
  }
  else
  {
    size_t srclen= (size_t)(Doc->Buf.size-(Doc->Buf.content-Doc->Buf.buf));
    result= parse_file(Agent, P, Doc, Doc->Buf.content, srclen, maxlen);
  }
  Doc->Buf.size= strlen(Doc->Buf.content) + (Doc->Buf.content-Doc->Buf.buf);
  return result;
}

#endif
