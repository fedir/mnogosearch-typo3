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

#ifndef UDM_SQLDBMS_H
#define UDM_SQLDBMS_H


/* SQL Data types */
#define UDM_SQLTYPE_UNKNOWN       0
#define UDM_SQLTYPE_LONGVARBINARY 1
#define UDM_SQLTYPE_LONGVARCHAR   2
#define UDM_SQLTYPE_VARCHAR       3
#define UDM_SQLTYPE_INT32         4

#define UDM_SQL_NULL_DATA	(-1)
#define UDM_SQL_DATA_AT_EXEC	(-2)
#define UDM_SQL_NTS		(-3)

#define UDM_SQL_HAVE_GROUPBY       1
#define UDM_SQL_HAVE_TRUNCATE      2
#define UDM_SQL_HAVE_SUBSELECT     4
#define UDM_SQL_HAVE_LIMIT         8
#define UDM_SQL_HAVE_TOP          16  /* SELECT TOP 10 FROM t1              */
#define UDM_SQL_HAVE_BIND_BINARY  32  /* Bind BINARY/VARBINARY/BLOB         */
#define UDM_SQL_HAVE_0xHEX        64  /* INSERT INTO t1 VALUES (0xAABBCC)   */
#define UDM_SQL_HAVE_STDHEX      128  /* INSERT INTO t1 VALUES (X'AABBCC')  */
#define UDM_SQL_HAVE_ROWNUM      256  /* ROWNUM to limit result size        */
#define UDM_SQL_HAVE_GOOD_COMMIT 512  /* failing query doesn't stop trasaction */
#define UDM_SQL_IGNORE_ERROR    1024
#define UDM_SQL_DEBUG_QUERY     2048
#define UDM_SQL_HAVE_FIRST_SKIP 4096  /* SELECT FIRST 10 * FROM t1 */
#define UDM_SQL_HAVE_TRANSACT   8192  /* If transactions are  supported */
#define UDM_SQL_HAVE_RENAME         0x00004000 /* If can rename tables */
#define UDM_SQL_HAVE_CREATE_LIKE    0x00008000 /* If can CREATE TABLE LIKE */
#define UDM_SQL_HAVE_DROP_IF_EXISTS 0x00010000 /* If can DROP TABLE IF EXISTS */
#define UDM_SQL_HAVE_BLOB_AS_HEX    0x00200000 /* INSERT INTO t1 (blob1) VALUES ('AABBCC') */
#define UDM_SQL_HAVE_BIND_TEXT      0x00400000 /* Bind CHAR/VARCHAR/TEXT */

#define UDM_SQL_HAVE_BIND (UDM_SQL_HAVE_BIND_BINARY|UDM_SQL_HAVE_BIND_TEXT)

#define UDM_SQL_MAX_BIND_PARAM 64

extern void DecodeHexStr (const char *src, UDM_PSTR *dst, size_t size);
extern int UdmSQLEscDSTR(UDM_DB *db, UDM_DSTR *dst, const char *src, size_t srclen);

size_t UdmSQLEscStrGeneric(UDM_DB *db, char *to, const char *from, size_t len);
size_t UdmSQLEscStr(UDM_DB *db, char *to, const char *from, size_t l);
char*  UdmSQLEscStrAlloc(UDM_DB *db, const char *src, size_t srclen);
char*  UdmSQLEscStrSimple(UDM_DB *db, char *to, const char *from, size_t l);
size_t UdmSQLBinEscStr(UDM_DB *db, char *dst, size_t dstlen, const char *src, size_t srclen);
int UdmSQLTableTruncateOrDelete(UDM_DB *db, const char *name);
int UdmSQLDropTableIfExists(UDM_DB *db, const char *name);
int UdmSQLQueryOneRowInt(UDM_DB *db, int *res, const char *qbuf);

#define UDM_SQL_TOP_BUF_SIZE 64

typedef struct udm_sql_top_st
{
  char rownum[UDM_SQL_TOP_BUF_SIZE];
  char limit[UDM_SQL_TOP_BUF_SIZE];
  char top[UDM_SQL_TOP_BUF_SIZE];
} UDM_SQL_TOP_CLAUSE;
void UdmSQLTopClause(UDM_DB *db, size_t top_num, UDM_SQL_TOP_CLAUSE *Top);

const char *UdmSQLParamPlaceHolder(UDM_DB *db, size_t i);
int UdmSQLLongVarCharBindType(UDM_DB *db);

extern __C_LINK int __UDMCALL _UdmSQLQuery(UDM_DB *db, UDM_SQLRES *R, const char *query, const char *file, const int line);
extern __C_LINK size_t __UDMCALL UdmSQLNumRows(UDM_SQLRES *res);
size_t		UdmSQLNumCols(UDM_SQLRES *res);
size_t		UdmSQLLen(UDM_SQLRES * res,size_t i,size_t j);
extern __C_LINK const char * __UDMCALL UdmSQLValue(UDM_SQLRES * res,size_t i,size_t j);
extern __C_LINK int __UDMCALL UdmSQLFetchRowSimple(UDM_DB *,UDM_SQLRES *res, UDM_PSTR *pstr);
extern __C_LINK int __UDMCALL UdmSQLStoreResultSimple(UDM_DB *,UDM_SQLRES *res);
extern __C_LINK int __UDMCALL UdmSQLFreeResultSimple(UDM_DB *,UDM_SQLRES *res);
extern __C_LINK void __UDMCALL UdmSQLFree(UDM_SQLRES*);
void		UdmSQLClose(UDM_DB *db);
                                                
