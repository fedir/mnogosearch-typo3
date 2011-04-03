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

#if HAVE_IODBC
#include <sql.h>
#include <sqlext.h>
#endif

#if HAVE_EASYSOFT
#include <sql.h>
#include <sqlext.h>
#endif

#if HAVE_VIRT
#include <iodbc.h>
#include <isql.h>
#include <isqlext.h>
#endif

#if HAVE_UNIXODBC
#include <sql.h>
#include <sqlext.h>
#endif

#if HAVE_SAPDB
#include <WINDOWS.H>
#include <sql.h>
#include <sqlext.h>
#endif

#if HAVE_SOLID
#include <cli0cli.h>
#endif

#if HAVE_DB2
#include <sqlcli1.h>
#endif

#ifdef WIN32
#include <process.h>
#endif


/************************** ODBC ***********************************/
#if (HAVE_ODBC)

typedef struct udm_odbc_conn_st
{
#if (HAVE_IODBC || HAVE_UNIXODBC || HAVE_SOLID || HAVE_VIRT || HAVE_EASYSOFT || HAVE_SAPDB)
  HDBC  hDbc;
  HENV  hEnv;
  HSTMT hstmt;
#endif
#if HAVE_DB2
  SQLHANDLE hDbc;
  SQLHANDLE hEnv;
  SQLHANDLE hstmt;
#endif
} UDM_ODBC_CONN;


typedef struct udm_odbc_field_st
{
  SQLCHAR     Name[32];
  SQLSMALLINT NameLen;
  SQLSMALLINT Type;
  SQLUINTEGER Size;
  SQLSMALLINT Scale;
  SQLSMALLINT Nullable;
} UDM_ODBC_FIELD;


typedef struct udm_odbc_bind_st
{
  SQLINTEGER size;
  void *data;
} UDM_ODBC_BINDBUF;


/* TODO: move this to virtual part of UDM_SQLRES */
static UDM_ODBC_BINDBUF BindBuf[UDM_SQL_MAX_BIND_PARAM];



static inline int
SQL_OK(int rc)
{
  return rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO;
}


static int
UdmODBCDisplayError(UDM_DB *db, const char *prefix)
{
  UCHAR szSqlState[10];
  UCHAR szErrMsg[400];
  SDWORD naterr;
  SWORD length;
  RETCODE rc=SQL_SUCCESS;
  size_t len= strlen(prefix);
  UDM_ODBC_CONN *sdb= (UDM_ODBC_CONN*) db->specific;

  strcpy(db->errstr, prefix);
  
  while (1)
  {   
    rc= SQLError(sdb->hEnv, sdb->hDbc, sdb->hstmt, szSqlState,
                 &naterr,szErrMsg,sizeof(szErrMsg)-1,&length);
    if ((SQL_SUCCESS != rc) && (SQL_SUCCESS_WITH_INFO != rc))
      break;
    szErrMsg[sizeof(szErrMsg)-1]='\0';
    len+= sprintf(db->errstr+len, "[SQLSTATE:%s]%s",szSqlState,szErrMsg);
  }
  return(0);
}


static int
UdmODBCTypeIsBinary(int Type)
{
  return  Type == SQL_BINARY ||
          Type == SQL_VARBINARY ||
          Type == SQL_LONGVARBINARY ||
          Type == -98/* DB2 SQL_BLOB */;
}


