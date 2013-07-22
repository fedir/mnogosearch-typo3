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

#if   (HAVE_MYSQL)
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
#include <mysql.h>

#define ER_DUP_ENTRY            1062
#define ER_DUP_KEY		1022
#define CR_SERVER_LOST          2013
#define CR_SERVER_GONE_ERROR    2006
#define ER_SERVER_SHUTDOWN      1053
#define ER_DUP_ENTRY_WITH_KEY_NAME 1582

#if (MYSQL_VERSION_ID > 40100)
#define HAVE_MYSQL_PS 1
#endif


/*
  Return MySQL version as number: 4.1.24 -> 40124
  This is a replacement for mysql_get_server_version()
  which appeared in 4.1.x
*/
static int
udm_mysql_version(MYSQL *mysql)
{
  int v1, v2, v3;
  char *pos= mysql->server_version, *end;
  v1=   strtol(pos, &end, 10); pos= end + 1;
  v2=   strtol(pos, &end, 10); pos= end + 1;
  v3= strtol(pos, &end, 10);
  return v1 * 10000 + v2 * 100 + v3;
}

#define UDM_MYSQL_MAX_BIND_PARAM UDM_SQL_MAX_BIND_PARAM

typedef struct udm_mysql_st
{
  MYSQL mysql;
#ifdef HAVE_MYSQL_PS
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[UDM_MYSQL_MAX_BIND_PARAM];
  unsigned long bind_length[UDM_MYSQL_MAX_BIND_PARAM];
#endif  
} UDM_MYSQL;


static int
UdmMySQLConnect(UDM_DB *db)
{
  const char* DBLog=  UdmVarListFindStr(&db->Vars,"sqllog",NULL);
  const char* DBLogBin= UdmVarListFindStr(&db->Vars,"sqllogbin",NULL);
  const char* DBSock= UdmVarListFindStr(&db->Vars,"socket",NULL);
  const char* DBUser= UdmVarListFindStr(&db->Vars,"DBUser",NULL);
  const char* DBPass= UdmVarListFindStr(&db->Vars,"DBPass",NULL);
  const char* DBHost= UdmVarListFindStr(&db->Vars, "DBHost", "localhost");
  const char* mycnfgroup= UdmVarListFindStr(&db->Vars, "mycnfgroup", "client");
  int DBPort= UdmVarListFindInt(&db->Vars, "DBPort", 0);
  const char* setnames=  UdmVarListFindStr(&db->Vars,"setnames", "");
  int opt_compress= UdmVarListFindBool(&db->Vars, "compress", 0);
  int opt_client_multi_statements= UdmVarListFindBool(&db->Vars, "ClientMultiStatements", 0);
  UDM_MYSQL *mydb= (UDM_MYSQL*) UdmMalloc(sizeof(UDM_MYSQL));
  unsigned long client_flag= 0;
  
  if (!(db->specific= mydb))
  {
    db->errcode= 1;
    sprintf(db->errstr, "Can't alloc mydb. Not enough memory?");
    return UDM_ERROR;
  }
  
  bzero((void*) mydb, sizeof(UDM_MYSQL));
  
  if (!mysql_init(&mydb->mysql))
  {
    db->errcode= 1;
    sprintf(db->errstr, "mysql_init() failed. Not enough memory?");
    return UDM_ERROR;
  }

  if (strcasecmp(mycnfgroup, "no"))
    mysql_options(&mydb->mysql, MYSQL_READ_DEFAULT_GROUP, mycnfgroup);
  
  if (opt_compress)
    mysql_options(&mydb->mysql, MYSQL_OPT_COMPRESS, 0);

#ifdef CLIENT_MULTI_STATEMENTS
  client_flag |= opt_client_multi_statements ? CLIENT_MULTI_STATEMENTS : 0;
#endif

  if(!(mysql_real_connect(&mydb->mysql, DBHost, DBUser, DBPass,
  			  db->DBName?db->DBName:"mnogosearch",
  			  (unsigned)DBPort, DBSock, client_flag)))
  {
    db->errcode=1;
    sprintf(db->errstr, "MySQL driver: #%d: %s",
    	    mysql_errno(&mydb->mysql), mysql_error(&mydb->mysql));
    return UDM_ERROR;
  }
  db->connected=1;
  db->version= udm_mysql_version(&mydb->mysql);
  if (db->version >= 40100)
  {
    db->flags|= UDM_SQL_HAVE_SUBSELECT;
    if (setnames[0])
    {
      int rc;
#if  (MYSQL_VERSION_ID > 40100)
      rc= mysql_set_character_set(&mydb->mysql, setnames);
#else
      {
        char qbuf[64];
        udm_snprintf(qbuf, sizeof(qbuf), "SET NAMES %s", setnames);
        rc= mysql_query(&mydb->mysql, qbuf);
      }
#endif
      if (rc)
      {
        db->errcode= 1;
        sprintf(db->errstr,"MySQL driver: in SET NAMES: #%d: %s",
                mysql_errno(&mydb->mysql), mysql_error(&mydb->mysql));
        return UDM_ERROR;
      }
    }
  }
  else
  {
#ifdef HAVE_MYSQL_PS
    /* Switch from native to general PS API */
    if (db->sql->SQLPrepare)
    {
      db->sql->SQLPrepare=  UdmSQLPrepareGeneric;
      db->sql->SQLBind=     UdmSQLBindGeneric;
      db->sql->SQLExec=     UdmSQLExecGeneric;
      db->sql->SQLStmtFree= UdmSQLStmtFreeGeneric;
    }
#endif
  }
  
  if (DBLog)
  {
    char qbuf[64];
    sprintf(qbuf, "SET SQL_LOG_OFF=%d", atoi(DBLog) ? 0 : 1);
    if (mysql_query(&mydb->mysql, qbuf))
    {
      db->errcode=1;
      sprintf(db->errstr,"MySQL driver: in SET SQL_LOG_OFF: #%d: %s",
              mysql_errno(&mydb->mysql), mysql_error(&mydb->mysql));
      return UDM_ERROR;
    }
  }
  if (DBLogBin)
  {
    char qbuf[64];
    sprintf(qbuf, "SET SQL_LOG_BIN=%d", atoi(DBLogBin) ? 1 : 0);
    if (mysql_query(&mydb->mysql, qbuf))
    {
      db->errcode=1;
      sprintf(db->errstr,"MySQL driver: in SET SQL_LOG_BIN: #%d: %s",
              mysql_errno(&mydb->mysql), mysql_error(&mydb->mysql));
      return UDM_ERROR;
    }
  }
  return UDM_OK;
}