#ifdef HAVE_DEBUG
extern int UdmSQLBeginDebug(UDM_DB *db, const char *file, const int line);
extern int UdmSQLCommitDebug(UDM_DB *db, const char *file, const int line);
extern int UdmSQLLockOrBeginDebug(UDM_DB *db, const char *param, const char *file, const int line);
extern int UdmSQLUnlockOrCommitDebug(UDM_DB *db, const char *file, const int line);
extern int UdmSQLExecDirectDebug(UDM_DB *db, UDM_SQLRES *R, const char *q, const char *file, const int line);
extern int UdmSQLPrepareDebug(UDM_DB *db, const char *q, const char *file, const int line);
extern int UdmSQLExecuteDebug(UDM_DB *db, const char *file, const int line);
extern int UdmSQLStmtFreeDebug(UDM_DB *db, const char *file, const int line);
extern int UdmSQLBindParameterDebug(UDM_DB *db,
                                    int pos, const void *data, int size, int type,
                                    const char *file, const int line);
extern int UdmSQLRenameTableDebug(UDM_DB *db,
                                  const char *from, const char *to,
                                  const char *file, const int line);
extern int UdmSQLCopyStructureDebug(UDM_DB *db,
                                    const char *from, const char *to,
                                    const char *file, const int line);
#define UdmSQLBegin(db)   UdmSQLBeginDebug(db, __FILE__, __LINE__)
#define UdmSQLCommit(db)  UdmSQLCommitDebug(db, __FILE__, __LINE__)
#define UdmSQLExecDirect(db,R,q) UdmSQLExecDirectDebug(db, R, q, __FILE__, __LINE__)
#define UdmSQLPrepare(db,q) UdmSQLPrepareDebug(db, q, __FILE__, __LINE__)
#define UdmSQLExecute(db) UdmSQLExecuteDebug(db, __FILE__, __LINE__)
#define UdmSQLStmtFree(db) UdmSQLStmtFreeDebug(db, __FILE__, __LINE__)
#define UdmSQLBindParameter(db, pos, data, size, type) UdmSQLBindParameterDebug(db, pos, data, size, type, __FILE__, __LINE__)
#define UdmSQLRenameTable(db, from, to) UdmSQLRenameTableDebug(db, from, to, __FILE__, __LINE__)
#define UdmSQLCopyStructure(db, from, to) UdmSQLCopyStructureDebug(db, from, to, __FILE__, __LINE__)
#define UdmSQLLockOrBegin(db, param) UdmSQLLockOrBeginDebug(db, param, __FILE__, __LINE__)
#define UdmSQLUnlockOrCommit(db)     UdmSQLUnlockOrCommitDebug(db, __FILE__, __LINE__)
#else
extern int UdmSQLBegin(UDM_DB *db);
extern int UdmSQLCommit(UDM_DB *db);
extern int UdmSQLExecDirect(UDM_DB *db, UDM_SQLRES *R, const char *query);
extern int UdmSQLPrepare(UDM_DB *db, const char *query);
extern int UdmSQLExecute(UDM_DB *db);
extern int UdmSQLStmtFree(UDM_DB *db);
extern int UdmSQLBindParameter(UDM_DB *db, int pos, const void *data, int size, int type);
extern int UdmSQLRenameTable(UDM_DB *db, const char *from, const char *to);
extern int UdmSQLCopyStructure(UDM_DB *db, const char *from, const char *to);
extern int UdmSQLLockOrBegin(UDM_DB *db, const char *param);
extern int UdmSQLUnlockOrCommit(UDM_DB *db);
#endif


#define UdmSQLQuery(db, R, query) _UdmSQLQuery(db, R, query, __FILE__, __LINE__)

#define UDM_SQLMON_DISPLAY_FIELDS	1
#define UDM_SQLMON_DONT_NEED_SEMICOLON  2 /* Execute immediately, for --exec */

#define UDM_SQLMON_MSG_ERROR		1
#define UDM_SQLMON_MSG_PROMPT		2

typedef enum udm_sqlmon_mode_enum
{
  udm_sqlmon_mode_batch= 0,
  udm_sqlmon_mode_interactive= 1
} udm_sqlmon_mode_t;

typedef struct udm_sqlmon_param_st
{
  int flags;
  int colflags[10];
  int loglevel;
  size_t nqueries;
  size_t ngood;
  size_t nbad;
  size_t lineno;
  udm_sqlmon_mode_t mode;
  FILE *infile;
  FILE *outfile;
  char *(*gets)(struct udm_sqlmon_param_st *prm, char *str, size_t size);
  int (*display)(struct udm_sqlmon_param_st *, UDM_SQLRES *sqlres);
  int (*prompt)(struct udm_sqlmon_param_st *, int msgtype, const char *msg);
  void* context_ptr;
} UDM_SQLMON_PARAM;

extern __C_LINK int __UDMCALL UdmSQLMonitor(UDM_AGENT *A, UDM_ENV *E, UDM_SQLMON_PARAM *prm);
extern __C_LINK const char* __UDMCALL UdmIndCmdStr(enum udm_indcmd cmd);

#endif
