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


#if HAVE_SQLITE
#include <sqlite.h>
#endif


#if HAVE_SQLITE3
#ifdef WIN32
#define HAVE_SQLITE3_AMALGAMATION
#endif
#ifdef HAVE_SQLITE3_AMALGAMATION
#define SQLITE_OMIT_LOAD_EXTENSION
#ifndef HAVE_DEBUG
#define NDEBUG 1
#endif
#include "sqlite3.c"
#else
#include <sqlite3.h>
#endif
#endif


#ifdef WIN32
#include <process.h>
#endif

/*********************************** SQLITE *********************/

#if (HAVE_SQLITE)

static int
UdmSQLiteConnect(UDM_DB *db)
{
  char edbname[1024];
  char dbname[1024], *e, *errmsg;
  strncpy(edbname,db->DBName,sizeof(dbname));
  dbname[sizeof(edbname)-1]='\0';
  
  UdmUnescapeCGIQuery(dbname,edbname);
  /* Remove possible trailing slash */
  e= dbname+strlen(dbname);
  if (e>dbname && e[-1]=='/')
    e[-1]='\0';
  
  if (!(db->specific= (void*) sqlite_open(dbname, 0, &errmsg)))
  {
    sprintf(db->errstr, "sqlite driver: %s", errmsg ? errmsg : "<NOERROR>");
    UDM_FREE(errmsg);
    db->errcode=1;
    return UDM_ERROR;
  }
  db->connected=1;
  sqlite_busy_timeout((struct sqlite*)(db->specific),30*1000);
  return UDM_OK;
}


static int
xCallBack(void *pArg, int argc, char **argv, char **name)
{
  UDM_SQLRES *res= (UDM_SQLRES*) pArg;
  int i;

  if (res->nCols == 0)
  {
    res->nCols= argc;
    for (i=0 ; i<argc; i++)
    {
      res->Fields=(UDM_SQLFIELD*)UdmMalloc(res->nCols*sizeof(UDM_SQLFIELD));
      bzero(res->Fields,res->nCols*sizeof(UDM_SQLFIELD));
      for (i=0 ; i < res->nCols; i++)
      {
        res->Fields[i].sqlname = (char*)UdmStrdup(name[i]);
        res->Fields[i].sqllen= 0;
        res->Fields[i].sqltype= 0;
      }
    }
  }
  res->nRows++;
  res->Items=(UDM_PSTR*)UdmRealloc(res->Items,res->nRows*res->nCols*sizeof(UDM_PSTR));
  
  for (i = 0; i < argc; i++)
  {
    size_t offs= (res->nRows-1)*res->nCols+(size_t)i;
    size_t len= strlen(argv[i]?argv[i]:"");
    res->Items[offs].len= len;
    res->Items[offs].val= (char*)UdmMalloc(len+1);
    memcpy(res->Items[offs].val,argv[i]?argv[i]:"",len+1);
  } 
  return 0;
}


static int
UdmSQLiteQuery(UDM_DB *db, UDM_SQLRES *res, const char *q)
{
  char *errmsg;
  int rc;
  
  if (res)
  {
    bzero((void*) res, sizeof(UDM_SQLRES));
    res->db= db;
  }
  
#ifdef WIN32
  /** Remove heading "/" from DBName (in case of absolute path) */
  if(db->DBName[2] == ':')
  {
    char *tmp_dbname = UdmStrdup(db->DBName + 1);
    UDM_FREE(db->DBName);
    db->DBName = tmp_dbname;
  }
#endif /* #ifdef WIN32 */

  db->errcode=0;
  db->errstr[0]='\0';
  if(!db->connected && UDM_OK != UdmSQLiteConnect(db))
    return UDM_ERROR;
  
  if ((rc= sqlite_exec((struct sqlite *)db->specific, q, xCallBack, (void*)res, &errmsg)))
  {
    sprintf(db->errstr,"sqlite driver: %s",errmsg ? errmsg : "<NOERROR>");
    sqlite_freemem(errmsg);
    if (!strstr(db->errstr,"unique"))
    {
      db->errcode=1;
      return UDM_ERROR;
    }
  }
  return UDM_OK;
}


static void
UdmSQLiteClose(UDM_DB *db)
{
  sqlite_close((struct sqlite*)(db->specific));
  db->specific = NULL;
}


static int
UdmSQLiteBegin(UDM_DB *db)
{
  return UdmSQLiteQuery(db,NULL,"BEGIN TRANSACTION");
}


static int
UdmSQLiteCommit(UDM_DB *db)
{
  return UdmSQLiteQuery(db,NULL,"COMMIT");
}


