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
#if (HAVE_PGSQL)
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

#if (HAVE_PGSQL)
#include <libpq-fe.h>
#endif

/*
  TODO: auto-detect from Pg client version
  7.3 does not have prepared statements
  7.4 does have, but incomplete.
  Check which version have PQprepare
*/


#ifdef PG_DIAG_INTERNAL_POSITION
#define HAVE_PGSQL_PS 1
#define HAVE_PGSQL_VR 1
#endif

#define UDM_PG_MAX_PARAM UDM_SQL_MAX_BIND_PARAM

typedef struct udm_pgsql_conn_st
{
  PGconn *pgsql;
#ifdef HAVE_PGSQL_PS
  PGresult *stmt;
  int nParams;
  Oid *paramTypes[UDM_PG_MAX_PARAM];
  const char *paramValues[UDM_PG_MAX_PARAM];
  int paramLengths[UDM_PG_MAX_PARAM];
  int paramFormats[UDM_PG_MAX_PARAM];
  char small[32][8];
#endif /* HAVE_PGSQL_PS */
} UDM_PGSQL;


static int
UdmPgSQLConnect(UDM_DB *db)
{
  char port[8];
  const char* DBUser= UdmVarListFindStr(&db->Vars, "DBUser", NULL);
  const char* DBPass= UdmVarListFindStr(&db->Vars, "DBPass", NULL);
  const char* DBHost= UdmVarListFindStr(&db->Vars, "DBHost", NULL);
  const char* DBSock= UdmVarListFindStr(&db->Vars, "socket", NULL);
  int DBPort= UdmVarListFindInt(&db->Vars, "DBPort", 0);
  const char* setnames=  UdmVarListFindStr(&db->Vars,"setnames", "");
  UDM_PGSQL *pgdb= (UDM_PGSQL*) UdmMalloc(sizeof(UDM_PGSQL));

  if (!(db->specific= pgdb))
  {
    db->errcode= 1;
    sprintf(db->errstr, "Can't alloc mydb. Not enough memory?");
    return UDM_ERROR;
  }
  bzero((void*) pgdb, sizeof(UDM_PGSQL));

  /*
    If no host given then check if an alternative
    Unix domain socket is specified.
  */
  DBHost= (DBHost && DBHost[0]) ? DBHost : DBSock;
  
  sprintf(port,"%d",DBPort);
  pgdb->pgsql= (void*) PQsetdbLogin(DBHost, DBPort ? port : 0, 0, 0,
                                    db->DBName, DBUser, DBPass);
  if (PQstatus(pgdb->pgsql) == CONNECTION_BAD)
  {
    db->errcode=1;
    sprintf(db->errstr, "PSstatus: %s", PQerrorMessage(pgdb->pgsql));
    return UDM_ERROR;
  }
  db->connected=1;


/*
  Extract server version.
  Protocol version is also available:
  extern int PQprotocolVersion(const PGconn *conn);
*/

#ifdef HAVE_PGSQL_VR
  db->version= PQserverVersion(pgdb->pgsql);
  /*
    DROP TABLE IF EXISTS t1; -- starting from Pg-8.2 (checked with 8.2.4)
  */
  if (db->version >= 80204)
  {
    if (UdmVarListFindBool(&db->Vars, "DropIfExists", 1))
      db->flags|= UDM_SQL_HAVE_DROP_IF_EXISTS;
  }
#endif

  if (setnames[0] && PQsetClientEncoding(pgdb->pgsql, setnames))
  {
    db->errcode= 1; 
    sprintf(db->errstr, "PQsetClientEncoding: %s", PQerrorMessage(pgdb->pgsql));
    return UDM_ERROR;
  }
  
  return UDM_OK;
}


#define udm_oct2int(ch) ((ch) - '0')
#define udm_isoct(ch)   ((ch) >= '0' && (ch) <= '7')

