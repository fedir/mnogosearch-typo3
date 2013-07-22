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
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>

#ifdef WIN32
#include <process.h>
#endif

#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#ifdef CHASEN
#include <chasen.h>
#endif

#ifdef MECAB
#include <mecab.h>
#endif

#include "udmsearch.h"
#include "udm_sqldbms.h"
#include "udm_uniconv.h"
#include "udm_parsexml.h"
#include "udm_http.h"

/* This should be last include */
#ifdef DMALLOC
#include "dmalloc.h"
#endif

static int log2stderr=1;
static int have_loglevel= 0;
static int loglevel= UDM_LOG_INFO;
static char cname[1024]="";
static int add_servers=UDM_FLAG_ADD_SERV;
static int add_server_urls = UDM_FLAG_ADD_SERVURL;
static int load_langmaps=UDM_FLAG_LOAD_LANGMAP;
static int load_spells=UDM_FLAG_SPELL;
static int load_for_dump= 0;
static int warnings=1;

static UDM_ENV Conf;

extern unsigned int seconds;   /* To sleep between documents   */
extern int flags;              /* For indexer                  */
extern int total_threads;      /* Total threads number         */
extern int sleep_threads;      /* Number of sleepping threads  */
extern int max_index_time;
extern int cur_url_number;
extern int maxthreads;
extern UDM_AGENT *ThreadIndexers;
extern int thd_errors;
extern udm_thread_t *threads;


#ifdef HAVE_PTHREAD
#ifdef WIN32
#include <time.h>
static struct tm*
localtime_r(const time_t *clock, struct tm *result)
{
  *result= *localtime(clock);
  return result;
}
#define strptime(x, y, z) (0)
#endif /* WIN32 */
#endif /* HAVE_PTHREAD */


/* CallBack function for log information */
#ifdef WIN32
void UdmShowInfo(UDM_AGENT* A, const char *state, const char* str)
{
  printf("%d %s %s\n", A ? A->handle : 0,state,str);
}
#else
extern __C_LINK void __UDMCALL UdmShowInfo(UDM_AGENT* A, const char *state, const char* str);
#endif /* WIN32 */


static int UdmDisplaySQLQuery(UDM_SQLMON_PARAM *prm, UDM_SQLRES *sqlres)
{
  int       res = UDM_OK;
#ifdef HAVE_SQL     
  size_t         i,j;

  if (prm->flags & UDM_SQLMON_DISPLAY_FIELDS)
  {
    for (i=0;i<sqlres->nCols;i++)
    {
      if (i>0)
        fprintf(prm->outfile,"\t");
      fprintf(prm->outfile,"%s", sqlres->Fields ?
                                 sqlres->Fields[i].sqlname : "<NONAME>");
      if (i+1==sqlres->nCols)
        fprintf(prm->outfile,"\n");
    }
  }
  
  for (i=0;i<sqlres->nRows;i++)
  {
    for (j=0;j<sqlres->nCols;j++)
    {
      const char *v=UdmSQLValue(sqlres,i,j);
      if (j>0)
        fprintf(prm->outfile,"\t");
      if (j < 10 && (prm->colflags[j] & 1))
      {
        const char* s, *e;
        fprintf(prm->outfile,"0x");
                 
        for (s=v, e= s+UdmSQLLen(sqlres,i,j); s < e; s++)
          fprintf(prm->outfile,"%02X",(int)(unsigned char)s[0]);
      }
      else
      {
        fprintf(prm->outfile,"%s",v?v:"NULL");
      }
      if (j+1==sqlres->nCols)fprintf(prm->outfile,"\n");
    }
  }
     
#endif
     return res;
}


static const char *execsql= NULL;
static char *
sqlexecgets(UDM_SQLMON_PARAM *prm, char *str, size_t len)
{
  if (!execsql)
    return NULL;
  udm_snprintf(str, len, "%s", execsql);
  prm->flags|= UDM_SQLMON_DONT_NEED_SEMICOLON;
  execsql= NULL;
  return str;
}


static char* sqlmongets(UDM_SQLMON_PARAM *prm, char *str, size_t size)
{
#ifdef HAVE_READLINE
  if ((prm->infile == stdin) && isatty(0))
  {
     char prompt[]="SQL>";
     char *line= readline(prompt);
     if (!line)
       return 0;
     
     if (*line) add_history(line);
     /* We need "\n" at the end to make sqlmon work properly */
     udm_snprintf(str, size, "%s\n", line);
  }
  else
#endif
  {
    if (prm->loglevel >= UDM_LOG_INFO)
      prm->prompt(prm, UDM_SQLMON_MSG_PROMPT, "SQL>");
    if (!fgets(str, size, prm->infile))
      return 0;
  }
  return str;
}

static int sqlmonprompt(UDM_SQLMON_PARAM *prm, int msqtype, const char *msg)
{
  fprintf(prm->outfile,"%s",msg);
  return UDM_OK;
}

static int CreateOrDrop(UDM_AGENT *A, enum udm_indcmd cmd)
{
  size_t i;
  char fname[1024];
  const char *sdir=UdmVarListFindStr(&Conf.Vars,"ShareDir",UDM_SHARE_DIR);
#ifdef HAVE_FHS_LAYOUT
  const char *sdir2= "create" UDMSLASHSTR;
#else
  const char *sdir2= "";
#endif
  UDM_DBLIST *L= &A->Conf->dbl;
  UDM_SQLMON_PARAM prm;
  size_t num= UdmVarListFindInt(&A->Conf->Vars, "DBLimit", 0);
  
  for (i=0; i<L->nitems; i++)
  {
    FILE *infile;
    UDM_DB *db= &L->db[i];
    if (num != 0 && num != i + 1)
      continue;
    udm_snprintf(fname,sizeof(fname),"%s%s%s%s%s%s.%s.sql",
      sdir,UDMSLASHSTR, sdir2, UdmDBTypeToStr(db->DBType),UDMSLASHSTR,
      UdmIndCmdStr(cmd),UdmDBModeToStr(db->DBMode));
    printf("'%s' dbtype=%d dbmode=%d\n",fname,db->DBType,db->DBMode);
    if (!(infile= fopen(fname,"r")))
    {
      sprintf(A->Conf->errstr,"Can't open file '%s'",fname);
      return UDM_ERROR;
    }
    L->currdbnum= i;
    bzero((void*)&prm,sizeof(prm));
    prm.infile= infile;
    prm.outfile= stdout;
    prm.flags= UDM_SQLMON_DISPLAY_FIELDS;
    prm.gets= sqlmongets;
    prm.display= UdmDisplaySQLQuery;
    prm.prompt= sqlmonprompt;
    prm.loglevel= UDM_LOG_DEBUG;
    UdmSQLMonitor(A, A->Conf,&prm);
    printf("%d queries sent, %d succeeded, %d failed\n",
           (int) prm.nqueries, (int) prm.ngood, (int) prm.nbad);
    fclose(infile);
  }
  return UDM_OK;
}


static int
UdmIndCreate(UDM_AGENT *Agent)
{
  int rc;
  if (UDM_OK != (rc= CreateOrDrop(Agent, UDM_IND_CREATE)))
    UdmLog(Agent, UDM_LOG_ERROR, "Error: '%s'", UdmEnvErrMsg(Agent->Conf));
  return rc;
}


