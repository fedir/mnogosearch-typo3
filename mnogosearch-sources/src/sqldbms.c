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

#if (HAVE_PGSQL)
#include <libpq-fe.h>
#endif

#ifdef WIN32
#include <process.h>
#endif

static char udm_hex_digits[]= "0123456789ABCDEF";


void DecodeHexStr (const char *src, UDM_PSTR *dst, size_t size)
{
  if (!(dst->val= UdmMalloc(size / 2 + 1)))
  {
    dst->len= 0;
    return;
  }
  dst->len= UdmHexDecode(dst->val, src, size);
  dst->val[dst->len]= '\0';
}


/***************************************************************/


int
UdmSQLResFreeGeneric(UDM_SQLRES *res)
{
  size_t i;
  size_t nitems;
  if(res)
  {
    if(res->Items)
    {
      nitems = res->nCols * res->nRows;
      for(i=0;i<nitems;i++)
        if(res->Items[i].val)
          UDM_FREE(res->Items[i].val);
      UDM_FREE(res->Items);
    }
  }
  return(0);
}




/***********************************************************************/

/*
 *   Wrappers for different databases
 *
 *   UdmDBEscStr();
 *   UdmSQLBinEscStr();
 *   UdmSQLQuery();
 *   UdmSQLValue();
 *   UdmSQLLen();
 *   UdmSQLFree();
 *   UdmSQLClose();
 */  


int
UdmSQLDropTableIfExists(UDM_DB *db, const char *name)
{
  char qbuf[128];
  int rc;
  int have_if_exists= (db->flags & UDM_SQL_HAVE_DROP_IF_EXISTS);
  const char *if_exists= have_if_exists ? "IF EXISTS " : "";
  
  if (db->DBType == UDM_DB_MSSQL)
  {
    /*
    Another option:
    IF OBJECT_ID(N'tempdb..#temptable', N'U') IS NOT NULL 
    DROP TABLE #temptable;
    */
    udm_snprintf(qbuf, sizeof(qbuf),
                 "IF EXISTS "
                 "(SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "
                 "WHERE TABLE_NAME='%s') "
                 "DROP TABLE %s", name, name);
    return UdmSQLQuery(db, NULL, qbuf);
  }
  if (!have_if_exists)
    db->flags|= UDM_SQL_IGNORE_ERROR;
  udm_snprintf(qbuf, sizeof(qbuf), "DROP TABLE %s%s", if_exists, name);
  rc= UdmSQLQuery(db, NULL, qbuf);
  if (!have_if_exists)
    db->flags^= UDM_SQL_IGNORE_ERROR;

  /*
    TODO:
    
    SQLite3:  DROP TABLE IF EXISTS t1; (DONE)
    Sybase ?  Perhaps simimalr to MSSQL
    Oracle ?
    Ibase  ?
    Mimer  ?
    DB2    ?
  */
  return rc;
}


/*
 Bind type for long varchar columns, like urlinfo.sval
*/
int
UdmSQLLongVarCharBindType(UDM_DB *db)
{
  int bindtype= 
      db->DBDriver == UDM_DB_ORACLE8 ? UDM_SQLTYPE_LONGVARCHAR :
         (db->DBType == UDM_DB_MSSQL   ||
          db->DBType == UDM_DB_SYBASE  ||
          db->DBType == UDM_DB_SQLITE3 ||
          db->DBType == UDM_DB_MONETDB ?
                  UDM_SQLTYPE_VARCHAR : UDM_SQLTYPE_LONGVARCHAR);
#ifdef HAVE_UNIXODBC
  if (db->DBType == UDM_DB_SYBASE)
    bindtype= UDM_SQLTYPE_LONGVARCHAR;
#endif
  return bindtype;
}


int
UdmSQLTableTruncateOrDelete(UDM_DB *db, const char *name)
{
  char qbuf[128];
  if (db->flags & UDM_SQL_HAVE_TRUNCATE)
    udm_snprintf(qbuf, sizeof(qbuf), "TRUNCATE TABLE %s", name);
  else
    udm_snprintf(qbuf, sizeof(qbuf), "DELETE FROM %s", name);
  return UdmSQLQuery(db,NULL,qbuf);
}


static size_t
UdmSQLEscStrMonet(UDM_DB *db, char *to, const char *from, size_t len)
{
  char *to0= to;
  /* Escape single quote with single quote, backslash with backslash */
  for ( ; len && *from; from++, len--)
  {
    switch (*from)
    {
      case '\\':
      case '\'':
       /* Note that no break here!*/
       *to++= *from;
      default:
       *to++= *from;
    }
  }
  *to= '\0';
  return to - to0;
}


