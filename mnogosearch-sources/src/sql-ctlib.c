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

#ifdef HAVE_SQL

/*
#define DEBUG_SQL
*/

#define DEBUG_ERR_QUERY



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "udm_common.h"
#include "udm_db.h"
#include "udm_db_int.h"
#include "udm_utils.h"
#include "udm_vars.h"
#include "udm_sqldbms.h"
#include "udm_xmalloc.h"

#if HAVE_CTLIB
#include <ctpublic.h>
#endif
#ifdef WIN32
#include <process.h>
#endif


#if HAVE_CTLIB

#ifndef MAX
#define MAX(X,Y)  (((X) > (Y)) ? (X) : (Y))
#endif

#ifndef MIN
#define MIN(X,Y)  (((X) < (Y)) ? (X) : (Y))
#endif

/* Should be fine for 8 million documents in rop_rank records (8 byte) */
#define MAX_CHAR_BUF  64*1024*1024

typedef struct
{
  CS_CONTEXT	*ctx;
  CS_CONNECTION	*conn;
} UDM_CTLIB;


typedef struct _ex_column_data
{
  CS_INT        datatype;
  CS_CHAR         *value;
  CS_INT        valuelen;
  CS_SMALLINT  indicator;
} EX_COLUMN_DATA;


static char sybsrvmsg[2048]="";


static CS_RETCODE CS_PUBLIC
ex_clientmsg_cb(CS_CONTEXT *context, 
                CS_CONNECTION *connection,
                CS_CLIENTMSG *errmsg)
{
  fprintf(stderr, "\nOpen Client Message:\n");
  fprintf(stderr, "Message number: LAYER = (%ld) ORIGIN = (%ld) ",(long)CS_LAYER(errmsg->msgnumber), (long)CS_ORIGIN(errmsg->msgnumber));
  fprintf(stderr, "SEVERITY = (%ld) NUMBER = (%ld)\n",(long)CS_SEVERITY(errmsg->msgnumber), (long)CS_NUMBER(errmsg->msgnumber));
  fprintf(stderr, "Message String: %s\n", errmsg->msgstring);
  if (errmsg->osstringlen > 0)
  {
    fprintf(stderr, "Operating System Error: %s\n",errmsg->osstring);
  }
  return CS_SUCCEED;
}


static CS_RETCODE CS_PUBLIC
ex_servermsg_cb(CS_CONTEXT *context,CS_CONNECTION *connection,CS_SERVERMSG *srvmsg)
{
#ifdef DEBUG_SQL
  char msg[1024];
  udm_snprintf(msg, sizeof(msg)-1, "Server message: Message number: %ld, Severity %ld, State: %ld, Line: %ld, Msg: '%s'",(long)srvmsg->msgnumber, (long)srvmsg->severity, (long)srvmsg->state, (long)srvmsg->line,srvmsg->text);
  fprintf(stderr, "%s\n",msg);
#endif  

  /*
  if (srvmsg->svrnlen > 0){fprintf(stderr, "Server '%s'\n", srvmsg->svrname);}
  if (srvmsg->proclen > 0){fprintf(stderr, " Procedure '%s'\n", srvmsg->proc);}
  */
  
  if (srvmsg->msgnumber != 3621) /* for SYBASE*/
  if(srvmsg->severity>1)
    udm_snprintf(sybsrvmsg, sizeof(sybsrvmsg)-1, "Message number: %ld, Severity %ld, State: %ld, Line: %ld, Msg: '%s'",(long)srvmsg->msgnumber, (long)srvmsg->severity, (long)srvmsg->state, (long)srvmsg->line,srvmsg->text);
  
  return CS_SUCCEED;
}


static void
DisplayError(UDM_DB *db, const char *func)
{
  udm_snprintf(db->errstr, sizeof(db->errstr),
               "%s failed: %s", func, sybsrvmsg);
}


static int
UdmCTLIBStmtFree(UDM_DB *db, CS_COMMAND *cmd)
{
  int rc;
  if ((rc= ct_cmd_drop(cmd)) != CS_SUCCEED)
  {
    DisplayError(db, "ct_cmd_drop");
    db->errcode= 1;
    return UDM_ERROR;
  }
  return UDM_OK;
}