static int
UdmIndDrop(UDM_AGENT *Agent)
{
  int rc;
  if (UDM_OK != (rc= CreateOrDrop(Agent, UDM_IND_DROP)))
    UdmLog(Agent, UDM_LOG_ERROR, "Error: '%s'", UdmEnvErrMsg(Agent->Conf));
  return rc;
}


static int ShowStatistics(UDM_AGENT *Indexer)
{
  int       res;
  struct tm tm;
  const char *stat_time;
  char sbuf[32];
  UDM_STATLIST   Stats;
  size_t         snum;
  UDM_STAT  Total;
     
  bzero((void*)&Total, sizeof(Total));
  Stats.time = time(NULL);
  stat_time = UdmVarListFindStr(&Conf.Vars, "stat_time", "0");
  bzero(&tm, sizeof(tm));

#ifndef WIN32
  if (stat_time &&
      strlen(stat_time) >= 7 &&
      stat_time[4] == '-' &&
      (stat_time[7] == '-' || !stat_time[7]) &&
      (strptime(stat_time, "%Y-%m-%d %H:%M:%S", &tm) ||
       strptime(stat_time, "%Y-%m-%d %H:%M", &tm) ||
       strptime(stat_time, "%Y-%m-%d %H:%M", &tm) ||
       strptime(stat_time, "%Y-%m-%d %H", &tm) ||
       strptime(stat_time, "%Y-%m-%d", &tm) ||
       strptime(stat_time, "%Y-%m", &tm)))
  {
    Stats.time = mktime(&tm);
  }
  else if (stat_time && (Stats.time = Udm_dp2time_t(stat_time)) >= 0)
  {
    Stats.time += time(NULL);
    localtime_r(&Stats.time, &tm);
  }
  else
  {
    Stats.time = time(NULL);
    localtime_r(&Stats.time, &tm);
  }
#else
  {
    struct tm *tm1;
    Stats.time= time(NULL);
    tm= *(tm1= localtime(&Stats.time));
  }
#endif
  if (UDM_OK != (res= UdmStatAction(Indexer,&Stats)))
  {
    UdmLog(Indexer, UDM_LOG_ERROR, "Error: '%s'", UdmEnvErrMsg(Indexer->Conf));
    goto ex;
  }

  strftime(sbuf, sizeof(sbuf), "%Y-%m-%d %H:%M:%S", &tm);
  printf("\n          Database statistics [%s]\n\n", sbuf);
  printf("%10s %10s %10s\n","Status","Expired","Total");
  printf("   -----------------------------\n");
  for(snum=0;snum<Stats.nstats;snum++)
  {
    UDM_STAT  *S=&Stats.Stat[snum];
    printf("%10d %10d %10d %s\n",S->status,S->expired,S->total,UdmHTTPErrMsg(S->status));
    Total.expired+=S->expired;
    Total.total+=S->total;
  }
  printf("   -----------------------------\n");
  printf("%10s %10d %10d\n","Total",Total.expired,Total.total);
  printf("\n");

ex:
  UDM_FREE(Stats.Stat);
  return(res);
}

/* CallBack Func for Referers*/
static void UdmRefProc(int code, const char *url, const char * ref)
{
     printf("%d %s %s\n",code,url,ref);
}

__C_LINK static int __UDMCALL ShowReferers(UDM_AGENT * Indexer)
{
  int res;
  printf("\n          URLs and referers \n\n");
  res= UdmURLAction(Indexer, NULL, UDM_URL_ACTION_REFERERS);
  return(res);
}


#undef THINFO_TEST
#ifdef THINFO_TEST
/* CallBack function for Thread information */
void UdmShowThreadInfoProc(int handle,char *state, char* str)
{
  printf("%d %s %s\n",handle,state,str);
}
#endif


static int cmpgrp(const void *v1, const void *v2)
{
  int res;
  const UDM_CHARSET *c1=v1;
  const UDM_CHARSET *c2=v2;
  if ((res = strcasecmp(UdmCsGroup(c1), UdmCsGroup(c2))))
    return res;
  return strcasecmp(c1->name,c2->name);
}

static void display_charsets(FILE *file)
{
  UDM_CHARSET *cs=NULL;
  UDM_CHARSET c[100];
  size_t i=0;
  size_t n=0;
  int family=-1;
     
  for(cs=UdmGetCharSetByID(0) ; cs && cs->name ; cs++)
  {
    /* Skip not compiled charsets */
    if (cs->family != UDM_CHARSET_UNKNOWN)
      c[n++]=*cs;
  }
  fprintf(file,"\n%d charsets available:\n", (int) n);

  UdmSort(c,n,sizeof(UDM_CHARSET),&cmpgrp);
  for(i=0;i<n;i++)
  {
    if (family!=c[i].family)
    {
      fprintf(file, "\n%19s : ", UdmCsGroup(&c[i]));
      family=c[i].family;
    }
    fprintf(file,"%s ",c[i].name);
  }
  fprintf(file,"\n");
}


typedef struct udm_indcmd2str_st
{
  const char *name;
  enum udm_indcmd cmd;
  const char *comment;
} UDM_CMDLINE_EXTENDED_OPTION;


static UDM_CMDLINE_EXTENDED_OPTION indcmd[]=
{
  {"index",         UDM_IND_INDEX,         ""},
  {"crawl",         UDM_IND_CRAWL,         ""},
  {"statistics",    UDM_IND_STAT,          ""},
#ifdef HAVE_SQL
  {"create",        UDM_IND_CREATE,        "create SQL table structure and exit"},
  {"drop",          UDM_IND_DROP,          "drop SQL table structure and exit"},
  {"blob",          UDM_IND_MULTI2BLOB,    "create fast search index"},
#endif
  {"delete",        UDM_IND_DELETE,        ""},
  {"referers",      UDM_IND_REFERERS,      ""},
  {"checkconf",     UDM_IND_CHECKCONF,     ""},
  {"export",        UDM_IND_EXPORT,        ""},
#ifdef HAVE_SQL
  {"wordstat",      UDM_IND_WRDSTAT,       "create statistics for misspelled word suggestions"},
#endif
#ifdef HAVE_SQL
  {"sqlmon",        UDM_IND_SQLMON,        "run interactive SQL monitor"},
#endif
  {"rewriteurl",    UDM_IND_REWRITEURL,    ""},
  {"rewritelimits", UDM_IND_REWRITELIMITS, ""},
  {"hashspell",     UDM_IND_HASHSPELL,     ""},
  {"dumpspell",     UDM_IND_DUMPSPELL,     ""},
  {"dumpconf",      UDM_IND_DUMPCONF,      ""},
  {"dumpdata",      UDM_IND_DUMPDATA,      ""},
  {"restoredata",   UDM_IND_RESTOREDATA,   ""},
  {"",              UDM_IND_CRAWL,         ""}
};


static enum udm_indcmd
UdmIndCmd(const char *cmdname)
{
  struct udm_indcmd2str_st *cmd;
  size_t matches;
  enum udm_indcmd res= UDM_IND_CRAWL;
  size_t cmdlen= strlen(cmdname);
  
  for (matches= 0, cmd= indcmd; cmd->name[0]; cmd++)
  {
    if (!strncasecmp(cmd->name, cmdname, cmdlen))
    {
      res= cmd->cmd;
      matches++;
    }
  }
  if (matches == 1)
    return res;
  return matches ? UDM_IND_AMBIGUOUS : UDM_IND_UNKNOWN;
}