static int
execDB(UDM_DB*db,UDM_SQLRES *result, const char *sqlstr)
{
  RETCODE rc;
  SWORD iResColumns;
  SDWORD iRowCount;
  static char  bindbuf[(int)(64L * 1024L - 16L)];
  UDM_ODBC_CONN *sdb= (UDM_ODBC_CONN*) db->specific;

  if(!strcmp(sqlstr,"COMMIT"))
  {
    if (!SQL_OK(rc= SQLTransact(sdb->hEnv, sdb->hDbc, SQL_COMMIT)))
    {
      db->errcode= 1;
      return UDM_ERROR;
    }
    else
    {
      db->errcode= 0;
      return UDM_OK;
    }
  }

  if (!SQL_OK(rc= SQLAllocStmt(sdb->hDbc, &sdb->hstmt)))
  {
    db->errcode= 1;
    return UDM_ERROR;
  }

  if (!SQL_OK(rc= SQLExecDirect(sdb->hstmt,(SQLCHAR *)sqlstr, SQL_NTS)))
  {
    if(rc==SQL_NO_DATA) goto ND;
    db->errcode= 1;
    return UDM_ERROR; 
  }
  
  if (!SQL_OK(rc= SQLNumResultCols(sdb->hstmt, &iResColumns)))
  {
    db->errcode= 1;
    return UDM_ERROR;
  }
  
  if (!iResColumns)
  {
    if (!SQL_OK(rc= SQLRowCount(sdb->hstmt, &iRowCount)))
    {
      db->errcode= 1;
      return UDM_ERROR;
    }
  }
  else
  {
    int res_count, col;
    size_t mRows;

    UDM_ODBC_FIELD Field[128];

    result->nRows= 0;
    result->nCols= iResColumns;
    result->Items= NULL;

    result->Fields=(UDM_SQLFIELD*)UdmMalloc(result->nCols*sizeof(UDM_SQLFIELD));
    bzero(result->Fields, result->nCols*sizeof(UDM_SQLFIELD));

    for(col=0; col < iResColumns; col++)
    {
      SQLDescribeCol(sdb->hstmt, col+1,
                     Field[col].Name, 
                     (SQLSMALLINT) sizeof(Field[col].Name) - 1,
                     &Field[col].NameLen,
                     &Field[col].Type,
                     &Field[col].Size,
                     &Field[col].Scale,
                     &Field[col].Nullable);
      result->Fields[col].sqlname= (char*)UdmStrdup((const char*)Field[col].Name);
    }

    rc= SQL_NO_DATA_FOUND;
    for (mRows=0, res_count=0;(db->res_limit?(res_count<db->res_limit):1);res_count++)
    {
      int i;
      
      if (!SQL_OK(rc= SQLFetch(sdb->hstmt)))
      {
        if (rc != SQL_NO_DATA_FOUND)
        {
          db->errcode=1;
          if (db->DBType == UDM_DB_DB2) /* FIXME: check with all databases */
            return UDM_ERROR;
        }
        break;
      }
      if (mRows <= (result->nRows+1) * iResColumns)
      {
        mRows+= 1024 * iResColumns;
        result->Items=(UDM_PSTR*)UdmXrealloc(result->Items,mRows*sizeof(UDM_PSTR));
      }
    
      for (i = 0; i < iResColumns; i++)
      {
        size_t offs=result->nRows*iResColumns+i;
        const char *p;
        char *tmp= NULL;
        SDWORD  pcbValue;
        SWORD   get_type;
        int is_binary;

        if (Field[i].Type == SQL_FLOAT)
        {
          get_type= SQL_FLOAT;
        }
        if (Field[i].Type == SQL_DOUBLE)
        {
          get_type= SQL_C_DOUBLE;
        }
        else if ((is_binary= UdmODBCTypeIsBinary(Field[i].Type)))
        {
          /*
            PostgreSQL bytea
            MSSQL BLOB
            XTG Interbase/FireBird BLOB
            Oracle BLOB
            Sybase IMAGE & VARBINARY
          */
          if (db->DBType == UDM_DB_PGSQL ||
              db->DBType == UDM_DB_MSSQL ||
              db->DBType == UDM_DB_IBASE ||
              db->DBType == UDM_DB_ORACLE8 ||
              db->DBType == UDM_DB_SYBASE ||
              db->DBType == UDM_DB_DB2 ||
              db->DBType == UDM_DB_MYSQL)
            get_type= SQL_C_BINARY;
          /*
           SQLGetData as SQL_C_BINARY does not work with MonetDB:
          "Restricted data type attribute violation" error is returned
          
          else if (db->DBType == UDM_DB_MONETDB)
            get_type= SQL_C_BINARY;
          */
          else
            get_type= SQL_CHAR;
        }

#ifdef HAVE_UNIXODBC
        /*
           For some weird reasons, Sybase native (i.e. not FreeTDS)
           ODBC driver in conjunction with unixODBC always returns 4
           in pcbValue if get_type is SQL_CHAR. Let's fetch as SQL_C_SLONG.
        */
        else if (Field[i].Type == SQL_INTEGER && db->DBType == UDM_DB_SYBASE)
          get_type= SQL_C_SLONG;
#endif
        else
          get_type= SQL_CHAR;

        rc= SQLGetData(sdb->hstmt,i+1,get_type,bindbuf,sizeof(bindbuf),&pcbValue);
        if (rc == SQL_SUCCESS_WITH_INFO)
        {
          if (pcbValue > sizeof(bindbuf) &&
              (tmp= UdmMalloc(pcbValue)))
          {
            SDWORD tmp_pcbValue;
            memcpy(tmp, bindbuf, sizeof(bindbuf));
            rc= SQLGetData(sdb->hstmt,i+1,get_type,tmp+sizeof(bindbuf),pcbValue - sizeof(bindbuf),&tmp_pcbValue);
            p= tmp;
          }
          else
          {
            p= "";
            pcbValue= 0;
          }
        }
        else if (rc == SQL_SUCCESS)
        {
          if (pcbValue == SQL_NULL_DATA)
          {
            bindbuf[0]= '\0';
            pcbValue= 0;
          }
          else if (get_type == SQL_C_FLOAT)
          {
            float num;
            memcpy(&num, bindbuf, sizeof(num));
            pcbValue= sprintf(bindbuf, "%f", num);
          }
          else if (get_type == SQL_C_DOUBLE)
          {
            double num;
            memcpy(&num, bindbuf, sizeof(num));
            pcbValue= sprintf(bindbuf, "%f", num);
          }
          else if (get_type == SQL_C_SLONG)
          {
            long int num;
            memcpy(&num, bindbuf, sizeof(num));
            pcbValue= sprintf(bindbuf, "%ld", num);
          }
          p= bindbuf;
        }
        else if (rc == SQL_NO_DATA)
        {
          /* MSSQL returns this in case of an empty IMAGE value */
          /* Do nothing */
        }
        else if (is_binary && rc == SQL_ERROR && db->DBType == UDM_DB_MONETDB)
        {
          /* SQLGetData returns SQL_ERROR on empty BLOB for MonetDB */
          bindbuf[0]= '\0';
          pcbValue= 0;
        }
        else
        {
          db->errcode= 1;
          return UDM_ERROR;
        }


        if (get_type != SQL_C_BINARY &&
            UdmODBCTypeIsBinary(Field[i].Type) &&
            db->DBType != UDM_DB_MONETDB
            /*
              MonetDB does not encode binary values to HEX 
              when bind type is SQL_C_CHAR
            */
            )
        {
          DecodeHexStr(p, &result->Items[offs], pcbValue);
        }
        else
        {
          result->Items[offs].val= (char*)malloc(pcbValue+1);
          memcpy(result->Items[offs].val, p, pcbValue);
          result->Items[offs].val[pcbValue]='\0';
          result->Items[offs].len= pcbValue;
        }
        if (tmp)
          UdmFree(tmp);
      }
      result->nRows++;
    }

  }
ND:

  SQLFreeStmt(sdb->hstmt, SQL_DROP);
  db->res_limit=0;
  db->errcode=0;
  return UDM_OK;
}