static int
ex_execute_cmd(UDM_DB *db,CS_CHAR *cmdbuf)
{
  CS_RETCODE      retcode;
  CS_INT          restype;
  CS_COMMAND      *cmd;
  CS_RETCODE      query_code=CS_SUCCEED;
  UDM_CTLIB       *ct= (UDM_CTLIB*) db->specific;

  if ((retcode = ct_cmd_alloc(ct->conn, &cmd)) != CS_SUCCEED)
  {
    DisplayError(db, "ct_cmd_alloc");
    query_code=retcode;
    goto unlock_ret;
  }
  
  if ((retcode = ct_command(cmd, CS_LANG_CMD, cmdbuf,
                            CS_NULLTERM,CS_UNUSED)) != CS_SUCCEED)
  {
    DisplayError(db, "ct_command");
    UdmCTLIBStmtFree(db, cmd);
    query_code=retcode;
    goto unlock_ret;
  }
  
  if ((retcode = ct_send(cmd)) != CS_SUCCEED)
  {
    DisplayError(db, "ct_send");
    UdmCTLIBStmtFree(db, cmd);
    query_code=retcode;
    goto unlock_ret;
  }
  
  while ((retcode = ct_results(cmd, &restype)) == CS_SUCCEED)
  {
    switch((int)restype)
    {
      case CS_CMD_SUCCEED:
      case CS_CMD_DONE:
        break;

      case CS_CMD_FAIL:
        query_code= CS_FAIL;
        break;

      case CS_STATUS_RESULT:
        retcode = ct_cancel(NULL, cmd, CS_CANCEL_CURRENT);
        if (retcode != CS_SUCCEED)
        {
          DisplayError(db, "ct_cancel");
          query_code= CS_FAIL;
        }
        break;

      default:
        query_code= CS_FAIL;
        break;
    }
    
    if (query_code == CS_FAIL)
    {
      retcode= ct_cancel(NULL, cmd, CS_CANCEL_ALL);
      if (retcode != CS_SUCCEED)
        DisplayError(db, "ct_cancel");
      break;
    }
  }
  
  if (retcode == CS_END_RESULTS)
  {
    retcode= (UDM_OK == UdmCTLIBStmtFree(db, cmd)) ? CS_SUCCEED : CS_FAIL;
  }
  else
  {
    UdmCTLIBStmtFree(db, cmd);
    query_code= CS_FAIL;
  }
  
unlock_ret:
  return query_code;
}


