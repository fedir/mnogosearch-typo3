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

#ifdef HAVE_SQL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "udm_common.h"
#include "udm_utils.h"

#include "udm_db.h"
#include "udm_db_int.h"
#include "udm_sqldbms.h"
#include "udm_mutex.h"
#include "udm_vars.h"
#include "udm_log.h" /* For UDM_LOG_INFO */


static void
ReportError(UDM_SQLMON_PARAM *prm, const char *error)
{
  char errstr[512];
  if (prm->mode == udm_sqlmon_mode_batch)
  {
    udm_snprintf(errstr, sizeof(errstr), "ERROR at line %d: %s",
                 (int) prm->lineno + 1, error); 
  }
  else
  {
    udm_snprintf(errstr, sizeof(errstr), "ERROR: %s", error);
  }
  prm->prompt(prm, UDM_SQLMON_MSG_ERROR, errstr);
  prm->prompt(prm, UDM_SQLMON_MSG_ERROR, "\n");
}



/*
  Set connection to the given database.
  newnum is the database number.
  1 means the first DBAddr.
*/
static int
SetConnection(UDM_ENV *Env, UDM_SQLMON_PARAM *prm, int newnum)
{
  char msg[255];
  if (newnum >= 1 && newnum <= (int) Env->dbl.nitems)
  {
    Env->dbl.currdbnum= newnum - 1;
    if (prm->loglevel >= UDM_LOG_INFO)
    {
      sprintf(msg,"Connection changed to #%d", (int) Env->dbl.currdbnum);
      prm->prompt(prm, UDM_SQLMON_MSG_PROMPT, msg);
      prm->prompt(prm, UDM_SQLMON_MSG_PROMPT, "\n");
    }
    return UDM_OK;
  }
  else
  {
    if (prm->loglevel >= UDM_LOG_WARN)
    {
      sprintf(msg,"Wrong connection number %d", newnum);
      ReportError(prm, msg);
    }
    return UDM_ERROR;
  }
}


/*
  Execute an SQL query, or a build-in command.
*/
static int
RunCommand(UDM_AGENT *A, UDM_ENV *Env, UDM_SQLMON_PARAM *prm, const char *str)
{
  int rc= UDM_OK;
  if (prm->loglevel >= UDM_LOG_INFO)
  {
    prm->prompt(prm, UDM_SQLMON_MSG_PROMPT, "'");
    prm->prompt(prm, UDM_SQLMON_MSG_PROMPT, str);
    prm->prompt(prm, UDM_SQLMON_MSG_PROMPT, "'");
    prm->prompt(prm, UDM_SQLMON_MSG_PROMPT, "\n");
  }
  
  if (!strncasecmp(str,"connection",10))
  {
    int newnum= atoi(str + 10);
    SetConnection(Env, prm, newnum + 1);
  }
  else if (!strcasecmp(str,"fields=off"))
  {
    prm->flags=0;
  }
  else if (!strcasecmp(str,"fields=on"))
  {
    prm->flags=1;
  }
  else if (!strncasecmp(str,"colflags",8))
  {
    int colnum= atoi(str+8);
    int colval= atoi(str+10);
    if (colnum>=0 && colnum<10)
      prm->colflags[colnum]= colval;
  }
  else
  {
    int res;
    UDM_SQLRES sqlres;
    UDM_DB *db= &Env->dbl.db[Env->dbl.currdbnum];
    prm->nqueries++;
    bzero((void*)&sqlres,sizeof(sqlres));
    UDM_GETLOCK(A, UDM_LOCK_DB);
    res= UdmSQLQuery(db,&sqlres,str);
    UDM_RELEASELOCK(A, UDM_LOCK_DB);
    if (res!=UDM_OK)
    {
      prm->nbad++;
      rc= UDM_ERROR;
      ReportError(prm, db->errstr);
    }
    else
    {
      prm->ngood++;
      res=prm->display(prm,&sqlres);
    }
    UdmSQLFree(&sqlres);
  }
  return rc;
}


static size_t
RemoveTrailingSpaces(UDM_SQLMON_PARAM *prm,
                     char *s, size_t slen,
                     size_t *space_count)
{
  char *e= s + slen;
  for ( space_count[0]= 0 ; e > s ; *--e= '\0')
  {
    int ch= e[-1];
    if (ch == ' ' || ch == '\t' || ch == '\r')
    {
      space_count[0]++;
    }
    else if (ch == '\n')
    {
      space_count[0]++;
      prm->lineno++;
    }
    else
      break;
  }
  return e - s;
}