static int
UdmODBCGetVersion(UDM_DB *db)
{
  UDM_ODBC_CONN *sdb= (UDM_ODBC_CONN*) db->specific;
  char version[128];
  SQLSMALLINT DummyLen;
  SQLRETURN rc= SQLGetInfo(sdb->hDbc, SQL_DBMS_VER,
                           version, sizeof(version), &DummyLen);
  if (SQL_OK(rc))
  {
    version[sizeof(version) - 1]= '\0';
    if (!strncmp(version, "PostgreSQL ", 11))
    {
      int a, b, c;
      if (3 == sscanf(version + 11, "%d.%d.%d", &a, &b, &c))
        db->version= a * 10000 + b * 100 + c;
    }
    return UDM_OK;
  }
  db->version= 0;
  return UDM_ERROR;
}


static int
UdmODBCConnect(UDM_DB *db)
{
  char DSN[512]="";
  const char* DBUser= UdmVarListFindStr(&db->Vars,"DBUser",NULL);
  const char* DBPass= UdmVarListFindStr(&db->Vars,"DBPass",NULL);
#if HAVE_SOLID || HAVE_SAPDB
  const char* DBHost= UdmVarListFindStr(&db->Vars, "DBHost", "localhost");
  int DBPort= UdmVarListFindInt(&db->Vars, "DBPort", 0);
#endif
  UDM_SQLRES SQLRes;
  UDM_ODBC_CONN *sdb= (UDM_ODBC_CONN*) UdmMalloc(sizeof(UDM_ODBC_CONN));
  db->specific= (void*) sdb;

  sdb->hDbc=  SQL_NULL_HDBC;
  sdb->hEnv=  SQL_NULL_HENV;
  sdb->hstmt= SQL_NULL_HSTMT;

#if (HAVE_SOLID)
  udm_snprintf(DSN, sizeof(DSN)-1, "tcp %s %d", DBHost, DBPort?DBPort:1313);
#elif (HAVE_SAPDB)
  udm_snprintf(DSN, sizeof(DSN)-1, "%s:%s", DBHost, db->DBName?db->DBName:"");
#else
  strncpy(DSN, db->DBName?db->DBName:"", sizeof(DSN)-1);
#endif
  
  if (SQL_SUCCESS != (db->errcode= SQLAllocEnv(&sdb->hEnv)) ||
      SQL_SUCCESS != (db->errcode= SQLAllocConnect(sdb->hEnv, &sdb->hDbc)) ||
      SQL_SUCCESS != (db->errcode= SQLSetConnectOption(sdb->hDbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_ON)) ||
      !SQL_OK(db->errcode = SQLConnect(sdb->hDbc, (SQLCHAR *)DSN, SQL_NTS, (SQLCHAR*) DBUser, SQL_NTS, (SQLCHAR*) DBPass, SQL_NTS)))
    return UDM_ERROR;

  db->errcode= 0;
  db->connected= 1;

  /*
  if (db->DBType == UDM_DB_DB2)
  {
    1253 = SQL_ATTR_LONGDATA_COMPAT
    int rc= SQLSetConnectAttr(sdb->hDbc, 1253, (SQLPOINTER) 1, 0);
  }
  */
  if (db->DBType == UDM_DB_ORACLE8)
    execDB(db, &SQLRes, "ALTER SESSION SET NLS_NUMERIC_CHARACTERS='. '");

  if (db->DBType == UDM_DB_MYSQL || db->DBType == UDM_DB_PGSQL)
  {
    const char* setnames= UdmVarListFindStr(&db->Vars, "setnames", NULL);
    if (setnames)
    {
      char qbuf[64];
      udm_snprintf(qbuf, sizeof(qbuf), "SET NAMES '%s'", setnames);
      if (UDM_OK != execDB(db, &SQLRes, qbuf))
        return UDM_ERROR;;
    }
  }
  
  UdmODBCGetVersion(db);
  return UDM_OK;
}