static size_t
PgUnescape(char *dst, const char *src, size_t length)
{
  char *dst0= dst;
  const char *srcend= src + length;
  
  /* Postgres 9.0 hex format */
  if (length > 1 && src[0] == '\\' && src[1] == 'x')
    return UdmHexDecode(dst, src + 2, length - 2);
  
  /* Traditional Postgres escape format */
  for ( ; src < srcend; dst++, src++)
  {
    if (*src == '\\' && src + 4 <= srcend &&
        udm_isoct(src[1]) && udm_isoct(src[2]) && udm_isoct(src[3]))
    {
      *dst= udm_oct2int(src[1]) * 64 +
            udm_oct2int(src[2]) * 8 +
            udm_oct2int(src[3]);
      src+= 3;
    }
    else if (*src == '\\' && src[1] == '\\')
    {
      *dst= '\\';
      src++;
    }
    else
      *dst= *src;
  }
  return dst - dst0;
}



static int
UdmPgSQLResultUnescape(UDM_SQLRES *res)
{
  size_t row;
  PGresult *pgsqlres= (PGresult*) res->specific;
  res->Items= (UDM_PSTR*)UdmMalloc(res->nRows * res->nCols * sizeof(UDM_PSTR));
  for (row= 0; row < res->nRows; row++)
  {
    size_t col, offs= res->nCols * row;
    for (col= 0; col < res->nCols; col++)
    {
      UDM_PSTR *I= &res->Items[offs + col];
      const char *ptr= PQgetvalue(pgsqlres, (int) row, (int) col);
      size_t len= PQgetlength(pgsqlres, (int) row, (int )col);
      I->val= (char*) UdmMalloc(len + 1);
      if (PQftype(pgsqlres, (int) col) == 17)
      {
        I->len= PgUnescape(I->val, ptr, len);
        I->val[I->len]= '\0';
      }
      else
      {
        memcpy(I->val, ptr, len);
        I->val[len]= '\0';
        I->len= len;
      }
    }
  }
  return UDM_OK;
}


static int
UdmPgSQLProcessResult(UDM_DB *db, PGresult *PGres, UDM_SQLRES *res,
                      const char* caller,
                      int unescape, int clear)
{
  int have_bytea= 0;
  PGconn *pgsql= ((UDM_PGSQL*) db->specific)->pgsql;
  size_t i;


  if (PQresultStatus(PGres)==PGRES_COMMAND_OK)
  {
    /* Free non-SELECT query */
    if (clear)
      PQclear(PGres);
    return UDM_OK;
  }


  if (PQresultStatus(PGres) != PGRES_TUPLES_OK)
  {
#ifdef PG_DIAG_SQLSTATE
    {
      char *sqlstate= PQresultErrorField(PGres, PG_DIAG_SQLSTATE);
      if (sqlstate && !strcmp(sqlstate, "23505"))
      {
        PQclear(PGres);
        return UDM_OK;
      }
    }
#endif

    PQclear(PGres);
    sprintf(db->errstr, "%s: %s", caller, PQerrorMessage(pgsql));
    if (strstr(db->errstr,"uplicate") ||
        strstr(db->errstr, "duplicada") ||
        strstr(db->errstr, "повторный") ||
        strstr(db->errstr, "п©п╬п╡я┌п╬я─п╫я▀п╧") ||
        strcasestr(db->errstr,"Duplizierter"))
    {
      return UDM_OK;
    }
    else
    {
      db->errcode = 1;
      return UDM_ERROR;
    }
  }
  
  if (!res)
  {
    /* 
     Don't allow to call UdmPgSQLQuery
     returning data with NULL res pointer
    */
    sprintf(db->errstr, 
            "UdmPgSQLQuery executed with res=NULL returned result %d, %s",
             PQresultStatus(PGres),PQerrorMessage(pgsql));
    db->errcode=1;
    return UDM_ERROR;
  }
  
  res->specific= (void*) PGres;
  res->nCols=(size_t)PQnfields(PGres);
  res->nRows=(size_t)PQntuples(PGres);
  res->Fields=(UDM_SQLFIELD*)UdmMalloc(res->nCols*sizeof(UDM_SQLFIELD));
  for (i=0;i<res->nCols;i++)
  {
    res->Fields[i].sqlname = (char*)UdmStrdup(PQfname(PGres,(int)i));
    if (PQftype(PGres, (int) i) == 17)
      have_bytea++;
  }

  if (have_bytea && unescape)
    UdmPgSQLResultUnescape(res);
  
  return UDM_OK;
}