static size_t
UdmSQLEscStrStandard(UDM_DB *db, char *to,const char *from,size_t len)
{
  char *to0= to;
  for ( ; len && *from; from++, len--)
  {
    switch (*from)
    {
      case '\'':
       /* Note that no break here!*/
       *to++= *from;
      default:
       *to++= *from;
    }
  }
  *to= '\0';
  return to - to0;
}


static size_t
UdmSQLEscStrPgSQL(UDM_DB *db, char *to,const char *from,size_t len)
{
  char *to0= to;
  int DBType= db->DBType;
  const char *fend= from + len;

  while (*from && from < fend)
  {
#ifdef WIN32
    /*
      A workaround against a bug in SQLExecDirect
      in PostgreSQL ODBC driver. It produces
      an error when meets a question mark in a string
      constant. For some reasons question marks are
      considered as parameters. This should not happen
      with SQLExecDirect.
      Let's escape question mark using \x3F notation.
    */
    if (DBType == UDM_DB_PGSQL &&
        (*from == '?' ||
         *from == '{' ||
         *from == '}'))
    {
      *to++= '\\';
      *to++= 'x';
      *to++= '3';
      *to++= 'F';
      from++;
      continue;
    }
#endif
    /*
    "ODBC escape convert error" is returned when '{' or '}' are not escaped.
    */
    if (DBType == UDM_DB_PGSQL && (*from == '{' || *from == '}'))
    {
      *to++= '\\';
      *to++= 'x';
      *to++= '7';
      *to++= udm_hex_digits[*from & 0x0F];
      from++;
      continue;
    }
    switch(*from)
    {
      case '\'':
      case '\\':
        *to='\\';to++;
      default:*to=*from;
    }
    to++;from++;
  }
  *to= '\0';
  return to - to0;
}


size_t
UdmSQLEscStrGeneric(UDM_DB *db, char *to,const char *from,size_t len)
{
  switch (db->DBType)
  {
    case UDM_DB_MYSQL:
      return UdmSQLEscStrPgSQL(db, to, from, len);
    case UDM_DB_MONETDB:
      return UdmSQLEscStrMonet(db, to, from, len);
    case UDM_DB_PGSQL:
      /*
        Older version also did PgSQL-style escaping for:
        UDM_DB_SOLID, UDM_DB_VIRT, UDM_DB_ORACLE7.
        Restore PgSQL-style escaping if there are trobles with 
      */
      return (db->version < 90000) ?
             UdmSQLEscStrPgSQL(db, to, from, len) :
             UdmSQLEscStrStandard(db, to, from, len);
    default:
      return UdmSQLEscStrStandard(db, to, from, len);
  }
}


size_t
UdmSQLEscStr(UDM_DB *db, char *to, const char *from, size_t len)
{
  UDM_ASSERT(from);
  UDM_ASSERT(to);
  return db->sql->SQLEscStr(db, to, from, len);
}


static int
UdmSQLEscStrLength(UDM_DB *db, size_t srclen)
{
  return ((db->DBType == UDM_DB_PGSQL) ? 4 : 2) * srclen + 1;
}


char *
UdmSQLEscStrAlloc(UDM_DB *db, const char *src, size_t srclen)
{
  char *dst;
  if(!src ||
     !(dst= (char*) UdmMalloc(UdmSQLEscStrLength(db, srclen))))
    return NULL;
  db->sql->SQLEscStr(db, dst, src, srclen);
  return dst;
}


/*
  SQL-Escape string to DSTR
  dstr - a pointer to a initialized DSTR
*/
int
UdmSQLEscDSTR(UDM_DB *db, UDM_DSTR *dstr, const char *src, size_t srclen)
{
  int rc;
  if (UDM_OK != (rc= UdmDSTRAlloc(dstr, UdmSQLEscStrLength(db, srclen))))
    return rc;
  dstr->size_data= db->sql->SQLEscStr(db, dstr->data, src, srclen);
  return rc;
}



