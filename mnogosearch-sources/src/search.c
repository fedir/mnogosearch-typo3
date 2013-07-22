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
#include <errno.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef   HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include "udmsearch.h"

/* This should be last include */
#ifdef DMALLOC
#include "dmalloc.h"
#endif


#define POWERED_LOGO "<BR><a href=\"http://www.mnogosearch.org/\"><IMG BORDER=0 SRC=\"http://www.mnogosearch.org/img/mwin.gif\"></A>&nbsp<FONT SIZE=1><A HREF=\"http://www.mnogosearch.org/\">Powered by mnoGoSearch - free web search engine software</A></FONT><BR>\n"

#define UDM_HTTPD_CMDLINE 0
#define UDM_HTTPD_CGI     1
#define UDM_HTTPD_DAEMON  2

typedef struct udm_self_template_query_st
{
  char template_name[1024];
  char self[1024];
  char query_string[1024];
  int  httpd;                /* Execution type: httpd, inetd or command line */
} UDM_SELF_TEMPLATE_QUERY;


static void
UdmSelfTemplateQueryInit(UDM_SELF_TEMPLATE_QUERY *q)
{
  /*
    Some HTTPD servers do not pass QUERY_STRING if the query was empty,
    so additionally check REQUEST_METHOD to be safe.
  */
  q->httpd= getenv("QUERY_STRING") || getenv("REQUEST_METHOD") ?
            UDM_HTTPD_CGI : 0;
  q->template_name[0]= 0;
  q->self[0]= 0;
  q->query_string[0]= 0;
}


/******************** Variable functions *****************************/
/*
  Wrappers for UdmVarListFindXXX(),
  with a limit to command parameters only, ignoring query string values.
  Have two purposes:
  - generally avoid overwriting search.htm commands in query string
  - avoid cross-site scripting, in case of String variables
*/
static const char *
UdmVarListFindSafeStr(UDM_VARLIST *List, const char *name, const char *def)
{
  UDM_VAR *Var= UdmVarListFind(List, name);
  if (!Var || Var->section == UDM_VARSRC_QSTRING || !Var->val)
    return def;
  return Var->val;
}

/* Avoid using directly */
#define UdmVarListFindStr(x)  { error;}

static int
UdmVarListFindSafeInt(UDM_VARLIST *List, const char *name, int def)
{
  UDM_VAR *Var= UdmVarListFind(List, name);
  if (!Var || Var->section == UDM_VARSRC_QSTRING || !Var->val)
    return def;
  return atoi(Var->val);
}


/******************** Misc functions *********************************/

static void ParseQStringUnescaped(UDM_VARLIST *vars, const char *qstring)
{
  char *tok, *lt, *qs; 
  
  if ((qs= (char*)UdmStrdup(qstring)))
  {
    UdmSGMLUnescape(qs);
    tok = udm_strtok_r(qs, "&", &lt);
    while(tok)
    {
      char *arg= strchr(tok,'=');
      if(arg)
        *arg++='\0';
      UdmVarListAddStr(vars,tok,arg?arg:"");
      tok= udm_strtok_r(NULL, "&", &lt);
    }
    UdmFree(qs);
  }
}

static int UdmVarListReplaceOrDelInt(UDM_VARLIST *V, const char *name, int i)
{
  return i ? UdmVarListReplaceInt(V, name, i) : UdmVarListDel(V, name);
}


static size_t
UdmURLCanonizePathAppend(char *dst, const char *src)
{
  size_t len= UDM_URL_CANONIZE_MULTIPLY * strlen(src) + 1;
  len= UdmURLCanonizePath(dst, len, src);
  return len;
}