static int
UdmPgSQLQueryInternal(UDM_DB *db, UDM_SQLRES *res,
                      const char *q, int unescape, int clear)
{
  PGresult *PGres;
  PGconn *pgsql;
  int rc;
  
  if (res)
  {
    bzero((void*) res, sizeof(UDM_SQLRES));
    res->db= db;
  }

  db->errcode= 0;
  if (!db->connected)
  {
    UdmPgSQLConnect(db);
    if (db->errcode)
      return UDM_ERROR;
  }
  
  pgsql= ((UDM_PGSQL*) db->specific)->pgsql;
  
  if (!(PGres= PQexec(pgsql,q)))
  {
    sprintf(db->errstr, "PQexec: %s", PQerrorMessage(pgsql));
    db->errcode= 1;
    return UDM_ERROR;
  }

  rc= UdmPgSQLProcessResult(db, PGres, res, "PQexec", unescape, clear);
  
  return rc;
}


static int
UdmPgSQLQuery(UDM_DB *db, UDM_SQLRES *res, const char *q)
{
  return UdmPgSQLQueryInternal(db, res, q, 1, 1);
}


static int
UdmPgSQLExecDirect(UDM_DB *db, UDM_SQLRES *res, const char *q)
{
  return UdmPgSQLQueryInternal(db, res, q, 0, 1);
}


#if HAVE_PGSQL_PS

static inline int
UdmSQLType2PgSQLType(int type)
{
  /*return type == UDM_SQLTYPE_INT32 ? MYSQL_TYPE_LONG : MYSQL_TYPE_STRING;*/
  return 0;
}


static int
UdmPgSQLPrepare(UDM_DB *db, const char *query)
{
  int rc= UDM_OK;
  UDM_PGSQL *pgdb= (UDM_PGSQL*) db->specific;
  PGconn *pgsql= pgdb->pgsql;
  db->errcode= 0;
  db->errstr[0]= '\0';
  pgdb->nParams= 0;
  
  if (!(pgdb->stmt= PQprepare(pgsql,
                              "" /* "stmtName" */,
                              query,
                              0 /*int nParams*/,
                              NULL /*const Oid *paramTypes*/)))
  {
    db->errcode= 1;
    sprintf(db->errstr, "PQprepare: %s", PQerrorMessage(pgsql));
    return UDM_ERROR;
  }
  else
  {
    rc= UdmPgSQLProcessResult(db, pgdb->stmt, NULL, "PQprepare", 0, 0);
  }
  
  return rc;
}


static int
UdmPgSQLBind(UDM_DB *db, int pos, const void *data, int size, int type)
{
  UDM_PGSQL *pgdb= (UDM_PGSQL*) db->specific;
  if (pgdb->nParams < pos)
    pgdb->nParams= pos;
  pos--;
  pgdb->paramValues[pos]= data;
  pgdb->paramLengths[pos]= (int) size;
  pgdb->paramFormats[pos]= 1;
#ifndef WORDS_BIGENDIAN
  if (type == UDM_SQLTYPE_INT32)
  {
    char *tmp= pgdb->small[pos];
    const char *src= (const char*) data;
    /* Postgres uses big endian values in the protocol */
    tmp[0]= src[3];
    tmp[1]= src[2];
    tmp[2]= src[1];
    tmp[3]= src[0];
    pgdb->paramValues[pos]= tmp;
  }
#endif
  return UDM_OK;
}


static int
UdmPgSQLStmtFree(UDM_DB *db)
{
  UDM_PGSQL *pgdb= (UDM_PGSQL*) db->specific;
  PQclear(pgdb->stmt);
  return UDM_OK;
}


