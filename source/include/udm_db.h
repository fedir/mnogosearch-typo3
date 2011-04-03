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

#ifndef _UDM_DBMS_H
#define _UDM_DBMS_H

#include "udm_utils.h" /* For DSTR */

/* Database type and driver type */
#define UDM_DB_UNK			0
#define UDM_DB_ODBC			1
#define UDM_DB_MYSQL			2
#define UDM_DB_PGSQL			3
#define UDM_DB_SOLID			4
#define UDM_DB_VIRT			6
#define UDM_DB_IBASE			7
#define UDM_DB_ORACLE8			8
#define UDM_DB_ORACLE7			9
#define UDM_DB_MSSQL			10
#define UDM_DB_SAPDB			11
#define UDM_DB_DB2			12
#define UDM_DB_SQLITE			13
#define UDM_DB_ACCESS			14
#define UDM_DB_MIMER			15
#define UDM_DB_CACHE			16
#define UDM_DB_SYBASE			17
#define UDM_DB_SQLITE3			18
#define UDM_DB_MONETDB			19
#define UDM_DB_SEARCHD			200

#define UDM_URL_ACTION_DELETE           0
#define UDM_URL_ACTION_ADD              1
#define UDM_URL_ACTION_SUPDATE          2
#define UDM_URL_ACTION_LUPDATE          3
#define UDM_URL_ACTION_DUPDATE          4
#define UDM_URL_ACTION_UPDCLONE         5
#define UDM_URL_ACTION_REGCHILD         6
#define UDM_URL_ACTION_FINDBYURL        7
#define UDM_URL_ACTION_FINDBYMSG        8
#define UDM_URL_ACTION_FINDORIG         9
#define UDM_URL_ACTION_EXPIRE           10
#define UDM_URL_ACTION_REFERERS         11
#define UDM_URL_ACTION_DOCCOUNT         12
#define UDM_URL_ACTION_LINKS_DELETE     13
#define UDM_URL_ACTION_ADD_LINK         14
#define UDM_URL_ACTION_GET_CACHED_COPY  15
#define UDM_URL_ACTION_WRDSTAT          16
#define UDM_URL_ACTION_DOCPERSITE       17
#define UDM_URL_ACTION_SQLIMPORTSEC     18
#define UDM_URL_ACTION_FLUSH            19
#define UDM_URL_ACTION_DUMPDATA         20 
#define UDM_URL_ACTION_RESTOREDATA      21

#define UDM_RES_ACTION_DOCINFO		1
#define UDM_RES_ACTION_INSWORDS		2
#define UDM_RES_ACTION_SUGGEST		3

#define UDM_CAT_ACTION_PATH		1
#define UDM_CAT_ACTION_LIST		2

#define UDM_SRV_ACTION_TABLE		1
#define UDM_SRV_ACTION_FLUSH            2
#define UDM_SRV_ACTION_ADD              3
#define UDM_SRV_ACTION_ID               4
#define UDM_SRV_ACTION_POPRANK          5

#define BLOB_CACHE_SIZE 0xff

#ifdef WIN32
#define UDM_DEFAULT_ZINT4   1
#define UDM_DEFAULT_DEFLATE 1
#else
#define UDM_DEFAULT_ZINT4   0
#define UDM_DEFAULT_DEFLATE 0
#endif


extern void	UdmDBFree(void *db);

extern int UdmURLActionNoLock(UDM_AGENT *A, UDM_DOCUMENT *D, int cmd);
extern __C_LINK int __UDMCALL UdmURLAction(UDM_AGENT *A, UDM_DOCUMENT   *D, int cmd);
extern __C_LINK int __UDMCALL UdmResAction(UDM_AGENT *A, UDM_RESULT     *R, int cmd);
extern __C_LINK int __UDMCALL UdmCatAction(UDM_AGENT *A, UDM_CATEGORY   *C, int cmd);
extern __C_LINK int __UDMCALL UdmSrvAction(UDM_AGENT *A, UDM_SERVERLIST *S, int cmd);
extern __C_LINK int __UDMCALL UdmTargets(UDM_AGENT *Indexer);


extern __C_LINK UDM_RESULT * __UDMCALL UdmFind(UDM_AGENT *);
extern __C_LINK UDM_RESULT * __UDMCALL UdmFind2(UDM_AGENT *, const char *query);
extern __C_LINK int __UDMCALL UdmClearDatabase(UDM_AGENT *A);
extern __C_LINK int __UDMCALL UdmStatAction(UDM_AGENT *A, UDM_STATLIST *S);

extern __C_LINK int __UDMCALL UdmTrack(UDM_AGENT * query, UDM_RESULT *Res);
extern __C_LINK UDM_RESULT * __UDMCALL UdmCloneList(UDM_AGENT * Indexer, UDM_DOCUMENT *Doc);