static void
UdmODBCClose(UDM_DB *db)
{
  if(db->connected)
  {
    UDM_ODBC_CONN *sdb= (UDM_ODBC_CONN*) db->specific;
    db->errcode= SQLTransact(sdb->hEnv, sdb->hDbc, SQL_COMMIT);
    if(SQL_SUCCESS != db->errcode)
      goto ret;
    db->errcode= SQLDisconnect(sdb->hDbc );
    if(SQL_SUCCESS != db->errcode)
      goto ret;
    db->errcode= SQLFreeConnect(sdb->hDbc);
    if(SQL_SUCCESS != db->errcode)
      goto ret;
    db->errcode= SQLFreeEnv(sdb->hEnv);
    if(SQL_SUCCESS != db->errcode)
      goto ret;
ret:
    db->connected= 0;
    UDM_FREE(db->specific);
  }
}


static int
UdmODBCExecDirect(UDM_DB *db,UDM_SQLRES *res, const char *qbuf)
{
  int rc;
   
  if (res)
  {
    bzero((void*) res, sizeof(UDM_SQLRES));
    res->db= db;
  }

  if (!db->connected)
  {
    UdmODBCConnect(db);
    if(db->errcode)
    {
      UdmODBCDisplayError(db,"");
      return UDM_ERROR;
    }
    else
    {
      db->connected=1;
    }
  }

  rc= execDB(db,res,qbuf);
  if((db->errcode))
  {
    UdmODBCDisplayError(db,"");
    if(strstr(db->errstr,"[SQLSTATE:23000]")) /* MS Access */
    {
      db->errcode=0;
      rc= UDM_OK;
    }
#if 0
    /* Cannot use S1000: it's returned by unixODBC in many cases */
    else if(strstr(db->errstr,"[SQLSTATE:S1000]"))
    { /* Oracle 8i */
      db->errcode=0;
      rc= UDM_OK;
    }
#endif
    else if(strstr(db->errstr,"uplicat")) /* PgSQL,MySQL*/
    {
      db->errcode=0;
      rc= UDM_OK;
    }
    else if(strstr(db->errstr,"nique")) /* Solid, Virtuoso */
    {
      db->errcode=0;
      rc= UDM_OK;
    }
    else if(strstr(db->errstr,"UNIQUE")) /* Mimer */
    {
      db->errcode=0;
      rc= UDM_OK;
    }
    else if (strstr(db->errstr, "PRIMARY KEY")) /* MonetDB */
    {
      db->errcode= 0;
      rc= UDM_OK;
    }
    else if (db->DBType == UDM_DB_MONETDB &&
             strstr(db->errstr, "General error"))
    {
      /*
        MonetDB returns "General error" in case of primary key
        violation in some cases.
      */
      fprintf(stderr, "MONETDB FIXME: %s\n%s\n", db->errstr, qbuf);
      db->errcode= 0;
      rc= UDM_OK;
    }
    else
    {
      db->errcode=1;
    }
    {
      UDM_ODBC_CONN *sdb= (UDM_ODBC_CONN*) db->specific;
      SQLFreeStmt(sdb->hstmt, SQL_DROP);
      sdb->hstmt= SQL_NULL_HSTMT;
    }
  }
  return rc;
}