static char * BuildPageURL(UDM_VARLIST * vars, char **dst)
{
  size_t i, dstlen= 0;
  char *end;
  
  for(i= 0; i < vars->nvars; i++)
    dstlen+= 7 + strlen(vars->Var[i].name) + strlen(vars->Var[i].val);
  
  if (!(*dst= (char*)UdmRealloc(*dst, UDM_URL_CANONIZE_MULTIPLY * dstlen)))
    return NULL;
  end = *dst;
  
  for(i=0; i < vars->nvars; i++)
  {
    UDM_VAR *V= &vars->Var[i];
    strcpy(end, i ? "&amp;" : "?");
    end+= i ? 5 : 1;
    end+= UdmURLCanonizePathAppend(end, V->name);
    *end++= '=';
    end+= UdmURLCanonizePathAppend(end, V->val);
  }
  return NULL;
}

/*****************************************************************/

#ifndef WIN32
#include <grp.h>

static int UdmLoadGroups (UDM_ENV *E) {
  struct group *grp;
  int cnt = 0;
  char name[32];
  char **p;
  const char *user= UdmVarListFindSafeStr(&E->Vars, "REMOTE_USER", NULL);

  if (! user) return(UDM_OK);

  setgrent();
  while ((grp = getgrent()))
  {
    for (p = grp->gr_mem; *p; p++)
    {
      if (! strcmp(user, *p))
      {
        udm_snprintf(name, sizeof(name), "REMOTE_GROUP%d", cnt);
        UdmVarListAddStr(&E->Vars, name, grp->gr_name);
        cnt++;
	break;
      }
    }
  }
  getgrent();

  return(UDM_OK);
}
#endif

/************************************************************/

static int
StoreDocCGI(UDM_AGENT *Agent, UDM_VARLIST *tmpl, UDM_SELF_TEMPLATE_QUERY *q)
{
  UDM_ENV *Env= Agent->Conf;

  /* Fetch cached copy */
  UdmCachedCopyGet(Agent);

  /* Print headers */
  if (q->httpd & UDM_HTTPD_DAEMON)
    printf("HTTP/1.0 200 OK\r\n");
  printf("Content-type: text/html; charset=%s\r\n\r\n",
         UdmVarListFindSafeStr(&Env->Vars, "Charset", "iso-8859-1"));

  /* Now start displaying template*/
  UdmTemplatePrint(Agent, stdout, NULL, 0, &Env->Vars, tmpl, "storedoc_top");
  UdmTemplatePrint(Agent, stdout, NULL, 0, &Env->Vars, tmpl, "storedoc");
  UdmTemplatePrint(Agent, stdout, NULL, 0, &Env->Vars, tmpl, "storedoc_bottom");

  /* Free variables */
  UdmVarListFree(tmpl);
  UdmAgentFree(Agent);
  UdmEnvFree(Env);
  return UDM_OK;
}
/************************************************************/

/*
   We need to initialize WSA to process possible
   includes in template.
*/
static void UdmWSAStartup(void)
{
#ifdef WIN32
  WSADATA wsaData;
  if(WSAStartup(0x101,&wsaData)!=0)
  {
    fprintf(stderr,"WSAStartup() error %d\n",WSAGetLastError);
    exit(1);
  }
#endif
}

static void UdmWSACleanup(void)
{
#ifdef WIN32
  WSACleanup();
#endif
}
/************************************************************/
#if defined(HAVE_PTHREAD) && !defined(WIN32)
static
int UdmThreadCreate(void **thd, void *(*start_routine)(void *), void *arg)
{
  return pthread_create((pthread_t*) thd, NULL, start_routine, arg); 
}

static
int UdmThreadJoin(void *thd)
{
  return pthread_join((pthread_t) thd, NULL);
}        
#endif

static int
UdmHTTPHeadersSend(UDM_SELF_TEMPLATE_QUERY *q,
                   const char *content_type, const char *bcs)
{
  if (q->httpd & UDM_HTTPD_DAEMON)
    printf("HTTP/1.0 200 OK\r\n");
  
  if (q->httpd && strcasecmp(content_type, "none"))
  {
    if (bcs)
      printf("Content-type: %s; charset=%s\r\n\r\n", content_type, bcs);
    else
      printf("Content-Type: %s\r\n\r\n", content_type);
  }
  return UDM_OK;
}