static int
UdmCTLIBConnect(UDM_DB *db)
{
  CS_RETCODE  retcode;
  CS_INT      netio_type = CS_SYNC_IO;
  CS_CHAR     cmdbuf[128];
  UDM_CTLIB   *ct;
  const char* DBUser= UdmVarListFindStr(&db->Vars,"DBUser",NULL);
  const char* DBPass= UdmVarListFindStr(&db->Vars,"DBPass",NULL);
  const char* DBHost= UdmVarListFindStr(&db->Vars, "DBHost", NULL);
  
  db->specific= malloc(sizeof(UDM_CTLIB)); 
  bzero(db->specific, sizeof(UDM_CTLIB));
  ct= (UDM_CTLIB*) db->specific;
  
  if (CS_SUCCEED != (retcode= cs_ctx_alloc(CS_VERSION_100, &ct->ctx)))
  {
    DisplayError(db, "ct_ctx_alloc");
    goto unlock_ex;
  }
  
  if (CS_SUCCEED != (retcode = ct_init(ct->ctx, CS_VERSION_100)))
  {
    DisplayError(db, "ct_init");
    cs_ctx_drop(ct->ctx);
    ct->ctx = NULL;
    goto unlock_ex;
  }

#ifdef EX_API_DEBUG
  if (CS_SUCCEED != (retcode= ct_debug(db->ctx, NULL, CS_SET_FLAG, CS_DBG_API_STATES,NULL, CS_UNUSED)))
  {
    DisplayError(db, "ct_debug");
    goto unlock_ex;
  }
#endif
  
  
  if (CS_SUCCEED != (retcode= ct_callback(ct->ctx, NULL, CS_SET, CS_CLIENTMSG_CB,(CS_VOID *)ex_clientmsg_cb)))
  {
    DisplayError(db, "ct_callback(client)");
    goto ex1;
  }
  
  
  if (CS_SUCCEED != (retcode= ct_callback(ct->ctx, NULL, CS_SET, CS_SERVERMSG_CB,(CS_VOID *)ex_servermsg_cb)))
  {
    DisplayError(db, "ct_callback(server)");
    goto ex1;
  }


  if (CS_SUCCEED != (retcode= ct_config(ct->ctx, CS_SET, CS_NETIO, &netio_type, CS_UNUSED, NULL)))
  {
    DisplayError(db, "ct_config");
    goto ex1;
  }


ex1:
  if (retcode != CS_SUCCEED)
  {
    ct_exit(ct->ctx, CS_FORCE_EXIT);
    cs_ctx_drop(ct->ctx);
    ct->ctx= NULL;
    goto unlock_ex;
  }

  if (CS_SUCCEED != (retcode= ct_con_alloc(ct->ctx, &ct->conn)))
  {
    DisplayError(db, "ct_con_alloc");
    goto unlock_ex;
  }

  if (DBUser != NULL &&
      CS_SUCCEED != (retcode= ct_con_props(ct->conn, CS_SET, CS_USERNAME, (char*) DBUser, CS_NULLTERM, NULL)))
  {
    DisplayError(db, "ct_con_props(username)");
    goto ex2;
  }

  if (DBPass != NULL &&
      CS_SUCCEED != (retcode = ct_con_props(ct->conn, CS_SET, CS_PASSWORD, (char*) DBPass, CS_NULLTERM, NULL)))
  {
    DisplayError(db, "ct_con_props(password)");
    goto ex2;
  }

  if (CS_SUCCEED != (retcode = ct_con_props(ct->conn, CS_SET, CS_APPNAME, (char*) "indexer", CS_NULLTERM, NULL)))
  {
    DisplayError(db, "ct_con_props(appname)");
    goto ex2;
  }

  if (CS_SUCCEED != (retcode = ct_connect(ct->conn, (char*) DBHost, DBHost ? CS_NULLTERM : 0)))
  {
    DisplayError(db, "ct_connect(appname)");
    goto ex2;
  }


ex2:
  if (retcode != CS_SUCCEED)
  {
    ct_con_drop(ct->conn);
    ct->conn= NULL;
    goto unlock_ex;
  }

  db->connected= 1;
  udm_snprintf(cmdbuf, sizeof(cmdbuf), "use %s\n", db->DBName);
  if ((retcode= ex_execute_cmd(db, cmdbuf)) != CS_SUCCEED)
    db->errcode= 1;
  
unlock_ex:
  db->errcode= (retcode == CS_SUCCEED) ? 0 : 1;
  return (retcode == CS_SUCCEED) ? UDM_OK : UDM_ERROR;
}


static void
UdmCTLIBClose(UDM_DB *db)
{
  CS_RETCODE  retcode=CS_SUCCEED;
  CS_INT      close_option;
  CS_INT      exit_option;
  CS_RETCODE  status=CS_SUCCEED; /* FIXME: previous retcode was here */
  UDM_CTLIB  *ct= (UDM_CTLIB*) db->specific;

  if (!ct)
    return;
  
  if(ct->conn)
  {
    close_option = (status != CS_SUCCEED) ? CS_FORCE_CLOSE : CS_UNUSED;
    retcode = ct_close(ct->conn, close_option);
    if (retcode != CS_SUCCEED)
    {
      sprintf(db->errstr,"ex_con_cleanup: ct_close() failed");
      db->errcode=1;
      goto unlock_ret;
    }
    retcode = ct_con_drop(ct->conn);
    if (retcode != CS_SUCCEED)
    {
      sprintf(db->errstr,"ex_con_cleanup: ct_con_drop() failed");
      db->errcode=1;
      goto unlock_ret;
    }
  }
  
  if(ct->ctx)
  {
    exit_option = (status != CS_SUCCEED) ? CS_FORCE_EXIT : CS_UNUSED;
    retcode = ct_exit(ct->ctx, exit_option);
    if (retcode != CS_SUCCEED)
    {
      sprintf(db->errstr,"ex_ctx_cleanup: ct_exit() failed");
      db->errcode=1;
      goto unlock_ret;
    }
    retcode = cs_ctx_drop(ct->ctx);
    if (retcode != CS_SUCCEED)
    {
      sprintf(db->errstr,"ex_ctx_cleanup: cs_ctx_drop() failed");
      db->errcode=1;
      goto unlock_ret;
    }
  }
unlock_ret:
  UDM_FREE(db->specific);
  return;
}