static char
dangerous_character[256]=
{
/*00*/  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/*10*/  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
/*20*/  0,1,1,1,1,0,1,1,1,1,0,0,0,0,0,0,  /*  !"#$%&'()*+,-./ */
/*30*/  0,0,0,0,0,0,0,0,0,0,0,1,1,0,1,0,  /* 0123456789:;<=>? */
/*40*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* @ABCDEFGHIJKLMNO */
/*50*/  0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,  /* PQRSTUVWXYZ[\]^_ */
/*60*/  1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* `abcdefghijklmno */
/*70*/  0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,  /* pqrstuvwxyz{|}~  */
/*80*/  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
/*90*/  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
/*A0*/  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
/*B0*/  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
/*C0*/  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
/*D0*/  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
/*E0*/  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
/*F0*/  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3
};


/*
  Escape simple strings, not containing dangerous characters.
  Used for things like user limit names, user score names, etc.
*/
char *
UdmSQLEscStrSimple(UDM_DB *db, char *dst, const char *src, size_t len)
{
  size_t multiply= 1;
  const char *srcend= src + len;
  char *dst0;
  /*
    Backslash, quote and double quote character are not allowed,
    to avoid SQL injection.
  */
  UDM_ASSERT(!dangerous_character['%']); /* Need this for "indexer -u url% */
  UDM_ASSERT(!dangerous_character['?']); /* "indexer -u http://host/?a=b"  */
  UDM_ASSERT(!dangerous_character['=']); /* "indexer -u http://host/?a=b"  */

  UDM_ASSERT(dangerous_character['"']);
  UDM_ASSERT(dangerous_character['\'']);
  UDM_ASSERT(dangerous_character['\\']);
  
  if (!dst && !(dst= (char*) UdmMalloc(len * multiply + 1)))
    return NULL;
  for ( dst0= dst, srcend= src + len; src < srcend; src++)
  {
    *dst++= dangerous_character[(unsigned char) *src] ? '?' : *src;
  }
  *dst= '\0';
  return  dst0;
}


/*
  Escape a byte for PgSQL bytea encoding
*/
static inline size_t
UdmSQLBinEscCharForPg(char *dst, unsigned int ch)
{
  if (ch  >= 0x20 && ch <= 0x7F && ch != '\'' && ch != '\\')
  {
    *dst= ch;
    return 1;
  }
  
  dst[4]= udm_hex_digits[ch & 0x07]; ch /= 8;
  dst[3]= udm_hex_digits[ch & 0x07]; ch /= 8;
  dst[2]= udm_hex_digits[ch & 0x07];
  dst[1]= '\\';
  dst[0]= '\\';
  return 5;
}


size_t
UdmSQLBinEscStr(UDM_DB *db, char *dst, size_t dstlen, const char *src0, size_t len)
{
  const unsigned char *src= (const unsigned char*) src0;
  UDM_ASSERT(dst != NULL);
  
  if (db->DBType == UDM_DB_PGSQL)
  {
    char *dst0;
    for (dst0= dst ; len > 0 ; len--, src++)
      dst+= UdmSQLBinEscCharForPg(dst, *src);
    *dst= '\0';
    return dst - dst0;
  }
  UdmSQLEscStr(db, dst, src0, len);
  return 0;
}


#ifdef HAVE_DEBUG
int
UdmSQLBeginDebug(UDM_DB *db, const char *file, const int line)
{
  udm_timer_t ticks= UdmStartTimer(); 
  int rc= db->sql->SQLBegin(db);
  if (db->flags & UDM_SQL_DEBUG_QUERY)
    fprintf(stderr, "[%d] %.2f SQLBegin {%s:%d}\n",
            getpid(), UdmStopTimer(&ticks)/1000, file, line);
  return rc;
}

int
UdmSQLCommitDebug(UDM_DB *db, const char *file, const int line)
{
  udm_timer_t ticks= UdmStartTimer();
  int rc= db->sql->SQLCommit(db);
  if (db->flags & UDM_SQL_DEBUG_QUERY)
    fprintf(stderr, "[%d] %.2f SQLCommit {%s:%d}\n",
            getpid(), UdmStopTimer(&ticks)/1000, file, line);
  return rc;
}
int
UdmSQLExecDirectDebug(UDM_DB *db, UDM_SQLRES *R, const char *query,
                      const char *file, const int line)
{
  if (db->flags & UDM_SQL_DEBUG_QUERY)
  {
    int rc;
    udm_timer_t ticks= UdmStartTimer();
    rc= db->sql->SQLExecDirect(db, R, query);
    fprintf(stderr,"[%d] %.2f SQL {%s:%d}: %s\n",
            getpid(), UdmStopTimer(&ticks), file, line, query);
    return rc;
  }
  else
    return db->sql->SQLExecDirect(db, R, query);
}

int
UdmSQLPrepareDebug(UDM_DB *db, const char *q,
                   const char* file, const int line)
{
  int rc;
  rc= db->sql->SQLPrepare(db, q);
  if (db->flags & UDM_SQL_DEBUG_QUERY)
    fprintf(stderr, "[%d] Prepare {%s:%d}: %s\n", getpid(), file, line, q);
  return rc;
}

int
UdmSQLExecuteDebug(UDM_DB *db,
                   const char* file, const int line)
{
  udm_timer_t ticks= UdmStartTimer();
  int rc= db->sql->SQLExec(db);
  if (db->flags & UDM_SQL_DEBUG_QUERY)
    fprintf(stderr, "[%d] %.2f Execute {%s:%d}\n",
            getpid(), UdmStopTimer(&ticks), file, line);
  return rc;
}

int
UdmSQLStmtFreeDebug(UDM_DB *db,
                    const char* file, const int line)
{
  int rc;
  rc= db->sql->SQLStmtFree(db);
  if (db->flags & UDM_SQL_DEBUG_QUERY)
    fprintf(stderr, "[%d] StmtFree {%s:%d}\n", getpid(), file, line);
  return rc;
}

static const char *
UdmSQLTypeToStr(int type)
{
  switch (type)
  {
    case UDM_SQLTYPE_LONGVARBINARY : return "LONGVARBINARY";
    case UDM_SQLTYPE_LONGVARCHAR   : return "LONGVARCHAR";
    case UDM_SQLTYPE_VARCHAR       : return "VARCHAR";
    case UDM_SQLTYPE_INT32         : return "INT";
    case UDM_SQLTYPE_UNKNOWN       :
    default:
      return "UNKNOW_TYPE";
  }
  return NULL; /* Make compiler happy */
}


static void
UdmSQLPrintParameter(FILE *file, const void *data, int size)
{
  const unsigned char *s= (const unsigned char*) data;
  const unsigned char *e= s + size;
  fprintf(stderr, "'");
  for ( ; s < e; s++)
  {
    if (*s >= 0x20 && *s <= 0x7E)
      fprintf(file, "%c", *s);
    else
      fprintf(file, "\\x%02X", (int) *s);
  }
  fprintf(stderr, "'");
}

int
UdmSQLBindParameterDebug(UDM_DB *db,
                         int pos, const void *data, int size, int type,
                         const char *file, const int line)
{
  UDM_ASSERT(size == 4 || type != UDM_SQLTYPE_INT32);
  if (db->flags & UDM_SQL_DEBUG_QUERY)
  {
    fprintf(stderr, "[%d] BindParameter {%s:%d} pos=%d %s(%d)",
            getpid(), file, line, pos, UdmSQLTypeToStr(type), size);
    UdmSQLPrintParameter(stderr, data, size);
    fprintf(stderr, "\n");
  }
  return db->sql->SQLBind(db, pos, data, size, type);
}

int
UdmSQLRenameTableDebug(UDM_DB *db, const char *from, const char *to,
                       const char *file, const int line)
{
  if (db->flags & UDM_SQL_DEBUG_QUERY)
  {
    fprintf(stderr, "[%d] RenameTable {%s:%d} '%s' to '%s'\n",
            getpid(), file, line, from, to);
  }
  return db->sql->SQLRenameTable(db, from, to);
}

int
UdmSQLCopyStructureDebug(UDM_DB *db, const char *from, const char *to,
                         const char *file, const int line)
{
  if (db->flags & UDM_SQL_DEBUG_QUERY)
  {
    fprintf(stderr, "[%d] CopyStructure {%s:%d} '%s' to '%s'\n",
            getpid(), file, line, from, to);
  }
  return db->sql->SQLCopyStructure(db, from, to);
}

int
UdmSQLLockOrBeginDebug(UDM_DB *db, const char *param,
                       const char *file, const int line)
{
  udm_timer_t ticks= UdmStartTimer(); 
  int rc= db->sql->SQLLockOrBegin(db, param);
  if (db->flags & UDM_SQL_DEBUG_QUERY)
    fprintf(stderr, "[%d] %.2f SQLLockOrBegin(%s) {%s:%d}\n",
           getpid(), UdmStopTimer(&ticks)/1000, param, file, line);
  return rc;
}

int
UdmSQLUnlockOrCommitDebug(UDM_DB *db, const char *file, const int line)
{
  udm_timer_t ticks= UdmStartTimer();
  int rc= db->sql->SQLUnlockOrCommit(db);
  if (db->flags & UDM_SQL_DEBUG_QUERY)
    fprintf(stderr, "[%d] %.2f SQLUnlockOrCommit {%s:%d}\n",
            getpid(), UdmStopTimer(&ticks)/1000, file, line);
  return rc;
}

#else /* not HAVE_DEBUG */
int
UdmSQLBegin(UDM_DB *db)
{
  return db->sql->SQLBegin(db);
}

int
UdmSQLCommit(UDM_DB *db)
{
  return db->sql->SQLCommit(db);
}

int
UdmSQLExecDirect(UDM_DB *db, UDM_SQLRES *R, const char *q)
{
  return db->sql->SQLExecDirect(db, R, q);
}

int
UdmSQLPrepare(UDM_DB *db, const char *q)
{
  return db->sql->SQLPrepare(db, q);
}

int
UdmSQLExecute(UDM_DB *db)
{
  return db->sql->SQLExec(db);
}

int
UdmSQLStmtFree(UDM_DB *db)
{
  return db->sql->SQLStmtFree(db);
}

int
UdmSQLBindParameter(UDM_DB *db, int pos, const void *data, int size, int type)
{
  return db->sql->SQLBind(db, pos, data, size, type);
}

int
UdmSQLRenameTable(UDM_DB *db, const char *from, const char *to)
{
  return db->sql->SQLRenameTable(db, from, to);
}

int
UdmSQLCopyStructure(UDM_DB *db, const char *from, const char *to)
{
  return db->sql->SQLCopyStructure(db, from, to);
}

int
UdmSQLLockOrBegin(UDM_DB *db, const char *param)
{
  return db->sql->SQLLockOrBegin(db, param);
}

int
UdmSQLUnlockOrCommit(UDM_DB *db)
{
  return db->sql->SQLUnlockOrCommit(db);
}

#endif /* HAVE_DEBUG */


int __UDMCALL
_UdmSQLQuery(UDM_DB *db, UDM_SQLRES *SQLRes, const char * query,
             const char *file, const int line)
{
  UDM_SQLRES res;
  
#ifdef HAVE_DEBUG
  udm_timer_t ticks;
  if (db->flags & UDM_SQL_DEBUG_QUERY)
  {
    ticks= UdmStartTimer();
  }
#endif
  
  /* FIXME: call UdmSQLFree at exit if SQLRes = NULL */
  if (! SQLRes) SQLRes = &res;
  
  db->sql->SQLQuery(db, SQLRes, query);

#ifdef HAVE_DEBUG
  if (db->flags & UDM_SQL_DEBUG_QUERY)
  {
    fprintf(stderr,"[%d] %.2f SQL {%s:%d}: %s\n",
            getpid(), UdmStopTimer(&ticks), file, line, query);
  }
#endif
  
  if (db->errcode && (db->flags & UDM_SQL_IGNORE_ERROR))
    db->errcode= 0;
  
#ifdef DEBUG_ERR_QUERY
  if (db->errcode)
    fprintf(stderr, "{%s:%d} Query: %s\n\n", file, line, query);
#endif

  return db->errcode ? UDM_ERROR : UDM_OK;
}


size_t __UDMCALL UdmSQLNumRows(UDM_SQLRES * res)
{
  return(res?res->nRows:0);
}

size_t UdmSQLNumCols(UDM_SQLRES * res)
{
  return(res?res->nCols:0);
}

const char * __UDMCALL UdmSQLValue(UDM_SQLRES * res,size_t i,size_t j){

#if HAVE_PGSQL
  if(res->db->DBDriver==UDM_DB_PGSQL && !res->Items)
    return(PQgetvalue((PGresult*)res->specific,(int)(i),(int)(j)));
#endif
  
  if (i<res->nRows)
  {
    size_t offs=res->nCols*i+j;
    return res->Items[offs].val;
  }
  else
  {
    return NULL;
  }
}


int __UDMCALL UdmSQLFetchRowSimple (UDM_DB *db, UDM_SQLRES *res, UDM_PSTR *buf) {
  size_t j;
  size_t offs = res->nCols * res->curRow;

  if (res->curRow >= res->nRows)
    return UDM_ERROR;
  
  for (j = 0; j < res->nCols; j++)
  {
    buf[j]= res->Items[offs + j];
  }
  res->curRow++;
  
  return(UDM_OK);
}


int __UDMCALL UdmSQLStoreResultSimple(UDM_DB *db, UDM_SQLRES *res)
{
  return UDM_OK;
}

int __UDMCALL UdmSQLFreeResultSimple(UDM_DB *db, UDM_SQLRES *res)
{
  if (res->Fields)
  {
    size_t i;
    for(i=0;i<res->nCols;i++)
    {
      /*
      printf("%s(%d)\n",res->Fields[i].sqlname,res->Fields[i].sqllen);
      */
      UDM_FREE(res->Fields[i].sqlname);
    }
    UDM_FREE(res->Fields);
  }
  
#if HAVE_PGSQL
  if(res->db->DBDriver==UDM_DB_PGSQL)
  {
    PQclear((PGresult*)res->specific);
    /*
      Continue to UdmSQLResFreeGeneric() to
      free allocated memory if we have bytea datatype
    */
  }
#endif

  UdmSQLResFreeGeneric(res);

  return UDM_OK;
}


void __UDMCALL UdmSQLFree(UDM_SQLRES * res)
{
  res->db->sql->SQLFreeResult(res->db, res);
}


size_t UdmSQLLen(UDM_SQLRES * res,size_t i,size_t j)
{
  size_t offs=res->nCols*i+j;
#if HAVE_PGSQL
  if(res->db->DBDriver==UDM_DB_PGSQL && !res->Items)
    return PQgetlength((PGresult*)res->specific, i, j);
#endif
  return res->Items[offs].len;
}


void UdmSQLClose(UDM_DB *db)
{
  if(!db->connected)
    return;
  db->sql->SQLClose(db);
  db->connected=0;
  return;
}


void
UdmSQLResListInit(UDM_SQLRESLIST *List)
{
  bzero((void*)List, sizeof(*List));
}


int
UdmSQLResListAdd(UDM_SQLRESLIST *List, UDM_SQLRES *Res)
{
  size_t nbytes= (List->nitems + 1) * sizeof(UDM_SQLRES);
  if (!(List->Item= (UDM_SQLRES*) UdmRealloc(List->Item, nbytes)))
    return UDM_ERROR;
  List->Item[List->nitems]= Res[0];
  List->nitems++;
  return UDM_OK;
}


void
UdmSQLResListFree(UDM_SQLRESLIST *List)
{
  size_t i;
  for (i= 0; i < List->nitems; i++)
  {
    UdmSQLFree(&List->Item[i]);
  }
  UdmFree(List->Item);
  UdmSQLResListInit(List);
}


int
UdmSQLQueryOneRowInt(UDM_DB *db, int *res, const char *qbuf)
{
  UDM_SQLRES sqlRes;
  int rc;
  if (UDM_OK != (rc= UdmSQLQuery(db, &sqlRes, qbuf)))
    return rc;
  if (UdmSQLNumRows(&sqlRes) < 1)
  {
    rc= UDM_ERROR;
    *res= 0;
    sprintf(db->errstr, "Query should have returned one row");
  }
  else
    *res= UDM_ATOI(UdmSQLValue(&sqlRes, 0, 0));
  UdmSQLFree(&sqlRes);
  return rc;
}


static const char*
odbc_params[UDM_SQL_MAX_BIND_PARAM]=
{
  "?","?","?","?","?","?","?","?",
  "?","?","?","?","?","?","?","?",
  "?","?","?","?","?","?","?","?",
  "?","?","?","?","?","?","?","?",
  "?","?","?","?","?","?","?","?",
  "?","?","?","?","?","?","?","?",
  "?","?","?","?","?","?","?","?",
  "?","?","?","?","?","?","?","?",
};


static const char*
oracle_params[UDM_SQL_MAX_BIND_PARAM]=
{
   ":1", ":2", ":3", ":4", ":5", ":6", ":7", ":8", ":9",
  ":10",":11",":12",":13",":14",":15",":16",":17",":18",":19",
  ":20",":21",":22",":23",":24",":25",":26",":27",":28",":29",
  ":30",":31",":32",":33",":34",":35",":36",":37",":38",":39",
  ":40",":41",":42",":43",":44",":45",":46",":47",":48",":49",
  ":50",":51",":52",":53",":54",":55",":56",":57",":58",":59",
  ":60",":61",":62",":63",":64"
};


static const char*
pgsql_params[UDM_SQL_MAX_BIND_PARAM]= 
{
   "$1", "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
  "$10","$11","$12","$13","$14","$15","$16","$17","$18","$19",
  "$20","$21","$22","$23","$24","$25","$26","$27","$28","$29",
  "$30","$31","$32","$33","$34","$35","$36","$37","$38","$39",
  "$40","$41","$42","$43","$44","$45","$46","$47","$48","$49",
  "$50","$51","$52","$53","$54","$55","$56","$57","$58","$59",
  "$60","$61","$62","$63","$64"
};


const char *
UdmSQLParamPlaceHolder(UDM_DB *db, size_t i)
{
  UDM_ASSERT(i < UDM_SQL_MAX_BIND_PARAM);
  if (db->sql->SQLBind == UdmSQLBindGeneric)
    return odbc_params[i - 1];
  if (db->DBDriver == UDM_DB_ORACLE8)
    return oracle_params[i - 1];
  if (db->DBDriver == UDM_DB_PGSQL)
    return pgsql_params[i - 1];
  return odbc_params[i - 1];
}


/*
  Generic prepared statement API for libraries
  not supporting their own prepared statements.
*/

#define UDM_GENERIC_MAX_BIND_PARAM UDM_SQL_MAX_BIND_PARAM


typedef struct generic_sql_ps_st
{
  char *sql;
  int nParams;
  int paramTypes[UDM_GENERIC_MAX_BIND_PARAM];
  const void *paramValues[UDM_GENERIC_MAX_BIND_PARAM];
  int paramLengths[UDM_GENERIC_MAX_BIND_PARAM];
} UDM_PS;


static UDM_PS *
alloc_ps(UDM_DB *db)
{
  if (db->ps)
    return db->ps;
  if ((db->ps= (UDM_PS*) UdmMalloc(sizeof(UDM_PS))))
    return db->ps;
  return NULL;
}


int
UdmSQLPrepareGeneric(UDM_DB *db, const char *query)
{
  UDM_PS *ps= alloc_ps(db);
  
  if (!ps)
    return UDM_ERROR;

  ps->nParams= 0;

  /*
    TODO: add automatic connecting here
  */
  if (!(ps->sql= UdmStrdup(query)))
  {
    udm_snprintf(db->errstr, sizeof(db->errstr),
                 "UdmSQLPrepareGeneric: could not allocate memory");
    return UDM_ERROR;
  }
  return UDM_OK;
}


int
UdmSQLBindGeneric(UDM_DB *db, int pos, const void *data, int size, int type)
{
  UDM_PS *ps= (UDM_PS*) db->ps;
  UDM_ASSERT(ps->sql);

  if (!ps)
    return UDM_ERROR;

  if (ps->nParams < pos)
    ps->nParams= pos;   
  pos--;
  ps->paramValues[pos]= data;
  ps->paramLengths[pos]= (int) size;
  ps->paramTypes[pos]= type;
  return UDM_OK;
}


static size_t
prepared_query_length(const char *sql, UDM_PS *param)
{
  size_t i;
  size_t len= strlen(param->sql) + 1;
  
  for (i= 0; i < param->nParams; i++)
  {
    switch (param->paramTypes[i])
    {
      case UDM_SQLTYPE_LONGVARBINARY:
      case UDM_SQLTYPE_LONGVARCHAR  :
      case UDM_SQLTYPE_VARCHAR:
        len+= param->paramLengths[i] < 0 ?
              4 :/* NULL in ODBC*/
              param->paramLengths[i] * 10 + 10; /* TODO: proper constant */
        break;
      case UDM_SQLTYPE_INT32:
        len+= 21;
        break;
    }
  }
  return len;
}


static size_t
prepared_query_add_params(UDM_DB *db, char *dst, size_t dstlen,
                          UDM_PS *param, size_t param_num)
{
  char *dst0= dst;
  int is_bin= (param->paramTypes[param_num] == UDM_SQLTYPE_LONGVARBINARY);
  size_t srclen= param->paramLengths[param_num];
  const void *src= param->paramValues[param_num];

  if (!srclen)
  {
    /* Note, srclen is never 0 when type is UDM_SQLTYPE_INT32 */
    if (db->DBType == UDM_DB_MIMER)
      *dst++= 'X';
    *dst++= '\'';
    *dst++= '\'';
    goto ret;
  }

  switch (param->paramTypes[param_num])
  {
    case UDM_SQLTYPE_LONGVARBINARY:
    case UDM_SQLTYPE_LONGVARCHAR  :
    case UDM_SQLTYPE_VARCHAR:
      if (is_bin && (db->flags & UDM_SQL_HAVE_0xHEX))
      {
        /*
          MSSQL:   0x20C883 notation  (0xHEX)
          Sybase:  0x20C883 notation  (0xHEX)
          Access:  0x20C883 notation  (0xHEX)
        */
        *dst++= '0';
        *dst++= 'x';
        dst+= UdmHexEncode(dst, src, srclen);
      }
      else if (is_bin && (db->flags & UDM_SQL_HAVE_STDHEX))
      {
        /*
          SQLite3: X'20C883'          (STDHEX)
        */
        *dst++= 'X';
        *dst++= '\'';
        dst+= UdmHexEncode(dst, src, srclen);
        *dst++= '\'';
      }
      else if (is_bin && (db->DBType == UDM_DB_ORACLE8))
      {
        if (param->paramLengths[param_num] < 0) /* Oracle via ODBC */
        {
          strcpy(dst, "NULL");
          dst+= 4;
        }
        else
        {
          /* Oracle: '20C883', i.e. binary notation by default */
          *dst++= '\'';
          dst+= UdmHexEncode(dst, src, srclen);
          *dst++= '\'';
        }
      }
      else
      {
        /*
          TODO:
          PgSQL:   E'\x20\xC8\x83' notation
          SQLite:  ???
        */
        if (db->DBType == UDM_DB_PGSQL && db->version >= 80101)
          *dst++= 'E';
        *dst++= '\'';
        if (is_bin)
          UdmSQLBinEscStr(db, dst, dstlen, src, srclen); /* TODO: get rid of strlen below*/
        else
          UdmSQLEscStr(db, dst, src, srclen);
        dst+= strlen(dst);
        *dst++= '\'';
      }
      break;
    case UDM_SQLTYPE_INT32:
      return sprintf(dst, "%d", *((const int*) src));
      break;
  }

ret:
  *dst= '\0';
  return dst - dst0;
}


int
UdmSQLExecGeneric(UDM_DB *db)
{
  UDM_PS *ps= (UDM_PS*) db->ps;
  char *qbuf, *dst;
  const char *src;
  size_t qlen= prepared_query_length(ps->sql, ps);
  size_t param_num= 0;
  int rc;
  UDM_SQLRES SQLRes;
  
  if (!(qbuf= UdmMalloc(qlen)))
  {
    udm_snprintf(db->errstr, sizeof(db->errstr),
                 "UdmSQLExecGeneric: Failed to allocated buffer %d bytes",
                 (int) qlen);
    return UDM_ERROR;
  }
  
  for (src= ps->sql, dst= qbuf; *src; src++)
  {
    if (*src == '?')
    {
      size_t len= prepared_query_add_params(db, dst, qlen, ps, param_num);
      param_num++;
      dst+= len;
    }
    else
    {
      *dst++= *src;
    }
  }
  *dst= '\0';

  rc= UdmSQLExecDirect(db, &SQLRes, qbuf);

  UdmSQLFree(&SQLRes);
  UdmFree(qbuf);

  return rc;
}


int
UdmSQLStmtFreeGeneric(UDM_DB *db)
{
  UDM_PS *ps= (UDM_PS*) db->ps;
  UDM_ASSERT(ps->sql);
  UDM_FREE(ps->sql);
  UDM_FREE(db->ps);
  return UDM_OK;
}


/*

Prepared statement API

Name         Native  Generic None Default
----         ------  ------- ---- -------
Oracle       Yes     No      No   Generic
MSSQL        TODO    ?       ?    None
Sybase       TODO    ?       ?    None
MySQL        Yes     Yes     Yes  Native|Generic (client version dependent)
PgSQL        Yes     Yes     Yes  Native|Generic (client version dependend)
IBase        Yes     No      No   Native
SQLite       TODO    Yes     No   Generic
SQLite3      TODO    Yes     Yes  Generic

ODBC-DB2     Yes     No      No   Native
ODBC-MIMER   Yes     No      No   Native
ODBC-ORACLE  Yes     No      No   Native
ODBC-MSQQL   Yes     ?       Yes  None
ODBC-SYBASE  Yes     ?       Yes  None
ODBC-MYSQL   Yes     Yes     Yes  Native
ODDB-IBASE   Yes     No      No   Native

ODBC-CACHE   Yes     ?       ?    Native
ODBC-VIRT    Yes     ?       ?    Native
ODBC-Solid   ?       ?       ?    Native
ODBC-SapDB   ?       ?       ?    Native
ODBC-ACCESS  ?       ?       ?    Native

*/


int
UdmSQLLockOrBeginGeneric(UDM_DB *db, const char *query)
{
  return db->sql->SQLBegin(db);
}

int
UdmSQLUnlockOrCommitGeneric(UDM_DB *db)
{
  return db->sql->SQLCommit(db);
}

#endif /* HAVE_SQL */