extern void		UdmDBListFree(UDM_DBLIST *List);

extern unsigned int     UdmGetCategoryId(UDM_ENV *Conf, char *category);

extern int UdmHTDBGet(UDM_AGENT *Indexer,UDM_DOCUMENT *Doc);

extern int UdmCheckUrlid(UDM_AGENT *A, urlid_t id);

extern int UdmDBSetAddr(UDM_DB *db, const char *dbaddr, int mode);

extern size_t UdmDBNumBySeed(UDM_ENV *Env, udmhash32_t seed);
/******************/

typedef struct st_udm_findword_args
{
  UDM_AGENT *Agent;
  UDM_DB *db;
  UDM_WIDEWORDLIST *WWList;
  UDM_URLID_LIST urls; /* Combination of WHERE limit, fl, and live_deleted */
  UDM_URLID_LIST live_update_active_urls;  /* Combination of WHERE and fl */
  UDM_URLID_LIST live_update_deleted_urls; /* The list of deleted */
  UDM_WIDEWORDLIST CollationMatches;
  UDM_SECTIONLISTLIST SectionListList;
  const char *cmparg;
  const char *where;
  char *wf;
  UDM_WIDEWORD Word;
  int save_section_size;
  int live_updates;
  int need_coords; /* 0 for regular word, 1 for multi-word */
  int use_crosswords;
  int use_qcache;
  UDM_SQLRESLIST SQLResults;
} UDM_FINDWORD_ARGS;




/******************/
/* changed by prv for Win32(COM) */
extern __C_LINK UDM_DBLIST	* __UDMCALL UdmDBListInit(UDM_DBLIST *List);
extern __C_LINK size_t	__UDMCALL UdmDBListAdd(UDM_DBLIST *List, const char * addr, int mode);

extern __C_LINK void * __UDMCALL UdmDBInit(void *db);

/******************/


/******************/

#ifdef HAVE_SQL
extern int (*udm_url_action_handlers[])(UDM_AGENT *A, UDM_DOCUMENT *D, UDM_DB *db);
#define UdmURLActionSQL(A, D, cmd, db) udm_url_action_handlers[cmd](A, D, db)

extern int   UdmClearDBSQL(UDM_AGENT    *A, UDM_DB *db);
extern int   UdmResActionSQL(UDM_AGENT  *A, UDM_RESULT     *R, int cmd, UDM_DB *db, size_t dbnum);
extern int   UdmCatActionSQL(UDM_AGENT  *A, UDM_CATEGORY   *C, int cmd, UDM_DB *db);
extern int   UdmSrvActionSQL(UDM_AGENT  *A, UDM_SERVERLIST *S, int cmd, UDM_DB *db);
extern int   UdmStatActionSQL(UDM_AGENT *A, UDM_STATLIST   *S, UDM_DB *db);
extern int   UdmFindWordsSQL(UDM_AGENT *, UDM_RESULT *Res, UDM_DB *db, size_t num_best_rows);
extern unsigned int   UdmGetCategoryIdSQL(UDM_ENV *Conf, char *category, UDM_DB *db);
extern int UdmResAddDocInfoSQL(UDM_AGENT *query, UDM_DB *db, UDM_RESULT *Res, size_t i);
extern int UdmTrackSQL(UDM_AGENT * query, UDM_RESULT *Res, UDM_DB *db);
extern int UdmCloneListSQL(UDM_AGENT * Indexer, UDM_DOCUMENT *Doc, UDM_RESULT *Res, UDM_DB *db);
extern int UdmTargetsSQL(UDM_AGENT *Indexer, UDM_DB *db);
extern int UdmQueryCachePutSQL(UDM_AGENT *Indexer, UDM_RESULT *Res, UDM_DB *db);
extern int UdmQueryCacheGetSQL(UDM_AGENT *Indexer, UDM_RESULT *Res, UDM_DB *db);
extern int UdmStoreHrefsSQL(UDM_AGENT *Indexer);
extern int UdmDBIsActive(UDM_AGENT *A, size_t num_arg);
#endif

/* MultiCache stuff. */
extern UDM_MULTI_CACHE *UdmMultiCacheInit (UDM_MULTI_CACHE *cache);
extern void UdmMultiCacheFree (UDM_MULTI_CACHE *cache);
extern size_t UdmMultiCacheAdd (UDM_MULTI_CACHE *cache, urlid_t url_id, unsigned char reindex, UDM_WORD *word);
extern int UdmCoordListMultiUnpack(UDM_URLCRDLIST *CoordList,
                                   UDM_URL_CRD *CoordTemplate,
                                   const unsigned char *intag, size_t lintag,
                                   int save_section_size);