static void
usage_print_extended_commands(FILE *file)
{
  UDM_CMDLINE_EXTENDED_OPTION *cmd;
  for (cmd= indcmd; cmd->name[0]; cmd++)
  {
    if (cmd->comment[0])
      fprintf(file, "  -E%-14s%s\n", cmd->name, cmd->comment);
  }
}


static int usage(int level, UDM_CMDLINE_OPT *options)
{
  FILE *file= stdout;
  fprintf(file, "\n");
  fprintf(file, "indexer from %s-%s-%s\n", PACKAGE, VERSION, UDM_DBTYPE);
  fprintf(file, "http://www.mnogosearch.org/ (C)1998-2013, LavTech Corp.\n");
  fprintf(file, "\n");
  fprintf(file, "Usage: indexer [OPTIONS]  [configfile]\n");

  UdmCmdLineOptionsPrint(options, stdout);
  if (0)
    usage_print_extended_commands(file);
  
  fprintf(file, "\n");
  fprintf(file, "\n");
  fprintf(file, "Please post bug reports and suggestions at http://www.mnogosearch.org/bugs/\n");


  if (level>1)display_charsets(file);
  return(0);
}


/*
  Load indexer.conf and check if any DBAddr were given
*/
static int UdmIndexerEnvLoad(UDM_AGENT *Indexer, const char *fname,int lflags)
{
  int rc;
  if (UDM_OK == (rc= UdmEnvLoad(Indexer, fname, lflags)))
  {
    if (Indexer->Conf->dbl.nitems == 0)
    {
      sprintf(Indexer->Conf->errstr, "Error: '%s': No required DBAddr commands were specified", fname);
      rc= UDM_ERROR;
    }
  }
  return rc;
}



/*
  Parse command line
*/
static int UdmARGC;
static char **UdmARGV;
static enum udm_indcmd cmd = UDM_IND_CRAWL;
static int insert = 0, expire = 0, pop_rank = 0, block = 0, help = 0;
static const char *url_filename=NULL;


/* 
  Available new options:
  Capital letters:  B    GH JK M OP   T VXWXYZ
  Small   letters:           k             x z
  Digits:          123456789
*/
static int
UdmCmdLineHandleOption(UDM_CMDLINE_OPT *opt, const char *value)
{
  switch (opt->id)
  {
    case 'F':
    {
      UDM_VARLIST V,W;
      size_t i;
      UdmVarListInit(&V);
      UdmVarListInit(&W);
      UdmFeatures(&V);
      UdmVarListAddLst(&W,&V,NULL,value);
      for(i=0;i<W.nvars;i++)
        printf("%s:%s\n",W.Var[i].name,W.Var[i].val);
      exit(0);
    }
    case 'C': cmd= UDM_IND_DELETE;  add_servers=0;load_langmaps=0;load_spells=0;break;
    case 'S': cmd= UDM_IND_STAT;    add_servers=0;load_langmaps=0;load_spells=0;break;
    case 'I': cmd= UDM_IND_REFERERS;add_servers=0;load_langmaps=0;load_spells=0;break;
    case 'Q': cmd= UDM_IND_SQLMON;  add_servers=0;load_langmaps=0;load_spells=0;break;
    case UDM_IND_INDEX:
    case UDM_IND_CRAWL:
    case UDM_IND_CREATE:
    case UDM_IND_DROP:
    case UDM_IND_CHECKCONF:
    case UDM_IND_CONVERT:
    case UDM_IND_MULTI2BLOB:
    case UDM_IND_EXPORT:
    case UDM_IND_WRDSTAT:
    case UDM_IND_REWRITEURL:
    case UDM_IND_HASHSPELL:
    case UDM_IND_DUMPSPELL:
    case UDM_IND_REWRITELIMITS:
    case UDM_IND_DUMPCONF:
    case UDM_IND_DUMPDATA:
    case UDM_IND_RESTOREDATA:
      cmd= opt->id;
      break;
    case UDM_IND_EXECSQL:
      cmd= UDM_IND_SQLMON;
      UdmVarListReplaceStr(&Conf.Vars, "exec", value);
      add_servers= load_langmaps= load_spells= 0;
      break;
    case UDM_IND_SET0:
      UDM_ASSERT(opt->name && opt->name[0]);
      UdmVarListReplaceStr(&Conf.Vars, opt->name, value);
      break;
    case UDM_IND_SET:
      {
        const char *eq= strchr(value, '=');
        char name[80];
        if (eq && (eq - value) < sizeof(name))
        {
          memcpy(name, value, eq - value);
          name[eq-value]= '\0';
          UdmVarListReplaceStr(&Conf.Vars, name, eq + 1);
        }
        else
          UdmVarListReplaceInt(&Conf.Vars, value, 1);
      }
      break;

    case 'E': cmd= UdmIndCmd(value);break;
    case 'R': pop_rank++; break;
    case 'q': 
      if (add_server_urls == 0) /* -q already given */
      {
        /*
          "indexer -qq" is given, do even faster start-up.
          Don't synchonize "Server" commands in indexer.conf
          with the "server" table content.
        */
        add_servers|= UDM_FLAG_DONT_ADD_TO_DB;
      }
      add_server_urls= 0;
      break;
    case 'l': log2stderr=0;break;
    case 'a': expire=1;break;
    case 'b': block++;break;
    case 'e': flags|=UDM_FLAG_SORT_EXPIRED;break;
    case 'o': flags|=UDM_FLAG_SORT_HOPS;break;
    case 'r': flags|=UDM_FLAG_DONTSORT_SEED; break;
    case 'm': flags|=UDM_FLAG_REINDEX;break;
    case 'n': Conf.url_number=atoi(value);break;
    case 'c': max_index_time=atoi(value);break;
    case 'v': loglevel= atoi(value); have_loglevel= 1; break;
    case 'p': seconds=atoi(value);break;
    case 't': UdmVarListAddStr(&Conf.Vars,"tag" , value);break;
    case 'g': UdmVarListAddStr(&Conf.Vars,"cat" , value);break;
    case 's': UdmVarListAddStr(&Conf.Vars, "status", value);break;
    case 'y': UdmVarListAddStr(&Conf.Vars,"type", value);break;
    case 'L': UdmVarListAddStr(&Conf.Vars,"lang", value);break;
    case 'u': UdmVarListAddStr(&Conf.Vars,"u"   , value);
      if (insert)
      {
        UDM_HREF Href;
        UdmHrefInit(&Href);
        Href.url= (char*) value;
        Href.method=UDM_METHOD_GET;
        UdmHrefListAdd(&Conf.Hrefs, &Href);
      }
      break;
    case 'N':
      maxthreads=atoi(value);
      UdmVarListReplaceInt(&Conf.Vars, "CrawlerThreads", maxthreads);
      break;
    case 'f': url_filename= value; break;
    case 'i': insert=1;break;
    case 'w': warnings=0;break;
    case 'j': UdmVarListAddStr(&Conf.Vars, "stat_time", value); break;
    case 'd': strncpy(cname, value, sizeof(cname));
      cname[sizeof(cname) - 1] = '\0';
      break;
    case 'D': UdmVarListAddStr(&Conf.Vars,"DBLimit"   , value); break;
    case '-':
      {
        char *arg= strchr(value, '=');
        if (arg)
        {
          *arg++= '\0';
          UdmVarListAddStr(&Conf.Vars, value, arg);
        }
        else
          help++;
      }
      break;
    case '?':
    case 'h':
    default:
      help++;
  }
  return 0;
}