UDM_SQLDB_HANDLER udm_sqldb_sqlite_handler =
{
  UdmSQLEscStrGeneric,
  UdmSQLiteQuery,
  UdmSQLiteConnect,
  UdmSQLiteClose,
  UdmSQLiteBegin,
  UdmSQLiteCommit,
  UdmSQLPrepareGeneric,
  UdmSQLBindGeneric,
  UdmSQLExecGeneric,
  UdmSQLStmtFreeGeneric,
  UdmSQLFetchRowSimple,
  UdmSQLStoreResultSimple,
  UdmSQLFreeResultSimple,
  UdmSQLiteQuery,
  NULL, /* RenameTable */
  UdmSQLLockOrBeginGeneric,
  UdmSQLUnlockOrCommitGeneric,
};

#endif  /* HAVE_SQLITE */


#if (HAVE_SQLITE3)


static int
UdmSQLite3Connect(UDM_DB *db)
{
  sqlite3 *ppDb;
  char edbname[1024];
  char dbname[1024], *e;
  strncpy(edbname,db->DBName,sizeof(dbname));
  dbname[sizeof(edbname)-1]='\0';
  
  UdmUnescapeCGIQuery(dbname,edbname);
  /* Remove possible trailing slash */
  e= dbname+strlen(dbname);
  if (e>dbname && e[-1]=='/')
    e[-1]='\0';
  
  if (SQLITE_OK != sqlite3_open(dbname, &ppDb))
  {
    const char *errmsg= sqlite3_errmsg(ppDb);
    udm_snprintf(db->errstr, sizeof(db->errstr),
                "sqlite3 driver: %s", errmsg ? errmsg : "<NOERROR>");
    db->errcode= 1;
    sqlite3_close(ppDb);
    return UDM_ERROR;
  }
  db->specific= (void*) ppDb;
  db->connected= 1;
  sqlite3_busy_timeout((sqlite3*)(db->specific),30*1000);
  return UDM_OK;
}


#if 0
static int SQLite3CallBack(void *pArg, int argc, char **argv, char **name)
{
  UDM_SQLRES *res= (UDM_SQLRES*) pArg;
  int i;

  if (res->nCols == 0)
  {
    res->nCols= argc;
    for (i=0 ; i<argc; i++)
    {
      res->Fields=(UDM_SQLFIELD*)UdmMalloc(res->nCols*sizeof(UDM_SQLFIELD));
      bzero(res->Fields,res->nCols*sizeof(UDM_SQLFIELD));
      for (i=0 ; i < res->nCols; i++)
      {
        res->Fields[i].sqlname = (char*)UdmStrdup(name[i]);
        res->Fields[i].sqllen= 0;
        res->Fields[i].sqltype= 0;
      }
    }
  }

  res->nRows++;
  res->Items=(UDM_PSTR*)UdmRealloc(res->Items,res->nRows*res->nCols*sizeof(UDM_PSTR));
  
  for (i = 0; i < argc; i++)
  {
    size_t offs= (res->nRows-1)*res->nCols+(size_t)i;
    size_t len= strlen(argv[i]?argv[i]:"");
    res->Items[offs].len= len;
    res->Items[offs].val= (char*)UdmMalloc(len+1);
    memcpy(res->Items[offs].val,argv[i]?argv[i]:"",len+1);
  } 
  return 0;
}
#endif