static void usage(void)
{
}


static int
UdmDetectSelfTemplateQueryCGI(UDM_ENV *Env,
                              UDM_SELF_TEMPLATE_QUERY *q,
                              int argc, char **argv)
{
  char *env;
  /* Detect self */
  if((env= getenv("UDMSEARCH_SELF")) || (env= getenv("REQUEST_URI")))
  {
    /* Use Apache's REQUEST_URI, which is not in CGI standard */
    char *end;
    udm_snprintf(q->self, sizeof(q->self), "%s", env);
    if ((end= strchr(q->self,'?')))
      end[0]='\0';
  }
  else if ((env= getenv("SCRIPT_NAME")))
  {
    /* Use standard CGI variables if not under Apache */
    const char *path_info= getenv("PATH_INFO");
    udm_snprintf(q->self, sizeof(q->self),"%s%s", env, path_info ? path_info:"");
  }
  else
  {
    udm_snprintf(q->self, sizeof(q->self), "search.cgi");
  }
  
  
  /* Detect template name */
  if (q->template_name[0])
  {
    /* Do nothing - already set in command line via -d option */
  }
  else if((env= getenv("UDMSEARCH_TEMPLATE")))
  {
    char ename[1024];
    strncpy(ename,env,sizeof(ename)-1);
    ename[sizeof(ename)-1]='\0';
    UdmUnescapeCGIQuery(q->template_name, ename);
  }
  else if(getenv("REDIRECT_STATUS") && (env= getenv("PATH_TRANSLATED")))
  {
    /* Check Apache internal redirect  */
    /* via   "AddHandler" and "Action" */
    udm_snprintf(q->template_name, sizeof(q->template_name), "%s", env);
  }
  else if ((env= getenv("PATH_INFO")) && env[0])
  {
    /* http://localhost/cgi-bin/search.cgi/path/to/search.htm */
    char name[128];
    
    strncpy(name,env,sizeof(name)-1);
    name[sizeof(name)-1]='\0';
    if (UDMSLASH !='/')
    {
      char *s;
      for (s=name; s[0]; s++)
        if (s[0]=='\\')s[0]='/';
    }
    
    /* Take from the config directory */
    UdmGetFileName(q->template_name, sizeof(q->template_name)-5, 
                   UDM_DIRTYPE_CONF, name);
  }
  else
  {
    /* CGI executed without Apache internal redirect */
    /* or started from command line                  */
    char *end;
    size_t length;
    
    /* Take from the config directory */
    length= UdmGetFileName(q->template_name, sizeof(q->template_name)-5,
                           UDM_DIRTYPE_CONF,
                           (end= strrchr(q->self,'/')) ? (end+1) : (q->self));
    end= q->template_name + length;
    if ( length > 3 && (!strcmp(end-4,".exe") || !strcmp(end-4,".cgi")))
      end-=4;
    strcpy(end,".htm");
  }
  
  if((env= getenv("QUERY_STRING")))
  {
    udm_snprintf(q->query_string, sizeof(q->query_string), "%s", env);
  }
  else if (!q->httpd && argv[0])
  {
    udm_snprintf(q->query_string, sizeof(q->query_string), "q=%s", argv[0]);
    UdmVarListReplaceStr(&Env->Vars, "ENV.QUERY_STRING", q->query_string);
  }
  else
  {
    udm_snprintf(q->query_string, sizeof(q->query_string), "q=");
  }
  return UDM_OK;
}