/*
  Available new options:
  Capital letters: AB    GH JK M OP   TUVXWXYZ
  Small   letters:           k             x z
  Digits:          123456789
*/
static UDM_CMDLINE_OPT udm_indexer_options[]=
{
  {-1, "",  UDM_OPT_TITLE,NULL, "\nCrawler options:"},
  {'a', "", UDM_OPT_BOOL, NULL, "Revisit all documents even if not expired (can be\n"
                                "limited using -t, -u, -s, -c, -y and -f options)"},
  {'m', "", UDM_OPT_BOOL, NULL, "Update expired documents even if not modified (can be\n"
                                "limited using -t, -u, -c, -s, -y and -f options)"},
  {'e', "", UDM_OPT_BOOL, NULL, "Visit 'most expired' (oldest) documents first"},
  {'o', "", UDM_OPT_BOOL, NULL, "Visit documents with less depth (hops value) first"},
  {'r', "", UDM_OPT_BOOL, NULL, "Do not try to reduce remote servers load by randomising\n"
                                "crawler queue order (faster, but less polite)"},
  {'n', "", UDM_OPT_INT,  NULL, "Visit only # documents and exit"},
  {'c', "", UDM_OPT_INT,  NULL, "Visit only # seconds and exit"},
  {'q', "", UDM_OPT_BOOL, NULL, "Quick startup (do not add Server URLs);  -qq even quicker"},
  {'b', "", UDM_OPT_BOOL, NULL, "Block starting more than one indexer instances"},
  {'i', "", UDM_OPT_BOOL, NULL, "Insert new URLs (URLs to insert must be given using -u or -f)"},
  {'p', "", UDM_OPT_INT,  NULL, "Sleep # seconds after downloading every URL"},
  {'w', "", UDM_OPT_BOOL, NULL, "Do not ask for confirmation when clearing documents\n"
                                "from the database (e.g.: indexer -Cw)"},
  {'N', "", UDM_OPT_INT,  NULL, "Run # crawler threads"},


  {-1, "",  UDM_OPT_TITLE,NULL, "\nSubsection control options (can be combined):"},
  {'s', "", UDM_OPT_STR,  NULL, "Limit indexer to documents matching status (HTTP Status code)"},
  {'t', "", UDM_OPT_STR,  NULL, "Limit indexer to documents matching tag"},
  {'g', "", UDM_OPT_STR,  NULL, "Limit indexer to documents matching category"},
  {'y', "", UDM_OPT_STR,  NULL, "Limit indexer to documents matching content-type"},
  {'L', "", UDM_OPT_STR,  NULL, "Limit indexer to documents matching language"},
  {'u', "", UDM_OPT_STR,  NULL, "Limit indexer to documents with URLs matching pattern\n"
                                "(supports SQL LIKE wildcards '%' and '_')"},
  {0, "seed",UDM_OPT_STR, NULL, "Limit indexer to documents with the given seed (0-255)"},
  {'D', "", UDM_OPT_STR,  NULL, "Work with the n-th database only (i.e. with the n-th DBAddr)"},
  {'f', "", UDM_OPT_STR,  NULL, "Read URLs to be visited/inserted/deleted from file (with -a\n"
                                "or -C option, supports SQL LIKE wildcard '%%'; has no effect\n"
                                "when combined with -m option)"},
  {-1,  "", UDM_OPT_TITLE,NULL,
  "  -f -            Use stdin instead of a file as an URL list"},


  {-1,  "", UDM_OPT_TITLE,NULL, "\nLogging options:"},
  {'l', "", UDM_OPT_BOOL, NULL, "Do not log to stdout/stderr"},
  {'v', "", UDM_OPT_INT,  NULL, "Verbose level (0-5)"},


  {-1, "",  UDM_OPT_TITLE,NULL, "\nMisc. options:"},
  {'F', "", UDM_OPT_STR,  NULL, "Print compile configuration and exit (e.g.: indexer -F '*')"},
  {'h',"help",UDM_OPT_BOOL, NULL,"Print help page and exit; -hh print more help"},
  {'?', "", UDM_OPT_BOOL, NULL, "Print help page and exit; -?? print more help"},
  {'d', "", UDM_OPT_STR,  NULL, "Use the given configuration file instead of indexer.conf"
#ifndef WIN32
                                "\nThis option is usefull when running indexer as an\n"
                                "interpreter, e.g.: #!/usr/local/sbin/indexer -d"
#endif
  },
  {'j', "", UDM_OPT_STR,  NULL, "Set current time for statistic (use with -S),\n"
                                "format: YYYY-MM[-DD[ HH[:MM[:SS]]]]\n"
                                "or time offset, e.g. 1d12h (see Period in indexer.conf)"},
  {UDM_IND_SET,          "set",          UDM_OPT_STR, NULL, "Set variable"},
  {-1, "",  UDM_OPT_TITLE,NULL, "\nCommands (can be used with subsection control options):"},
  {UDM_IND_CRAWL,        "crawl",        UDM_OPT_BOOL,NULL, "Crawl (default command)"},
  {UDM_IND_MULTI2BLOB,   "index",        UDM_OPT_BOOL,NULL, "Create search index"},
  {UDM_IND_WRDSTAT,      "wordstat",     UDM_OPT_BOOL,NULL, "Create statistics for misspelled word suggestions"},
  {UDM_IND_REWRITEURL,   "rewriteurl",   UDM_OPT_BOOL,NULL, "Rewrite URL data into the current search index"},
  {UDM_IND_REWRITELIMITS,"rewritelimits",UDM_OPT_BOOL,NULL, "Recreate all Limit, UserScore, UserOrder data"},
  {UDM_IND_DELETE, /*C*/  "delete",      UDM_OPT_BOOL,NULL, "Delete documents from the database"},
  {UDM_IND_STAT,   /*S*/ "statistics",   UDM_OPT_BOOL,NULL, "Print statistics and exit"},
  {UDM_IND_REFERERS,/*I*/ "referers",    UDM_OPT_BOOL,NULL, "Print referers and exit "},
  {'R', "", UDM_OPT_BOOL, NULL, "Crawl then calculate popularity rank"},

  {-1, "",  UDM_OPT_TITLE,NULL, "\nOther commands:"},
#ifdef HAVE_SQL
  {UDM_IND_CREATE,       "create",       UDM_OPT_BOOL,NULL, "Create SQL table structure and exit"},
  {UDM_IND_DROP,         "drop",         UDM_OPT_BOOL,NULL, "Drop SQL table structure and exit"},
  {UDM_IND_SQLMON, /*Q*/ "sqlmon",       UDM_OPT_BOOL,NULL, "Run interactive SQL monitor"},
  {UDM_IND_EXECSQL,      "exec",         UDM_OPT_STR, NULL, "Execute SQL query"},
#endif
  {UDM_IND_CHECKCONF,    "checkconf",    UDM_OPT_BOOL,NULL, "Check configuration file for good syntax"},
  {UDM_IND_EXPORT,       "export",       UDM_OPT_BOOL,NULL, NULL}, /* TODO */
  {UDM_IND_HASHSPELL,    "hashspell",    UDM_OPT_BOOL,NULL, "Create hash files for the active Ispell dictionaries"},
  {UDM_IND_DUMPSPELL,    "dumpspell",    UDM_OPT_BOOL,NULL, "Dump Ispell data for use with SQLWordForms"},
  {UDM_IND_DUMPCONF,     "dumpconf",     UDM_OPT_BOOL,NULL, NULL},
  {UDM_IND_DUMPDATA,     "dumpdata",     UDM_OPT_BOOL,NULL, "Dump collected data using SQL statements"},
  {UDM_IND_RESTOREDATA,  "restoredata",  UDM_OPT_BOOL,NULL, "Load prevously dumped data (give a filename using -f)"},

  {'E',"", UDM_OPT_STR, NULL, NULL},
  {UDM_IND_SET0, "fl", UDM_OPT_STR, NULL, NULL},
  {0,NULL,0,NULL,NULL}
};


