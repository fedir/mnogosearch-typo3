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
#if HAVE_ORACLE8
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
#ifdef WIN32
#include <process.h>
#endif

/*
  Oracle headers have some "Function declaration is not a prototype"
  warnings. Let's suppress them.
*/
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <oci.h>
#pragma GCC diagnostic pop

#define MAX_BIND_PARAM		UDM_SQL_MAX_BIND_PARAM
#define	MAX_COLS_IN_TABLE	64
#define BUF_OUT_SIZE		16

/* (C) copyleft 2000 Anton Zemlyanov, az@hotmail.ru */
/* TODO: efficient transactions, multi-row fetch, limits stuff */

typedef struct udm_oracle_db_st
{
  OCIEnv     *envhp;
  OCIError   *errhp;
  OCISvcCtx  *svchp;
  OCIStmt    *stmthp;
  OCIParam   *param;
  OCIDefine  *defb[MAX_COLS_IN_TABLE];
  OCIBind    *bndhp[MAX_BIND_PARAM];
} UDM_ORACLE_DB;


static inline int
SQL_OK(int rc)
{
  return (rc == OCI_SUCCESS) || (rc == OCI_SUCCESS_WITH_INFO);
}


static int
UdmOracleTypes[] =
{
  0,         /* UDM_SQLTYPE_UNKNOWN       */
  SQLT_LBI,  /* UDM_SQLTYPE_LONGVARBINARY */
  SQLT_LNG,  /* UDM_SQLTYPE_LONGVARCHAR   */
  SQLT_CHR,  /* UDM_SQLTYPE_VARCHAR       */
  SQLT_INT,  /* UDM_SQLTYPE_INT32         */
};