/*

  UdmSQLMonitor()
  
  - Executes SQL queries arriving from some virtual input stream
  - Displays query results to some virtual output streams
  
  The incoming strings are checked against:
    
    - SQL comments, i.e. lines starting with "--".
      The comment lines are skipped.
    
    - A set of end-of-query markers, i.e. 
      * MSSQL   style: 'GO' keyword written in the beginning
      * mysql   style: ';'  character written in the end of the string
      * minisql style: '\g' characters written in the end of the string
      
      As soon as a end-of-query marker has found, current collected query
      is executed.
    
    - In other case, the next line is concatenated to the previously
      collected data thus helping to run multi-line queries.
  
  
  The particular ways of handling input and output streams are
  passed in the UDM_SQLMON_PARAM argument. This approach makes 
  it possible to use UdmSQLMonitor() for many purposes:
  
  - executing a bundle of queries written in a text file.
  - running a command-line SQL monitor ("mysql", "pgsql" or "sqlplus" alike),
    i.e. executing queries arriving from the keyboard.
    
  - running a GUI SQL monitor,
    i.e. executing querues arriving from the keyboard in some GUI application.
  
  
  UDM_SQLMON_PARAM has got this structure:
  
  typedef struct udm_sqlmon_param_st
  {
    int flags;
    FILE *infile;
    FILE *outfile;
    char *(*gets)(struct udm_sqlmon_param_st *prm, char *str, size_t size);
    int (*display)(struct udm_sqlmon_param_st *, UDM_SQLRES *sqlres);
    int (*prompt)(struct udm_sqlmon_param_st *, const char *msg);
  void* tag_ptr;
  } UDM_SQLMON_PARAM;
  
  
  gets():     a function to get one string from the incoming stream.
  display():  a function to display an SQL query results
  prompt():   a function to display some prompts:
                - an invitation to enter data, for example: "SQL>".
                - an error message
  
  flags:      sets various aspects of the monitor behaviour:
              the only one so far:
                - whether to display field names in the output

  infile:     if the caller wants to input SQL queries from some FILE stream,
              for example, from STDIN or from a textual file opened via fopen(),
              it can use this field for this purpose
              
  outfile:    if the caller wants to output results to some FILE stream,
              for example, to STDOUT or to a textial file opened via fopen(),
              it can use this field for this purpose

  context_ptr:    extra pointer parameter for various purposes.
  
  To start using UdmSQLMonitor() one have to set
    - gets()
    - diplay()
    - prompt()
  
  If diplay() or prompt() use some FILE streams, the caller should
  also set infile and outfile with appropriative stream pointers.
  
*/


#define CHUNK_SIZE 64*1024


__C_LINK int __UDMCALL
UdmSQLMonitor(UDM_AGENT *A, UDM_ENV *Env, UDM_SQLMON_PARAM *prm)
{
  int  rc=UDM_OK;
  int DBLimit= UdmVarListFindInt(&Env->Vars, "DBLimit", 0);
  UDM_DSTR qbuf;

  UdmDSTRInit(&qbuf, 256);

  if (DBLimit && UDM_OK != (rc= SetConnection(Env, prm, DBLimit)))
    return rc;
  
  while(1)
  {
    int  exec=0;
    size_t new_chunk_len, space_count;
    char *snd;
    
    UdmDSTRRealloc(&qbuf, qbuf.size_data + CHUNK_SIZE);
    
    if (qbuf.size_total > 512 * 1024 * 1024)
    {
      char errstr[160];
      rc= UDM_ERROR;
      udm_snprintf(errstr, sizeof(errstr), "Too long query: %s", qbuf.data);
      ReportError(prm, errstr);
      break;
    }

    snd= qbuf.data + qbuf.size_data;
    if (!prm->gets(prm, snd, CHUNK_SIZE))
      break;

    /* Remove trailing spaces/tabs/NLs */
    if (!(new_chunk_len= RemoveTrailingSpaces(prm, snd, strlen(snd), &space_count)))
      continue;

    if (!strncmp(snd, "--seed=", 7))
    {
      udmhash32_t seed= atoi(snd + 7);
      SetConnection(Env, prm, UdmDBNumBySeed(Env, seed) + 1);
    }
    
    /*
      Skip comments
      TODO: detect long comments which do not fit into CHUNK_SIZE
      TODO: detect semicolon correctly, only when outside a string
    */
    if(snd[0]=='#' || !strncmp(snd, "--", 2))
      continue;
    
    /* Append the newly loaded chunk into qbuf */
    qbuf.size_data+= new_chunk_len;

    snd= snd + new_chunk_len;
   
    if (prm->flags & UDM_SQLMON_DONT_NEED_SEMICOLON)
    {
      exec= 1;
    }
    else if (snd[-1] == ';')
    {
      exec=1;
      *--snd='\0';
      qbuf.size_data--;
    }
    else if(snd - 2 >= qbuf.data && snd[-1]=='g' && snd[-2]=='\\')
    {
      exec=1;
      snd-=2;
      *snd='\0';
      qbuf.size_data-= 2;
    }
    else if(snd-2 >= qbuf.data && strchr("oO", snd[-1]) && strchr("gG",snd[-2]))
    {
      exec=1;
      snd-=2;
      *snd='\0';
      qbuf.size_data-= 2;
    }
    
    if(exec)
    {
      rc= RunCommand(A, Env, prm, qbuf.data);
      UdmDSTRReset(&qbuf);
      if (prm->mode == udm_sqlmon_mode_interactive)
        prm->lineno= 0;
    }
    else
    {
      if (space_count)
        UdmDSTRAppend(&qbuf, " ", 1);
    }
  }
  UdmDSTRFree(&qbuf);
  if (prm->loglevel >= UDM_LOG_INFO)
    prm->prompt(prm, UDM_SQLMON_MSG_PROMPT, "\n");
  return rc;
}


#endif /* HAVE_SQL */