static int
UdmParseCmdLine(int argc, char **argv, size_t *noptions)
{
  return UdmCmdLineOptionsGet(argc, argv, udm_indexer_options,
                              UdmCmdLineHandleOption, noptions);
}


/*
static int
UdmReloadEnv(UDM_AGENT *Indexer)
{
  UDM_ENV   NewConf;
  int  rc;

  UdmLog(Indexer,UDM_LOG_ERROR,"Reloading config '%s'",cname);
  UdmEnvInit(&NewConf);
  UdmSetLockProc(&NewConf,UdmLockProc);
  UdmSetRefProc(&NewConf,UdmRefProc);

  UDM_GETLOCK(Indexer, UDM_LOCK_CONF);
  Indexer->Conf = &NewConf;
  rc = UdmIndexerEnvLoad(Indexer, cname, add_servers + load_langmaps + UDM_FLAG_SPELL);
  Indexer->Conf = &Conf;
               
  if (rc!=UDM_OK)
  {
    UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);
    UdmLog(Indexer,UDM_LOG_ERROR,"Can't load config: %s",UdmEnvErrMsg(&NewConf));
    UdmLog(Indexer,UDM_LOG_ERROR,"Continuing with old config");
    UdmEnvFree(&NewConf);
  }
  else
  {
    size_t noptions;
    UdmEnvFree(&Conf);
    Conf=NewConf;
    UdmParseCmdLine(UdmARGC, UdmARGV, &noptions);
#ifndef WIN32
    UdmOpenLog("indexer", &Conf, log2stderr);
#endif
    UDM_RELEASELOCK(Indexer, UDM_LOCK_CONF);
  }
  return UDM_OK;
}
*/  


static size_t
create_shared_info(UDM_AGENT *A, char *str, size_t len)
{
  UDM_ENV *Env= A->Conf;
  size_t res= udm_snprintf(str, len,
                          "Hrefs: %d,"
                          "Targets: %d,"
                          "Cookies: %d,"
                          "Robots: %d,"
                          "Hosts: %d,",
                          (int) Env->Hrefs.nhrefs,
                          (int) Env->Targets.num_rows,
                          (int) Env->Cookies.nvars,
                          (int) Env->Robots.nrobots,
                          (int) Env->Hosts.nhost_addr);
  return res;
}


static int
httpd_client_handler(int client, UDM_AGENT *A)
{
  char request[4096];
  char response[1024];
  char speed_info[128]= "";
  char shared_info[128]= "";
  ssize_t nrecv;
  size_t i, len, total_docs= 0, total_sec= 0;
  udm_uint8 total_bytes= 0;
  time_t now= time(0);
  
  nrecv= recv(client, request, sizeof(request), 0);
  UdmLog(A, UDM_LOG_ERROR, "Received request len=%d", (int) nrecv);
  udm_snprintf(response, sizeof(response) - 1,
               "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
  UdmSend(client, response, strlen(response), 0);
  len= sprintf(response,
               "Threads:"
               "<table border=1 cellspacing=1 cellpadding=1>\n"
               "<tr><th>ID</th>"
               "<th>Docs</th>"
               "<th>Size</th>"
               "<th>Task</th>"
               "<th>Time</th>"
               "<th>Param</th>"
               "<th>Extra</th></tr>");
  UdmSend(client, response, len, 0);
  for (i= 0; i < maxthreads; i++)
  {
    UDM_AGENT *Tmp= &ThreadIndexers[i];
    char mutex_owned_info[64]= "";
    size_t mutex, sec;
    
    for (mutex= 0;
         mutex < Tmp->State.nmutexes && mutex < UDM_AGENT_STATE_MUTEXES;
         mutex++)
    {
      sprintf(UDM_STREND(mutex_owned_info), " #%d",
              (int) Tmp->State.mutex_owned[mutex]);
    }
    
    /*
    size_t mutex_owned= Tmp->State.mutex_owned;
    
    if (mutex_owned)
      sprintf(mutex_owned_info, "Owner for mitex #%d", mutex_owned);
    */
    
    len= sprintf(response,
                 "<tr><td>%d</td>"
                 "<td align=right>%d</td>"
                 "<td align=right>%llu</td>"
                 "<td>%s</td>"
                 "<td align=right>%d</td>"
                 "<td>%s&nbsp;</td>"
                 "<td>%s%s%s&nbsp;</td></tr>\n",
                 Tmp->handle,
                 (int) Tmp->ndocs,
                 (unsigned long long) Tmp->nbytes,
                 Tmp->State.task,
                 (int) (now - Tmp->State.start_time),
                 UDM_NULL2EMPTY(Tmp->State.param),
                 mutex_owned_info[0] ? "Owner for mutex: " : "",
                 mutex_owned_info,
                 UDM_NULL2EMPTY(Tmp->State.extra));
    UdmSend(client, response, len, 0);
    total_docs+= Tmp->ndocs;
    total_bytes+= Tmp->nbytes;
    sec= (size_t) (now - Tmp->start_time);
    if (sec > total_sec)
      total_sec= sec;
  }
  if (total_sec)
  {
    udm_snprintf(speed_info, sizeof(speed_info) - 1,
                 "%d seconds, %d docs/sec, %d bytes/sec",
                 (int) total_sec,
                 (int) (total_docs / total_sec),
                 (int) (total_bytes / total_sec));
  }
  
  len= sprintf(response,
                 "<tr><td>&nbsp;</td>"
                 "<td align=right>%d</td>"
                 "<td align=right>%llu</td>"
                 "<td>&nbsp;</td>"
                 "<td align=right>&nbsp;</td>"
                 "<td>%s&nbsp;</td>"
                 "<td>&nbsp;</td></tr>\n",
                 (int) total_docs,
                 (unsigned long long) total_bytes,
                 speed_info);
  UdmSend(client, response, len, 0);
  len= sprintf(response, "</table>\n");
  UdmSend(client, response, len, 0);
  len= create_shared_info(A, shared_info, sizeof(shared_info));
  UdmSend(client, shared_info, len, 0);
  return UDM_OK;
}


#ifdef  WIN32
unsigned int __stdcall thread_main_httpd(void *arg)
#else
static void* thread_main_httpd(void *arg)
#endif
{
  UDM_AGENT *A= (UDM_AGENT*) arg;
#ifndef WIN32
  UdmStartHTTPD(A, httpd_client_handler);
#endif
  return 0;
}



static char pidname[1024];
static char time_pid[100];

static void exitproc(void)
{
  unlink(pidname);
}


static char * time_pid_info(void)
{
  struct tm * tim;
  time_t t;
  t= time(NULL);
  tim= localtime(&t);
  strftime(time_pid,sizeof(time_pid),"%a %d %H:%M:%S",tim);
  sprintf(time_pid+strlen(time_pid)," [%d]",(int)getpid());
  return(time_pid);
}


static void UdmWSAStartup(void)
{
#ifdef WIN32
  WSADATA wsaData;
  if (WSAStartup(0x101,&wsaData)!=0)
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
  return;
}

static int UdmConfirm(const char *msg)
{
  char str[5];
  printf("%s",msg);
  return (fgets(str,sizeof(str),stdin) && !strncmp(str,"YES",3));
}


static int
UdmIndDelete(UDM_AGENT *A)
{
  int clear_confirmed=1, rc= UDM_OK;
  if (warnings)
  {
    size_t i;
    printf("You are going to delete content from the database(s):\n");
    for (i = 0; i < Conf.dbl.nitems; i++)
    {
      const char *dbaddr;
      dbaddr= UdmVarListFindStr(&Conf.dbl.db[i].Vars,"DBAddr","<noaddr>");
      printf("%s\n", dbaddr);
    }
    clear_confirmed=UdmConfirm("Are you sure?(YES/no)");
  }
     
  if (clear_confirmed)
  {
    if (url_filename)
    {
      rc= UdmURLFile(A ,url_filename, UDM_URL_FILE_CLEAR);
    }
    else
    {
      printf("Deleting...");
      rc= UdmClearDatabase(A);
      printf("%s\n", rc == UDM_OK ? "Done" : "");
    }
  }
  else
  {
    printf("Canceled\n");
  }
  if (rc != UDM_OK)
  {
    fflush(stdout);
    UdmLog(A, UDM_LOG_ERROR, "Error: '%s'", UdmEnvErrMsg(A->Conf));
  }
  return rc;
}

#ifdef WIN32
typedef unsigned (__stdcall *udm_thread_routine_t)(void);
#else
typedef void *(*udm_thread_routine_t)(void*);
#endif


#ifdef HAVE_PTHREAD
static int
#ifdef WIN32
UdmThreadCreate(udm_thread_t *thd, unsigned (__stdcall *start_routine)(void *), void *arg)
#else
UdmThreadCreate(udm_thread_t *thd, void *(*start_routine)(void*), void *arg)
#endif
{
#ifdef WIN32
  {
    int res;
    res= _beginthreadex(NULL, 0, start_routine, arg, 0, NULL);
    UDM_ASSERT(res != -1);
    return res <= 0; /* res must be positive on success */
  }
#else
  {
    int res;
    pthread_attr_t attr;
    size_t stksize= 1024 * 512;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, stksize);
    res= pthread_create(thd, &attr, start_routine, arg);
    pthread_attr_destroy(&attr);
    return res; /* res must be 0 on success */
  }
#endif
}
#endif