static int
UdmPgSQLExec(UDM_DB *db)
{
  UDM_PGSQL *pgdb= (UDM_PGSQL*) db->specific;
  PGconn *pgsql= pgdb->pgsql;
  PGresult *PGres;
  
  PGres= PQexecPrepared(pgsql,
                        "", /*"stmtName"*/
                        pgdb->nParams,
                        pgdb->paramValues,
                        pgdb->paramLengths,
                        pgdb->paramFormats,
                        0 /*int resultFormat*/);

  return UdmPgSQLProcessResult(db, PGres, 
                               /*UDM_SQLRES*/ NULL, 
                               "PQexecPrepared",
                               /*unescape  */ 1,
                               /* clear    */ 1);
}
#endif /* HAVE_PGSQL_PS */ 


static void
UdmPgSQLClose(UDM_DB *db)
{
  UDM_PGSQL *pgdb= (UDM_PGSQL*) db->specific;
  PQfinish(pgdb->pgsql);
  UDM_FREE(db->specific);
  db->connected= 0;
  db->specific= NULL;
}


static int
UdmPgSQLBegin(UDM_DB *db)
{
  return UdmPgSQLQuery(db,NULL,"BEGIN WORK");
}


static int
UdmPgSQLCommit(UDM_DB *db)
{
  return UdmPgSQLQuery(db,NULL,"END WORK");
}


static int
UdmPgSQLFetchRow(UDM_DB *db, UDM_SQLRES *res, UDM_PSTR *buf)
{
  size_t j;
  PGresult *pgsqlres= (PGresult*) res->specific;

  if (res->curRow >= res->nRows)
    return UDM_ERROR;
  
  for (j = 0; j < res->nCols; j++)
  {
    buf[j].val= PQgetvalue(pgsqlres,(int)res->curRow,(int)(j));
    buf[j].len= PQgetlength(pgsqlres, res->curRow, j);
    if (PQftype(pgsqlres, (int) j) == 17)
    {
      buf[j].len= PgUnescape(buf[j].val, buf[j].val, buf[j].len);
      buf[j].val[buf[j].len]= '\0';
    }
  }
  res->curRow++;
  return(UDM_OK);
}


static int
UdmPgSQLRenameTable(UDM_DB *db, const char *from, const char *to)
{
  char buf[256];
  udm_snprintf(buf, sizeof(buf), "ALTER TABLE %s RENAME TO %s", from, to);
  return UdmSQLExecDirect(db, NULL, buf);
}


static int
UdmPgSQLCopyStructure(UDM_DB *db, const char *from, const char *to)
{
  char buf[256];
  /*
    Does not copy indexes by default.
    Statring with 8.3 also this syntax is supported:
    CREATE TABLE t2 (LIKE t1 INCLUDING INDEXES); -- copy indexes
  */
  udm_snprintf(buf, sizeof(buf), "CREATE TABLE %s (LIKE %s)", to, from);
  return UdmSQLExecDirect(db, NULL, buf);
}


UDM_SQLDB_HANDLER udm_sqldb_pgsql_handler =
{
  UdmSQLEscStrGeneric,
  UdmPgSQLQuery,
  UdmPgSQLConnect,
  UdmPgSQLClose,
  UdmPgSQLBegin,
  UdmPgSQLCommit,
#if HAVE_PGSQL_PS
  UdmPgSQLPrepare,
  UdmPgSQLBind,
  UdmPgSQLExec,
  UdmPgSQLStmtFree,
#else       
  UdmSQLPrepareGeneric,
  UdmSQLBindGeneric,
  UdmSQLExecGeneric,
  UdmSQLStmtFreeGeneric,
#endif
  UdmPgSQLFetchRow,
  UdmSQLStoreResultSimple,
  UdmSQLFreeResultSimple,
  UdmPgSQLExecDirect,
  UdmPgSQLRenameTable,
  UdmPgSQLCopyStructure,
  UdmSQLLockOrBeginGeneric,
  UdmSQLUnlockOrCommitGeneric,
};

#endif