static CS_INT CS_PUBLIC
ex_display_dlen(CS_DATAFMT * column)
{
  CS_INT    len;

  switch ((int) column->datatype)
  {
    case CS_CHAR_TYPE:
    case CS_VARCHAR_TYPE:
    case CS_LONGCHAR_TYPE:
    case CS_TEXT_TYPE:
    case CS_IMAGE_TYPE:
      len = MIN(column->maxlength*2+2, MAX_CHAR_BUF);
      break;

    case CS_BINARY_TYPE:
    case CS_VARBINARY_TYPE:
      len = MIN((2 * column->maxlength) + 2, MAX_CHAR_BUF);
      break;

    case CS_BIT_TYPE:
    case CS_TINYINT_TYPE:
      len = 3;
      break;

    case CS_SMALLINT_TYPE:
      len = 6;
      break;

    case CS_INT_TYPE:
      len = 11;
      break;

    case CS_REAL_TYPE:
    case CS_FLOAT_TYPE:
      len = 24;
      break;

    case CS_MONEY_TYPE:
    case CS_MONEY4_TYPE:
      len = 24;
      break;

    case CS_DATETIME_TYPE:
    case CS_DATETIME4_TYPE:
      len = 30;
      break;

    case CS_NUMERIC_TYPE:
    case CS_DECIMAL_TYPE:
      len = (CS_MAX_PREC + 2);
      break;

    default:
      len = 12;
      break;
  }
  return MAX((CS_INT)(strlen(column->name) + 1), len);
}