static int UdmIndex(UDM_AGENT *A)
{
  UDM_AGENT httpd_agent;
  size_t nbytes;
  UDM_VAR *Listen;
  
  maxthreads= UdmVarListFindInt(&A->Conf->Vars, "CrawlerThreads", 1);
  nbytes= maxthreads * sizeof(UDM_AGENT);
  ThreadIndexers= (UDM_AGENT*)UdmMalloc(nbytes);
  bzero((void*) ThreadIndexers, nbytes);
  UdmAgentInit(&httpd_agent, A->Conf, 0);
  
  if ((Listen= UdmVarListFind(&A->Conf->Vars, "Listen")))
  {
#ifdef HAVE_PTHREAD
    if (Listen->val && Listen->val[0])
    {
      udm_thread_t httpd_thread;
      UdmThreadCreate(&httpd_thread, thread_main_httpd, &httpd_agent);
    }
    else
#endif
    {
      UdmLog(A, UDM_LOG_ERROR, "Not starting HTTPD");
    }
  }
  
  
#ifdef HAVE_PTHREAD
  {
    int i;

#ifdef WIN32
    threads= (HANDLE*)UdmMalloc(maxthreads * sizeof(HANDLE));
#else
    threads= (pthread_t*)UdmMalloc(maxthreads * sizeof(pthread_t));
#endif
               
    for(i= 0; i < maxthreads; i++)
    {
      UDM_AGENT *Indexer;
      if (seconds) UDMSLEEP(seconds);

      UDM_GETLOCK(A, UDM_LOCK_THREAD);
      UDM_GETLOCK(A, UDM_LOCK_CONF);
      Indexer= UdmAgentInit(&ThreadIndexers[i], A->Conf, i + 1);
      Indexer->flags = flags;
      UDM_RELEASELOCK(A, UDM_LOCK_CONF);
      UDM_RELEASELOCK(A, UDM_LOCK_THREAD);
      
      UdmThreadCreate(&threads[i], &thread_main, Indexer);

      UDM_GETLOCK(A, UDM_LOCK_THREAD);
      total_threads= i + 1;
      UDM_RELEASELOCK(A,UDM_LOCK_THREAD);
    }
#ifndef WIN32        
    for (i = 0; i < maxthreads; i++)
      pthread_join(threads[i], NULL);
#else
    while(1)
    {
      int num;
      UDM_GETLOCK(A,UDM_LOCK_THREAD);
      num=total_threads;
      UDM_RELEASELOCK(A,UDM_LOCK_THREAD);
      if (!num)break;
      UDMSLEEP(1);
    }
#endif
    UDM_FREE(threads);
  }
#else
  A->handle = 1;
  thread_main(A);
#endif

  UdmAgentFree(&httpd_agent);
  UDM_FREE(ThreadIndexers);
  return thd_errors ? UDM_ERROR : UDM_OK;
}


static int
UdmDumpDocument(UDM_AGENT *A, UDM_DOCUMENT *D)
{
  size_t i;
  printf("  <item>\n");
  /*
  printf("    <link>%s</link>\n", UdmVarListFindStr(&D->Sections, "URL", ""));
  printf("    <id>%s</id>\n", UdmVarListFindStr(&D->Sections, "ID", ""));
  printf("    <content-length>%s</content-length>\n", UdmVarListFindStr(&D->Sections, "Content-Length", ""));
  printf("    <status>%s</status>\n", UdmVarListFindStr(&D->Sections, "Status", ""));
  printf("    <hops>%s</hops>\n", UdmVarListFindStr(&D->Sections, "Hops", ""));
  printf("    <crc32>%s</crc32>\n", UdmVarListFindStr(&D->Sections, "crc32", ""));
  printf("    <modified>%s</modified>\n", UdmVarListFindStr(&D->Sections, "Last-Modified", ""));
  */
  for (i= 0; i < D->Sections.nvars; i++)
  {
    UDM_VAR *S= &D->Sections.Var[i];
    printf("    <%s>%s</%s>\n", S->name, S->val, S->name);
  }
  printf("  </item>\n");
  return 0;
}


static int
UdmDumpDocuments(UDM_AGENT *A)
{
  int rc;
  A->Conf->DumpDoc= UdmDumpDocument;
  
  /* printf("<rss><channel>\n"); */
  if (UDM_OK != (rc= UdmURLAction(A, NULL, UDM_URL_ACTION_DUMPDATA)))
    return rc;
  /* printf("</channel></rss>\n"); */
  return UDM_OK;
}