extern char*
UdmMultiCachePutIntagUsingEncoding(UDM_MULTI_CACHE_WORD *cache,
                                   UDM_DB *db, char type);

extern char*
UdmMultiCachePutIntagUsingRaw(UDM_MULTI_CACHE_WORD *cache);

extern int UdmConvert2BlobSQL(UDM_AGENT *Indexer, UDM_DB *db);
extern __C_LINK int __UDMCALL UdmMulti2Blob (UDM_AGENT *Indexer);
extern __C_LINK int __UDMCALL UdmRewriteURL(UDM_AGENT *Indexer);
extern __C_LINK int __UDMCALL UdmRewriteLimits(UDM_AGENT *Indexer);
extern int UdmBlobWriteURL(UDM_AGENT *A, UDM_DB *db, const char *table,
    int use_deflate);
extern int UdmBlobWriteLimits(UDM_AGENT *A, UDM_DB *db, const char *table,
    int use_deflate);

/************* DBMode handler **************/

typedef struct udm_dbmode_handler_st
{
  const char *name;
  int (*ToBlob)(UDM_AGENT *Indexer, UDM_DB *db, UDM_URLDATALIST *URLList);
  int (*StoreWords)(UDM_AGENT *Indexer, UDM_DB *db, UDM_DOCUMENT *Doc);
  int (*Truncate)(UDM_AGENT *Indexer, UDM_DB *db);
  int (*DeleteWordsFromURL)(UDM_AGENT *Indexer, UDM_DB *db, urlid_t url_id);
  int (*FindWord)(UDM_FINDWORD_ARGS *args);
  int (*DumpWordInfo)(UDM_AGENT *A, UDM_DB *db, UDM_DOCUMENT *Doc);
  int (*WordStatCreate)(UDM_AGENT *A, UDM_DB *db);
} UDM_DBMODE_HANDLER;

extern UDM_DBMODE_HANDLER udm_dbmode_handler_single;
extern UDM_DBMODE_HANDLER udm_dbmode_handler_multi;
extern UDM_DBMODE_HANDLER udm_dbmode_handler_blob;
extern UDM_DBMODE_HANDLER udm_dbmode_handler_rawblob;


/* Functions from dbmode-blob.c */
extern UDM_BLOB_CACHE *UdmBlobCacheInit (UDM_BLOB_CACHE *cache);
extern void UdmBlobCacheFree (UDM_BLOB_CACHE *cache);
extern size_t UdmBlobCacheAdd (UDM_BLOB_CACHE *cache, urlid_t url_id,
                               unsigned char secno, const char *word,
                               size_t nintags, const char *intag,
                               size_t intag_length);
extern size_t UdmBlobCacheAdd2(UDM_BLOB_CACHE *cache, urlid_t url_id,
                               unsigned char secno, char *word,
                               size_t nintags, char *intag,
                               size_t intag_length);

extern int UdmBlobLoadLiveUpdateLimit(UDM_FINDWORD_ARGS *args, UDM_AGENT *A, UDM_DB *db);

extern int UdmBlobSetTable(UDM_AGENT *A, UDM_DB *db);
extern int UdmBlobGetWTable(UDM_AGENT *A, UDM_DB *db, char *name, size_t namelen);
extern int UdmBlobWriteTimestamp(UDM_AGENT *A, UDM_DB *db, const char *table);
extern int UdmBlobCacheWrite(UDM_AGENT *A, UDM_DB *db,
                             UDM_BLOB_CACHE *cache,
                             const char *table, int use_deflate);
extern int UdmBlobWriteLimitsInternal(UDM_AGENT *A, UDM_DB *db, 
                                      const char *table,
                                      int use_deflate);
extern int UdmBlobLoadFastURLLimit(UDM_AGENT *A, UDM_DB *db, const char *name,
                                   UDM_URLID_LIST *buf);
extern int UdmBlobLoadFastOrder(UDM_AGENT *A, UDM_DB *db, UDM_URL_INT4_LIST *,const char *name);
extern int UdmBlobLoadFastScore(UDM_AGENT *A, UDM_DB *db, UDM_URL_INT4_LIST *,const char *name);
extern int UdmLoadURLDataFromBdict(UDM_AGENT *A, UDM_URLDATALIST *DataList,
                                   UDM_DB *db, size_t num_best_rows, int flags);

extern UDM_COORD2 *UdmBlobPackedCoordsToUnpackedCoords(const unsigned char *s,
                                                       const unsigned char *e,
                                                       size_t nrecs,
                                                       UDM_COORD2 *C,
                                                       UDM_COORD2 *Coord,
                                                       const unsigned char **end);