static int
UdmSQLite3Query(UDM_DB *db, UDM_SQLRES *res, const char *q)
{
  int rc;
  sqlite3_stmt *pStmt;
  const char *Tail;
  int columns= 0;
  
  if (res)
  {
    bzero((void*) res, sizeof(UDM_SQLRES));
    res->db= db;
  }
  
#ifdef WIN32
  /** Remove heading "/" from DBName (in case of absolute path) */
  if(db->DBName[2] == ':')
  {
    char *tmp_dbname= UdmStrdup(db->DBName + 1);
    UDM_FREE(db->DBName);
    db->DBName= tmp_dbname;
  }
#endif /* #ifdef WIN32 */

  db->errcode= 0;
  db->errstr[0]= '\0';

  if (!db->connected && UDM_OK != UdmSQLite3Connect(db))
    return UDM_ERROR;

  if ((rc= sqlite3_prepare((sqlite3*) db->specific, q, -1, &pStmt, &Tail)))
  {
    udm_snprintf(db->errstr, sizeof(db->errstr),
                 "sqlite3 driver: (%d) %s", 
                 sqlite3_errcode((sqlite3*) db->specific),
                 sqlite3_errmsg((sqlite3*) db->specific));
    db->errcode= 1;
    return UDM_ERROR;
  }
  
  for (rc= sqlite3_step(pStmt) ; ; rc= sqlite3_step(pStmt))
  {
    int i;
    switch (rc)
    {
      case SQLITE_DONE:
        goto fin;

      case SQLITE_ROW:
        if (!columns)
        {
          columns= sqlite3_column_count(pStmt);
          res->nCols= columns;
          res->Fields=(UDM_SQLFIELD*)UdmMalloc(res->nCols*sizeof(UDM_SQLFIELD));
          bzero(res->Fields,res->nCols*sizeof(UDM_SQLFIELD));
          for (i=0 ; i < res->nCols; i++)
          {
            const char *name= sqlite3_column_name(pStmt, i);
            res->Fields[i].sqlname= (char*)UdmStrdup(name);
            res->Fields[i].sqllen= 0;
            res->Fields[i].sqltype= 0;
          }
        }
        res->nRows++;
        res->Items=(UDM_PSTR*)UdmRealloc(res->Items,res->nRows*res->nCols*sizeof(UDM_PSTR));

        for (i= 0 ; i < columns; i++)
        {
          const unsigned char *value;
          int type, bytes;
          size_t offs= (res->nRows-1)*res->nCols+(size_t)i;
          UDM_PSTR *Item= &res->Items[offs];
          type= sqlite3_column_type(pStmt, i);
          if (type == SQLITE_BLOB)
            value= sqlite3_column_blob(pStmt, i);
          else
            value= sqlite3_column_text(pStmt, i);
          bytes= sqlite3_column_bytes(pStmt, i);
          Item->len= bytes;
          Item->val= (char*)UdmMalloc(bytes + 1);
          if (bytes)
            memcpy(Item->val, value, bytes);
          res->Items[offs].val[bytes]= '\0';
        }
        break;

      case SQLITE_ERROR:
        sqlite3_finalize(pStmt);
        udm_snprintf(db->errstr, sizeof(db->errstr),
                     "sqlite3 driver: (%d) %s",
                     sqlite3_errcode((sqlite3*) db->specific),
                     sqlite3_errmsg((sqlite3*) db->specific));
        if (!strstr(db->errstr,"unique"))
        {
          db->errcode=1;
          return UDM_ERROR;
        }
        return UDM_OK;
        break;
    
      case SQLITE_MISUSE:
      case SQLITE_BUSY:
      default:
        udm_snprintf(db->errstr, sizeof(db->errstr),
                     "sqlite3_step() returned MISUSE or BUSY");
        db->errcode= 1;
        sqlite3_finalize(pStmt);
        return UDM_ERROR;
        break;
      }
    }

fin:
  if ((rc= sqlite3_finalize(pStmt)))
    return UDM_ERROR;

  return UDM_OK;
}


static void
UdmSQLite3Close(UDM_DB *db)
{
  sqlite3_close((sqlite3*)(db->specific));
  db->specific = NULL;
}


static int
UdmSQLite3Begin(UDM_DB *db)
{
  return UdmSQLite3Query(db,NULL,"BEGIN TRANSACTION");
}


static int
UdmSQLite3Commit(UDM_DB *db)
{
  return UdmSQLite3Query(db,NULL,"COMMIT");
}


static int
UdmSQLite3RenameTable(UDM_DB *db, const char *from, const char *to)
{
  char buf[256];
  udm_snprintf(buf, sizeof(buf), "ALTER TABLE %s RENAME TO %s", from, to);
  return UdmSQLExecDirect(db, NULL, buf);
}


static int
UdmSQLite3CopyStructure(UDM_DB *db, const char *from, const char *to)
{
  char buf[256];
  /*
    CREATE TABLE t1 (LIKE t2); -- works, but creates a table with 
    one column name "LIKE".
  */
  udm_snprintf(buf, sizeof(buf),
               "CREATE TABLE %s AS SELECT * FROM %s WHERE 0=1", to, from);
  return UdmSQLExecDirect(db, NULL, buf);
}


UDM_SQLDB_HANDLER udm_sqldb_sqlite3_handler =
{
  UdmSQLEscStrGeneric,
  UdmSQLite3Query,
  UdmSQLite3Connect,
  UdmSQLite3Close,
  UdmSQLite3Begin,
  UdmSQLite3Commit,
  UdmSQLPrepareGeneric,
  UdmSQLBindGeneric,
  UdmSQLExecGeneric,
  UdmSQLStmtFreeGeneric,
  UdmSQLFetchRowSimple,
  UdmSQLStoreResultSimple,
  UdmSQLFreeResultSimple,
  UdmSQLite3Query,
  UdmSQLite3RenameTable,
  UdmSQLite3CopyStructure,
  UdmSQLLockOrBeginGeneric,
  UdmSQLUnlockOrCommitGeneric,
};

#endif  /* HAVE_SQLITE3 */

#endif  /* HAVE_SQL */