static int
UdmODBCBegin(UDM_DB *db)
{
  UDM_ODBC_CONN *sdb;
  if(!db->connected)
  {
    UdmODBCConnect(db);
    if(db->errcode)
    {
      UdmODBCDisplayError(db,"");
      return UDM_ERROR;
    }
    else
    {
      db->connected=1;
    }
  }
  sdb= (UDM_ODBC_CONN*) db->specific;
  db->errcode= SQLSetConnectOption(sdb->hDbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF);
  if(SQL_SUCCESS != db->errcode)
  {
    sdb->hstmt= SQL_NULL_HSTMT;
    UdmODBCDisplayError(db,"SQLSetConnectOption(SQL_AUTOCOMMIT_OFF) failed");
    return UDM_ERROR;
  }
  return UDM_OK;
}


static int
UdmODBCCommit(UDM_DB *db)
{
  UDM_ODBC_CONN *sdb= (UDM_ODBC_CONN*) db->specific;
  int rc;
  if (UDM_OK!=(rc=UdmODBCExecDirect(db,NULL,"COMMIT")))
    return UDM_ERROR;
  if (!SQL_OK(db->errcode= SQLSetConnectOption(sdb->hDbc,
                                               SQL_AUTOCOMMIT,
                                               SQL_AUTOCOMMIT_ON)))
  {
    sdb->hstmt= SQL_NULL_HSTMT;
    UdmODBCDisplayError(db,"SQLSetConnectOption(SQL_AUTOCOMMIT_ON) failed");
    return UDM_ERROR;
  }
  return UDM_OK;
}