static int
UdmOraDisplayError(UDM_DB *db, const char *fname)
{
  sb4  errcode=0;
  char  errbuf[512];
  char  *ptr;
  UDM_ORACLE_DB *ob= (UDM_ORACLE_DB *)db->specific;

  db->errstr[0]='\0';

  switch (db->errcode)
  {
    case OCI_SUCCESS:
      sprintf(db->errstr,"Oracle - OCI_SUCCESS");
      break;
    case OCI_SUCCESS_WITH_INFO:
      sprintf(db->errstr,"Oracle - OCI_SUCCESS_WITH_INFO");
      break;
    case OCI_NEED_DATA:
      sprintf(db->errstr,"Oracle - OCI_NEED_DATA");
      break;
    case OCI_NO_DATA:
      sprintf(db->errstr,"Oracle - OCI_NODATA");
      break;
    case OCI_ERROR:
      OCIErrorGet((dvoid *)ob->errhp, (ub4) 1, (text *) NULL, &errcode,
                  (text *)errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
      ptr=errbuf;
      while (*ptr)
      {
        if (*ptr=='\n')
          *ptr='!';
        ++ptr;
      }
      udm_snprintf(db->errstr, sizeof(db->errstr) - 1,
                   "Oracle: %s: %.*s", fname, 512, errbuf);
      break;
    case OCI_INVALID_HANDLE:
      sprintf(db->errstr,"Oracle - OCI_INVALID_HANDLE");
      break;
    case OCI_STILL_EXECUTING:
      sprintf(db->errstr,"Oracle - OCI_STILL_EXECUTE");
      break;
    case OCI_CONTINUE:
      sprintf(db->errstr,"Oracle - OCI_CONTINUE");
      break;
    default:
      sprintf(db->errstr,"Oracle - unknown internal bug");
      break;
  }
  return 0;
}


static void
UdmOraClose(UDM_DB *db)
{
  UDM_ORACLE_DB *ob= (UDM_ORACLE_DB *)db->specific;
  if (!db->connected)
    return;
  db->errcode=OCILogoff(ob->svchp,ob->errhp);
  db->connected=0;
  UDM_FREE(db->specific);
}


static int
UdmOraConnect(UDM_DB *db)
{
  const char* DBUser= UdmVarListFindStr(&db->Vars, "DBUser", "");
  const char* DBPass= UdmVarListFindStr(&db->Vars, "DBPass", "");
  UDM_ORACLE_DB *ob;

  db->specific= UdmMalloc(sizeof(UDM_ORACLE_DB));
  if (! db->specific) return(UDM_ERROR);
  ob= (UDM_ORACLE_DB *)db->specific;
  OCIInitialize( OCI_DEFAULT, NULL, NULL, NULL, NULL );
  OCIEnvInit( &(ob->envhp), OCI_DEFAULT, 0, NULL );
  OCIHandleAlloc( ob->envhp, (dvoid **)&(ob->errhp), OCI_HTYPE_ERROR,0, NULL );
  db->errcode= OCILogon(ob->envhp, ob->errhp, &(ob->svchp),
                        (const text *)DBUser, strlen(DBUser), (const text *)DBPass, strlen(DBPass),
                        (const text *)db->DBName, strlen(db->DBName));
  
  if (db->errcode!=OCI_SUCCESS)
    return UDM_ERROR;
  
  db->errcode= 0;
  db->connected= 1;
  db->commit_fl= 0;

  /*sql_query(((UDM_DB*)(db)), "ALTER SESSION SET SQL_TRACE=TRUE"); */
  
  return UDM_OK;
}


static int
oracle_read_blob(UDM_DB *db, OCILobLocator *lobp, UDM_PSTR *items)
{
  int rc;
  ub4 len;
  char *buf;
  UDM_ORACLE_DB *ob= (UDM_ORACLE_DB *)db->specific;

  rc= OCILobGetLength(ob->svchp, ob->errhp, lobp, &len);
   /*
     Convert number of characters to number of bytes.
     It used to work with len*3, however oracle-xe-univ-10.2.0.1
     ("universal" edition) requires len*4.
  */
  len= len*4;
  if (!SQL_OK(rc))
  {
    db->errcode= rc;
    return(UDM_ERROR);
  }

  buf= UdmMalloc(len + 1);
  if (! buf)
  {
    return(UDM_ERROR);
  }

  rc= OCILobRead(ob->svchp, ob->errhp, lobp, &len, 1, buf, len, 0, 0, 0, SQLCS_IMPLICIT);
  if (!SQL_OK(rc))
  {
    db->errcode= rc;
    UdmFree(buf);
    return(UDM_ERROR);
  }
  buf[len]= 0;

  items->len= len;
  items->val= buf;

  return(UDM_OK);
}


static int
UdmOraStmtFree(UDM_DB *db)
{
  int rc;
  UDM_ORACLE_DB *ob= (UDM_ORACLE_DB *)db->specific;
  if (!SQL_OK(rc= OCIHandleFree(ob->stmthp, OCI_HTYPE_STMT)))
  {
    db->errcode= rc;
    return UDM_ERROR;
  }
  return UDM_OK;
}


static int
sql_oracle_query(UDM_DB *db, UDM_SQLRES *result, const char *qbuf)
{
  sword  rc;
  ub2  stmt_type;
  int  cnt, buf_nRows, row;

  int  colcnt;
  ub2  coltype;
  ub2  colsize;
  int  oci_fl;
  sb4  errcode=0;
  char  errbuf[512];
  int  num_rec;
  char	*defbuff[MAX_COLS_IN_TABLE];
  int	col_size[MAX_COLS_IN_TABLE];
  int	col_type[MAX_COLS_IN_TABLE];
  sb2	indbuff[MAX_COLS_IN_TABLE][BUF_OUT_SIZE];
  UDM_ORACLE_DB *ob= (UDM_ORACLE_DB *)db->specific;

  db->errcode=0;
  rc= OCIHandleAlloc(ob->envhp, (dvoid *)&(ob->stmthp), OCI_HTYPE_STMT,0,NULL);
  if (!SQL_OK(rc))
  {
    db->errcode=rc;
    return(UDM_ERROR);
  }
  rc= OCIStmtPrepare(ob->stmthp,ob->errhp,(const text *)qbuf,strlen(qbuf),
                     OCI_NTV_SYNTAX,OCI_DEFAULT);
  if (!SQL_OK(rc))
  {
    db->errcode=rc;
    return(UDM_ERROR);
  }

  num_rec= 1;
    
  rc= OCIAttrGet(ob->stmthp,OCI_HTYPE_STMT,&stmt_type,0,
                 OCI_ATTR_STMT_TYPE,ob->errhp);
  if (!SQL_OK(rc))
  {
    db->errcode=rc;
    return(UDM_ERROR);
  }
  
  if (stmt_type!=OCI_STMT_SELECT)
  {
    /* non-select statements */
    /* COMMIT_ON_SUCCESS in inefficient */
    if (db->commit_fl)
      oci_fl=OCI_DEFAULT;
    else
      oci_fl=OCI_COMMIT_ON_SUCCESS;

    rc= OCIStmtExecute(ob->svchp,ob->stmthp,ob->errhp, num_rec,0,
                       NULL,NULL,oci_fl);
    
    if (num_rec>1)
      (void) OCITransCommit(ob->svchp, ob->errhp, (ub4) 0);
    
    if (!SQL_OK(rc))
    {
      db->errcode=rc;
      OCIErrorGet((dvoid *)ob->errhp, (ub4) 1, (text *) NULL, &errcode,
                   (text *)errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
      if (strncmp(errbuf,"ORA-00001",9)) /* ignore ORA-00001 */
        return(UDM_ERROR);
      else 
        db->errcode=OCI_SUCCESS;
    }
    
    /* OCI_ATTR_ROW_COUNT of db->stmthp - Rows affected */
    
    goto stmt_free;
  }

  /* select statements */

  rc=OCIStmtExecute(ob->svchp,ob->stmthp,ob->errhp,0,0,
                    NULL,NULL,OCI_DEFAULT);
  if (!SQL_OK(rc))
  {
    db->errcode=rc;
    return(UDM_ERROR);
  }

  /*describe the select list and define buffers for the select list */
  colcnt=0;
  while (OCIParamGet(ob->stmthp,OCI_HTYPE_STMT,ob->errhp,(dvoid *)&(ob->param),
                    colcnt+1 ) == OCI_SUCCESS)
  {
    
    ub4 str_len;
    char *namep;
    
    result->Fields=(UDM_SQLFIELD*)UdmRealloc(result->Fields,(colcnt+1)*sizeof(UDM_SQLFIELD));
    bzero((void*)&result->Fields[colcnt],sizeof(UDM_SQLFIELD));
    
    rc=OCIAttrGet(ob->param,OCI_DTYPE_PARAM,(dvoid*) &namep, (ub4*) &str_len,
                  (ub4) OCI_ATTR_NAME, ob->errhp);
    if (!SQL_OK(rc))
    {
      db->errcode=rc;
      return(UDM_ERROR);
    }
    namep[str_len]= '\0';
    result->Fields[colcnt].sqlname= (char*)UdmStrdup(namep);
    
    rc=OCIAttrGet(ob->param,OCI_DTYPE_PARAM,&(coltype),0,
                  OCI_ATTR_DATA_TYPE,ob->errhp);
    if (!SQL_OK(rc))
    {
      db->errcode=rc;
      return(UDM_ERROR);
    }
    rc=OCIAttrGet(ob->param,OCI_DTYPE_PARAM,&colsize,0,
                  OCI_ATTR_DATA_SIZE,ob->errhp);
    if (!SQL_OK(rc))
    {
      db->errcode=rc;
      return(UDM_ERROR);
    }

    /* OCIStmtFetch do not terminate data with \0 -
    add a byte for terminator to define buffers - insurance*/

    col_type[colcnt]= coltype;
    switch(coltype) {
    size_t i;
    case SQLT_BLOB:
    case SQLT_CLOB:
      col_size[colcnt]= sizeof(OCILobLocator *);
      defbuff[colcnt]= (char *)UdmXmalloc(sizeof(OCILobLocator *) * BUF_OUT_SIZE);
      for (i= 0; i < BUF_OUT_SIZE; i++)
      {
        rc= OCIDescriptorAlloc(ob->envhp, (dvoid **)(defbuff[colcnt] + i * sizeof(OCILobLocator *)), OCI_DTYPE_LOB, 0, 0);
	if (!SQL_OK(rc))
	{
	  db->errcode=rc;
	  return(UDM_ERROR);
	}
      }
      break;
    case SQLT_CHR: /* variable length string */
    case SQLT_AFC: /* fixed length string */
    case SQLT_BIN:
    case SQLT_LNG:
    case SQLT_LBI:
      col_size[colcnt]=colsize;
      defbuff[colcnt]=(char *)UdmXmalloc(col_size[colcnt]*BUF_OUT_SIZE+1);
      break;
    case SQLT_NUM: /* numbers up to 14 digits now */
      col_size[colcnt]=14;
      defbuff[colcnt]=(char *)UdmXmalloc(col_size[colcnt]*BUF_OUT_SIZE+1);
      break;
    default:
      printf("<P>Unknown datatype: %d\n",coltype);
      return(UDM_ERROR);
    }

    if (coltype == SQLT_BLOB || coltype == SQLT_CLOB)
      rc=OCIDefineByPos(ob->stmthp,&(ob->defb[colcnt]),ob->errhp,
                        colcnt+1,defbuff[colcnt],
                        col_size[colcnt],coltype,
                        &(indbuff[colcnt]),0,0,OCI_DEFAULT);
    else
      rc=OCIDefineByPos(ob->stmthp,&(ob->defb[colcnt]),ob->errhp,
                        colcnt+1,defbuff[colcnt],
                        col_size[colcnt],SQLT_CHR,
                        &(indbuff[colcnt]),0,0,OCI_DEFAULT);
    
    if (!SQL_OK(rc))
    {
      db->errcode=rc;
       return(UDM_ERROR);
    }
    
    rc=OCIDefineArrayOfStruct(ob->defb[colcnt], ob->errhp,
                              col_size[colcnt],
                              sizeof(indbuff[0][0]), 0, 0);
    if (!SQL_OK(rc))
    {
      db->errcode=rc;
      return(UDM_ERROR);
    }
    colcnt++;
  }
  result->nCols=colcnt;

  /* Now fetch the selected rows into the memory */
  while (1)
  {
    if (db->res_limit)
      if (db->res_limit == result->nRows)
        break;
    /*
      Fix me: Process of determine real fetched rows
      Fill indicator buffer for value -6 to
    */
    for (row=0; row<BUF_OUT_SIZE; row++)
      indbuff[result->nCols-1][row]=-6;

    rc=OCIStmtFetch(ob->stmthp,ob->errhp,BUF_OUT_SIZE,
                    OCI_FETCH_NEXT,OCI_DEFAULT);
    
    if ((rc!=OCI_NO_DATA) && !SQL_OK(rc))
    {
      db->errcode=rc;
      return(UDM_ERROR);
    }

    /* Find number of fetched rows */
    for (buf_nRows=0; buf_nRows<BUF_OUT_SIZE; buf_nRows++)
    {
      if (indbuff[result->nCols-1][buf_nRows]==-6)
      break;
    }

    if (!buf_nRows)
      break;

    if (!result->nRows)  /* first row - allocate */
      result->Items=(UDM_PSTR *)UdmXmalloc(buf_nRows*result->nCols*sizeof(UDM_PSTR));
    else
      result->Items=(UDM_PSTR *)UdmXrealloc(result->Items,
                                            ((result->nRows+buf_nRows)*
                                            result->nCols*sizeof(UDM_PSTR)));
    /* Limit rows */  
    if (db->res_limit)
      buf_nRows=result->nRows+buf_nRows<=db->res_limit?buf_nRows:(db->res_limit-result->nRows);
    
    for (row=0; row<buf_nRows; row++)
    {
      for (cnt=0; cnt<result->nCols; ++cnt)
      {
        if (indbuff[cnt][row]==OCI_IND_NULL)
        {
          result->Items[(result->nRows + row) * result->nCols + cnt].val= (char *)UdmStrdup("");
          result->Items[(result->nRows + row) * result->nCols + cnt].len= 0;
        }
        else
        {
          int offset=row*col_size[cnt];
          if (col_type[cnt] == SQLT_BIN)
          {
            DecodeHexStr(defbuff[cnt] + offset, &result->Items[(result->nRows + row) * result->nCols + cnt], col_size[cnt]);
          }
          else if (col_type[cnt] == SQLT_CLOB || col_type[cnt] == SQLT_BLOB)
	  {
	    OCILobLocator **lobp= (OCILobLocator **)(defbuff[cnt] + offset);
	    oracle_read_blob(db, *lobp, &result->Items[(result->nRows + row) * result->nCols + cnt]);
          }
	  else
	  {
            char *val=UdmXmalloc(col_size[cnt]+1);
            udm_snprintf(val, col_size[cnt]+1, "%s",
                         defbuff[cnt]+offset);
            result->Items[(result->nRows + row) * result->nCols + cnt].val= (char *)UdmStrdup(UdmRTrim(val, " "));
            result->Items[(result->nRows + row) * result->nCols + cnt].len= strlen(result->Items[(result->nRows + row) * result->nCols + cnt].val);
            UDM_FREE(val);
          }
        }
      }
    }
    result->nRows+=row;
    if (rc==OCI_NO_DATA)
      break;
    if (!SQL_OK(rc))
    {
      db->errcode=rc;
      return(UDM_ERROR);
    }
  }
  
  for (cnt= 0; cnt < result->nCols; cnt++)
  {
    if (col_type[cnt] == SQLT_CLOB || col_type[cnt] == SQLT_BLOB)
    {
      size_t i;
      for (i= 0; i < BUF_OUT_SIZE; i++)
      {
        OCILobLocator **lobp= (OCILobLocator **)(defbuff[cnt] + i * sizeof(OCILobLocator *));
        OCIDescriptorFree(*lobp, OCI_DTYPE_LOB);
      }
    }
    UdmFree(defbuff[cnt]);
  }

stmt_free:

  db->res_limit= 0;
  rc= UdmOraStmtFree(db);
  return rc;
}


static int
UdmOraExecDirect(UDM_DB *db, UDM_SQLRES *res, const char *qbuf)
{
  int rc;
  
  if (res)
  {
    bzero((void*) res, sizeof(UDM_SQLRES));
    res->db= db;
  }

  if (!db->connected)
  {
    size_t i;
    for (i= 0; i < 3; i++)
    {
      UdmOraConnect(db);
      if (db->errcode)
      {
        UdmOraDisplayError(db, "InitDB");
        if (strstr(db->errstr, "ORA-12520"))
        {
          /* No listener available, try again */
          db->errcode= 0;
          db->errstr[0]= '\0';
          UDMSLEEP(1);
          continue;
        }
        return UDM_ERROR;
      }
      else
      {
        db->connected= 1;
        goto query;
      }
    }
    if (!db->connected)
    {
      rc= UDM_ERROR;
      goto error;
    }
  }


query:
  rc= sql_oracle_query(db, res, qbuf);

error:
  if (db->errcode)
  {
    UdmOraDisplayError(db, "ExecDirect");
    return UDM_ERROR;
  }
  return rc;
}


static int
UdmOraPrepare(UDM_DB *db, const char *query)
{
  int rc;
  UDM_ORACLE_DB *ob= (UDM_ORACLE_DB *)db->specific;

  db->errcode= 0;
  db->errstr[0]= 0;
  rc= OCIHandleAlloc(ob->envhp, (dvoid *)&(ob->stmthp), OCI_HTYPE_STMT, 0, NULL);
  if (!SQL_OK(rc))
  {
    db->errcode= rc;
    UdmOraDisplayError(db, "Prepare");
    return UDM_ERROR;
  }

  rc= OCIStmtPrepare(ob->stmthp, ob->errhp, (const unsigned char *)query,
                     strlen(query), OCI_NTV_SYNTAX, OCI_DEFAULT);
  if (!SQL_OK(rc))
  {
    db->errcode= rc;
    UdmOraDisplayError(db, "Prepare");
    return UDM_ERROR;
  }
  return UDM_OK;
}


static int
UdmOraBind(UDM_DB *db, int position, const void *data, int size, int type)
{
  int rc;
  UDM_ORACLE_DB *ob= (UDM_ORACLE_DB *)db->specific;
  
  db->errcode= 0;
  db->errstr[0]= 0;
  ob->bndhp[position]= NULL;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
  rc= OCIBindByPos(ob->stmthp, &ob->bndhp[position], ob->errhp,
                  (ub4) position,
                  (dvoid *) data,
                  (sb4) size,
                  (ub2) UdmOracleTypes[type],
                  (dvoid *) 0,
                  (ub2 *)0, (ub2 *)0, (ub4)0, (ub4 *)0,
                  (ub4) OCI_DEFAULT);
#pragma GCC diagnostic pop

  if (!SQL_OK(rc))
  {
    db->errcode= rc;
    UdmOraDisplayError(db, "Bind");
    return UDM_ERROR;
  }
  return UDM_OK;
}


static int
UdmOraExec(UDM_DB *db)
{
  int rc;
  size_t num_rec= 1;
  UDM_ORACLE_DB *ob= (UDM_ORACLE_DB *)db->specific;
  
  db->errcode= 0;
  db->errstr[0]= 0;
  
  if (!SQL_OK(rc= OCIStmtExecute(ob->svchp, ob->stmthp, ob->errhp,
                                 num_rec, 0, NULL, NULL, OCI_DEFAULT)))
  {
    db->errcode= rc;
    UdmOraDisplayError(db, "Exec");
    return UDM_ERROR;
  }

  return UDM_OK;
}


static int
UdmOraBegin(UDM_DB *db)
{
  int rc= UdmOraExecDirect(db,NULL,"COMMIT");
  db->commit_fl= 1;
  return rc;
}


static int
UdmOraCommit(UDM_DB *db)
{
  int rc= UdmOraExecDirect(db,NULL,"COMMIT");
  db->commit_fl= 0;
  return rc;
}


static int
UdmOraRenameTable(UDM_DB *db, const char *from, const char *to)
{
  char buf[256];
  /* "RENAME t1 TO t2" also works */
  udm_snprintf(buf, sizeof(buf), "ALTER TABLE %s RENAME TO %s", from, to);
  return UdmSQLExecDirect(db, NULL, buf);
}


static int
UdmOraCopyStructure(UDM_DB *db, const char *from, const char *to)
{
  char buf[256];
  udm_snprintf(buf, sizeof(buf),
               "CREATE TABLE %s AS SELECT * FROM %s WHERE 1=0", to, from);
  return UdmSQLExecDirect(db, NULL, buf);
}


UDM_SQLDB_HANDLER udm_sqldb_oracle_handler =
{
  UdmSQLEscStrGeneric,
  UdmOraExecDirect,
  UdmOraConnect,
  UdmOraClose,
  UdmOraBegin,
  UdmOraCommit,
  UdmOraPrepare,
  UdmOraBind,
  UdmOraExec,
  UdmOraStmtFree,
  UdmSQLFetchRowSimple,
  UdmSQLStoreResultSimple,
  UdmSQLFreeResultSimple,
  UdmOraExecDirect,
  UdmOraRenameTable,
  UdmOraCopyStructure,
  UdmSQLLockOrBeginGeneric,
  UdmSQLUnlockOrCommitGeneric,
};

#endif