#include <sys/stat.h>
/*
  TODO for dump/restore:
  - Escape bad characters to entities on dump, unescape on restore.
  - Huge file size
  - Subsection control
  - Database number limit
  - Insert into database
*/
static int
UdmRestoreDocuments(UDM_AGENT *A)
{
  int rc= UDM_OK;
  UDM_DSTR dstr;
  struct stat sb;
  int fd;
  ssize_t nbytes;
  UDM_CHARSET *cs;
  
  if (!url_filename)
  {
    fprintf(stderr, "Required option -f ommitted\n");
    return UDM_ERROR;
  }
  fprintf(stderr, "Restoring data from '%s'\n", url_filename);
  
  if (stat(url_filename, &sb))
  {
    fprintf(stderr, "Can't stat '%s'\n", url_filename);
    return UDM_ERROR;
  }
  UdmDSTRInit(&dstr, 1024);
  if (UDM_OK != (rc= UdmDSTRAlloc(&dstr, sb.st_size + 1)))
  {
#ifndef WIN32
    fprintf(stderr, "failed to allocate %lld bytes\n",
            (long long) sb.st_size + 1);
#endif
    goto ex;
  }
  
  if((fd= open(url_filename, O_RDONLY|UDM_BINARY)) <= 0)
  {
    rc= UDM_ERROR;
    fprintf(stderr, "Can't open '%s'", url_filename);
    goto ex;
  }

  if ((nbytes= read(fd, dstr.data, sb.st_size)) != sb.st_size)
  {
    rc= UDM_ERROR;
#ifndef WIN32
    fprintf(stderr, "Failed to read %lld bytes from '%s'\n",
            (long long) sb.st_size, url_filename);
#endif
    goto ex; 
  }
  dstr.size_data= nbytes + 1;
  dstr.data[nbytes]= '\0';
  
  cs= UdmGetCharSet("latin1");
  rc= UdmResultFromXML(A, NULL, dstr.data, dstr.size_data, cs);
ex:
  UdmDSTRFree(&dstr);
  return rc;
}


static int
UdmIndHashSpell(UDM_AGENT *Agent)
{
  UDM_ENV *Env= Agent->Conf;
  char errmsg[256];
  int rc;
  if (UDM_OK != (rc= UdmSpellListListLoad(&Env->Spells, errmsg, sizeof(errmsg)))||
      UDM_OK != (rc= UdmSpellListListWriteHash(&Env->Spells, errmsg, sizeof(errmsg))))
  {
    fprintf(stderr, "error: %s\n", errmsg);
  }
  return rc;
}


static int
UdmIndDumpSpell(UDM_AGENT *Agent)
{
  char errmsg[256];
  int spflags= UDM_SPELL_NOPREFIX;
  UDM_ENV *Env= Agent->Conf;
  int rc;
  if (UDM_OK != (rc= UdmSpellListListLoad(&Env->Spells,
                                          errmsg, sizeof(errmsg)))||
      UDM_OK != (rc= UdmAffixListListLoad(&Env->Affixes, spflags,
                                          errmsg, sizeof(errmsg)))||
      UDM_OK != (rc= UdmSpellDump(&Env->Spells, &Env->Affixes,
                                          errmsg, sizeof(errmsg))))
  {
    fprintf(stderr, "error: %s\n", errmsg);
  }
  return rc;
}


static int
UdmIndSQLMonitor(UDM_AGENT *Agent)
{
  int rc;
  UDM_SQLMON_PARAM prm;
  bzero((void*)&prm,sizeof(prm));
  prm.infile= stdin;
  prm.outfile= stdout;
  prm.flags= UDM_SQLMON_DISPLAY_FIELDS;
  execsql= UdmVarListFindStr(&Conf.Vars, "exec", NULL);
  prm.gets= execsql ? sqlexecgets : sqlmongets;
  prm.display= UdmDisplaySQLQuery;
  prm.prompt= sqlmonprompt;
  prm.loglevel= loglevel;
#ifdef HAVE_READLINE
  if (isatty(0))
    prm.mode= udm_sqlmon_mode_interactive;
#endif
  rc= UdmSQLMonitor(Agent, &Conf, &prm);
  return rc;
}


static int
UdmIndWordStat(UDM_AGENT *Agent)
{
  int rc= UdmURLAction(Agent, NULL, UDM_URL_ACTION_WRDSTAT);
  return rc;
}


static int
UdmIndBlock(void)
{
  int pid_fd;
  char pidbuf[128];
  /* Check that another instance isn't running and create PID file. */
  const char *vardir= UdmVarListFindStr(&Conf.Vars,"VarDir",UDM_VAR_DIR);
  sprintf(pidname,"%s/%s", vardir ,"indexer.pid");
  pid_fd = open(pidname,O_CREAT|O_EXCL|O_WRONLY,0644);
  if (pid_fd < 0)
  {
    fprintf(stderr,"%s Can't create '%s': %s\n", time_pid_info(), pidname, strerror(errno));
    if (errno == EEXIST)
    {
      fprintf(stderr,"It seems that another indexer is already running!\n");
      fprintf(stderr,"Remove '%s' if it is not true.\n",pidname);
    }
    return UDM_ERROR;
  }
  udm_snprintf(pidbuf, sizeof(pidbuf), "%d\n", (int)getpid());
  write(pid_fd, &pidbuf, strlen(pidbuf));
#ifdef HAVE_ATEXIT
  atexit(&exitproc);
#endif
  return UDM_OK;
}


static int
UdmIndInsertFromFile(UDM_AGENT *Agent)
{
  int rc= UDM_OK;
  if (strcmp(url_filename,"-"))
  {
    /* Make sure all URLs to be inserted are OK */
    if (UDM_OK != (rc= UdmURLFile(Agent, url_filename,UDM_URL_FILE_PARSE)))
    {
      UdmLog(Agent, UDM_LOG_ERROR,"Error: Invalid URL in '%s'",url_filename);
      goto ex;
    }
  }
  if (UDM_OK != (rc= UdmURLFile(Agent, url_filename,UDM_URL_FILE_INSERT)))
  {
    UdmLog(Agent, UDM_LOG_ERROR,"Error: '%s'", UdmEnvErrMsg(Agent->Conf));
    goto ex;
  }
ex:
  return rc;
}


static int
UdmIndExpire(UDM_AGENT *Agent)
{
  int rc= url_filename ?
          UdmURLFile(Agent, url_filename, UDM_URL_FILE_REINDEX) :
          UdmURLAction(Agent, NULL, UDM_URL_ACTION_EXPIRE);
  return rc;
}


static int
UdmIndCrawl(UDM_AGENT *Agent)
{
  int rc;
  if (block && UDM_OK != (rc= UdmIndBlock()))
    goto ex;
  UdmLog(Agent, UDM_LOG_WARN, "indexer from %s-%s-%s started with '%s'", PACKAGE, VERSION, UDM_DBTYPE, cname);
  UdmStoreHrefs(Agent);    /**< store hrefs from config and command line */
  UdmSigHandlersInit(Agent);

  if (expire)
  {
    if (UDM_OK != (rc= UdmIndExpire(Agent)))
      goto ex;
  }

  if (UDM_OK != (rc= UdmIndex(Agent)))
    goto ex;
#ifdef HAVE_SQL
  if (pop_rank)
    rc= UdmSrvAction(Agent, NULL, UDM_SRV_ACTION_POPRANK);
#endif

ex:
  return rc;
}