static int
UdmDetectSelfTemplateQueryDaemon(UDM_ENV *Env,
                                 UDM_SELF_TEMPLATE_QUERY *q,
                                 FILE *stream)
{
  char str[1024];
  while (fgets(str, sizeof(str), stream))
  {
    UdmRTrim(str, "\r\n");
    if (!strncmp(str, "GET ", 4))
    {
      char *method, *uri, *lt;
      if ((method= udm_strtok_r(str, " ", &lt)) &&
          (uri= udm_strtok_r(NULL, " ", &lt)))
      {
        char *query_string= strchr(uri, '?');
        if (query_string)
        {
          *query_string= '\0';
          query_string++;
          udm_snprintf(q->query_string, sizeof(q->query_string), "%s", query_string);
        }
        UdmGetFileName(q->template_name, sizeof(q->template_name)-5, 
                       UDM_DIRTYPE_CONF, uri);
        udm_snprintf(q->self, sizeof(q->self), "%s",  uri);
      }
    }
    if (!strcmp(str,""))
      break;
  }
  return UDM_OK;
}


static UDM_SELF_TEMPLATE_QUERY q;


static int
UdmCmdLineHandleOption(UDM_CMDLINE_OPT *opt, const char *value)
{
  switch (opt->id)
  {
    case 'x':
      q.httpd|= UDM_HTTPD_DAEMON;
      break;
    case 'd':
      udm_snprintf(q.template_name, sizeof(q.template_name), "%s", value);
      break;
    case '?':
    case 'h':
    default:
      usage();
      exit(0);
  }
  return 0;
}


static UDM_CMDLINE_OPT udm_search_cgi_options[]=
{
  {'x', "", UDM_OPT_BOOL, NULL, "Act as itetd/xinetd service"},
  {'h', "", UDM_OPT_BOOL, NULL, "Print help page and exit; -hh print more help"},
  {'?', "", UDM_OPT_BOOL, NULL, "Print help page and exit; -?? print more help"},
  {'d', "", UDM_OPT_STR,  NULL, "Use the given template file instead of search.htm"},
  {0, NULL, 0, NULL, NULL},
};


#if ((defined WIN32) && (defined _DEBUG))
static void
WIN32_IIS_DEBUG_MSG(const char *name)
{
  char szMessage [256];
  wsprintf(szMessage, "Please attach a debugger to the process 0x%X (%s) and click OK",
           GetCurrentProcessId(), name);
  MessageBox(NULL, szMessage, "CGI Debug Time!", MB_OK|MB_SERVICE_NOTIFICATION);
}
#endif