/* Functions from dbmode-single.c */
extern int UdmStoreCrossWords(UDM_AGENT *Indexer, UDM_DOCUMENT *Doc,UDM_DB *db);
extern int UdmTruncateCrossDict(UDM_AGENT *Indexer,UDM_DB *db);
extern int UdmDeleteCrossWordFromURL(UDM_AGENT *Indexer,
                                     UDM_DOCUMENT *Doc, UDM_DB *db);
extern int UdmFindCrossWord(UDM_FINDWORD_ARGS *args);
extern int UdmURLCRDListListAddWithSort2(UDM_FINDWORD_ARGS *args,
                                         UDM_URLCRDLIST *CoordList);
extern int UdmMultiWordAdd(UDM_FINDWORD_ARGS *args,
                           UDM_SECTIONLISTLIST *OriginalSectionListList,
                           size_t orig_wordnum, size_t nparts);

extern int UdmAddOneCoord(UDM_URLCRDLIST *CoordList,
                          urlid_t url_id, uint4 coord, udm_wordnum_t num);

extern int UdmLoadURLDataFromURL(UDM_AGENT *A, UDM_URLDATALIST *DataList,
                                 UDM_DB *db, size_t num_best_rows, int flag);

/* Functions from dbmode-rawblob.c */
#define UDM_RAWBLOB_SEARCH  0
#define UDM_RAWBLOB_DELTA   1
extern int UdmFindWordRawBlobDelta(UDM_FINDWORD_ARGS *args);


/* Functions from sql.c */
extern int UdmFindURL(UDM_AGENT *Indexer, UDM_DOCUMENT * Doc, UDM_DB *db);
extern int UdmApplyFastLimit(UDM_URLCRDLIST *Coord, UDM_URLID_LIST *urls);
extern int UdmBuildCmpArgSQL(UDM_DB *db, int match, const char *word,
                             char *cmparg, size_t maxlen);
extern const char* UdmSQLBuildWhereCondition(UDM_ENV * Conf,UDM_DB *db);

/***************/

int       
UdmLoadSlowLimit(UDM_AGENT *A, UDM_DB *db, UDM_URLID_LIST *list, const char *q);

int
UdmUserScoreListLoad(UDM_AGENT *A, UDM_DB *db,
                     UDM_URL_INT4_LIST *List, const char *q);


/**************/

extern int UdmExport (UDM_AGENT *Indexer);
extern int UdmExportSQL (UDM_AGENT *Indexer, UDM_DB *db);

extern void UdmDecodeHex8Str(const char *hex_str, uint4 *hi, uint4 *lo, uint4 *fhi, uint4 *flo);
extern __C_LINK int __UDMCALL UdmAddSearchLimit(UDM_AGENT *Agent, int type, const char *file_name, const char *val);

extern int UdmCheckUrlidSQL(UDM_AGENT *A, UDM_DB *db, urlid_t id);

extern int UdmBlobReadTimestamp(UDM_AGENT *A, UDM_DB *db, int *ts, int def);

int UdmApplyCachedQueryLimit(UDM_AGENT *query,
                             UDM_URLSCORELIST *ScoreList, UDM_DB *db);

extern __C_LINK const char* __UDMCALL UdmDBTypeToStr(int dbtype);
extern __C_LINK const char* __UDMCALL UdmDBModeToStr(int dbmode);


/* Functions from wordcache.c */
extern int UdmWordCacheAddWordList(UDM_WORD_CACHE *Cache,
                                   UDM_WORDLIST *List, urlid_t id);
extern UDM_WORD_CACHE *UdmWordCacheInit (UDM_WORD_CACHE *cache);
extern void UdmWordCacheFree (UDM_WORD_CACHE *cache);
extern void UdmWordCacheSort (UDM_WORD_CACHE *cache);
extern int UdmWordCacheAdd (UDM_WORD_CACHE *cache, urlid_t url_id, UDM_WORD *W);
extern int UdmWordCacheAddURL (UDM_WORD_CACHE *cache, urlid_t url_id);
extern int UdmWordCacheWrite (UDM_AGENT *Indexer, UDM_DB *db, size_t limit);
extern int UdmWordCacheFlush (UDM_AGENT *Indexer);

/* Functions from suggest.c */
extern int UdmWordStatQuery(UDM_AGENT *A, UDM_DB *db, const char *src);
extern int UdmWordStatCreate(UDM_AGENT *A, UDM_DOCUMENT *D, UDM_DB *db);
extern int UdmResSuggest(UDM_AGENT *A, UDM_DB *db, UDM_RESULT *Res, size_t dbnum);

#endif
