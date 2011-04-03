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

#ifndef _UDM_DB_INT_H
#define _UDM_DB_INT_H

#include <limits.h>


/* Multi-dict mode defines */
#define MULTI_DICTS	0xff

#define	URL_DELETE_CACHE		256
#define	URL_SELECT_CACHE		256
#define URL_LOCK_TIME			4*60*60
#define UDM_MAX_MULTI_INSERT_QSIZE	16*1024
#define NDOCS_QUERY			"SELECT count(*) FROM url"

typedef struct {
	char	*sqlname;
	int	sqltype;
	int	sqllen;
} UDM_SQLFIELD;

typedef struct {
	size_t	len;
	char	*val;
} UDM_PSTR;

typedef struct {
	size_t nRows;
	size_t nCols;
	size_t curRow;
	
	UDM_SQLFIELD	*Fields;
	UDM_PSTR	*Items;
	
	void *specific;
	struct udm_db_st *db;
} UDM_SQLRES;


int
UdmSQLResFreeGeneric(UDM_SQLRES *res);

typedef struct udm_sql_res_list_st
{
  size_t nitems;
  UDM_SQLRES *Item;
} UDM_SQLRESLIST;

void UdmSQLResListInit(UDM_SQLRESLIST *List);
int UdmSQLResListAdd(UDM_SQLRESLIST *List, UDM_SQLRES *Res);
void UdmSQLResListFree(UDM_SQLRESLIST *List);


/********************************************/

typedef struct {
	char *word;
	size_t nintags;
	uint4 *intags;
} UDM_MULTI_CACHE_WORD;

typedef struct {
	unsigned char secno;
	size_t nwords;
	UDM_MULTI_CACHE_WORD *words;
} UDM_MULTI_CACHE_SECTION;

typedef struct {
	urlid_t url_id;
	unsigned char reindex;
	size_t nsections;
	UDM_MULTI_CACHE_SECTION *sections;
} UDM_MULTI_CACHE_URL;

typedef struct {
	size_t nurls;
	UDM_MULTI_CACHE_URL *urls;
} UDM_MULTI_CACHE_TABLE;

typedef struct {
	unsigned char free;
	size_t nrecs;
	UDM_MULTI_CACHE_TABLE tables[MULTI_DICTS + 1];
	size_t nurls;
	urlid_t *urls;
} UDM_MULTI_CACHE;

/****************************************/

typedef struct {
	char *word;
	urlid_t url_id;
	size_t nintags;    /* Length in coords */
	size_t ntaglen;    /* Length in bytes  */
	char *intags;
	unsigned char secno;
	unsigned char freeme; /* Whether word was alloced */
} UDM_BLOB_CACHE_WORD;

typedef struct {
	unsigned char free;
	size_t errors;
	size_t nwords;
	size_t awords;
	UDM_BLOB_CACHE_WORD *words;
} UDM_BLOB_CACHE;


/********************************************/

typedef struct {
	urlid_t url_id;
	char    *word;
	udm_pos_t pos;
	unsigned char secno;
	unsigned char seed;
} UDM_WORD_CACHE_WORD;

typedef struct {
	int free;
	size_t nbytes;
	size_t nwords;
	size_t awords;
	UDM_WORD_CACHE_WORD *words;
	size_t nurls;
	size_t aurls;
	urlid_t *urls;
} UDM_WORD_CACHE;


/*******************************************/
struct udm_db_st;
struct udm_dbmode_handler_st;

typedef struct udm_db_handler_st
{
  size_t (*SQLEscStr)(struct udm_db_st *db, char *to, const char *from, size_t len);
  int    (*SQLQuery)(struct udm_db_st *db, UDM_SQLRES *R, const char *query);
  int    (*SQLConnect)(struct udm_db_st *db);
  void   (*SQLClose)(struct udm_db_st *db);
  int    (*SQLBegin)(struct udm_db_st *db);
  int    (*SQLCommit)(struct udm_db_st *db);
  int    (*SQLPrepare)(struct udm_db_st *db, const char *query);
  int    (*SQLBind)(struct udm_db_st *db, int position, const void *data, int size, int type);
  int    (*SQLExec)(struct udm_db_st *db);
  int    (*SQLStmtFree)(struct udm_db_st *db);
  int    (*SQLFetchRow)(struct udm_db_st *db, UDM_SQLRES *R, UDM_PSTR *);
  int    (*SQLStoreResult)(struct udm_db_st *db, UDM_SQLRES *R);
  int    (*SQLFreeResult)(struct udm_db_st *db, UDM_SQLRES *R);
  int    (*SQLExecDirect)(struct udm_db_st *db, UDM_SQLRES *R, const char *q);
  int    (*SQLRenameTable)(struct udm_db_st *db, const char *from, const char *to);
  int    (*SQLCopyStructure)(struct udm_db_st *db, const char *from, const char *to);
  int    (*SQLLockOrBegin)(struct udm_db_st *db, const char *param);
  int    (*SQLUnlockOrCommit)(struct udm_db_st *db);
} UDM_SQLDB_HANDLER;

extern UDM_SQLDB_HANDLER udm_sqldb_ctlib_handler;
extern UDM_SQLDB_HANDLER udm_sqldb_mysql_handler;
extern UDM_SQLDB_HANDLER udm_sqldb_pgsql_handler;
extern UDM_SQLDB_HANDLER udm_sqldb_ibase_handler;
extern UDM_SQLDB_HANDLER udm_sqldb_sqlite_handler;
extern UDM_SQLDB_HANDLER udm_sqldb_sqlite3_handler;
extern UDM_SQLDB_HANDLER udm_sqldb_odbc_handler;
extern UDM_SQLDB_HANDLER udm_sqldb_oracle_handler;

typedef struct udm_db_st
{
	int	freeme;
	char	*DBName;
	int	DBMode;
	char	*where;
        char    *from;
	int	DBType;
	int	DBDriver;
	int	version; /* vendor specific version */
	int	DBSQL_IN;
        int     flags;
	
	int	connected;
	int	res_limit;
	int	commit_fl;
	unsigned int	numtables;
	int	errcode;
	char	errstr[2048];
	
	int		  searchd;    /* Searchd daemon descriptor         */
        UDM_VARLIST       Vars;       /* optional parameters and variables */
	UDM_SQLDB_HANDLER *sql;
	struct udm_dbmode_handler_st *dbmode_handler;
	void	*specific;
	void    *ps;                  /* for prepared statements */

	UDM_WORD_CACHE WordCache;
} UDM_DB;

int UdmSQLPrepareGeneric(UDM_DB *db, const char *query);
int UdmSQLBindGeneric(UDM_DB *db, int position, const void *data, int size, int type);
int UdmSQLExecGeneric(UDM_DB *db);
int UdmSQLStmtFreeGeneric(UDM_DB *db);
int UdmSQLLockOrBeginGeneric(UDM_DB *, const char *param);
int UdmSQLUnlockOrCommitGeneric(UDM_DB *);
#endif