static
int UdmMySQLStoreResult(UDM_DB *db, UDM_SQLRES *R)
{
  size_t mitems= 0;
  MYSQL_ROW mysqlrow;
  
  while((mysqlrow=mysql_fetch_row((MYSQL_RES *)R->specific)))
  {
    size_t col;
    size_t coloffs= R->nRows * R->nCols;  
    unsigned long  *lengths= mysql_fetch_lengths((MYSQL_RES *)R->specific);
    
    if (coloffs + R->nCols >= mitems)
    {
      mitems= mitems ? mitems * 8 : 256;
      R->Items=(UDM_PSTR*)UdmRealloc(R->Items,mitems*sizeof(UDM_PSTR));
    }
    
    for(col= 0; col < R->nCols ; col++)
    {
      UDM_PSTR *I= &R->Items[coloffs + col];
      size_t len;
      len= I->len= lengths[col];
      I->val= (char*) UdmMalloc(len+1);
      memcpy(I->val, mysqlrow[col], len);
      I->val[len]='\0';
    }
    R->nRows++;
  }
  return UDM_OK;
}


static int
UdmMySQLStoreMetaData(UDM_DB *db, UDM_SQLRES *R)
{
  if(R->specific)
  {
    MYSQL_FIELD  *field;
    size_t    nfields;
    
    R->nCols= mysql_num_fields((MYSQL_RES*)R->specific);
    R->nRows=0;
    R->Items=NULL;
    R->Fields=(UDM_SQLFIELD*)UdmMalloc(R->nCols*sizeof(UDM_SQLFIELD));
    bzero(R->Fields,R->nCols*sizeof(UDM_SQLFIELD));
    
    for(nfields=0; (field=mysql_fetch_field((MYSQL_RES *)R->specific)); nfields++)
    {
      R->Fields[nfields].sqlname = (char*)UdmStrdup(field->name);
      R->Fields[nfields].sqllen=field->length;
    }
  }
  return UDM_OK;
}