static int
ex_fetch_data(UDM_DB *db, UDM_SQLRES *res, CS_COMMAND *cmd)
{
  CS_RETCODE    retcode;
  CS_INT      num_cols;
  CS_INT      i;
  CS_INT      j;
  CS_INT      row_count = 0;
  CS_INT      rows_read;
  CS_DATAFMT    *datafmt;
  EX_COLUMN_DATA    *coldata;

  retcode = ct_res_info(cmd, CS_NUMDATA, &num_cols, CS_UNUSED, NULL);
  if (retcode != CS_SUCCEED)
  {
    DisplayError(db, "ct_res_info");
    return UDM_ERROR;
  }
  if (num_cols <= 0)
  {
    sprintf(db->errstr,"ex_fetch_data: ct_res_info() returned zero columns");
    return UDM_ERROR;
  }

  coldata = (EX_COLUMN_DATA *)UdmMalloc(num_cols * sizeof (EX_COLUMN_DATA));
  datafmt = (CS_DATAFMT *)UdmMalloc(num_cols * sizeof (CS_DATAFMT));

  for (i = 0; i < num_cols; i++)
  {
    retcode = ct_describe(cmd, (i + 1), &datafmt[i]);
    if (retcode != CS_SUCCEED)
    {
      DisplayError(db, "ct_describe");
      break;
    }

    /* Remember the original datatype */
    coldata[i].datatype= datafmt[i].datatype;

    datafmt[i].maxlength = ex_display_dlen(&datafmt[i]) + 1;
    datafmt[i].datatype = CS_CHAR_TYPE;
#ifdef CS_CURRENT_VERSION
    if (CS_CURRENT_VERSION >= 15000)
      datafmt[i].datatype= CS_LONGCHAR_TYPE;
#endif
    datafmt[i].format   = CS_FMT_NULLTERM;

    /* Allocate data */
    coldata[i].value = (CS_CHAR *)UdmMalloc((size_t)datafmt[i].maxlength);
    
    retcode = ct_bind(cmd, (i + 1), &datafmt[i],coldata[i].value, &coldata[i].valuelen,&coldata[i].indicator);
    
    if (retcode != CS_SUCCEED)
    {
      DisplayError(db, "ct_bind");
      break;
    }
  }
  
  if (retcode != CS_SUCCEED)
  {
    for (j = 0; j < i; j++)
    {
      UDM_FREE(coldata[j].value);
    }
    UDM_FREE(coldata);
    UDM_FREE(datafmt);
    return UDM_ERROR;
  }
  
  res->nCols=num_cols;
  if (res->nCols){
    res->Fields=(UDM_SQLFIELD*)UdmMalloc(res->nCols*sizeof(UDM_SQLFIELD));
    bzero(res->Fields,res->nCols*sizeof(UDM_SQLFIELD));
    for (i=0 ; i < res->nCols; i++)
    {
      res->Fields[i].sqlname = (char*)UdmStrdup(datafmt[i].name);
      res->Fields[i].sqllen= 0;
      res->Fields[i].sqltype= 0;
    }
  }
  
  while (((retcode = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED,&rows_read)) == CS_SUCCEED) || (retcode == CS_ROW_FAIL))
  {
    row_count = row_count + rows_read;
    if (retcode == CS_ROW_FAIL)
    {
      DisplayError(db, "ct_fetch");
      break;
    }
    res->Items=(UDM_PSTR*)UdmRealloc(res->Items,(res->nRows+1)*res->nCols*sizeof(UDM_PSTR));
    
    for (i = 0; i < num_cols; i++)
    {
      size_t offs= res->nRows*res->nCols+i;
      
      if (coldata[i].datatype == CS_IMAGE_TYPE ||
          coldata[i].datatype == CS_BINARY_TYPE ||
          coldata[i].datatype == CS_VARBINARY_TYPE ||
          coldata[i].datatype == CS_LONGBINARY_TYPE )
      {
        /*
          Unfortunately bind to CS_IMAGE_TYPE,
          CS_BINARY_TYPE, CS_VARBINARY_TYPE,
          CS_LONGBINARY_TYPE either didn't
          work or returned zero-padded values
          with FreeTDS ctlib. So, let's bind 
          these types as CHAR and convert back
          into BINARY. This is slower, but at
          least works as expected.
          
          Note, if valuelen is a positive number, then
          the value includes trailing '\0'.
        */
        size_t valuelen= coldata[i].valuelen;
        valuelen= valuelen > 0 ? valuelen - 1 : valuelen;
        DecodeHexStr(coldata[i].value,&res->Items[offs],valuelen);
      }
      else
      {
        /* valuelen includes trailing 0 */
        if (!coldata[i].indicator)
        {
          res->Items[offs].val= (char*)UdmMalloc(coldata[i].valuelen);
          memcpy(res->Items[offs].val,coldata[i].value,coldata[i].valuelen);
          res->Items[offs].len= coldata[i].valuelen-1;
        }
        else
        {
          res->Items[offs].val= NULL;
          res->Items[offs].len= 0;
        }
      }
    } 
    res->nRows++;
  }

  for (i = 0; i < num_cols; i++)
  {
    UDM_FREE(coldata[i].value);
  }
  UDM_FREE(coldata);
  UDM_FREE(datafmt);

  switch ((int)retcode)
  {
    case CS_END_DATA:
      retcode = CS_SUCCEED;
      db->errcode=0;
      break;

    case CS_FAIL:
      DisplayError(db, "ct_fetch");
      db->errcode=1;
      return UDM_ERROR;
      break;

    default:
      sprintf(db->errstr,"ex_fetch_data: ct_fetch() returned an expected retcode (%d)",(int)retcode);
      db->errcode=1;
      return UDM_ERROR;
      break;
  }
  return UDM_OK;
}