static int
UdmODBCPrepare(UDM_DB *db, const char *query)
{
  UDM_ODBC_CONN *sdb= (UDM_ODBC_CONN*) db->specific;
  int rc;
  db->errcode = 0;
  db->errstr[0] = 0;
  rc= SQLAllocStmt(sdb->hDbc, &sdb->hstmt);

  if (!SQL_OK(rc))
  {
    db->errcode = rc;
    UdmODBCDisplayError(db,"UdmODBCPrepare: ");
    return(UDM_ERROR);
  }
  rc= SQLPrepare(sdb->hstmt, (SQLCHAR*) query, SQL_NTS);
  if (!SQL_OK(rc))
  {
    db->errcode = rc;
    UdmODBCDisplayError(db, "UdmODBCPrepare: ");
    return(UDM_ERROR);
  }
  return(UDM_OK);
}


static int
UdmSQLType2ODBCType(int udm_type)
{
  switch (udm_type)
  {
    case UDM_SQLTYPE_LONGVARBINARY: return SQL_LONGVARBINARY;
    case UDM_SQLTYPE_LONGVARCHAR  : return SQL_LONGVARCHAR;
    case UDM_SQLTYPE_VARCHAR      : return SQL_VARCHAR;
    case UDM_SQLTYPE_INT32        : return SQL_INTEGER;
    default                       : break;
  }
  return SQL_UNKNOWN_TYPE;
}


static int
UdmSQLParamLen2ODBC(int udm_len)
{
  switch (udm_len)
  {
    case UDM_SQL_NULL_DATA    : return SQL_NULL_DATA;
    case UDM_SQL_DATA_AT_EXEC : return SQL_DATA_AT_EXEC;
    case UDM_SQL_NTS          : return SQL_NTS;
  }
  return udm_len;
}


static int
UdmODBCBind(UDM_DB *db, int position, const void *data, int size, int type)
{
  UDM_ODBC_CONN *sdb= (UDM_ODBC_CONN*) db->specific;
  int rc;
  BindBuf[position].size= UdmSQLParamLen2ODBC(size);
  BindBuf[position].data= (char*) data;
  
  type= UdmSQLType2ODBCType(type);
  db->errcode = 0;
  db->errstr[0] = 0;

  rc= SQLBindParameter(sdb->hstmt, position, SQL_PARAM_INPUT, SQL_C_DEFAULT,
                       type, 0 /* Column size */, 0 /* Decimal digits */,
                       /*&BindBuf[position]*/ (char*) data, size < 0 ? 0 : size,
                       &BindBuf[position].size);

#ifdef WIN32  
/* s_size= SQL_DATA_AT_EXEC; */
#endif

  if (!SQL_OK(rc))
  {
    db->errcode= rc;
    UdmODBCDisplayError(db, "UdmOBDCBind: ");
    return(UDM_ERROR);
  }
  return(UDM_OK);
}


static int
UdmODBCStmtFree(UDM_DB *db)
{
  UDM_ODBC_CONN *sdb= (UDM_ODBC_CONN*) db->specific;
  int rc= SQLFreeStmt(sdb->hstmt, SQL_DROP);
  return SQL_OK(rc) ? UDM_OK : UDM_ERROR;
}


static int
UdmODBCExec(UDM_DB *db)
{
  UDM_ODBC_CONN *sdb= (UDM_ODBC_CONN*) db->specific;
  int rc, put_data= 0;
  UDM_ODBC_BINDBUF *Buf;

  db->errcode = 0;
  db->errstr[0] = 0;
  rc= SQLExecute(sdb->hstmt);

#ifdef WIN32
  put_data= 1;
#endif

  if (!put_data || rc != SQL_NEED_DATA)
    goto check_err;

  for (rc= SQLParamData(sdb->hstmt, (void*)&Buf);
       rc == SQL_NEED_DATA;
       rc= SQLParamData(sdb->hstmt, (void*)&Buf))
  {
    if (SQL_ERROR == (rc= SQLPutData(sdb->hstmt, Buf->data, Buf->size)))
      goto check_err;
  }

check_err:

  if (!SQL_OK(rc))
  {
    UdmODBCDisplayError(db, "UdmODBCExec: ");
    db->errcode= rc;
    return UDM_ERROR;
  }
  return UDM_OK;
}