static int
UdmMySQLExecDirect(UDM_DB *db,UDM_SQLRES *R,const char *query)
{
  size_t  i;
  UDM_MYSQL *mydb;
  MYSQL *mysql;
  
  db->errcode=0;
  
  if (R)
  {
    bzero((void*) R, sizeof(UDM_SQLRES));
    R->db= db;
  }

  if (!db->connected)
  {
    int rc;
    if (UDM_OK != (rc= UdmMySQLConnect(db)))
      return rc;
  }

  mydb= (UDM_MYSQL*) db->specific;
  mysql= &mydb->mysql;

  for(i=0;i<2;i++)
  {
    if((mysql_query(mysql,query)))
    {
      if((mysql_errno(mysql)==CR_SERVER_LOST)||
         (mysql_errno(mysql)==CR_SERVER_GONE_ERROR)||
         (mysql_errno(mysql)==ER_SERVER_SHUTDOWN))
      {
        UDMSLEEP(5);
      }
      else
      {
        sprintf(db->errstr,"MySQL driver: #%d: %s",
                mysql_errno(mysql),mysql_error(mysql));
        if((mysql_errno(mysql)!=ER_DUP_ENTRY) &&
           (mysql_errno(mysql)!=ER_DUP_KEY) &&
           (mysql_errno(mysql)!=ER_DUP_ENTRY_WITH_KEY_NAME))
        {
          db->errcode=1;
          return UDM_ERROR;
        }
        db->errcode=0;
        return UDM_OK;
      }
    }
    else
    {
      if (R)
      {
        R->specific= mysql_use_result((MYSQL*) db->specific);
        return UdmMySQLStoreMetaData(db, R);
      }
      else
        return UDM_OK;
    }
  }
  db->errcode=1;
  sprintf(db->errstr,"MySQL driver: #%d: %s",
          mysql_errno(mysql),mysql_error(mysql));
  return UDM_ERROR;
}


static int
UdmMySQLQuery(UDM_DB *db,UDM_SQLRES *R,const char *query)
{
  int rc= UdmMySQLExecDirect(db, R, query);
  if (rc != UDM_OK)
    return rc;

  if (!R->specific)
    return UDM_OK;
  
  rc= UdmMySQLStoreResult(db, R);

#if (MYSQL_VERSION_ID > 40100)
  {
    UDM_MYSQL *mydb= (UDM_MYSQL*) db->specific;
    while (mysql_next_result(db->specific) == 0)
    {
      MYSQL_RES *res;
      if ((res= mysql_store_result(&mydb->mysql)))
      {
        mysql_free_result(res);
      }
      else
      {
        /* Non-select query */
      }
    }
  }
#endif

  return rc;
}


static void
UdmMySQLClose(UDM_DB *db)
{
  UDM_MYSQL *mydb= (UDM_MYSQL*) db->specific;
  MYSQL *mysql= &mydb->mysql;
  mysql_close(mysql);
  UdmFree(db->specific);
  db->specific = NULL;
}


static int
UdmMySQLBegin(UDM_DB *db)
{
  return UDM_OK;
}


static int
UdmMySQLCommit(UDM_DB *db)
{
  return UDM_OK;
}


static size_t
UdmMySQLEscStr(UDM_DB *db, char *to, const char *from, size_t len)
{
  return mysql_escape_string(to, from, len);
}


static int
UdmMySQLFreeResult(UDM_DB *db, UDM_SQLRES *R)
{
  UdmSQLFreeResultSimple(db, R);
  if (R->specific)
  {
    mysql_free_result((MYSQL_RES *)R->specific);
    R->specific= NULL;
  }
  return UDM_OK;
}


static int
UdmMySQLFetchRow(UDM_DB *db, UDM_SQLRES *R, UDM_PSTR *buf)
{
  size_t j;
  MYSQL_ROW row;
  unsigned long *lengths;
  if (!(row= mysql_fetch_row((MYSQL_RES *)R->specific)))
    return UDM_ERROR;
  lengths= mysql_fetch_lengths((MYSQL_RES *)R->specific);
  
  for (j = 0; j < R->nCols; j++)
  {
    buf[j].val= row[j];
    buf[j].len= lengths[j];
  }
  
  return(UDM_OK);
}


#ifdef HAVE_MYSQL_PS
static int
UdmMySQLPrepare(UDM_DB *db, const char *query)
{
  UDM_MYSQL *mydb= (UDM_MYSQL*) db->specific;
  db->errcode= 0;
  db->errstr[0]= '\0';
  if (!(mydb->stmt= mysql_stmt_init(&mydb->mysql)))
  {
    db->errcode= 1;
    sprintf(db->errstr, "mysql_stmt_init() failed. Out of memory?");
    return UDM_ERROR;
  }
  if (mysql_stmt_prepare(mydb->stmt, query, strlen(query)))
  {
    db->errcode= 1;
    udm_snprintf(db->errstr, sizeof(db->errstr), "mysql_stmt_prepare failed: %s", mysql_stmt_error(mydb->stmt));
    return UDM_ERROR;
  }
  return UDM_OK;
}