static int
UdmCTLIBQuery(UDM_DB *db, UDM_SQLRES *res, const char * query)
{
  CS_RETCODE  retcode;
  CS_COMMAND  *cmd;
  CS_INT      res_type;
  UDM_CTLIB   *ct;
  
  db->errcode=0;
  db->errstr[0]='\0';
  
  if (res)
  {
    bzero((void*) res, sizeof(UDM_SQLRES));
    res->db= db;
  }

  if(!db->connected)
  {
    UdmCTLIBConnect(db);
    if(db->errcode)
      goto unlock_ret;
  }
  
  ct= (UDM_CTLIB*) db->specific;
  
  if ((retcode = ct_cmd_alloc(ct->conn, &cmd)) != CS_SUCCEED)
  {
    DisplayError(db, "ct_cmd_allow");
    db->errcode=1;
    goto unlock_ret;
  }
  
  retcode = ct_command(cmd, CS_LANG_CMD, (char*) query, CS_NULLTERM, CS_UNUSED);
  if (retcode != CS_SUCCEED)
  {
    DisplayError(db, "ct_command");
    db->errcode=1;
    goto unlock_ret;
  }
  
  if (ct_send(cmd) != CS_SUCCEED)
  {
    DisplayError(db, "ct_send");
    db->errcode=1;
    goto unlock_ret;
  }
  
  while ((retcode = ct_results(cmd, &res_type)) == CS_SUCCEED)
  {
    switch ((int)res_type)
    {
    case CS_CMD_SUCCEED:
    case CS_CMD_DONE:
         break;

    case CS_STATUS_RESULT:
      break;

    case CS_CMD_FAIL:
      /*printf("here: %s\n",sybsrvmsg);*/
      if(!strstr(sybsrvmsg,"UNIQUE KEY") && 
         !strstr(sybsrvmsg,"PRIMARY KEY") &&
         !strstr(sybsrvmsg,"unique index"))
      {
        DisplayError(db, "ct_results");
        db->errcode=1;
      }
      break;

    case CS_ROW_RESULT:
      if (!res)
      {
        udm_snprintf(db->errstr,sizeof(db->errstr)-1,"CTLibQuery: Query returned result but called without 'res'");
        db->errcode=1;
        goto unlock_ret;
      }
      if(UDM_OK != ex_fetch_data(db,res,cmd))
      {
        db->errcode=1;
        goto unlock_ret;
      }
      break;

    case CS_COMPUTE_RESULT:
      /* FIXME: add checking */
      break; 

    default:
      sprintf(db->errstr,"ct_results() returned unexpected result type (%d)",(int)res_type);
      db->errcode=1;
      goto unlock_ret;
    }
  }

  switch ((int)retcode)
  {
    case CS_END_RESULTS:
      break;

    case CS_FAIL:
      sprintf(db->errstr,"ct_results() failed");
      db->errcode=1;
      goto unlock_ret;

    default:
      sprintf(db->errstr,"ct_results() returned unexpected result code");
      db->errcode=1;
      goto unlock_ret;
  }


  if (UDM_OK != (retcode= UdmCTLIBStmtFree(db, cmd)))
  {
    db->errcode=1;
    UdmSQLResFreeGeneric(res);
    return retcode;
  }

unlock_ret:

  return db->errcode ? UDM_ERROR : UDM_OK;
}


static int
UdmCTLibBegin(UDM_DB *db)
{
  return UdmCTLIBQuery(db,NULL,"BEGIN TRANSACTION");
}


static int
UdmCTLibCommit(UDM_DB *db)
{
  return UdmCTLIBQuery(db,NULL,"COMMIT");
}


static int
UdmCTLibRenameTable(UDM_DB *db, const char *from, const char *to)
{
  char buf[256];
  udm_snprintf(buf, sizeof(buf), "sp_rename %s, %s", from, to);
  return UdmSQLExecDirect(db, NULL, buf);
}


static int
UdmCTLibCopyStructure(UDM_DB *db, const char *from, const char *to)
{
  char buf[256];
  udm_snprintf(buf, sizeof(buf),
               "SELECT * INTO %s FROM %s WHERE 1=0", to, from);
  return UdmSQLExecDirect(db, NULL, buf);
}


UDM_SQLDB_HANDLER udm_sqldb_ctlib_handler =
{
  UdmSQLEscStrGeneric,
  UdmCTLIBQuery,
  UdmCTLIBConnect,
  UdmCTLIBClose,
  UdmCTLibBegin,
  UdmCTLibCommit,
  NULL,
  NULL,
  NULL,
  NULL,
  UdmSQLFetchRowSimple,
  UdmSQLStoreResultSimple,
  UdmSQLFreeResultSimple,
  UdmCTLIBQuery,
  UdmCTLibRenameTable,
  UdmCTLibCopyStructure,
  UdmSQLLockOrBeginGeneric,
  UdmSQLUnlockOrCommitGeneric,
};

#endif /* HAVE_CTLIB */

#endif /* HAVE_SQL */