static int
UdmODBCRenameTable(UDM_DB *db, const char *from, const char *to)
{
  if (db->DBType == UDM_DB_MYSQL ||
      db->DBType == UDM_DB_PGSQL ||
      db->DBType == UDM_DB_ORACLE8)
  {
    char buf[256];
    udm_snprintf(buf, sizeof(buf), "ALTER TABLE %s RENAME TO %s", from, to);
    return UdmSQLExecDirect(db, NULL, buf);
  }
  else if (db->DBType == UDM_DB_DB2)
  {
    char buf[256];
    udm_snprintf(buf, sizeof(buf), "RENAME TABLE %s TO %s", from, to);
    return UdmSQLExecDirect(db, NULL, buf);
  }
  else if (db->DBType == UDM_DB_MSSQL)
  {
    char buf[256];
    udm_snprintf(buf, sizeof(buf), "sp_rename %s, %s", from, to);
    return UdmSQLExecDirect(db, NULL, buf);
  }
  sprintf(db->errstr, "This database type does not support RENAME TABLE");
  return UDM_ERROR;
}


/*
  Copy table structure from another table, without indexes.
*/
static int
UdmODBCCopyStructure(UDM_DB *db, const char *from, const char *to)
{
  char buf[256];
  switch (db->DBType)
  {
    case UDM_DB_MYSQL:
      udm_snprintf(buf, sizeof(buf),
                   "CREATE TABLE %s MAX_ROWS=300000000 AVG_ROW_LENGTH=512 "
                   "SELECT * FROM %s LIMIT 0", to, from);
      return UdmSQLExecDirect(db, NULL, buf);

    case UDM_DB_PGSQL:
      udm_snprintf(buf, sizeof(buf), "CREATE TABLE %s (LIKE %s)", to, from);
      return UdmSQLExecDirect(db, NULL, buf);

    case UDM_DB_ORACLE8:
      udm_snprintf(buf, sizeof(buf),
                   "CREATE TABLE %s AS SELECT * FROM %s WHERE 1=0", to, from);
      return UdmSQLExecDirect(db, NULL, buf);

    case UDM_DB_DB2:
      udm_snprintf(buf, sizeof(buf), "CREATE TABLE %s LIKE %s", to, from);
      return UdmSQLExecDirect(db, NULL, buf);

    case UDM_DB_MSSQL:
      udm_snprintf(buf, sizeof(buf), "SELECT TOP 0 * INTO %s FROM %s", to, from);
      return UdmSQLExecDirect(db, NULL, buf);
    default: break;
  }
  sprintf(db->errstr, "This database type does not support"
          "<CREATE TABLE name LIKE> or <CREATE TABLE name AS SELECT>");
  return UDM_ERROR;
}


static int
UdmODBCLockOrBegin(UDM_DB *db, const char *param)
{
  if (db->DBType == UDM_DB_MYSQL)
  {
    char buf[128];
    udm_snprintf(buf, sizeof(buf), "LOCK TABLE %s", param);
    return UdmSQLExecDirect(db, NULL, buf);
  }
  return UdmSQLBegin(db);
}


static int
UdmODBCUnlockOrCommit(UDM_DB *db)
{
  if (db->DBType == UDM_DB_MYSQL)
    return UdmSQLExecDirect(db, NULL, "UNLOCK TABLES");
  else
    return UdmSQLCommit(db);
}


UDM_SQLDB_HANDLER udm_sqldb_odbc_handler =
{
  UdmSQLEscStrGeneric,
  UdmODBCExecDirect,
  UdmODBCConnect,
  UdmODBCClose,
  UdmODBCBegin,
  UdmODBCCommit,
  UdmODBCPrepare,
  UdmODBCBind,
  UdmODBCExec,
  UdmODBCStmtFree,
  UdmSQLFetchRowSimple,
  UdmSQLStoreResultSimple,
  UdmSQLFreeResultSimple,
  UdmODBCExecDirect,
  UdmODBCRenameTable,
  UdmODBCCopyStructure,
  UdmODBCLockOrBegin,
  UdmODBCUnlockOrCommit,
};

#endif /* HAVE_ODBC */

#endif /* HAVE_SQL */