static inline int
UdmSQLType2MySQLType(int type)
{
  return type == UDM_SQLTYPE_INT32 ? MYSQL_TYPE_LONG : MYSQL_TYPE_STRING;
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
static int
UdmMySQLBind(UDM_DB *db, int position, const void *data, int size, int type)
{
  UDM_MYSQL *mydb= (UDM_MYSQL*) db->specific;
  MYSQL_BIND *b= &mydb->bind[position - 1];
  b->buffer_type= UdmSQLType2MySQLType(type);
  b->buffer= (char*) data;
  b->is_null= 0;
  b->length= &mydb->bind_length[position - 1];
  b->length[0]= (unsigned long) size;
  return UDM_OK;
}
#pragma GCC diagnostic pop


static int
UdmMySQLStmtFree(UDM_DB *db)
{
  UDM_MYSQL *mydb= (UDM_MYSQL*) db->specific;
  if (mysql_stmt_close(mydb->stmt))
  {
    db->errcode= 1;
    udm_snprintf(db->errstr, sizeof(db->errstr),
                 "mysql_stmt_close() failed: %s",
                 mysql_stmt_error(mydb->stmt));
    return UDM_ERROR;
  }
  return UDM_OK;
}


static int
UdmMySQLExec(UDM_DB *db)
{
  UDM_MYSQL *mydb= (UDM_MYSQL*) db->specific;

  if (mysql_stmt_bind_param(mydb->stmt, mydb->bind))
  {
    db->errcode= 1;
    udm_snprintf(db->errstr, sizeof(db->errstr),
                 "mysql_stmt_bind_param() failed: %s",
                 mysql_stmt_error(mydb->stmt));
    return UDM_ERROR;
  }
  
  if (mysql_stmt_execute(mydb->stmt))
  {
    db->errcode= 1;
    udm_snprintf(db->errstr, sizeof(db->errstr),
                 "mysql_stmt_execute() failed: %s",
                 mysql_stmt_error(mydb->stmt));
    return UDM_ERROR;
  }

  return UDM_OK;
}
#endif


static int
UdmMySQLRenameTable(UDM_DB *db, const char *from, const char *to)
{
  char buf[256];
  udm_snprintf(buf, sizeof(buf), "ALTER TABLE %s RENAME TO %s", from, to);
  return UdmSQLExecDirect(db, NULL, buf);
}


static int
UdmMySQLCopyStructure(UDM_DB *db, const char *from, const char *to)
{
  char buf[256];
  udm_snprintf(buf, sizeof(buf),
              "CREATE TABLE %s ENGINE=MyISAM MAX_ROWS=300000000 AVG_ROW_LENGTH=512 "
              "SELECT * FROM %s LIMIT 0", to, from);
  return UdmSQLExecDirect(db, NULL, buf);
}


static int
UdmMySQLLockOrBegin(UDM_DB *db, const char *param)
{
  char buf[128];
  udm_snprintf(buf, sizeof(buf), "LOCK TABLE %s", param);
  return UdmSQLExecDirect(db, NULL, buf);
}


static int
UdmMySQLUnlockOrCommit(UDM_DB *db)
{
  return UdmSQLExecDirect(db, NULL, "UNLOCK TABLES");
}


UDM_SQLDB_HANDLER udm_sqldb_mysql_handler =
{
  UdmMySQLEscStr,
  UdmMySQLQuery,
  UdmMySQLConnect,
  UdmMySQLClose,
  UdmMySQLBegin,
  UdmMySQLCommit,
#ifdef HAVE_MYSQL_PS
  UdmMySQLPrepare,
  UdmMySQLBind,
  UdmMySQLExec,
  UdmMySQLStmtFree,
#else
  UdmSQLPrepareGeneric,
  UdmSQLBindGeneric,
  UdmSQLExecGeneric,
  UdmSQLStmtFreeGeneric,
#endif
  UdmMySQLFetchRow,
  UdmSQLStoreResultSimple,
  UdmMySQLFreeResult,
  UdmMySQLExecDirect,
  UdmMySQLRenameTable,
  UdmMySQLCopyStructure,
  UdmMySQLLockOrBegin,
  UdmMySQLUnlockOrCommit,
};
#endif