UDM_AGENT Main;


int main(int argc, char **argv)
{
  int   rc= 0;
  char  *REQUEST_METHOD= getenv("REQUEST_METHOD");
  FILE  *logfile= REQUEST_METHOD ? stdout : stderr;
  size_t noptions;
#ifdef CHASEN
  char  *chasen_argv[] = { "chasen", "-b", "-f", "-F", "%m ", NULL };
  chasen_getopt_argv(chasen_argv, NULL);
#endif

  if (REQUEST_METHOD)
    printf("Content-Type: text/plain\r\n\r\n");

  UdmWSAStartup();

  UdmInit(); /* Initialize library */

  UdmInitMutexes();
  UdmEnvInit(&Conf);
  UdmVarListAddEnviron(&Conf.Vars,"ENV");
  UdmEnvSetDirs(&Conf);
  UdmSetLockProc(&Conf,UdmLockProc);
  UdmSetRefProc(&Conf,UdmRefProc);
#ifdef THINFO_TEST
  UdmSetThreadProc(&Conf,UdmShowThreadInfoProc);
#endif
  UdmAgentInit(&Main,&Conf,0);

  UdmARGC= argc;
  UdmARGV= argv;

  if (UdmParseCmdLine(UdmARGC, UdmARGV, &noptions))
    goto ex;

  if (cmd == UDM_IND_INDEX)
  {
    fprintf(stderr, "\nWARNING: \"indexer -Eindex\" is deprecated. "
                    "Use \"indexer -Ecrawl\" instead!\n\n");
    cmd= UDM_IND_CRAWL;
  }
  
  if (cmd == UDM_IND_AMBIGUOUS)
  {
    fprintf(stderr, "Ambiguous indexer command in -E\n");
    help++;
  }
  
  if (cmd == UDM_IND_UNKNOWN)
  {
    fprintf(stderr, "Unknown indexer command in -E\n");
    help++;
  }
  
  if (cmd == UDM_IND_DUMPCONF)
  {
    load_for_dump|= UDM_FLAG_DONT_ADD_TO_DB;
    load_langmaps= 0;
    load_spells= 0;
  }
  else if (cmd != UDM_IND_CRAWL)
  {
    add_servers=0;
    load_langmaps=0;
    if (cmd != UDM_IND_HASHSPELL && cmd  != UDM_IND_DUMPSPELL)
      load_spells=0;
  }

  flags|=add_servers;
  flags |= add_server_urls;
  Main.flags = flags;

  argc-= noptions;
  argv+= noptions;

  if ((argc>1) || (help))
  {
    usage(help, udm_indexer_options);
    rc= 1;
    goto ex;
  }
  
  if (!*cname)
  {
    if (argc == 1)
    {
      strncpy(cname,argv[0],sizeof(cname));
      cname[sizeof(cname)-1]='\0';
    }
    else
    {
      const char *cd=UdmVarListFindStr(&Conf.Vars,"ConfDir",UDM_CONF_DIR);
      udm_snprintf(cname,sizeof(cname),"%s%s%s",cd,UDMSLASHSTR,"indexer.conf");
      cname[sizeof(cname)-1]='\0';
    }
  }
  
  
  if (UDM_OK != (rc= UdmIndexerEnvLoad(&Main, cname,
                                       add_servers + load_langmaps +
                                       load_spells +
                                       add_server_urls + load_for_dump)))
  {
    fprintf(logfile, "%s\n", UdmEnvErrMsg(&Conf));
    goto ex;
  }


  if (url_filename && strcmp(url_filename,"-"))
  {
    /* Make sure URL file is readable if not STDIN */
    FILE *url_file;
    if (!(url_file=fopen(url_filename,"r")))
    {
      UdmLog(&Main,UDM_LOG_ERROR,"Error: can't open url file '%s': %s",url_filename, strerror(errno));
      goto ex;
    }
    fclose(url_file);
  }
  
  
  
  if (cmd == UDM_IND_DUMPCONF)
  {
    if (UDM_OK != (rc= UdmEnvSave(&Main, "-", 0)))
      fprintf(logfile, "%s\n", UdmEnvErrMsg(&Conf));
    goto ex;
  }
  
  
  if (cmd == UDM_IND_CHECKCONF)
  {
    rc= 0;
    goto ex;
  }
  
  
#ifndef WIN32
   UdmSetLogLevel(NULL, have_loglevel ? loglevel :
                  UdmVarListFindInt(&Main.Conf->Vars, "LogLevel", UDM_LOG_INFO));
   UdmOpenLog("indexer",&Conf, log2stderr);
#endif

  if (insert && url_filename)
  {
    if (UDM_OK != UdmIndInsertFromFile(&Main))
    {
      UdmLog(&Main,UDM_LOG_ERROR,"Error: '%s'",UdmEnvErrMsg(Main.Conf));
      goto ex;
    }
  }

  switch (cmd)
  {
    case UDM_IND_HASHSPELL:     rc= UdmIndHashSpell(&Main);     goto ex;
    case UDM_IND_DUMPSPELL:     rc= UdmIndDumpSpell(&Main);     goto ex;
    case UDM_IND_DUMPDATA:      rc= UdmDumpDocuments(&Main);    goto ex;
    case UDM_IND_RESTOREDATA:   rc= UdmRestoreDocuments(&Main); goto ex;
    case UDM_IND_SQLMON:        rc= UdmIndSQLMonitor(&Main);    goto ex;
    case UDM_IND_EXECSQL:       rc= UdmIndSQLMonitor(&Main);    goto ex;
    case UDM_IND_MULTI2BLOB:    rc= UdmMulti2Blob(&Main);       goto ex;
    case UDM_IND_EXPORT:        rc= UdmExport(&Main);           goto ex;
    case UDM_IND_WRDSTAT:       rc= UdmIndWordStat(&Main);      goto ex;
    case UDM_IND_REWRITEURL:    rc= UdmRewriteURL(&Main);       goto ex;
    case UDM_IND_REWRITELIMITS: rc= UdmRewriteLimits(&Main);    goto ex;
    case UDM_IND_STAT:          rc= ShowStatistics(&Main);      goto ex;
    case UDM_IND_REFERERS:      rc= ShowReferers(&Main);        goto ex;
    case UDM_IND_CREATE:        rc= UdmIndCreate(&Main);        goto ex;
    case UDM_IND_DROP:          rc= UdmIndDrop(&Main);          goto ex;
    case UDM_IND_DELETE:        rc= UdmIndDelete(&Main);        goto ex;
    case UDM_IND_CRAWL:         rc= UdmIndCrawl(&Main);         goto ex;
    case UDM_IND_INDEX:         rc= UdmIndCrawl(&Main);         goto ex;

    case UDM_IND_AMBIGUOUS:
    case UDM_IND_UNKNOWN:
    case UDM_IND_CHECKCONF:
    case UDM_IND_CONVERT:
    case UDM_IND_DUMPCONF:
    case UDM_IND_SET:
    case UDM_IND_SET0:
      break;
  }
  
  
ex:
  total_threads= 0;
  UdmAgentFree(&Main);
  UdmEnvFree(&Conf);
  UdmDestroyMutexes();
  UdmWSACleanup();
#ifndef HAVE_ATEXIT
  exitproc();
#endif
  return rc;
}