int main(int argc, char ** argv)
{
  const char  *env, *bcharset, *lcharset;
  char        search_time[100]= "";
  char        *nav= NULL, *url= NULL;
  int         page_size, page_number, res;
  size_t      i, nav_len;
  ssize_t     page1,page2,npages,ppp=10;
  UDM_ENV     *Env;
  UDM_AGENT   *Agent;
  UDM_RESULT  *Res;
  UDM_VARLIST query_vars;
  UDM_VARLIST tmpl;

  UdmSelfTemplateQueryInit(&q);

#if ((defined WIN32) && (defined _DEBUG))
  if(q.httpd)
  {
    WIN32_IIS_DEBUG_MSG(argv[0]);
    DebugBreak();
  }
#endif


  if (!q.httpd)
  {
    /*
      Don't parse command line options when under HTTPD,
      to avoid opening passing an arbitrary file as a template name:
      http://localhost/cgi-bin/search.cgi/search.htm?-d/path/to/arbitrary/file
    */
    size_t noptions;
    if (UdmCmdLineOptionsGet(argc, argv, udm_search_cgi_options,
                             UdmCmdLineHandleOption, &noptions))
      return 0;
    argc-= noptions;
    argv+= noptions;
  }

  UdmWSAStartup();
  
  UdmInit();
  Env=UdmEnvInit(NULL);
#if defined(HAVE_PTHREAD) && !defined(WIN32)
  UdmInitMutexes();
  UdmSetLockProc(Env, UdmLockProc);
  Env->ThreadCreate= UdmThreadCreate;
  Env->ThreadJoin= UdmThreadJoin;
#endif
  UdmVarListInit(&tmpl);
  UdmVarListInit(&query_vars);
  Agent = UdmAgentInit(NULL, Env, 0);
  UdmVarListAddEnviron(&Env->Vars,"ENV");
  UdmVarListReplaceStr(&Env->Vars, "version", VERSION);
  
  if (q.httpd & UDM_HTTPD_DAEMON)
    UdmDetectSelfTemplateQueryDaemon(Env, &q, stdin);
  else
    UdmDetectSelfTemplateQueryCGI(Env, &q, argc, argv);
  
  
  /*
    Hack for Russian Apache from apache.lexa.ru
    QUERY_STRING is already converted to server
    character set. We must print original query
    string instead however. Under usual apache
    we'll use QUERY_STRING. Note that query_vars
    list will contain not unescaped values, so
    we don't have to escape them when displaying
  */
  env= getenv("CHARSET_SAVED_QUERY_STRING");
  ParseQStringUnescaped(&query_vars, env ? env : q.query_string);
  
  /* Unescape and save variables from QUERY_STRING */
  /* Env->Vars will have unescaped values however  */
  UdmParseQueryString(Agent, &Env->Vars, q.query_string);

#ifndef WIN32
  env= getenv("REMOTE_USER");
  if (env)
  {
    UdmVarListAddStr(&Env->Vars, "REMOTE_USER", env);
    UdmLoadGroups(Env);
  }
#endif
  
  UdmEnvSetDirs(Env);
  if((res=UdmTemplateLoad(Agent, q.template_name, &tmpl)))
  {
    UdmHTTPHeadersSend(&q, "text/plain", NULL);
    printf("%s\n",Env->errstr);
    UdmVarListFree(&tmpl);
    UdmVarListFree(&query_vars);
    UdmEnvFree(Env);
    UdmAgentFree(Agent);
    return(0);
  }


  /* This is for query tracking, "tmplt" is also used in QCache */
  UdmVarListReplaceStr(&Env->Vars, "tmplt", q.template_name);
  UdmVarListAddStr(&Env->Vars,"QUERY_STRING", q.query_string);
  UdmVarListAddStr(&Env->Vars,"self", q.self);

  UdmTemplatePrint(Agent, NULL, NULL, 0, &Env->Vars, &tmpl, "variables");


#ifdef HAVE_LOCALE_H
  if ((env= UdmVarListFindSafeStr(&Env->Vars, "Locale", NULL)))
    setlocale(LC_ALL, env);
#endif

#ifdef HAVE_SETVBUF
  {
    /*
      Set stdout buffer size. It allows to avoid re-rendering
      in the browser. After setting StdoutBufferSize to a
      reasonably big value, search.cgi will send whole output
      at once in the end, rather than gradually.
    */
    size_t bsz= (size_t) UdmVarListFindSafeInt(&Env->Vars, "StdoutBufferSize", 0);
    if (bsz > 0)
      setvbuf(stdout, NULL, _IOFBF, bsz);
  }
#endif

  UdmSetLogLevel(NULL, UdmVarListFindSafeInt(&Env->Vars, "LogLevel", 0));
  UdmOpenLog("search.cgi", Env, !strcasecmp(UdmVarListFindSafeStr(&Env->Vars, "Log2stderr", (!q.httpd) ? "yes" : "no"), "yes"));
  UdmLog(Agent,UDM_LOG_ERROR, "search.cgi started with '%s'", q.template_name);
  
  
  bcharset= UdmVarListFindSafeStr(&Env->Vars,"BrowserCharset","iso-8859-1");
  Env->bcs= UdmGetCharSet(bcharset);
  lcharset= UdmVarListFindSafeStr(&Env->Vars,"LocalCharset","iso-8859-1");
  Env->lcs= UdmGetCharSet(lcharset);
  ppp= UdmVarListFindSafeInt(&Env->Vars,"PagesPerScreen",10);
  
  if (! Env->bcs)
  {
    UdmHTTPHeadersSend(&q, "text/plain", NULL);
    printf("Unknown BrowserCharset '%s' in template '%s'\n",
           bcharset, q.template_name);
    exit(0);
  }
  
  if(! Env->lcs)
  {
    UdmHTTPHeadersSend(&q, "text/plain", NULL);
    printf("Unknown LocalCharset '%s' in template '%s'\n",
           lcharset, q.template_name);
    exit(0);
  }
  
  if (UdmVarListFindInt(&Env->Vars, "cc", 0))
  {
    UdmVarListFree(&query_vars);
    return StoreDocCGI(Agent, &tmpl, &q);
  }
  
  UdmHTTPHeadersSend(&q,
                     UdmVarListFindSafeStr(&Env->Vars, "ResultContentType", "text/html"),
                     bcharset);
  
  /* These parameters taken from "variable section of template"*/
  
  res= UdmVarListFindInt(&Env->Vars,"np",0)*UdmVarListFindInt(&Env->Vars,"ps",10);
  UdmVarListAddInt(&Env->Vars,"pn",res);
  
  if(NULL==(Res=UdmFind(Agent)))
  {
    UdmVarListAddStr(&Env->Vars,"E",UdmEnvErrMsg(Agent->Conf));
    UdmTemplatePrint(Agent, stdout, NULL, 0, &Env->Vars, &tmpl, "top");
    UdmTemplatePrint(Agent, stdout, NULL, 0, &Env->Vars, &tmpl, "error");
    if (Res != NULL)
      goto freeres;
    goto end;
  }
  UdmVarListAddInt(&Env->Vars,"first",(int)Res->first);
  UdmVarListAddInt(&Env->Vars,"last",(int)Res->last);
  UdmVarListAddInt(&Env->Vars,"total",(int)Res->total_found);
  sprintf(search_time,"%.3f",((double)Res->work_time)/1000);
  UdmVarListAddStr(&Env->Vars,"SearchTime",search_time);
  UdmTemplatePrint(Agent, stdout, NULL, 0, &Env->Vars, &tmpl, "top");
#ifdef TRIAL_VER
  fprintf(stdout,"%s",POWERED_LOGO);
#endif
  
  if(!Res->WWList.nwords)
  {
    UdmTemplatePrint(Agent, stdout, NULL, 0, &Env->Vars, &tmpl, "noquery");
    goto freeres;
  }
  
  if(!Res->num_rows)
  {
    UdmTemplatePrint(Agent, stdout, NULL, 0, &Env->Vars, &tmpl, "notfound");
    goto freeres;
  }
  
  page_size   = UdmVarListFindInt(&Agent->Conf->Vars,"ps",10);
  page_number = UdmVarListFindInt(&Agent->Conf->Vars,"np",0);
  
  
  npages= (Res->total_found/(page_size?page_size:10)) +
          ((Res->total_found % (page_size?page_size:10) != 0 ) ?  1 : 0);
  page1= page_number-ppp/2;
  page2= page_number+ppp/2;
  if(page1<0)
  {
    page2-=page1;
    page1=0;
  }
  else if(page2>npages)
  {
    page1-=(page2-npages);
    page2=npages;
  }
  if(page1 < 0)
    page1=0;
  if( page2 > npages)
    page2=npages;
  nav = (char *)UdmRealloc(nav, nav_len = (size_t)(page2 - page1 + 2) * (1024 + 1024)); 
                           /* !!! 1024 - limit for navbar0/navbar1 template size */ 
  nav[0] = '\0';
  
  /* build NL NB NR */
  for(i = (size_t)page1; i < (size_t)page2; i++)
  {
    UdmVarListReplaceOrDelInt(&query_vars,"np",(int)i);
    BuildPageURL(&query_vars, &url);
    UdmVarListReplaceStr(&Env->Vars,"NH",url);
    UdmVarListReplaceInt(&Env->Vars,"NP",(int)(i+1));
    UdmTemplatePrint(Agent, NULL, UDM_STREND(nav),
                     nav_len - (nav - UDM_STREND(nav)), &Env->Vars, &tmpl,
                     (i == (size_t)page_number)?"navbar0":"navbar1");
  }
  UdmVarListAddStr(&Env->Vars,"NB",nav);
  
  UdmVarListReplaceOrDelInt(&query_vars,"np",page_number-1);
  BuildPageURL(&query_vars, &url);
  UdmVarListReplaceStr(&Env->Vars,"NH",url);
  
  if(Res->first==1) /* First page */
  {
    UdmTemplatePrint(Agent, NULL, nav, nav_len, &Env->Vars, &tmpl, "navleft_nop");
    UdmVarListReplaceStr(&Env->Vars,"NL",nav);
  }
  else
  {
    UdmTemplatePrint(Agent, NULL, nav, nav_len, &Env->Vars, &tmpl, "navleft");
    UdmVarListReplaceStr(&Env->Vars,"NL",nav);
  }
  
  UdmVarListReplaceOrDelInt(&query_vars,"np",page_number+1);
  BuildPageURL(&query_vars, &url);
  UdmVarListReplaceStr(&Env->Vars,"NH",url);
  
  UdmVarListReplaceOrDelInt(&query_vars, "np", 0);
  UdmVarListDel(&query_vars, "s");
  BuildPageURL(&query_vars, &url);
  UdmVarListReplaceStr(&Env->Vars, "FirstPage", url);
  
  if(Res->last>=Res->total_found) /* Last page */
  {
    UdmTemplatePrint(Agent, NULL, nav, nav_len, &Env->Vars, &tmpl, "navright_nop");
    UdmVarListReplaceStr(&Env->Vars,"NR",nav);
  }
  else
  {
    UdmTemplatePrint(Agent, NULL, nav, nav_len, &Env->Vars, &tmpl, "navright");
    UdmVarListReplaceStr(&Env->Vars,"NR",nav);
  }
  
  UdmTemplatePrint(Agent, stdout, NULL, 0, &Env->Vars,&tmpl, "restop");

  for(i= 0; i < Res->num_rows; i++)
  {
    UDM_DOCUMENT *Doc= &Res->Doc[i];
    UDM_VARLIST Merge;
    size_t sc;

    if ((sc = UdmVarListFindInt(&Env->Vars, "site", 0)) == 0)
    {
      UdmVarListReplaceOrDelInt(&query_vars,"np", 0);
      UdmVarListReplaceInt(&query_vars, "site", UdmVarListFindInt(&Doc->Sections, "Site_id", 0));
      BuildPageURL(&query_vars, &url);
      UdmVarListReplaceStr(&Env->Vars, "sitelimit_href", url);
    }

    UdmVarListInit(&Merge);
    UdmVarListMerge(&Merge, &Env->Vars, &Doc->Sections);
    UdmTemplatePrint(Agent, stdout, NULL, 0, &Merge, &tmpl, "res");
    UdmVarListFree(&Merge);
  }

  UdmTemplatePrint(Agent, stdout, NULL, 0, &Env->Vars, &tmpl, "resbot");

  
freeres:
  UdmResultFree(Res);
  
end:
  UdmTemplatePrint(Agent, stdout, NULL, 0, &Env->Vars, &tmpl, "bottom");
  
  UdmVarListFree(&tmpl);
  UdmVarListFree(&query_vars);
  UdmEnvFree(Env);
  UdmAgentFree(Agent);
  UDM_FREE(url);
  UDM_FREE(nav);
  if (q.httpd) 
    fflush(NULL); 
  else
    fclose(stdout);
  UdmWSACleanup();
  return UDM_OK;
}
