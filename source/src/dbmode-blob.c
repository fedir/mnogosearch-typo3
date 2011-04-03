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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_db.h"
#include "udm_db_int.h"
#include "udm_sqldbms.h"
#include "udm_vars.h"
#include "udm_mutex.h"
#include "udm_searchtool.h"
#include "udm_log.h"
#include "udm_hash.h"
#include "udm_word.h"
#include "udm_crc32.h"

#ifdef HAVE_SQL

udm_timer_t timer_blob_cache_load= 0;
udm_timer_t timer_blob_cache_sort= 0;
udm_timer_t timer_blob_cache_pack= 0;
udm_timer_t timer_blob_cache_send= 0;
udm_timer_t timer_blob_cache_conv= 0;

udm_timer_t timer_blob_pair_to_pl= 0;
udm_timer_t timer_blob_pair_to_cache= 0;
udm_timer_t timer_blob_pair_sort_word= 0;
udm_timer_t timer_blob_words_cache_add= 0;

/********** Record encoding and compression ************/
/*
  Record compression types:
  - 0x01 = Deflate
  - 0x02 = Zint4
  - 0x03 = Zint4+Deflate
  - 0x04 = Single URL_ID range
*/

#define UDM_BLOB_COMP_NONE                            0x00
#define UDM_BLOB_COMP_DEFLATE                         0x01
#define UDM_BLOB_COMP_ZINT4                           0x02
#define UDM_BLOB_COMP_ZINT4_DEFLATE                   0x03
#define UDM_BLOB_COMP_SINGLE_RANGE                    0x04
#define UDM_BLOB_COMP_URLID_DELTA_VARIABLE            0x05
#define UDM_BLOB_COMP_URLID_DELTA_2BYTES              0x06
#define UDM_BLOB_COMP_URLID_DELTA_2BYTES_WITH_OFFSET  0x07
#define UDM_BLOB_COMP_URLID_DELTA_3BYTES              0x08
#define UDM_BLOB_COMP_URLID_DELTA_1BYTES_WITH_OFFSET  0x09
#define UDM_BLOB_COMP_URLID_RANGE_MULTI               0x0A


static inline int
UdmDSTRAppendCompressionType(UDM_DSTR *dstr, int compression_type)
{
  if (!UdmDSTRAppendINT4(dstr, 0xFFFFFFFF))
    return 0;
  return (compression_type <= UDM_BLOB_COMP_ZINT4_DEFLATE) ?
         UdmDSTRAppendINT4(dstr, compression_type) :
         UdmDSTRAppendINT2(dstr, compression_type);
}


/******************** BlobCache stuff *******************/

UDM_BLOB_CACHE*
UdmBlobCacheInit(UDM_BLOB_CACHE *cache)
{
  if (!cache)
  {
    cache= UdmMalloc(sizeof(UDM_BLOB_CACHE));
    if (!cache)
      return NULL;
    cache->free= 1;
  }
  else
  {
    cache->free= 0;
  }
  cache->errors= 0;
  cache->nwords= 0;
  cache->awords= 0;
  cache->words= NULL;

  return cache;
}


void
UdmBlobCacheFree(UDM_BLOB_CACHE *cache)
{
  size_t i;
  for (i= 0; i < cache->nwords; i++)
  {
    if (cache->words[i].freeme)
      UDM_FREE(cache->words[i].word);
  }

  UDM_FREE(cache->words);
  cache->errors= 0;
  cache->nwords= 0;
  cache->awords= 0;
  cache->words= NULL;

  if (cache->free)
    UdmFree(cache);
}


static inline int
UdmBlobCacheCheckValue(urlid_t url_id, unsigned char secno,
                       const char *word, size_t nintags,
                       const char *intag, size_t intaglen)
{
  if (! url_id)
  {
    fprintf(stderr, "url_id variable empty\n");
    return 1;
  }
  if (! secno)
  {
    fprintf(stderr, "secno variable empty\n");
    return 1;
  }
  if (! word)
  {
    fprintf(stderr, "word variable empty\n");
    return 1;
  }
  if (! nintags)
  {
    fprintf(stderr, "nintags variable empty\n");
    return 1;
  }
  if (! intag)
  {
    fprintf(stderr, "intag variable empty\n");
    return 1;
  }
  return 0;
}


#define BLOB_CACHE_REALLOC 16*1024
static inline int
UdmBlobCacheRealloc(UDM_BLOB_CACHE *cache)
{
  if (cache->nwords == cache->awords)
  {
    UDM_BLOB_CACHE_WORD *tmp;
    size_t nbytes= (cache->awords + BLOB_CACHE_REALLOC) * sizeof(UDM_BLOB_CACHE_WORD);
    tmp= UdmRealloc(cache->words, nbytes);
    if (!tmp)
    {
      cache->errors++;
      if (cache->errors < 10 || (cache->errors % 2048) == 0)
      fprintf(stderr, "BlobCacheRealloc: failed %d times: %d bytes, %d records\n",
              cache->errors, nbytes, (cache->awords + 256));
      return 1;
    }
    cache->words = tmp;
    cache->awords += BLOB_CACHE_REALLOC;
  }
  return 0;
}


/*
 Adding with allocating memory for word
*/
size_t
UdmBlobCacheAdd(UDM_BLOB_CACHE *cache, urlid_t url_id,
                unsigned char secno, const char *word,
                size_t nintags, const char *intag, size_t intaglen)
{
  size_t word_len;
  UDM_BLOB_CACHE_WORD *W;
  
  if (UdmBlobCacheCheckValue(url_id, secno, word, nintags, intag, intaglen) ||
      UdmBlobCacheRealloc(cache))
    return 0;
  
  word_len= strlen(word);
  W= &cache->words[cache->nwords];
  W->secno= secno;
  W->url_id= url_id;
  W->nintags= nintags;
  W->ntaglen= intaglen;
  W->word= UdmMalloc(word_len + intaglen + 2);
  W->intags= W->word + word_len + 1;
  memcpy(W->word, word, word_len + 1);
  memcpy(W->intags, intag, intaglen);
  W->intags[intaglen]= '\0';
  W->freeme= 1;

  cache->nwords++;

  return(1);
}


/*
  Adding word without allocation (taking over from the caller function)
*/
size_t
UdmBlobCacheAdd2(UDM_BLOB_CACHE *cache, urlid_t url_id,
                 unsigned char secno, char *word,
                 size_t nintags, char *intag, size_t intaglen)
{
  UDM_BLOB_CACHE_WORD *W;
  
  if (UdmBlobCacheCheckValue(url_id, secno, word, nintags, intag, intaglen) ||
      UdmBlobCacheRealloc(cache))
    return 0;
  
  W= &cache->words[cache->nwords];
  W->secno= secno;
  W->url_id= url_id;
  W->nintags= nintags;
  W->ntaglen= intaglen;
  W->word= word;
  W->intags= intag;
  W->freeme= 0;

  cache->nwords++;

  return(1);
}


static int
bccmpwrd(UDM_BLOB_CACHE_WORD *s1, UDM_BLOB_CACHE_WORD *s2)
{
  int _ = strcmp(s1->word, s2->word);
  if (! _) _ = s1->secno - s2->secno;
  if (! _)
  {
    if (s1->url_id > s2->url_id) _ = 1;
    else if (s1->url_id < s2->url_id) _ = -1;
    else _ = 0;
  }
  return(_);
}


static void
UdmBlobCacheSort(UDM_BLOB_CACHE *cache)
{
  if (cache->nwords)
    UdmSort(cache->words, cache->nwords, sizeof(UDM_BLOB_CACHE_WORD), (udm_qsort_cmp)bccmpwrd);
}


/*******************************************/

/*
  If can do "indexer -Eblob" using RENAME TABLE.
*/
static int
UdmBlobCanDoRename(UDM_DB *db)
{
  return
    (db->flags & UDM_SQL_HAVE_RENAME) &&
    (db->flags & UDM_SQL_HAVE_CREATE_LIKE) &&
    /* PgSQL can do RENAME only when "DROP TABLE IF EXISTS" is supported */
    (db->DBType != UDM_DB_PGSQL || db->flags & UDM_SQL_HAVE_DROP_IF_EXISTS);
}


static int
UdmBlobCreateIndexRandomName(UDM_DB *db, const char *table_name)
{
  char qbuf[128];
  /* Create index with an unique name */
  if (db->DBType == UDM_DB_MYSQL)
  {
    udm_snprintf(qbuf, sizeof(qbuf),
                 "ALTER TABLE %s ADD KEY (word)", table_name);
  }
  else
  {
    udm_snprintf(qbuf, sizeof(qbuf),
                "CREATE INDEX bdict_%d_%d ON %s (word)",
                 (int) time(0), (int) (UdmStartTimer() % 0xFFFF), table_name);
  }
  return UdmSQLQuery(db, NULL, qbuf);
}


static int
UdmBlobGetTable(UDM_AGENT *A, UDM_DB *db)
{
  UDM_SQLRES SQLRes;
  int rc;
  const char *val;

  return(1);

  rc = UdmSQLQuery(db, &SQLRes, "SELECT n FROM bdictsw");
  if (rc != UDM_OK) return(1);

  if (! UdmSQLNumRows(&SQLRes) || ! (val = UdmSQLValue(&SQLRes, 0, 0))) rc = 2;
  else if (*val != '1') rc = 3;
  else rc = 4;

  UdmSQLFree(&SQLRes);
  return(rc);
}


/*
  This function returns "bdict" by default,
  or the "bdict" parameter from DBAddr, if exists.
*/
static const char *
UdmBlobGetTableNamePrefix(UDM_DB *db)
{
  return UdmVarListFindStr(&db->Vars, "bdict", "bdict");
}


/*
  This function is used when "indexer -Erewritelimit"
  or "indexer -Erewriteurl" is called.
*/
static size_t
UdmBlobGetTableForRewrite(UDM_AGENT *A, UDM_DB *db, char *dst, size_t dstlen)
{
  const char *prefix= UdmBlobGetTableNamePrefix(db);
  return udm_snprintf(dst, dstlen, "%s", prefix);
}


static size_t
UdmBlobGetRTable(UDM_AGENT *A, UDM_DB *db, char *dst, size_t dstlen)
{
  const char *prefix= UdmBlobGetTableNamePrefix(db);
  if (db->DBType == UDM_DB_MYSQL)
    return udm_snprintf(dst, dstlen, "%s", prefix);
  if (UdmBlobGetTable(A, db) == 3)
    return udm_snprintf(dst, dstlen, "%s00", prefix);
  return udm_snprintf(dst, dstlen, "%s", prefix);
}


int
UdmBlobGetWTable(UDM_AGENT *A, UDM_DB *db, char *name, size_t namelen)
{
  int rc;

  if (UdmBlobCanDoRename(db))
  {
    if ((UDM_OK != (rc= UdmSQLDropTableIfExists(db, "bdict_tmp"))) ||
        (UDM_OK != (rc= UdmSQLCopyStructure(db, "bdict", "bdict_tmp"))) ||
        (UDM_OK != (rc= UdmBlobCreateIndexRandomName(db, "bdict_tmp"))))
      return rc;
    udm_snprintf(name, namelen, "bdict_tmp");
    return UDM_OK;
  }
  
  udm_snprintf(name, namelen, UdmBlobGetTableNamePrefix(db));
  if (UdmBlobGetTable(A, db) == 4)
    udm_snprintf(name, namelen, "%s00", UdmBlobGetTableNamePrefix(db));
  return UDM_OK;
}


int
UdmBlobSetTable(UDM_AGENT *A, UDM_DB *db)
{
  char qbuf[128];
  int rc, t, n;
  const char *table_name= UdmVarListFindBool(&A->Conf->Vars, "delta", 0) ?
                          "bdict_delta" :
                          UdmBlobGetTableNamePrefix(db);

  if (UdmBlobCanDoRename(db))
  {
    if (UDM_OK == (rc= UdmSQLDropTableIfExists(db, table_name)))
      rc= UdmSQLRenameTable(db, "bdict_tmp", table_name);
    return rc;
  }

  t= UdmBlobGetTable(A, db);
  if (t == 1) return(UDM_OK);
  else if (t == 4) n = 0;
  else n = 1;

  rc = UdmSQLQuery(db, NULL, "DELETE FROM bdictsw");
  if (rc != UDM_OK) return(UDM_OK);
  udm_snprintf(qbuf, sizeof(qbuf), "INSERT INTO bdictsw VALUES(%d)", n);
  rc = UdmSQLQuery(db, NULL, qbuf);
  if (rc != UDM_OK) return(UDM_OK);
  return(UDM_OK);
}


/*******************************************/

int
UdmRewriteURL(UDM_AGENT *Indexer)
{
#ifdef HAVE_SQL
  size_t i;
  udm_timer_t ticks= UdmStartTimer();

  UdmLog(Indexer,UDM_LOG_ERROR,"Rewriting URL data");

  for (i = 0; i < Indexer->Conf->dbl.nitems; i++)
  {
    UDM_DB *db= &Indexer->Conf->dbl.db[i];
    int rc, use_deflate, tr= (db->flags & UDM_SQL_HAVE_TRANSACT) ? 1 : 0;
    char tablename[64];
    if (!UdmDBIsActive(Indexer, i))
      continue;
    UDM_GETLOCK(Indexer, UDM_LOCK_DB);
    UdmBlobGetTableForRewrite(Indexer, db, tablename, sizeof(tablename));
    use_deflate= UdmVarListFindBool(&db->Vars, "deflate", 0);
    
    if ((tr && UDM_OK != (rc= UdmSQLBegin(db))) ||
        UDM_OK != (rc= UdmBlobWriteURL(Indexer, db, tablename, use_deflate)) ||
        (tr && UDM_OK != (rc= UdmSQLCommit(db))))
      return rc;
    UDM_RELEASELOCK(Indexer, UDM_LOCK_DB);
    if (rc != UDM_OK)
    {
      UdmLog(Indexer,UDM_LOG_ERROR,"%s",db->errstr); 
      return rc;
    }
  }

  UdmLog(Indexer,UDM_LOG_ERROR,"Converting to blob finished\t%.2f", UdmStopTimer(&ticks));
#endif
  return UDM_OK;
}


int
UdmRewriteLimits(UDM_AGENT *Indexer)
{
#ifdef HAVE_SQL
  size_t i;
  udm_timer_t ticks= UdmStartTimer();

  UdmLog(Indexer,UDM_LOG_ERROR,"Rewriting limits");

  for (i = 0; i < Indexer->Conf->dbl.nitems; i++)
  {
    int rc;
    UDM_DB *db= &Indexer->Conf->dbl.db[i];
    char tablename[64];
    int use_deflate;
    
    if (!UdmDBIsActive(Indexer, i))
      continue;
    UDM_GETLOCK(Indexer, UDM_LOCK_DB);
    use_deflate= UdmVarListFindBool(&db->Vars, "deflate", 0);
    UdmBlobGetTableForRewrite(Indexer, db, tablename, sizeof(tablename));
    rc= UdmBlobWriteLimits(Indexer, db, tablename, use_deflate);
    UDM_RELEASELOCK(Indexer, UDM_LOCK_DB);
    if (rc != UDM_OK)
    {
      UdmLog(Indexer,UDM_LOG_ERROR,"%s",db->errstr); 
      return rc;
    }
  }

  UdmLog(Indexer,UDM_LOG_ERROR,"Rewriting limits\t%.2f",UdmStopTimer(&ticks));
#endif
  return UDM_OK;
}

#define COORD_MAX_LEN 4


/*********** Coord tools *******************/

/*
 Position and length encoding components.
 Based on UTF-8 encoding:
  
 1. 0-7F         [00-7F]                      0xxxxxxx
 2. 80-7FF       [C2-DF][80-BF]               110yyyxx 10xxxxxx
 3. 8000-FFFF    [E0-EF][80-BF][80-BF]        1110yyyy 10yyyyxx 10xxxxxx
 4. 10000-10FFFF [F0-F4][80-BF][80-BF][80-BF] 11110zzz 10zzyyyy 10yyyyxx 10xxxxxx

Unused overlong sequences:
2. [C0-C1][?]                   1100000x 10xxxxxx
3. [E0-E0][80-9F][80-BF]        11100000 100xxxxx 10xxxxxx
4. 11110000 1000xxxx 10xxxxxx 10xxxxxx
5. 11111000 10000xxx 10xxxxxx 10xxxxxx 10xxxxxx
6. 11111100 100000xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx


Used bytes:
 [00-7F] 0xxxxxxx Single byte sequence
 [80-BF] 10xxxxxx Second, third and fourth byte in a multi-byte sequence
 [C2-DF] 110xxxxx Start of a two-byte sequence
 [E0-EF] 1110xxxx Start of a three-byte sequence
 [F0-F4] 11110xxx Start of a four-byte sequence
         11110000
         11110001
         11110010
         11110011
         11110100
 
Unused bytes:
 [C0-C1] Overlong encoding: start of a 2-byte sequence,    codepoint <= 127
 [F5-F7] 11110xxx Restricted by RFC 3629: start of 4-byte sequence, codepoint > 10FFFF
         11110101
         11110110
         11110111
         
 [F8-FB] 111110xx Restricted by RFC 3629: start of 5-byte sequence
 [FC-FD] 1111110x Restricted by RFC 3629: start of 6-byte sequence
 [FE-FF] 1111111x Invalid


Example statistics for DBMode=blob.
==================================
Collection: Apache manual.
Documents: 1253     = select count(*) from bdicti;
Index size: 6158768 =  select sum(length(intag)) from bdict where word not like '#%';
- URL_ID size: 31% = 1970652 = 492663 times x 4bytes-
- Coord size:  69% = 4188116 bytes
  492082 coords/lengths use 1 byte.
  581 coords/lengths use 2 bytes.

Top 16 words:
mysql> select word, secno, length(intag) from bdict where word not like '#%' order by 3 desc limit 16;
+--------+-------+---------------+
| word   | secno | length(intag) |
+--------+-------+---------------+
| the    |     1 |        121499 | 
| to     |     1 |         57876 | 
| a      |     1 |         46039 | 
| of     |     1 |         41643 | 
| is     |     1 |         40950 | 
| and    |     1 |         36484 | 
| in     |     1 |         35399 | 
| for    |     1 |         33173 | 
| this   |     1 |         31080 | 
| server |     1 |         30422 | 
| be     |     1 |         29393 | 
| apache |     1 |         29022 | 
| that   |     1 |         24360 | 
| it     |     1 |         23114 | 
| on     |     1 |         22491 | 
| are    |     1 |         22449 | 
+--------+-------+---------------+
16 rows in set (0.02 sec)

16   top words use 10% disk space, 17-96 bytes for word coords per document.
51   top words use 20% disk space, 10-96
110  top words use 30% disk space, 7-96
202  top words use 40% disk space, 4-96
350  top words use 50% disk space, 2.7-96
574  top words use 60% disk space
937  top words use 70% disk space
1593 top words use 80% disk space
3081 top words use 90% disk space

The same collection with Deflate=yes
====================================
Index size: 2680858

Top 16 words:
+--------+-------+---------------+
| word   | secno | length(intag) |
+--------+-------+---------------+
| the    |     1 |         25949 | 
| to     |     1 |         13272 | 
| a      |     1 |         10341 | 
| of     |     1 |          9938 | 
| is     |     1 |          9835 | 
| and    |     1 |          9323 | 
| in     |     1 |          9087 | 
| for    |     1 |          8709 | 
| this   |     1 |          8486 | 
| server |     1 |          8048 | 
| be     |     1 |          8024 | 
| apache |     1 |          7672 | 
| that   |     1 |          7019 | 
| it     |     1 |          6816 | 
| by     |     1 |          6760 | 
| are    |     1 |          6692 | 
+--------+-------+---------------+

35 top words use 10% disk space
97 top words use 20% disk space

The same collection, 1 byte per URL_ID (delta)
4680779

The same collation, 1 byte per URL_ID (delta), Deflate=yes
2289516

*/


/*
  Scan one coord.
  Return the number of bytes scanned.
*/
static inline size_t
udm_coord_get(size_t *pwc, const unsigned char *s, const unsigned char *e)
{
  unsigned char c;
  
  if (s >= e)
    return 0;
  
  c= s[0];
  if (c < 0x80) 
  {
    *pwc = c;
    return 1;
  } 
  else if (c < 0xc2) 
    return UDM_ERROR;
  else if (c < 0xe0) 
  {
    if (s+2 > e) /* We need 2 characters */ 
      return 0;
    
    if (!((s[1] ^ 0x80) < 0x40))
      return 0;
    
    *pwc = ((size_t) (c & 0x1f) << 6) | (size_t) (s[1] ^ 0x80);
    return 2;
  } 
  else if (c < 0xf0) 
  {
    if (s+3 > e) /* We need 3 characters */
      return 0;
    
    if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40 && (c >= 0xe1 || s[1] >= 0xa0)))
      return 0;
    
    *pwc = ((size_t) (c & 0x0f) << 12)   | 
           ((size_t) (s[1] ^ 0x80) << 6) | 
            (size_t) (s[2] ^ 0x80);
    
    return 3;
  } 
  else if (c < 0xf8)
  {
    if (s + 4 > e) /* Need 4 bytes */
      return 0;
    if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40 && (s[3] ^ 0x80) < 0x40 && (c >= 0xf1 || s[1] >= 0x90)))
      return 0;
    
    *pwc= ((size_t) (c & 0x07) << 18)    |
          ((size_t) (s[1] ^ 0x80) << 12) |
          ((size_t) (s[2] ^ 0x80) << 6)  |
          (size_t)  (s[3] ^ 0x80);
    return 4;
  }
  return 0;
}


/*
  A faster version, without range checking
  The incoming string must have enough space
  to scan full miltibyte sequence.
*/
static inline size_t
udm_coord_get_quick(size_t *pwc, const unsigned char *s)
{
  unsigned char c;
  
  c= s[0];
  if (c < 0x80) 
  {
    *pwc = c;
    return 1;
  } 
  else if (c < 0xc2) 
    return UDM_ERROR; /* The caller will skip one byte */
  else if (c < 0xe0) 
  {
    size_t s1;
    if (!((s1= (s[1] ^ 0x80)) < 0x40))
      return 0;
    *pwc= (((size_t) (c & 0x1f)) << 6) | s1;
    return 2;
  } 
  else if (c < 0xf0) 
  {
    if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40 && (c >= 0xe1 || s[1] >= 0xa0)))
      return 0;
    
    *pwc = ((size_t) (c & 0x0f) << 12)   | 
           ((size_t) (s[1] ^ 0x80) << 6) | 
            (size_t) (s[2] ^ 0x80);
    
    return 3;
  } 
  else if (c < 0xf8)
  {
    if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40 && (s[3] ^ 0x80) < 0x40 && (c >= 0xf1 || s[1] >= 0x90)))
      return 0;
    
    *pwc= ((size_t) (c & 0x07) << 18)    |
          ((size_t) (s[1] ^ 0x80) << 12) |
          ((size_t) (s[2] ^ 0x80) << 6)  |
          (size_t)  (s[3] ^ 0x80);
    return 4;
  }

  return 0;
}


/*
  Skip the given amount of coords, accumulating sum.
*/

static inline const unsigned char *
udm_coord_sum(size_t *sump,
              const unsigned char *s,
              const unsigned char *e,
              size_t n)
{
  size_t sum= 0, nbytes, tmp;
  
  /* String must have at least "n" bytes */
  if (s + (n * 4) > e)
    goto mb;
  
  /* Quickly sum leading 7bit numbers */
  for (; *s < 128 && n; n--)
    sum+= *s++;

  for ( ; n; n--)
  {
    if (!(nbytes= udm_coord_get_quick(&tmp, s)))
      return e;
    s+= nbytes;
    sum+= tmp;
  }
  *sump= sum;
  return s;
  
mb:
  for ( ; n; n--)
  {
    if (!(nbytes= udm_coord_get(&tmp, s, e)))
      return e;
    s+= nbytes;
    sum+= tmp;
  }
  *sump= sum;
  return s;
}


/*
  Skip the given anount of coords in the given string
  s - beginning of the string
  e - end of the string (last byte + 1)
  n - how many characters to skip
  RETURNS
    position after n characters, or end of the string (if shorter)
*/
static inline const unsigned char *
udm_coord_skip(const unsigned char *s, const unsigned char *e, size_t n)
{
#if defined(__i386__) && !defined(_WIN64)
  {
    const unsigned char *e4= e - 4;
    for ( ; n > 4 && s < e4; n-= 4, s+= 4)
    {
      if (*((const uint4*)s) & 0x80808080)
        break;
    }
  }
#endif

  for ( ; n && s < e; n--)
  {
    if (*s++ < 0x80)
    {
      /* Single byte character, nothing to do */
    }
    else
    {
      unsigned char c= s[-1];
      if (c < 0xc2) /* Error */
        return e;
      else if (c < 0xe0) /* Two-byte sequence */
      {
        s++;
      }
      else if (c < 0xf0) /* Three-byte sequence */
      {
        s+= 2;
      }
      else if (c < 0xf8) /* Four-byte sequence */
      {
        s+= 3;
      }
      else
      {
        /* TODO: longer sequences */
        return e;
      }
      if (s > e)
        return e;
    }
  }
  return s;
}


static inline size_t
udm_coord_len(const char *str)
{
  size_t len, nbytes, lintag, crd;
  const char *s, *e;

  if (!str)
    return 0;
  lintag= strlen(str);
  for (len= 0, s= str, e = str + lintag; e > s; s += nbytes, len++)
  {
    if (!(nbytes= udm_coord_get(&crd,
                                (const unsigned char *)s,
                                (const unsigned char *)e)))
      break;
  }
  return len;
}


static inline void
udm_coord_statistics(const char *s, const char *e, size_t *nbytes, size_t *ncoords)
{
  size_t crd, tmp;
  for (ncoords[0]= 0, nbytes[0]= 0; s < e; s+= tmp, nbytes[0]+= tmp, ncoords[0]++)
  {
    if (!(tmp= udm_coord_get(&crd, (const unsigned char*) s, (const unsigned char*) e)) ||
        crd == 0)
      break;
  }
}


static inline size_t
udm_coord_put(size_t wc, unsigned char *r, unsigned char *e)
{
  int count;
  
  if (r >= e)
    return 0;
  
  if (wc < 0x80) 
    count= 1;
  else if (wc < 0x800) 
    count= 2;
  else if (wc < 0x10000) 
    count= 3;
  else if (wc < 0x200000)
    count= 4;
  else return 0;
  
  /* 
    e is a character after the string r, not the last character of it.
    Because of it (r+count > e), not (r+count-1 >e )
   */
  if (r + count > e) 
    return 0;
  
  switch (count)
  { 
    /* Fall through all cases */
    case 4: r[3]= (unsigned char) (0x80 | (wc & 0x3f)); wc = wc >> 6; wc |= 0x10000;
    case 3: r[2]= (unsigned char) (0x80 | (wc & 0x3f)); wc = wc >> 6; wc |= 0x800;
    case 2: r[1]= (unsigned char) (0x80 | (wc & 0x3f)); wc = wc >> 6; wc |= 0xc0;
    case 1: r[0]= (unsigned char) wc;
  }
  return count;
}



typedef struct udm_blob_cache_stat_st
{
  urlid_t cur_url_id;
  urlid_t min_url_id;
  urlid_t max_url_id;
  urlid_t max_url_id_delta;
  urlid_t range_min_url_id;
  urlid_t range_max_url_id;
  size_t ndistinct_url_ids_minus_one;
  size_t compression_type;
  size_t range_offset;
  size_t nranges;
} UDM_BLOB_CACHE_WORD_STAT;


static size_t
UdmBlobCacheOneWordStat(UDM_BLOB_CACHE_WORD *First,
                        UDM_BLOB_CACHE_WORD *Last,
                        UDM_BLOB_CACHE_WORD_STAT *Stat)
{
  UDM_BLOB_CACHE_WORD *W;
  Stat->min_url_id= First->url_id;
  Stat->max_url_id= First->url_id;
  Stat->ndistinct_url_ids_minus_one= 0;
  Stat->max_url_id_delta= 0;
  for (W= First; W < Last; W++)
  {
    if (W->secno != First->secno ||
        strcmp(W->word, First->word))
      break;
    if (W->url_id != Stat->max_url_id)
    {
      urlid_t url_id_delta= W->url_id - Stat->max_url_id;
      Stat->ndistinct_url_ids_minus_one++;
      if (url_id_delta > Stat->max_url_id_delta)
        Stat->max_url_id_delta= url_id_delta;
    }
    Stat->max_url_id= W->url_id;
  }
  /*
  fprintf(stderr, "min=%d max=%d diff=%d\tdist=%d\tmax_delta=%d %s\n",
          Stat->min_url_id, Stat->max_url_id, 
          Stat->max_url_id - Stat->min_url_id,
          Stat->ndistinct_url_ids_minus_one,
          Stat->max_url_id_delta,
          First->word);
  */
  return W - First;
}


static void
UdmBlobCacheWordReportError(UDM_BLOB_CACHE_WORD *w)
{
  fprintf(stderr, "BlobCachePackOneWord: DSTRAppend() failed: "
                  "word='%s' secno=%d len=%d",
                   w->word, w->secno, w->ntaglen);
}


/*
  Pack words using simple format:
  { url_id, ncoords, coord list }*
  
*/
static void
UdmBlobCachePackOneWordForBdictSimple(UDM_BLOB_CACHE_WORD *First,
                                      UDM_BLOB_CACHE_WORD *Last,
                                      UDM_DSTR *buf)
{
  UDM_BLOB_CACHE_WORD *word1;
  unsigned char ubuf[COORD_MAX_LEN];
  unsigned char *ubufend= ubuf + sizeof(ubuf);
  
  for (word1= First; word1 < Last; word1++)
  {
    char utmp[4];
    size_t nbytes;
    udm_put_int4(word1->url_id, utmp);
    if (!(nbytes= udm_coord_put(word1->nintags, ubuf, ubufend)))
      continue;
    if (!UdmDSTRAppend(buf, utmp, sizeof(urlid_t)) ||
        !UdmDSTRAppend(buf, (char *)ubuf, nbytes) ||
        !UdmDSTRAppend(buf, word1->intags, word1->ntaglen))
    {
      UdmBlobCacheWordReportError(word1);
    }
  }
}


static inline void
udm_put_int2(int i, unsigned char *dst)
{
  dst[0]= (unsigned char) (i & 0xFF);
  dst[1]= (unsigned char) (i >> 8);
}

static inline int
udm_get_int2(const unsigned char *src)
{
  return ((int) src[0]) + (((int) src[1]) << 8);
}


static inline void
udm_put_int3(int i, unsigned char *dst)
{
  dst[0]= (unsigned char) (i & 0xFF);
  dst[1]= (unsigned char) ((i >> 8) & 0xFF);
  dst[2]= (unsigned char) ((i >> 16) & 0xFF);
}

static inline int
udm_get_int3(const unsigned char *src)
{
  return ((int) src[0]) + (((int) src[1]) << 8) + (((int) src[2]) << 16);
}


/*
  Pack integer using various formats:
  0 = don't put anything
  1 = one byte
  2 = two bytes
  3 = three bytes
  4 = four bytes
  5 = variable length encoding
  
  dst must have enough space to store the packed integer.
*/
static inline size_t
udm_put_int_with_format(int i, unsigned char *dst, int format)
{
  if (format == 1)
  {
    UDM_ASSERT((unsigned int) i < 256);
    dst[0]= (char) (unsigned char) i;
    return 1;
  }
  else if (format == 2)
  {
    udm_put_int2(i, (unsigned char*) dst);
    return 2;
  }
  else if (format == 3)
  {
    udm_put_int3(i, (unsigned char*) dst);
    return 3;
  }
  else if (format == 4)
  {
    udm_put_int4(i, (unsigned char*) dst);
    return 4;
  }
  else if (format == 5)
  {
    return udm_coord_put(i, dst, dst + 4);
  }
  UDM_ASSERT(format == 0);
  return 0;
}


/*
  Pack single word record using this format: {url_id, ncoords, coords}
*/
static inline void
UdmBlobCachePackOneWordDocForBdict(UDM_DSTR *buf,
                                   urlid_t urlid,
                                   UDM_BLOB_CACHE_WORD *w,
                                   int urlid_format)
{
  unsigned char ncoordsbuf[4];
  unsigned char urlidbuf[4];
  size_t urlidbuf_len= udm_put_int_with_format(urlid, urlidbuf, urlid_format);
  size_t ncoordsbuf_len= udm_coord_put(w->nintags, ncoordsbuf, ncoordsbuf + 4);
  
  if (!ncoordsbuf_len ||
      (!UdmDSTRAppend(buf, (char*) urlidbuf, urlidbuf_len) && urlid_format)||
      !UdmDSTRAppend(buf, (char*) ncoordsbuf, ncoordsbuf_len) ||
      !UdmDSTRAppend(buf, w->intags, w->ntaglen))
  {
    UdmBlobCacheWordReportError(w);
  }
}


/*
  Pack word array using simple format:
  { url_id_delta, ncoords, coord list }*
  
  where url_id_delta uses fixed 1, 2 or 3 bytes.
*/
static void
UdmBlobCachePackOneWordForBdictURLIDDeltaFixed(UDM_BLOB_CACHE_WORD *First,
                                               UDM_BLOB_CACHE_WORD *Last,
                                               UDM_DSTR *buf,
                                               size_t delta_format,
                                               size_t offset)
{
  UDM_BLOB_CACHE_WORD *w;
  urlid_t url_id_prev;

  /* Store compression type */
  if (delta_format == 1)
  {
    url_id_prev= offset;
    /*
      TODO: change offset storage to variable format,
      to be shorter than "DELTA" compression.
     */
    if (!UdmDSTRAppendCompressionType(buf, UDM_BLOB_COMP_URLID_DELTA_1BYTES_WITH_OFFSET) ||
        !UdmDSTRAppendINT4(buf, offset))
      return;
  }
  else if (!offset)
  {
    size_t compr_type= (delta_format == 2) ?
                       UDM_BLOB_COMP_URLID_DELTA_2BYTES :
                       UDM_BLOB_COMP_URLID_DELTA_3BYTES;
    url_id_prev= offset & 0xFFFF0000; /* Clear 2 lower bytes */
    if (!UdmDSTRAppendCompressionType(buf, compr_type))
      return;
  }
  else
  {
    url_id_prev= offset & 0xFFFF0000; /* Clear 2 lower bytes */
    if (!UdmDSTRAppendCompressionType(buf, UDM_BLOB_COMP_URLID_DELTA_2BYTES_WITH_OFFSET) ||
        !UdmDSTRAppendINT2(buf, offset >> 16))
      return;
  }

  for (w= First; w < Last; w++)
  {
    urlid_t delta= w->url_id - url_id_prev;
    UdmBlobCachePackOneWordDocForBdict(buf, delta, w, delta_format);
    url_id_prev= w->url_id;
  }
}


/*
  Pack words using variable length url_id delta compression.
  0x00..0x7F    - one byte
  0x80..0x7FF   - two bytes
  0x800..0xFFFF - three bytes
*/
static void
UdmBlobCachePackOneWordForBdictURLIDDeltaVariable(UDM_BLOB_CACHE_WORD *First,
                                                  UDM_BLOB_CACHE_WORD *Last,
                                                  UDM_DSTR *buf)
{
  UDM_BLOB_CACHE_WORD *w;
  urlid_t url_id_prev= 0;

  /* Store compression type */
  if (!UdmDSTRAppendCompressionType(buf, UDM_BLOB_COMP_URLID_DELTA_VARIABLE)) 
    return;

  for (w= First; w < Last; w++)
  {
    urlid_t delta= w->url_id - url_id_prev;
    UdmBlobCachePackOneWordDocForBdict(buf, delta, w, 5);
    url_id_prev= w->url_id;
  }
}


/*
  Pack words using single range format:
  { min_url_id, max_url_id },
  { ncoords, coord list }*
*/
static void
UdmBlobCachePackOneWordForBdictURLIDRange(UDM_BLOB_CACHE_WORD *First,
                                          UDM_BLOB_CACHE_WORD *Last,
                                          UDM_BLOB_CACHE_WORD_STAT *Stat,
                                          UDM_DSTR *buf)
{
  UDM_BLOB_CACHE_WORD *w;

  /* Store compression type and url_id range */
  if (!UdmDSTRAppendCompressionType(buf, UDM_BLOB_COMP_SINGLE_RANGE) ||
      !UdmDSTRAppendINT4(buf, Stat->min_url_id) ||
      !UdmDSTRAppendINT4(buf, Stat->max_url_id))
    return;

  /* Store ncoords and coord list */
  for (w= First; w < Last; w++)
  {
    UdmBlobCachePackOneWordDocForBdict(buf, 0, w, 0);
  }
}


/*
  Pack words using multipe range format:
  { nranges {
  { min_url_id, max_url_id }
  { ncoords, coordlist }*
*/
static void
UdmBlobCachePackOneWordForBdictURLIDRangeMulti(UDM_BLOB_CACHE_WORD *First,
                                               UDM_BLOB_CACHE_WORD *Last,
                                               UDM_BLOB_CACHE_WORD_STAT *Stat,
                                               UDM_DSTR *buf)
{
  UDM_BLOB_CACHE_WORD *w;
  size_t nranges_offset, nranges;

  /* Store compression type and url_id range */
  if (!UdmDSTRAppendCompressionType(buf, UDM_BLOB_COMP_URLID_RANGE_MULTI) ||
      !(nranges_offset= buf->size_data) /* Remember position of "nranges" */||
      !UdmDSTRAppendINT4(buf, 0))
    return;

  /* Store ranges */
  for (nranges= 0, w= First; w < Last; w++)
  {
    urlid_t urlid_prev= w->url_id;
    if (!UdmDSTRAppendINT4(buf, w->url_id))
      return;
    /*fprintf(stderr, "first=%d\n", w->url_id);*/
    for ( ; ; w++)
    {
      /*fprintf(stderr, "   %d\n", w + 1 == Last ? 0x7FFFFFFF : w[1].url_id);*/
      
      if (w + 1 == Last || w[1].url_id > w->url_id + 1)
      {
        /*fprintf(stderr, "last=%d\n", w->url_id);*/
        if (!UdmDSTRAppendINT4(buf, w->url_id))
          return;
        nranges++;
        break;
      }
      urlid_prev= w->url_id;
    }
  }
  /*
  fprintf(stderr, "NRanges=%d nbytes=%d dist=%d %s\n",
          nranges, nranges * 8,
          Stat->ndistinct_url_ids_minus_one  + 1, First->word);
  */
  udm_put_int4(nranges, (unsigned char*) &buf->data[nranges_offset]);
  
  for (w= First; w < Last; w++)
  {
    UdmBlobCachePackOneWordDocForBdict(buf, 0, w, 0); 
  }
}


/*
  Put Blob coords using various compression ways.
  MSDN33 stat:
  324585956 - non-comporessed
  247602660 - URLIDRange + URLIDDelta
  240471416 - URLIDRange + URLIDDelta + URLDelta2Bytes + URLDelta2BytesWithOfs
  228178145 - ???
  227985073 - changed new compression header size from 8 to 6.
  227617171 - Added URLDelta3Bytes
  227617000 - Changed ndistinct limit in Range to 4 docs.
  227489816 - Added URLIdRangeMulti

select hex(if(left(intag,4)=0xFFFFFFFF,substr(intag,5,1),0x00)) as compr,
sum(length(intag)) from bdict group by compr;
+-------+--------------------+
| compr | sum(length(intag)) |
+-------+--------------------+
| 00    |            9388655 |
| 04    |              29146 |
| 05    |          183886955 |
| 07    |           29249072 |
| 08    |            5063172 |
+-------+--------------------+
5 rows in set (1.05 sec)
*/
static size_t
UdmBlobCachePackOneWordForBdict(UDM_BLOB_CACHE *cache,
                                const size_t offs,
                                UDM_DSTR *buf, int use_compr)
{
  UDM_BLOB_CACHE_WORD_STAT Stat;
  UDM_BLOB_CACHE_WORD *First= &cache->words[offs];
  size_t count= UdmBlobCacheOneWordStat(First,
                                        &cache->words[cache->nwords], &Stat);
  UDM_BLOB_CACHE_WORD *Last= First + count;
  
  if (use_compr)
  {
    urlid_t range_size= Stat.max_url_id - Stat.min_url_id;
    urlid_t missing= range_size - Stat.ndistinct_url_ids_minus_one;
    
    if (Stat.ndistinct_url_ids_minus_one > 2 && /* at least 4 docs */
        range_size == Stat.ndistinct_url_ids_minus_one)
    {
      /*
       Compr 4 docs: 14bytes(hdr:6+4max+4min) vs 16 bytes
      */
      /*fprintf(stderr, "Cool: %s ndist=%d\n", First->word, Stat.ndistinct_url_ids_minus_one);*/
      UdmBlobCachePackOneWordForBdictURLIDRange(First, Last, &Stat, buf);
      goto ret;
    }
    else if ((missing + 1) < (range_size + 1) / 8 && range_size > 16) 
    {
      /* 6bytes(header) + 4bytes(nranges) + 8bytes(range) */
      UdmBlobCachePackOneWordForBdictURLIDRangeMulti(First, Last, &Stat, buf);
      goto ret;
    }
#if 0
    else if (Stat.max_url_id_delta < 256 &&
             Stat.ndistinct_url_ids_minus_one > 6) /* at least 8 docs */
    {
      /*
        1 byte url_id delta compression.
        Compr 4 docs: 10bytes(hdr), 4bytes(4 x url_id)= 14 bytes (vs 16)
        Also, extra 4 docs to make sure we're not longer than
        variable-length comression mode UDM_BLOB_COMP_URLID_DELTA
      */
      /*fprintf(stderr, "MaxDelta1: maxd=%d ndist=%d %s\n",
              Stat.max_url_id_delta,
              Stat.ndistinct_url_ids_minus_one,
              First->word);*/
      UdmBlobCachePackOneWordForBdictURLIDDeltaFixed(First, Last, buf, 1, Stat.min_url_id);
      goto ret;
    }
#endif
    else if (Stat.ndistinct_url_ids_minus_one > 1 /* at least 3 docs */ &&
             Stat.ndistinct_url_ids_minus_one > range_size / 256)
    {
      /*
        Compress using delta url_id compression.
        Good if at least 3 documents:
          Non-compr 3 docs: 12bytes(3 x url_id)                 = 12 bytes
          Compr 3 docs:     6bytes(header) + 3bytes(3 x url_id) =  9 bytes
      */
      /*
      fprintf(stderr, "Delta: %d/%d %s\n",
              Stat.ndistinct_url_ids_minus_one, range_size, First->word);
      */
      UdmBlobCachePackOneWordForBdictURLIDDeltaVariable(First, Last, buf);
      goto ret;
    }
    else if (Stat.max_url_id_delta <= 0xFFFF &&
             Stat.min_url_id <= 0xFFFF &&
             Stat.ndistinct_url_ids_minus_one > 2) /* at least 4 docs */
    {
      /*
        Delta2bytes, without offset. Good if at least 4 docs.
        Compr 3 docs: 6bytes(header) + 6bytes(3 x url_id) = 12 bytes.
      */
      /*
      fprintf(stderr, "MaxDelta: %d range=%d dist=%d %s\n",
              Stat.max_url_id_delta, range_size, 
              Stat.ndistinct_url_ids_minus_one, First->word);
      */
      UdmBlobCachePackOneWordForBdictURLIDDeltaFixed(First, Last, buf, 2, 0);
      goto ret;
    }
    else if (Stat.max_url_id_delta <= 0xFFFF &&
             Stat.min_url_id >= 0xFFFF &&
             Stat.ndistinct_url_ids_minus_one > 3) /* at least 5 docs */
    {
      /*
        Delta2bytes with offset. Good if at least 5 docs.
        Compr 4 docs: 8bytes(header) + 8bytes(4 x url_id) = 16 bytes.
       */
      /*
      fprintf(stderr, "MaxDelta2: %d range=%d dist=%d min=%d %s\n",
              Stat.max_url_id_delta, range_size, 
              Stat.ndistinct_url_ids_minus_one,
              Stat.min_url_id, First->word);
      */
      UdmBlobCachePackOneWordForBdictURLIDDeltaFixed(First, Last, buf, 2, Stat.min_url_id);
      goto ret;
    }
    else if (Stat.max_url_id_delta <= 0xFFFFFF &&
             Stat.min_url_id <= 0xFFFFFF &&
             Stat.ndistinct_url_ids_minus_one > 4) /* At least 6 docs */
   {
     /*
       Delta3bytes. Good if at least 6 docs.
       Compr 8 docs: 6bytes(hdr) + 24bytes(8 x url_id) = 30 bytes vs 32 bytes.
     */
     /*
      fprintf(stderr, "MaxDelta3: %d range=%d dist=%d min=%d %s\n",
              Stat.max_url_id_delta, range_size, 
              Stat.ndistinct_url_ids_minus_one,
              Stat.min_url_id, First->word);
     */
     UdmBlobCachePackOneWordForBdictURLIDDeltaFixed(First, Last, buf, 3, 0);
     goto ret;
   }
  }
  /*
  fprintf(stderr, "Simple: ndist=%d range=%d minid=%d maxid=%d max_del=%d %s\n",
          Stat.ndistinct_url_ids_minus_one,
          Stat.max_url_id - Stat.min_url_id,
          Stat.min_url_id,
          Stat.max_url_id,
          Stat.max_url_id_delta,
          First->word);
  */
  UdmBlobCachePackOneWordForBdictSimple(First, Last, buf);
  
ret:
  return count;
}


/*
  Format in "bdicti" table for the intag00..intag1F columns:
  
  NL    ::= !! 0x00
  Secno ::= !! one byte, not equal to 0x00.
  Coord ::= !! variable length number representation, 1..4 bytes, cannot contain 0x00 bytes.
  Word  ::= !! String in LocalCharset, variable length, cannot contain 0x00 bytes.
  
  SectionRecord ::= Secno Coord Coord*
  
  WordRecord    ::= Word NL SectionRecord (NL SectionRecord)*
  
  WordList      ::= WordRecord (NL NL WordRecord)*

  
Example:

  616C7465726E6174656C79  Word
  00                      NL
  01                      Secno
  C8BA                    Coord
  C39F                    Coord
  0000                    NL NL
  617265                  Word
  00                      NL
  01                      Secno
  C2AE                    Coord
  19                      Coord
  34                      Coord
  C39D                    Coord
  3D                      Coord
  C484                    Coord
  0000                    NL NL
  636F6E76656E74696F6E    Word
  00                      NL
  01                      Secno
  C382                    Coord
  15                      Coord
  C982                    Coord
  0000                    NL NL
  66696C656E616D65        Word
  00                      NL
  01                      Secno
  C9AE                    Coord
  01                      Coord
  2B                      Coord
  25                      Coord
  13                      Coord
  47                      Coord
  0000                    NL NL
  7365636F6E64            Word
  00                      NL
  01                      Secno
  C588                    Coord
  C791                    Coord
*/


#if 0
static inline
good(int x)
{
 /* ??aaaaaa ??bbbbbb ??cccccc ??dddddd */
 return (x >=0) && (x <= 63);
}
#endif


/*
  Record format in table "bdicti":

  word    - array of characters
  0x00    - 1 byte, end-of-word marker
  secno   - 1 byte
  coord1,...coordN - positions deltas, using variable length encoding.
  0x0000  - 2 bytes, end-of-record marker
*/
static int
UdmBlobEncodeBdictiRecord(UDM_DB *db,
                          UDM_DOCUMENT *Doc,
                          UDM_WORDLIST *WordList,
                          UDM_DSTR *dbuf,
                          UDM_PSTR *Chunks,
                          size_t save_section_size)
{
  size_t i, j, chunks[33];
  UdmDSTRInit(dbuf, 1024*64);
  for (j= 0, i= 0; i < 32; i++)
  {
    UDM_WORDLIST *WL= &WordList[i];
    UDM_WORD *WLWord= WL->Word;
    int prev_pos= 0;
    unsigned char prev_secno= 0;
    const char *prev_word= "";
    chunks[i]= dbuf->size_data;
    
    for (j= 0; j < WL->nwords; )
    {
      unsigned char buf[COORD_MAX_LEN];
      size_t nbytes;
      UDM_WORD *W= &WLWord[j];
      int pos= W->pos;
      unsigned char secno= W->secno;
      if (strcmp(W->word, prev_word))
      {
        if (*prev_word)
          UdmDSTRAppend(dbuf, "\0\0", 2);
        UdmDSTRAppendSTR(dbuf, W->word);
        prev_secno= 0;
        prev_pos= 0;
        prev_word= W->word;
      }
      if (secno != prev_secno)
      {
        UdmDSTRAppend(dbuf, "", 1);
        UdmDSTRAppend(dbuf, (char*) &secno, 1);
        prev_pos= 0;
        prev_secno= 0;
      }
      nbytes= udm_coord_put((int) pos - prev_pos, buf, buf + sizeof(buf));

#if 0
      {
        printf("coord=%d\n", pos - prev_pos);
        
        if (j > 4)
        {
          if (good(W[0].pos - W[-1].pos) &&  good(W[-1].pos - W[-2].pos))
          {
            printf("GOOD2 word=%s\n", W->word);
            if (good(W[-2].pos - W[-3].pos))
            {
              printf("GOOD3 word=%s\n", W->word);
              if (good(W[-3].pos - W[-4].pos))
              {
                printf("GOOD4 word=%s\n", W->word);
                if (j > 5 && good(W[-4].pos - W[-5].pos))
                {
                  printf("GOOD5 word=%s\n", W->word);
                  if (j > 6 && good(W[-5].pos - W[-6].pos))
                  {
                    printf("GOOD6 word=%s\n", W->word);
                    if (j > 7 && good(W[-6].pos - W[-7].pos))
                    {
                      printf("GOOD7 word=%s\n", W->word);
                    }
                  }
                }
              }
            }
          }
        }
      }
#endif

      UdmDSTRAppend(dbuf, (char *)buf, nbytes);
      prev_pos= pos;
      prev_secno= secno;
      j++;
      
      /* Detect end of section, and put section length */
      if (save_section_size &&
          (j >= WL->nwords ||
           strcmp(W->word, W[1].word) ||
           W->secno != W[1].secno))
      {
        int seclen= (Doc->CrossWords.wordpos[prev_secno] + 1) - prev_pos;
        nbytes= udm_coord_put(seclen, buf, buf + sizeof(buf));
        UdmDSTRAppend(dbuf, (char*) buf, nbytes);
      }
    }
  }
  chunks[32]= dbuf->size_data;
  for (i= 0; i < 32; i++)
  {
    Chunks[i].val= dbuf->data + chunks[i];
    Chunks[i].len= chunks[i + 1] - chunks[i];
  }
  return UDM_OK;
}



/*
  New VS Old statistics. Collection km.ru. Query q=1+2+3
  
  New:
    UdmBlobPackedCoordsToUnpackedCoords called 547748 times
  
  Old:
    UdmBlobPackedCoordsToUnpackedCoords called 2834091 times
*/


/*
  Unpack coordinates.
  s     - start of packed string
  e     - end of packed string
  nrecs - how many coordinates to unpack
  C     - pointer to initial coordinate, increment positions relatively to it.
  Coord - Where to out unpacked coordinates. Must have enough space allocated.
  **end - where unpacking stopped
  
  Returns - pointer after the last coordinate unpacked.
*/
UDM_COORD2 *
UdmBlobPackedCoordsToUnpackedCoords(const unsigned char *s,
                                    const unsigned char *e,
                                    size_t nrecs,
                                    UDM_COORD2 *C,
                                    UDM_COORD2 *Coord,
                                    const unsigned char **end)
{
  if (s + nrecs * COORD_MAX_LEN < e)
  {
    /* Not the last chunk: unpack without range check */
    for ( ; nrecs > 0 ; nrecs--)
    {
      if (*s < 128)
      {
        C->pos+= *s++;
      }
      else
      {
        size_t crd, nbytes;
        nbytes= udm_coord_get_quick(&crd, s);
        if (!nbytes) break;
        s+= nbytes;
        C->pos+= crd;
      }
      *Coord++= *C;
    }
  }
  else
  {
    /* Possibly last chunk: unpack with range check */
    for ( ; nrecs > 0 ; nrecs--)
    {
      if (s < e && *s < 128)
      {
        C->pos+= *s++;
      }
      else
      {
        size_t crd, nbytes;
        nbytes= udm_coord_get(&crd, s, e);
        if (!nbytes) break;
        s+= nbytes;
        C->pos+= crd;
      }
      *Coord++= *C;
    }
  }
  *end= s;
  return Coord;
}



/*
  Unpack coords when only minpos, maxpos and seclen is of interest
*/
static UDM_COORD2 *
UdmBlobPackedCoordsUnpackMinMaxLen(const unsigned char *s,
                                   const unsigned char *e,
                                   size_t nrecs,
                                   UDM_COORD2 *C,
                                   UDM_COORD2 *Coord,
                                   const unsigned char **end,
                                   int save_section_size,
                                   UDM_SECTION *Section)
{
  size_t crd, nbytes;
  if (save_section_size)
  {
    if (nrecs > 1)
    {
      s= udm_coord_sum(&crd, s, e, nrecs - 1); /* Sum middle coords */
      C->pos+= crd;
      Section->maxpos= C->pos;
    }
    else
      Section->maxpos= C->pos; /* One coord, minpos=maxpos */

    if ((nbytes= udm_coord_get(&crd, s, e))) /* Get seclen */
    {
      s+= nbytes;
      C->pos+= crd;
      Section->seclen= C->pos;
      Section->ncoords= nrecs;
      Coord+= nrecs;
    }
    else
    {
      Section->seclen= 0;
      Section->ncoords= 0;
    }
  }
  else
  {
    s= udm_coord_sum(&crd, s, e, nrecs);      /* Sum middle coords  */
    C->pos+= crd;
    Section->maxpos= C->pos;
    Section->seclen= 0;
    Section->ncoords= nrecs + 1;
    Coord+= nrecs + 1;
  }
  *end= s;
  return Coord;
}



static size_t
UdmBlobCoordsGetCompressionType(UDM_FINDWORD_ARGS *args,
                                UDM_BLOB_CACHE_WORD_STAT *Stat,
                                const unsigned char *s,
                                size_t length)
{
  size_t header_size;
  bzero((void*) Stat, sizeof(*Stat));
  Stat->compression_type= (length > 10 && udm_get_int4(s) == 0xFFFFFFFF) ?
                           udm_get_int4(s + 4) : 0;
  header_size= (Stat->compression_type & 0xFFFF0000) ? 6 : 8;
  Stat->compression_type&= 0x0000FFFF;
  /*
  fprintf(stderr, "Hdr size=%d cmpr=%08X len=%d\n", header_size, Stat->compression_type, length);
  */
  if (Stat->compression_type == UDM_BLOB_COMP_SINGLE_RANGE)
  {
    Stat->min_url_id= udm_get_int4(s + header_size);
    Stat->max_url_id= udm_get_int4(s + header_size + 4);
    Stat->cur_url_id= Stat->min_url_id;
    UdmLog(args->Agent, UDM_LOG_DEBUG,
           "Single-URLID-Range compression: %d-%d (%d docs)",
           Stat->min_url_id, Stat->max_url_id,
           Stat->max_url_id - Stat->min_url_id + 1);
    return header_size + 8;
  }
  else if (Stat->compression_type == UDM_BLOB_COMP_URLID_DELTA_VARIABLE)
  {
    UdmLog(args->Agent, UDM_LOG_DEBUG, "URLID-Delta compression");
    return header_size;
  }
  else if (Stat->compression_type == UDM_BLOB_COMP_URLID_DELTA_2BYTES)
  {
    UdmLog(args->Agent, UDM_LOG_DEBUG, "URLID-Delta-2bytes compression");
    return header_size;
  }
  else if (Stat->compression_type == UDM_BLOB_COMP_URLID_DELTA_2BYTES_WITH_OFFSET)
  {
    Stat->cur_url_id= udm_get_int2(s + header_size) << 16;
    UdmLog(args->Agent, UDM_LOG_DEBUG,
           "URLID-Delta-2bytes-offset compression, offs=%d", Stat->cur_url_id);
    return header_size + 2;
  }
  else if (Stat->compression_type == UDM_BLOB_COMP_URLID_DELTA_1BYTES_WITH_OFFSET)
  {
    Stat->cur_url_id= udm_get_int4(s + header_size);
    UdmLog(args->Agent, UDM_LOG_DEBUG,
           "URLID-Delta-1byte-offset compression, offs=%d", Stat->cur_url_id);
    return header_size + 4;
  }
  else if (Stat->compression_type == UDM_BLOB_COMP_URLID_DELTA_3BYTES)
  {
    UdmLog(args->Agent, UDM_LOG_DEBUG, "URLID-Delta-2bytes compression");
    return header_size;
  }
  else if (Stat->compression_type == UDM_BLOB_COMP_URLID_RANGE_MULTI)
  {
    Stat->nranges= udm_get_int4(s + header_size);
    Stat->range_offset= header_size + 4;
    UdmLog(args->Agent, UDM_LOG_DEBUG, "URLID-Range-Multi compression, nranges=%d", Stat->nranges);
    return header_size + 4 + 8 * Stat->nranges;
  }
  else if (Stat->compression_type != UDM_BLOB_COMP_NONE)
  {
    UdmLog(args->Agent, UDM_LOG_DEBUG,
           "Unknown coompression type: %08X", Stat->compression_type);
  }
  return 0;
}


static size_t
UdmExpectedSectionCount(size_t compression_type, size_t length)
{
  switch (compression_type)
  {
    case UDM_BLOB_COMP_NONE:
      /*
        Shortest section with no compression is 6 bytes:
        - 4 bytes for URL id
        - 1 byte for "ncoords"
        - 1 byte for coord
      */
      return length / 6;
    case UDM_BLOB_COMP_URLID_DELTA_VARIABLE:
      /*
        - 1..4 bytes URL id delta
        - 1 byte for "ncoordss"
        - 1 byte for coord
      */
      return length / 3;
    
    case UDM_BLOB_COMP_SINGLE_RANGE:
    case UDM_BLOB_COMP_URLID_RANGE_MULTI:
      /*
        Shortest section with single-range compression is 2 byest:
        - 1 byte for ncoords
        - 1 byte for coord
      */
      return length / 2;
    case UDM_BLOB_COMP_URLID_DELTA_1BYTES_WITH_OFFSET:
      /* 1 byte url_id, 1 byte ncoorrds, 1 byte coords */
      return length / 3;
    case UDM_BLOB_COMP_URLID_DELTA_2BYTES:
    case UDM_BLOB_COMP_URLID_DELTA_2BYTES_WITH_OFFSET:
      /* 2 bytes url_id, 1 byte ncoords, 1 byte coord */
      return length / 4;
    case UDM_BLOB_COMP_URLID_DELTA_3BYTES:
      /* 3 bytes url_id, 1 bbyte ncoords, 1 byte coord */
      return length / 5;
  }
  
  UDM_ASSERT(0); /* Should not normally get here */
  return length;
}


static int
UdmSectionListBlobCoordsUnpack(UDM_FINDWORD_ARGS *args,
                               UDM_SECTIONLIST *SectionList,
                               UDM_URLID_LIST *urls,
                               UDM_SECTION *SectionTemplate,
                               const unsigned char *s,
                               size_t length,
                               int save_section_size,
                               int need_coords)
{
  size_t ncoords= 0, nurls= urls->nurls;
  const unsigned char *s0= s;
  const unsigned char *e= s + length;
  const unsigned char *last_urlid_start= e - sizeof(urlid_t) - 1;
  UDM_COORD2 C, *Coord;
  unsigned char secno= SectionTemplate->secno;
  unsigned char wordnum= SectionTemplate->wordnum;
  unsigned char order= SectionTemplate->order;  
  UDM_SECTION *Section;
  UDM_BLOB_CACHE_WORD_STAT Stat;
  const unsigned char *range_ptr;
  int single_range, range_multi, compr_urlid_delta;
  int compr_urlid_delta_1bytes, compr_urlid_delta_2bytes, compr_urlid_delta_3bytes;
  size_t coords_alloced, sections_alloced;
  s+= UdmBlobCoordsGetCompressionType(args, &Stat, s, length);
  range_ptr= s0 + Stat.range_offset;

  UdmLog(args->Agent, UDM_LOG_DEBUG+1, "Secno=%d len=%d",
         SectionTemplate->secno, length);
  single_range= (Stat.compression_type == UDM_BLOB_COMP_SINGLE_RANGE);
  range_multi= (Stat.compression_type == UDM_BLOB_COMP_URLID_RANGE_MULTI);
  compr_urlid_delta= (Stat.compression_type == UDM_BLOB_COMP_URLID_DELTA_VARIABLE);
  compr_urlid_delta_2bytes= (Stat.compression_type == UDM_BLOB_COMP_URLID_DELTA_2BYTES) ||
                            (Stat.compression_type == UDM_BLOB_COMP_URLID_DELTA_2BYTES_WITH_OFFSET);
  compr_urlid_delta_3bytes= Stat.compression_type == UDM_BLOB_COMP_URLID_DELTA_3BYTES;
  compr_urlid_delta_1bytes= Stat.compression_type == UDM_BLOB_COMP_URLID_DELTA_1BYTES_WITH_OFFSET;

  if (single_range || range_multi)
    last_urlid_start= e - 1; /* TODO: check other modes */

  coords_alloced= length;
  sections_alloced= UdmExpectedSectionCount(Stat.compression_type, length);
  UdmSectionListAlloc(SectionList, coords_alloced, sections_alloced);
  Coord= SectionList->Coord;
  Section= SectionList->Section;

  /*
    A non-compressed chunk consists of:
    - sizeof(urlid_t)
    - at least one byte for length
  */
  for (C.order= order ; s < last_urlid_start; )
  {
    int active= 1;
    size_t nrecs;

    Section->Coord= Coord;
    Section->secno= secno;
    if (Stat.compression_type)
    {
      if (single_range)
      {
        Section->url_id= Stat.cur_url_id++;
      }
      else if (range_multi)
      {
        if (Stat.cur_url_id == Stat.range_max_url_id && Stat.nranges--)
        {
          Stat.range_min_url_id= udm_get_int4(range_ptr); range_ptr+= 4;
          Stat.range_max_url_id= udm_get_int4(range_ptr); range_ptr+= 4;
          Stat.cur_url_id= Stat.range_min_url_id;
          /*
          fprintf(stderr, "loading range: %d-%d nranges remain: %d\n",
                  Stat.range_min_url_id, Stat.range_max_url_id, Stat.nranges);
          */
        }
        else
        {
          Stat.cur_url_id++;
        }
        Section->url_id= Stat.cur_url_id;
      }
      else if (compr_urlid_delta)
      {
        size_t delta, nbytes= udm_coord_get(&delta, s, e);
        s+= nbytes;
        if (!nbytes)
          break;
        Stat.cur_url_id+= delta;
        Section->url_id= Stat.cur_url_id;
      }
      else if (compr_urlid_delta_1bytes)
      {
        size_t delta= (unsigned char) *s;
        s++;
        Stat.cur_url_id+= delta;
        Section->url_id= Stat.cur_url_id;
        /*
        fprintf(stderr, "url_id=%d delta=%d\n", Stat.cur_url_id, delta);
        */
      }
      else if (compr_urlid_delta_2bytes)
      {
        size_t delta= udm_get_int2(s);
        s+= 2;
        Stat.cur_url_id+= delta;
        Section->url_id= Stat.cur_url_id;
        /*
        fprintf(stderr, "url_id=%d delta=%d\n", Stat.cur_url_id, delta);
        */
      }
      else if (compr_urlid_delta_3bytes)
      {
        size_t delta= udm_get_int3(s);
        s+= 3;
        Stat.cur_url_id+= delta;
        Section->url_id= Stat.cur_url_id;
        /*
        fprintf(stderr, "url_id=%d delta=%d\n", Stat.cur_url_id, delta);
        */
      }
    }
    else
    {
      Section->url_id= (urlid_t) udm_get_int4(s);
      s+= 4;
    }
    Section->wordnum= wordnum;
    Section->order= order;

    if (nurls)
    {
      void *found= UdmBSearch(&Section->url_id, urls->urls, urls->nurls,
                              sizeof(urlid_t), (udm_qsort_cmp)UdmCmpURLID);
      if (found && urls->exclude)
        active= 0;
      if (!found && !urls->exclude)
        active= 0;
    }

    /* Get number of coords */
    if (*s < 128)
    {
      nrecs= *s++;
    }
    else
    {
      size_t nbytes= udm_coord_get(&nrecs, s, e);
      if (!nbytes) break;
      s+= nbytes;
    }

    if (!nrecs) /* extra safety */
      break;

    if (!active)
    {
      s= udm_coord_skip(s, e, nrecs);
      continue;
    }

    ncoords+= nrecs;

    if (save_section_size && nrecs > 1)
      ncoords--;
    
    Section->PackedCoord= s;
    
    /* Get first coord and put into S->minpos */
    if (*s < 128)
    {
      C.pos= *s++;
    }
    else
    {
      size_t crd;
      size_t nbytes= udm_coord_get(&crd, s, e);
      if (!nbytes) break;
      s+= nbytes;
      C.pos= crd;
    }

    Section->minpos= C.pos;
      
    /*
      If no coords anymore.
      Maybe the "section length" record didn't fit
      into "64K words per section" limit.
      Add section with seclen=0.
    */
    if (!--nrecs)
    {
      Section->seclen= C.pos;
      Section->ncoords= 1;
      Section->maxpos= C.pos;
      if (need_coords)
        *Coord= C;
      Coord++;
      Section++;
      continue;
    }

    if (!need_coords) /* Does not need coords, e.g. one word search */
    {
      Coord= UdmBlobPackedCoordsUnpackMinMaxLen(s, e, nrecs, &C, Coord, &s,
                                                save_section_size, Section);
      Section++;
      continue;
    }

    *Coord++= C; /* Add first coord */

    /* Unpack the other coordinates */
    Coord= UdmBlobPackedCoordsToUnpackedCoords(s, e, nrecs, &C, Coord, &s);
    
    /* Set section length */
    nrecs= Coord - Section->Coord;
    if (save_section_size)
    {
      /*
        We need to check whether Coord > Coord0 in the above
        condition: URL could be skipped because of limit
      */
      Section->seclen= ((--Coord)->pos);
      Section->ncoords= nrecs - 1;
      Section->maxpos= Coord[-1].pos;;
    }
    else
    {
      Section->seclen= 0;
      Section->ncoords= nrecs;
      Section->maxpos= C.pos;
    }
    Section++;
  }

  SectionList->ncoords= ncoords;
  SectionList->nsections= Section - SectionList->Section;

  UDM_ASSERT(SectionList->ncoords <= coords_alloced);
  UDM_ASSERT(SectionList->nsections <= sections_alloced);

  return UDM_OK;
}


static int
UdmDeleteWordsFromURLBlob(UDM_AGENT *Indexer, UDM_DB *db, urlid_t url_id)
{
  char buf[64];
  /*
   * Remove document if it is not in searching table (state = 1).
   * Mark document for deletion if it is in searching table
   * (state = 2).
   */
  udm_snprintf(buf, sizeof(buf),
               "DELETE FROM bdicti WHERE state=1 AND url_id=%d", url_id);
  if (UDM_OK != UdmSQLQuery(db, NULL, buf))
    return UDM_ERROR;
  udm_snprintf(buf, sizeof(buf),
               "UPDATE bdicti SET state=0 WHERE state=2 AND url_id=%d", url_id);
  if (UDM_OK != UdmSQLQuery(db, NULL, buf))
    return UDM_ERROR;
  return UDM_OK;
}


static int
UdmStoreWordBlobPg(UDM_DB *db, urlid_t url_id,
                   size_t approx_data_length,
                   UDM_PSTR *Chunks,
                   UDM_DSTR *qbuf)
{
  int rc= UDM_OK;
  size_t i;
  UdmDSTRAlloc(qbuf, approx_data_length * 5 + 256);
  UdmDSTRAppendf(qbuf, "INSERT INTO bdicti VALUES(%d,1", url_id);
  for (i= 0; i < 32; i++)
  {
    UDM_PSTR Src= Chunks[i];

    /* Add 'E' prefix for modern Pg versions */
    if (db->version >= 80101)
      UdmDSTRAppend(qbuf, ",E'", 3);
    else
      UdmDSTRAppend(qbuf, ",'", 2);

    if (Src.len)
    {
      size_t slen;
      slen= UdmSQLBinEscStr(db, qbuf->data + qbuf->size_data, qbuf->size_total,
                            Src.val, Src.len);
      qbuf->size_data+= slen;
    }
    UdmDSTRAppend(qbuf, "'", 1);
  }
  UdmDSTRAppend(qbuf, ")", 1);
  return rc;
}


static int
UdmStoreWordBlobUsingHex(UDM_DB *db, urlid_t url_id,
                         size_t approx_data_length,
                         UDM_PSTR *Chunks,
                         UDM_DSTR *qbuf)
{
  size_t i;
  int rc;
  const char *prefix= ",0x"; /* 0xAABBCC syntax by default */
  const char *suffix= "";
  size_t prefix_length= 3;
  size_t suffix_length= 0;

  if (db->flags & UDM_SQL_HAVE_STDHEX) /* X'AABBCC' syntax */
  {
    prefix= ",X'";
    suffix= "'";
    prefix_length= 3;
    suffix_length= 1;
  }
  else if (db->flags & UDM_SQL_HAVE_BLOB_AS_HEX) /* 'AABBCC' syntax */
  {
    prefix= ",'";
    suffix= "'";
    prefix_length= 2;
    suffix_length= 1;
  }
  UdmDSTRAlloc(qbuf, approx_data_length * 2 + 256);
  UdmDSTRAppendf(qbuf, "INSERT INTO bdicti VALUES(");
  if (url_id)
    UdmDSTRAppendf(qbuf, "%d", url_id);
  else
    UdmDSTRAppendSTR(qbuf, "last_insert_id()");
  UdmDSTRAppendSTR(qbuf, ",1");
  for (i= 0; i < 32; i++)
  {
    UDM_PSTR Src= Chunks[i];
    if (Src.len)
    {
      UdmDSTRAppend(qbuf, prefix, prefix_length);
      UdmDSTRAppendHex(qbuf, Src.val, Src.len);
      if (suffix_length)
        UdmDSTRAppend(qbuf, suffix, suffix_length);
    }
    else
      UdmDSTRAppend(qbuf, ",''", 3);
  }
  UdmDSTRAppend(qbuf, ")", 1);
  return rc;
}


static int
UdmStoreWordBlobUsingEncoding(UDM_DB *db, urlid_t url_id,
                              size_t approx_data_length,
                              UDM_PSTR *Chunks,
                              UDM_DSTR *qbuf)
{
  if (db->flags & UDM_SQL_HAVE_0xHEX ||
      db->flags & UDM_SQL_HAVE_BLOB_AS_HEX || /* MonetDB */
      db->flags & UDM_SQL_HAVE_STDHEX)        /* For SQLite3 */
  {
    return UdmStoreWordBlobUsingHex(db, url_id, approx_data_length, Chunks, qbuf);
  }
  else if (db->DBType == UDM_DB_PGSQL)
  {
    return UdmStoreWordBlobPg(db, url_id, approx_data_length, Chunks, qbuf);
  }
  return UDM_ERROR;
}


static int
UdmStoreWordBlobUsingBind(UDM_DB *db, urlid_t url_id, UDM_PSTR *Chunks)
{
  int rc= UDM_OK;
  size_t i;
  char qbuf[512];

  udm_snprintf(qbuf, sizeof(qbuf),
               "INSERT INTO bdicti VALUES(%d,1,"
               "%s,%s,%s,%s,%s,%s,%s,%s,"
               "%s,%s,%s,%s,%s,%s,%s,%s,"
               "%s,%s,%s,%s,%s,%s,%s,%s,"
               "%s,%s,%s,%s,%s,%s,%s,%s)",
               url_id,
               UdmSQLParamPlaceHolder(db, 1),
               UdmSQLParamPlaceHolder(db, 2),
               UdmSQLParamPlaceHolder(db, 3),
               UdmSQLParamPlaceHolder(db, 4),
               UdmSQLParamPlaceHolder(db, 5),
               UdmSQLParamPlaceHolder(db, 6),
               UdmSQLParamPlaceHolder(db, 7),
               UdmSQLParamPlaceHolder(db, 8),
               UdmSQLParamPlaceHolder(db, 9),
               UdmSQLParamPlaceHolder(db, 10),
               UdmSQLParamPlaceHolder(db, 11),
               UdmSQLParamPlaceHolder(db, 12),
               UdmSQLParamPlaceHolder(db, 13),
               UdmSQLParamPlaceHolder(db, 14),
               UdmSQLParamPlaceHolder(db, 15),
               UdmSQLParamPlaceHolder(db, 16),
               UdmSQLParamPlaceHolder(db, 17),
               UdmSQLParamPlaceHolder(db, 18),
               UdmSQLParamPlaceHolder(db, 19),
               UdmSQLParamPlaceHolder(db, 20),
               UdmSQLParamPlaceHolder(db, 21),
               UdmSQLParamPlaceHolder(db, 22),
               UdmSQLParamPlaceHolder(db, 23),
               UdmSQLParamPlaceHolder(db, 24),
               UdmSQLParamPlaceHolder(db, 25),
               UdmSQLParamPlaceHolder(db, 26),
               UdmSQLParamPlaceHolder(db, 27),
               UdmSQLParamPlaceHolder(db, 28),
               UdmSQLParamPlaceHolder(db, 29),
               UdmSQLParamPlaceHolder(db, 30),
               UdmSQLParamPlaceHolder(db, 31),
               UdmSQLParamPlaceHolder(db, 32));
  
  if (UDM_OK != (rc= UdmSQLPrepare(db, qbuf)))
    goto ret;

  for (i= 0; i < 32; i++)
  {
    UDM_PSTR Src= Chunks[i];
    if (Src.len == 0 &&
        db->DBDriver == UDM_DB_ODBC &&
        db->DBType == UDM_DB_ORACLE8)
      Src.len= UDM_SQL_NULL_DATA;
    if (UDM_OK != (rc= UdmSQLBindParameter(db, i + 1, Src.val, Src.len, UDM_SQLTYPE_LONGVARBINARY)))
      goto ret;
  }
  if(UDM_OK != (rc= UdmSQLExecute(db)) ||
     UDM_OK != (rc= UdmSQLStmtFree(db)))
    goto ret;

ret:
  return rc;
}


static int
swbcmp(UDM_WORD *w1, UDM_WORD *w2)
{
  register int _;
  /*
    We don't need to compare W->hash, because
    the words already distributed into different 32 lists,
    according to hash
    if ((_= (int) w1->hash - (int) w2->hash))
      return _;
  */
  if ((_= strcmp(w1->word, w2->word)))
    return _;
  if ((_= (int) w1->secno - (int) w2->secno))
    return _;
  return (int) w1->pos - (int) w2->pos;
}


/*   
   bdicti table has `state` field, that signals bdicti record state.
   There are three possible states:
   0 - deleted document (record is in bdicti and bdict)
   1 - new document (record is in bdicti only)
   2 - converted document (record is in bdicti and bdict)

   New document is added to a collection
   -------------------------------------
   New record is added to bdicti table with `state` set to 1.

   Convert to bdict (full)
   -----------------------
   Read records from bdicti with `state` > 0;
   Convert these records and write converted records to bdict;
   Set `state` to 2 for records with `state` = 1;
   Delete from bdicti records with `state` = 0;
   (afair no parallel bdicti updates assumed)

   Convert to bdict (partial)
   --------------------------
   Read records from bdicti with `state` set to either 0 (deleted) or 1 (new);
   Convert these records and merge converted records with those that are in bdict;
   Set `state` to 2 for records with `state` = 1;
   Delete from bdicti records with `state` = 0;
   (same as above - no parallel updates)

   Document is removed from collection
   -----------------------------------
   Either delete from bdicti document record with `state` = 1,
   Or set `state` to 0 if record `state` is 2.

   All documents are removed (truncation)
   --------------------------------------
   This is simple - truncate both bdict and bdicti tables.

   Document is updated
   -------------------
   See:
     Document is removed from collection;
     Document is added to a collection.

   Searching bdicti
   ----------------
   Only records with `state` 1 (new) or 2 (converted) matter.

   Searching bdicti + bdict
   ------------------------
   Only records with `state` 0 (deleted) or 1 (new) matter.
 */


static int
UdmStoreWordsBlob(UDM_AGENT *Indexer, UDM_DB *db, UDM_DOCUMENT *Doc)
{
  size_t i, nwords;
  urlid_t url_id= UdmVarListFindInt(&Doc->Sections, "ID", 0);
  UDM_DSTR dbuf;
  int rc= UDM_OK;
  int save_section_size= UdmVarListFindInt(&Indexer->Conf->Vars, "SaveSectionSize", 1);
  UDM_WORDLIST WordList[32];
  UDM_PSTR Chunks[32];

  /* Delete old words */
  if (UDM_OK != (rc= UdmDeleteWordsFromURLBlob(Indexer, db, url_id)))
    return rc;

  /*
   * Set hash to UdmStrHash32(word) & 31 for further sorting.
   * If this is stopword, set hash to 0xff (will be skipped while
   * inserting).
   */
  if (!(nwords= Doc->Words.nwords))
    return UDM_OK;
  bzero((void*) &WordList, sizeof(WordList));

  /* Count number of words in each WordList */
  for (i= 0; i < nwords; i++)
  {
    UDM_WORD *W= &Doc->Words.Word[i];
    W->hash= W->secno ? (UdmStrHash32(W->word) & 0x1f) : 0xff;
    if (W->hash < 32)
      WordList[W->hash].nwords++;
  }
  
  /* Allocate memory for each WordList */
  for (i= 0; i < 32; i++)
  {
    if (WordList[i].nwords)
    {
      WordList[i].Word= (UDM_WORD*) UdmMalloc(WordList[i].nwords * sizeof(UDM_WORD));
      WordList[i].nwords= 0;
    }
  }
  
  /* Copy distribute words into 32 lists */
  for (i= 0; i < nwords; i++)
  {
    UDM_WORD *W= &Doc->Words.Word[i];
    if (W->hash < 32)
    {
      UDM_WORDLIST *WL= &WordList[W->hash];
      UDM_ASSERT(WL->Word != NULL);
      WL->Word[WL->nwords++]= *W;
    }
  }
  
  /*
    Sort 32 small lists,
    this is faster than sorting a single big word list.
  */
  for (i= 0; i < 32; i++)
  {
    UDM_WORDLIST *WL= &WordList[i];
    if (WL->nwords)
    {
      UdmSort(WL->Word, WL->nwords, sizeof(UDM_WORD), (udm_qsort_cmp) swbcmp);
    }
  }


  if (UDM_OK != (rc= UdmBlobEncodeBdictiRecord(db, Doc, WordList, &dbuf,
                                               Chunks, save_section_size)))
    goto ret;


  if (db->flags & UDM_SQL_HAVE_BIND_BINARY)
  {
    rc= UdmStoreWordBlobUsingBind(db, url_id, Chunks);
  }
  else
  {
    UDM_DSTR qbuf;
    UdmDSTRInit(&qbuf, 256);
    UdmStoreWordBlobUsingEncoding(db, url_id, dbuf.size_data, Chunks, &qbuf);
    rc= UdmSQLQuery(db, NULL, qbuf.data);
    UdmDSTRFree(&qbuf);
  }

ret:
  /*
    Just free Word pointers
    Don't need UdmWordListFree here,
    because it also frees Word[x].word.
  */
  for (i= 0; i < 32; i++)
    UDM_FREE(WordList[i].Word);
  UdmDSTRFree(&dbuf);
  return rc;
}


static const char*
UdmBlobModeInflateOrSelf(UDM_AGENT *A,
                         UDM_DSTR *buf, const char *name,
                         const char *src, size_t *len)
{
  int use_zint4;
  int use_deflate;
  /* fprintf(stderr, "here, len=%d src=%p\n", *len, src); */
  if (!src || *len < 8 ||
      (unsigned char)src[0] != 0xFF ||
      (unsigned char)src[1] != 0xFF ||
      (unsigned char)src[2] != 0xFF ||
      (unsigned char)src[3] != 0xFF ||
      (src[4] != 1 && src[4] != 2 && src[4] != 3) ||
      src[5] != 0x00 ||
      src[6] != 0x00 ||
      src[7] != 0x00)
    return src;
  use_zint4= src[4] == 2 || src[4] == 3;
  use_deflate= src[4] == 1 || src[4] == 3;
  src+= 8;
  *len-= 8;
  if (name)
    UdmLog(A, UDM_LOG_DEBUG, "Unpacking '%s'", name);
  if (use_deflate)
  {
    udm_timer_t ticks= UdmStartTimer();
    size_t i, mul[4]= {10, 100, 1000, 10000};
    size_t len0= *len;
    UdmLog(A,UDM_LOG_DEBUG, "Deflate header detected");
#ifdef HAVE_ZLIB
    for (i= 0; i < 4; i++)
    {
      size_t reslen, nbytes= len[0] * mul[i];
      /* fprintf(stderr, "allocating %d bytes\n", nbytes); */
      UdmDSTRRealloc(buf, nbytes);
      reslen= UdmInflate(buf->data, buf->size_total, src, *len);
      /* fprintf(stderr, "reslen=%d site_total=%d\n", reslen, buf->size_total);*/
      if (reslen < buf->size_total)
      {
        src= buf->data;
        len[0]= reslen;
        UdmLog(A,UDM_LOG_DEBUG, "%d to %d bytes inflated", len0, reslen);
        break;
      }
    }
    UdmLog(A, UDM_LOG_DEBUG, "Inflating done: %.2f", UdmStopTimer(&ticks));
#endif
  }
  if (*len >= 5 && use_zint4)
  {
    udm_timer_t ticks= UdmStartTimer();
    char *zint4_buf= UdmMalloc(*len);
    UdmLog(A, UDM_LOG_DEBUG, "zint4 header detected (zint4ed data length: %d)", *len);
    if (! zint4_buf)
    {
      UdmLog(A, UDM_LOG_ERROR, "Malloc failed. Requested %u bytes", *len);
      return(NULL);
    }
    memcpy(zint4_buf, src, *len);
    if (buf->size_total < *len * 7 && UdmDSTRRealloc(buf, *len * 7) != UDM_OK)
    {
      UdmFree(zint4_buf);
      UdmLog(A, UDM_LOG_ERROR, "UdmDSTRRealloc failed. Requested %u bytes",
             *len * 7);
      return(NULL);
    }
    *len= udm_dezint4(zint4_buf, (int4 *)buf->data, *len) * 4;
    src= buf->data;
    UdmFree(zint4_buf);
    UdmLog(A, UDM_LOG_ERROR, "dezint4ed data length: %d", *len);
    UdmLog(A, UDM_LOG_ERROR, "dezint4 done: %.2f", UdmStopTimer(&ticks));
  }
  return src;
}


static const char *
UdmBlobModeInflateOrAlloc(UDM_AGENT *A,
                          UDM_DSTR *buf, const char *name,
                          const char *src, size_t *len)
{
  const char *res= UdmBlobModeInflateOrSelf(A, buf, name, src, len);
  if (res == src)
  {
    UdmDSTRRealloc(buf, *len);
    memcpy(buf->data, src, *len);
    buf->size_data= *len;
    res= buf->data;
  }
  return res;
}


static int
UdmInflateBlobModeSQLRes(UDM_AGENT *A, UDM_SQLRES *src)
{
  UDM_DSTR ibuf;
  size_t row;
  UdmDSTRInit(&ibuf, 1024);
  for (row= 0; row < src->nRows; row++)
  {
    size_t len= UdmSQLLen(src, row, 1);
    const char *val= UdmSQLValue(src, row, 1);
    const char *iflt;
    iflt= UdmBlobModeInflateOrSelf(A, &ibuf, NULL, val, &len);
    if (iflt != val)
    {
      size_t offs= src->nCols*row + 1;
      UdmFree(src->Items[offs].val);
      src->Items[offs].val= UdmMalloc(len + 1);
      memcpy(src->Items[offs].val, iflt, len);
      src->Items[offs].len= len;
      src->Items[offs].val[len]= '\0';
    }
  }
  UdmDSTRFree(&ibuf);
  return UDM_OK;
}


static int
UdmAddCollationMatch(UDM_FINDWORD_ARGS *args, const char *word, size_t count)
{
  /*
    If match is not full, then we don't know whether
    the word is a substring or a collation match.
    Let's assume it is a substring, to avoid long 
    word lists in $(WE).
  */
  if (args->Word.match == UDM_MATCH_FULL)
  {
    UDM_WIDEWORD WW= args->WWList->Word[args->Word.order];
    WW.origin= UDM_WORD_ORIGIN_COLLATION;
    WW.count= count;
    UdmWideWordListAddLike(&args->CollationMatches, &WW, word);
  }
  return UDM_OK;
}


static int
UdmBlobAddCoords(UDM_FINDWORD_ARGS *args,
                 UDM_SQLRES *SQLRes)
{
  size_t numrows= UdmSQLNumRows(SQLRes);
  size_t i;
  char *wf= args->wf;
  UDM_URLID_LIST *urls= &args->urls;
  int need_coords= new_version ? args->need_coords : (args->WWList->nwords > 1);
  int save_section_size= args->save_section_size;
  UDM_SECTION Section;
  
  bzero((void*) &Section, sizeof(Section));
  Section.wordnum= args->Word.order & 0xFF;
  Section.order= args->WWList->Word[Section.wordnum].order;

  for (i= 0; i < numrows; i++)
  {
    const unsigned char *s= (const unsigned char *)UdmSQLValue(SQLRes, i, 1);
    size_t length= UdmSQLLen(SQLRes, i, 1);
    unsigned char secno= UDM_ATOI(UdmSQLValue(SQLRes, i, 0));
    const char *cmatch= UdmSQLValue(SQLRes, i, 2);
    UDM_SECTIONLIST SectionList;

    if (!wf[secno])
      continue;

    Section.secno= secno;
        
    UdmSectionListBlobCoordsUnpack(args, &SectionList,
                                   urls, &Section,
                                   s, length,
                                   save_section_size,
                                   need_coords);

#ifdef HAVE_DEBUG
    if (UdmVarListFindBool(&args->db->Vars, "DebugSectionList", 0))
    UdmSectionListPrint(&SectionList);
#endif

    if (SectionList.nsections && SectionList.ncoords)
    {
      UdmSectionListListAdd(&args->SectionListList, &SectionList);
      args->Word.count+= SectionList.ncoords;
      UdmAddCollationMatch(args, cmatch, SectionList.ncoords);
    }
    else
    {
      UdmSectionListFree(&SectionList);
    }
  }

  return UDM_OK;
}


static int
UdmFindWordBlobFromTable(UDM_FINDWORD_ARGS *args, const char *table_name)
{
  char qbuf[4096];
  char secno[32]= "";
  char special[32]= "";
  udm_timer_t ticks;
  UDM_SQLRES SQLRes;
  int rc;

  if (args->urls.empty)
  {
    UdmLog(args->Agent, UDM_LOG_DEBUG,
           "Not searching '%s': Base URL limit is empty", table_name);
    return UDM_OK;
  }
  
  ticks= UdmStartTimer();
  UdmLog(args->Agent, UDM_LOG_DEBUG, "Start fetching");
  if (args->Word.secno)
    udm_snprintf(secno, sizeof(secno), " AND secno=%d", args->Word.secno);
  /*
    When performing substring or number search,
    don't include special data, like '#last_mod_time' or '#rec_id'
  */
  if (args->cmparg[0] != '=')
    udm_snprintf(special, sizeof(special), " AND word NOT LIKE '#%%'");
  udm_snprintf(qbuf, sizeof(qbuf),
               "SELECT secno,intag,word FROM %s WHERE word%s%s%s",
               table_name, args->cmparg, secno, special);
  if(UDM_OK != (rc= UdmSQLQuery(args->db, &SQLRes, qbuf)))
    return rc;
  UdmLog(args->Agent, UDM_LOG_DEBUG, "%-30s%.2f", "Stop  fetching", UdmStopTimer(&ticks));

  ticks= UdmStartTimer();
  UdmLog(args->Agent, UDM_LOG_DEBUG, "Start BlobAddCoords");
  UdmInflateBlobModeSQLRes(args->Agent, &SQLRes);
  UdmBlobAddCoords(args, &SQLRes);
  UdmLog(args->Agent, UDM_LOG_DEBUG, "%-30s%.2f", "Stop  BlobAddCoords", UdmStopTimer(&ticks));
  if (new_version)
  {
    UdmSQLResListAdd(&args->SQLResults, &SQLRes);
  }
  else
  {
    UdmSQLFree(&SQLRes);
  }
  return(UDM_OK);
}


static int
UdmFindWordBlobSimple(UDM_FINDWORD_ARGS *args)
{
  char tablename[64];
  int rc, delta= UdmVarListFindBool(&args->Agent->Conf->Vars, "UseDelta", 0);
  UdmBlobGetRTable(args->Agent, args->db, tablename, sizeof(tablename));
  if (UDM_OK != (rc= UdmFindWordBlobFromTable(args, tablename)))
    return rc;
  if (delta && UDM_OK != (rc= UdmFindWordBlobFromTable(args, "bdict_delta")))
    return rc;
  return UDM_OK;
}


static int
UdmLoadSlowLimitWithSort(UDM_AGENT *A, UDM_DB *db, UDM_URLID_LIST *list, const char *q)
{
  int rc= UdmLoadSlowLimit(A, db, list, q);
  if (rc == UDM_OK)
    rc= UdmURLIdListSort(list);
  return rc;
}

          
int
UdmBlobLoadLiveUpdateLimit(UDM_FINDWORD_ARGS *args, UDM_AGENT *A, UDM_DB *db)
{
  int rc;
  udm_timer_t ticks;
  UDM_ASSERT(db->DBMode == UDM_DBMODE_BLOB);
  if (!args->live_updates)
    return UDM_OK;
  ticks= UdmStartTimer();
  UdmLog(A, UDM_LOG_DEBUG, "Start loading LiveUpdate url_id list");
  if (UDM_OK != (rc= UdmLoadSlowLimitWithSort(A, db, &args->live_update_deleted_urls,
                               "SELECT url_id FROM bdicti WHERE state=0")))
    return rc;
  UdmLog(A, UDM_LOG_DEBUG,
         "Stop loading LiveUpdate url_id list: %.02f, %d updated docs found",
         UdmStopTimer(&ticks),
         args->live_update_deleted_urls.nurls);
  args->live_update_deleted_urls.exclude= 1;
  UdmURLIdListCopy(&args->live_update_active_urls, &args->urls);
  UdmURLIdListMerge(&args->urls, &args->live_update_deleted_urls);
  return UDM_OK;
}


static int
UdmFindWordBlobLiveUpdates(UDM_FINDWORD_ARGS *args)
{
  int rc;
  if ((!UDM_OK == (rc= UdmFindWordBlobSimple(args))) ||
      (!UDM_OK == (rc= UdmFindWordRawBlobDelta(args))))
    goto ret;

ret:
  return rc;
}


static int
UdmFindWordBlob(UDM_FINDWORD_ARGS *args)
{
  return args->live_updates ?
         UdmFindWordBlobLiveUpdates(args) :
         UdmFindWordBlobSimple(args);
}


int
UdmBlobReadTimestamp(UDM_AGENT *A, UDM_DB *db, int *ts, int def)
{
  int rc;
  char lname[]= "#ts";
  char qbuf[64], tablename[64];
  UDM_SQLRES SQLRes;

  UdmBlobGetRTable(A, db, tablename, sizeof(tablename));
  udm_snprintf(qbuf, sizeof(qbuf), "SELECT intag FROM %s WHERE word='%s'",
               tablename, lname);
  if (UDM_OK == (rc= UdmSQLQuery(db, &SQLRes, qbuf)) &&
      UdmSQLNumRows(&SQLRes) > 0)
    *ts= atoi(UdmSQLValue(&SQLRes, 0,0));
  else
    *ts= def;
  UdmSQLFree(&SQLRes);
  return rc;
}


static int
UdmBlobWriteWordPrepare(UDM_DB *db, const char *table)
{
  int rc;
  char qbuf[128];
  const char *int_cast= db->DBType == UDM_DB_PGSQL ? "::integer" : "";
  udm_snprintf(qbuf, sizeof(qbuf),
               "INSERT INTO %s (word, secno, intag) "
               "VALUES(%s, %s%s, %s)",
               table, 
               UdmSQLParamPlaceHolder(db, 1),
               UdmSQLParamPlaceHolder(db, 2),
               int_cast,
               UdmSQLParamPlaceHolder(db, 3));
  rc= UdmSQLPrepare(db, qbuf);
  return rc;
}


static int
UdmBlobWriteWordUsingBind(UDM_DB *db,  const char *table,
                          const char *word, uint4 secno,
                          char *data, size_t len, UDM_DSTR *buf,
                          int auto_prepare)
{
  int rc;
  if ((auto_prepare && 
       UDM_OK != (rc= UdmBlobWriteWordPrepare(db, table))) ||
       UDM_OK != (rc= UdmSQLBindParameter(db, 1, word, (int) strlen(word), UDM_SQLTYPE_VARCHAR)) ||
       UDM_OK != (rc= UdmSQLBindParameter(db, 2, &secno, (int) sizeof(secno), UDM_SQLTYPE_INT32)) ||
       UDM_OK != (rc= UdmSQLBindParameter(db, 3, data, (int) len, UDM_SQLTYPE_LONGVARBINARY) ) ||
       UDM_OK != (rc= UdmSQLExecute(db)) ||
       (auto_prepare && UDM_OK != (rc= UdmSQLStmtFree(db))))
    return rc;
  
  return UDM_OK;
}


static int
UdmBlobWriteWordUsingEncoding(UDM_DB *db,  const char *table,
                              const char *word, size_t secno,
                              char *data, size_t len, UDM_DSTR *buf,
                              int auto_prepare)
{
  int rc;
  size_t escape_factor= db->DBType == UDM_DB_PGSQL ? 5 : 2;
  const char *pf= db->DBType == UDM_DB_PGSQL ? "'" : "0x";
  const char *sf= db->DBType == UDM_DB_PGSQL ? "'" : "";
  const char *E= (db->DBDriver == UDM_DB_PGSQL && db->version >= 80101) ? "E" : "";
  char *s;
  size_t nbytes= 100 + len * escape_factor + 1;

  if (db->flags & UDM_SQL_HAVE_STDHEX) /* X'AABBCC' syntax */
  {
    pf= "X'";
    sf= "'";
  }
  else if (db->flags & UDM_SQL_HAVE_BLOB_AS_HEX) /* 'AABBCC' syntax */
  {
    pf= "'";
    sf= "'";
  }
  
  UdmDSTRReset(buf);

  if (UdmDSTRAlloc(buf, nbytes))
  {
    fprintf(stderr, "BlobWriteWord: DSTRAlloc(%d) failed: word='%s' secno=%d len=%d",
            nbytes, word, secno, len);
    return UDM_OK; /* Skip this word - try to continue */
  }
  UdmDSTRAppendf(buf, "INSERT INTO %s VALUES('%s', %d, %s%s",
                 table, word, secno, E, pf);
  s= buf->data + buf->size_data;
  if (db->DBType == UDM_DB_PGSQL)
  {
    buf->size_data+= UdmSQLBinEscStr(db, s, buf->size_total,
                                     (const char *) data, len);
  }
  else
  {
    buf->size_data+= UdmHexEncode(s, data, len);
  }
  UdmDSTRAppendf(buf, "%s)", sf);
  if (UDM_OK != (rc= UdmSQLQuery(db, NULL, buf->data)))
    return rc;

  UdmDSTRReset(buf);

  return UDM_OK;
}


static int
UdmBlobWriteWordUsingMultiInsert(UDM_DB *db,  const char *table,
                                 const char *word, size_t secno,
                                 char *data, size_t len, UDM_DSTR *buf,
                                 int auto_prepare)
{
  const char *comma= ",";
  size_t escape_factor= 2;
  size_t nbytes= buf->size_data + 100 + len * escape_factor + 1;

  if (UdmDSTRRealloc(buf, nbytes))
  {
    fprintf(stderr, "DSTRAlloc(%d) failed: word='%s' secno=%d len=%d",
            nbytes, word, secno, len);
    return UDM_ERROR;
  }

  if (!buf->size_data)
  {
    UdmDSTRAppendf(buf, "INSERT INTO %s VALUES ", table);
    comma= "";
  }

  UdmDSTRAppendf(buf, "%s('%s',%d,0x", comma, word, secno);
  buf->size_data+= UdmHexEncode(buf->data + buf->size_data, data, len);
  UdmDSTRAppendf(buf, ")");
  return UDM_OK;
}


static int
UdmBlobWriteWord(UDM_DB *db, const char *table,
                 const char *word, size_t secno,
                 char *data, size_t len, UDM_DSTR *buf,
                 int auto_prepare, int use_multi_insert)
{
  int rc, use_bind= db->flags & UDM_SQL_HAVE_BIND_BINARY;

  rc= use_multi_insert ?
      UdmBlobWriteWordUsingMultiInsert(db, table, word, secno, data, len,
                                       buf, auto_prepare) :
      use_bind ?
      UdmBlobWriteWordUsingBind(db, table, word, secno, data, len,
                                buf, auto_prepare):
      UdmBlobWriteWordUsingEncoding(db, table, word, secno, data, len,
                                    buf, auto_prepare);
  return rc;
}


static int
UdmBlobWriteWordCmpr(UDM_DB *db, const char *table,
                     const char *word, size_t secno,
                     char *data, size_t len,
                     UDM_DSTR *buf, UDM_DSTR *z,
                     int use_zint4,
                     int auto_prepare,
                     int allow_multi_insert)
{
#ifdef HAVE_ZLIB
  if (z && len > 256)
  {
    UdmDSTRReset(z);
    UdmDSTRRealloc(z, len + 8 + 1); /* 8 for two INTs */
    /* Append Format version */
    if (use_zint4)
    {
      UdmDSTRAppendCompressionType(z, UDM_BLOB_COMP_ZINT4_DEFLATE);
      z->size_data+= UdmDeflate(z->data + z->size_data,
                                z->size_total - z->size_data, data + 8, len - 8);
    }
    else
    {
      UdmDSTRAppendCompressionType(z, UDM_BLOB_COMP_DEFLATE);
      z->size_data+= UdmDeflate(z->data + z->size_data,
                                z->size_total - z->size_data, data, len);
    }
    if (z->size_data < len)
    {
      data= z->data;
      len= z->size_data;
    }
  }
#endif
  return UdmBlobWriteWord(db, table, word, secno, data, len, buf,
                          auto_prepare, allow_multi_insert);
}


int
UdmBlobWriteTimestamp(UDM_AGENT *A, UDM_DB *db, const char *table)
{
  UDM_DSTR buf;
  int rc= UDM_OK;
  size_t size_data;
  char lname[64]= "#ts";
  char vname[64]= "#version";
  char data[64];
  char qbuf[64];

  UdmLog(A, UDM_LOG_DEBUG, "Writing '%s'", lname);
  UdmDSTRInit(&buf, 128);
  size_data= udm_snprintf(data, sizeof(data), "%d", (int) time(0));
  udm_snprintf(qbuf, sizeof(qbuf),
              "DELETE FROM %s WHERE word='%s'", table, lname);
  if (UDM_OK != (rc= UdmSQLQuery(db, NULL, qbuf)) ||
      UDM_OK != (rc= UdmBlobWriteWord(db, table, lname, 0,
                                       data, size_data, &buf, 1, 0)))
    goto ex;

  size_data= udm_snprintf(data, sizeof(data), "%d", UDM_VERSION_ID);
  udm_snprintf(qbuf, sizeof(qbuf),
               "DELETE FROM %s WHERE word='%s'", table, vname);
  if (UDM_OK != (rc= UdmSQLQuery(db, NULL, qbuf)) ||
      UDM_OK != (rc= UdmBlobWriteWord(db, table, vname, 0,
                                      data, size_data, &buf, 1, 0)))
    goto ex;
ex:
  UdmDSTRFree(&buf);
  return rc;
}


static void
UdmSectionsSortedByID(UDM_VARLIST *Src, UDM_VAR **Dst, size_t ndst)
{
  size_t i;
  bzero((void*) Dst, ndst * sizeof(*Dst));
  for (i= 0; i < Src->nvars; i++)
  {
    UDM_VAR *V= &Src->Var[i];
    if (V->section < ndst)
      Dst[V->section]= V;
  }
}

static int
UdmBlob2BlobConvertOneColumn(UDM_AGENT *A, UDM_DB *db, 
                             UDM_URLDATALIST *URLList,
                             UDM_SQLRES *SQLRes, size_t intag_column_number,
                             UDM_BLOB_CACHE *cache, udm_uint8 *nbytes)
{
  size_t rownum, nrows= UdmSQLNumRows(SQLRes);
  UDM_PSTR row[2];
  UDM_VAR *SectionByID[256];
  udm_timer_t ticks= UdmStartTimer();
  
  UdmSectionsSortedByID(&A->Conf->Sections, SectionByID, 256);

  for (rownum= 0; rownum < nrows; rownum ++)
  {
    urlid_t url_id;
    size_t pos= 0;
    row[0].val= (char*) UdmSQLValue(SQLRes, rownum, 0);
    row[0].len= UdmSQLLen(SQLRes, rownum, 0);
    row[1].val= (char*) UdmSQLValue(SQLRes, rownum, intag_column_number);
    row[1].len= UdmSQLLen(SQLRes, rownum, intag_column_number);
    url_id= UDM_ATOI(row[0].val);
    
    if (!UdmURLDataListSearch(URLList, url_id))
      continue;
    
    while (pos < row[1].len)
    {
      char *word= &row[1].val[pos];
      udmhash32_t word_seed;
      while (pos < row[1].len && row[1].val[pos])
        pos++;
      if (++pos >= row[1].len)
        break;
      word_seed= UdmStrHash32(word) >> 8 & BLOB_CACHE_SIZE;
      while (pos < row[1].len)
      {
        unsigned char secno= (unsigned char)row[1].val[pos];
        char *intag= &row[1].val[++pos];
        size_t nintags, intaglen;
        while (pos < row[1].len && row[1].val[pos])
          pos++;
        nintags= udm_coord_len(intag);
        intaglen= row[1].val + pos - intag;
        if (!SectionByID[secno] ||
            !(SectionByID[secno]->flags & UDM_VARFLAG_NOINDEX))
        {
          (*nbytes)+= intaglen;
          UdmBlobCacheAdd2(&cache[word_seed],
                           url_id, secno, word, nintags, intag, intaglen);
        }
        if (++pos >= row[1].len || ! row[1].val[pos])
        {
          pos++;
          break;
        }
      }
    }
  }
  timer_blob_cache_conv+= (UdmStartTimer() - ticks);
  return UDM_OK;
}



typedef struct udm_wordpair_st
{
  udm_pos_t word0offset;
  udm_pos_t word1offset;
  udm_pos_t diff;
} UDM_WORDPAIR;

typedef struct udm_wordpairlist_st
{
  size_t mitems;
  size_t nitems;
  UDM_WORDPAIR *Item;
} UDM_WORDPAIRLIST;


static void
UdmWordPairListInit(UDM_WORDPAIRLIST *L)
{
  bzero((void*) L, sizeof(*L));
}
static void
UdmWordPairListReset(UDM_WORDPAIRLIST *L)
{
  L->nitems= 0;
}
static void
UdmWordPairListFree(UDM_WORDPAIRLIST *L)
{
  UDM_FREE(L->Item);
}
static int
UdmWordPairListAdd(UDM_WORDPAIRLIST *L, UDM_WORDPAIR *P)
{
  if (L->nitems >= L->mitems)
  {
    L->mitems+= 16*1024;
    L->Item= (UDM_WORDPAIR*) UdmRealloc(L->Item, L->mitems * sizeof(UDM_WORDPAIR));
    if (!L->Item)
      return UDM_ERROR;
  }
  L->Item[L->nitems]= P[0];
  L->nitems++;
  return UDM_OK;
}


static int
cmp_wordpair_by_words_pos_diff(const UDM_WORDPAIR *w1,
                               const UDM_WORDPAIR *w2)
{
  int diff;
  if ((diff= ((int) w1->word0offset - (int) w2->word0offset)))
    return diff;
  if ((diff= ((int) w1->word1offset - (int) w2->word1offset)))
    return diff;
  return (int) w1->diff - (int) w2->diff;
}


typedef struct udm_wordpair_param_st
{
  size_t maxdist;
  size_t maxdist_often;
  size_t ncoordslimit;
  int mode;
} UDM_WORDPAIR_PARAM;


/*
  Perhaps we don't need all pairs for very rare words.
  Very rare words (e.g. which present in a single document only)
  should be ranked high enough by themselfs even without "good"
  distance.
  We need pairs for rare words for distance=1, for phrase search.
  
  DONE: All words that appear only one or two times in a document
  section do not need mutual pairs. Their coordinates are stored in
  basic inverted index records. Not even needed to phrase search.
  So we need to store pairs only when at least one of the two words
  appear more than 2 times in a document section.
  
  IDEA: perhaps we should put pairs like this: 'minpos1-minpos2'
  to reduce amount of possible pairs.

  IDEA: put loose density information, packed in 1 byte.
  IDEA: don't put density if seclen ~= maxpos
  
  IDEA: If word density is low, this document will be displayed in the end
  of result set when searching for one word. Or it can be combined
  with the other words when searching for mulitple words.
  Therefore, in case of low density we can skip putting density information.
  Also, if maxpos is big enough, skip maxpos.
  Also, if minpos is big enough, skip minpos.
  Perhaps we need to store density only for the top N documents
  in one bdict record.
  
  James's statistics:
         4 query words
  11034514 coords
   4155807 sections
     93572 docs
*/

static inline int
UdmPairIsRare2(UDM_WORD *W0, UDM_WORD *W1, UDM_WORDPAIR_PARAM *prm)
{
  return W0->hash <= prm->ncoordslimit && W1->hash <= prm->ncoordslimit;
}


static inline int
UdmPairIsFrequent2(UDM_WORD *W0, UDM_WORD *W1, UDM_WORDPAIR_PARAM *prm)
{
  return W0->hash > prm->ncoordslimit && W1->hash > prm->ncoordslimit;
}

static int
UdmGoodPair(UDM_WORD *W0, UDM_WORD *W1, int diff, UDM_WORDPAIR_PARAM *prm)
{
  if (UdmPairIsRare2(W0, W1, prm))
    return 0; /* Both words are rare (stored in simple bdict records) */
  if (UdmPairIsFrequent2(W0, W1, prm))
    return prm->maxdist_often >= diff; /* frequent + frequent */
  return prm->maxdist >= diff; /* rare + frequent */
}


static int
UdmWordListToWordPairList(UDM_WORDLIST *WL,
                          UDM_WORDPAIRLIST *PL,
                          UDM_WORDPAIR_PARAM *prm)
{
  int rc= UDM_OK;
  size_t i;
  
  for (i= 0; i < WL->nwords - 1; i++)
  {
    UDM_WORD *W0= &WL->Word[i];
    size_t j;
    for (j= 1; j <= prm->maxdist && i + j < WL->nwords; j++)
    {
      UDM_WORD *W1= &WL->Word[i + j];
      if (W0->word && W1->word &&
          /* TODO: add density limit? */
          UdmGoodPair(W0, W1, j, prm))
      {
        UDM_WORDPAIR P;

        UDM_ASSERT(W0->secno == W1->secno);
        P.word0offset= W0->pos;
        P.word1offset= W1->pos;
        P.diff= j;
        /*
        fprintf(stderr, "[%d:%d] %d diff=%d %d/%d %s-%s\n",
                url_id, (int) W0->secno, W0->pos, W1->pos - W0->pos,
                W0->hash, W1->hash, W0->word, W1->word);
        */
        if (UDM_OK != (rc= UdmWordPairListAdd(PL, &P)))
          goto ex;
      }
    }
  }
ex:
  return rc;
}


static int
UdmBlobCacheAddPair(UDM_BLOB_CACHE *cache,
                    urlid_t url_id, unsigned char secno,
                    UDM_WORDLIST *WL,
                    UDM_WORDPAIR_PARAM *prm,
                    UDM_WORDPAIR *P, size_t mindiff, size_t npairs)
{
  size_t nintags= 2; /* mindiff, npairs */
  size_t nbytes, word_seed;
  unsigned char buf[256], *beg= buf/*, *end= buf + sizeof(buf)*/;
  char word_pair_key[128];
  UDM_WORD *W0= &WL->Word[P->word0offset];
  UDM_WORD *W1= &WL->Word[P->word1offset];
  
  switch (prm->mode)
  {
    case 0:
    default:
      udm_snprintf(word_pair_key, sizeof(word_pair_key),
                   "%s-%s", W0->word, W1->word);
      break;
    case 3:
    {
      int delimiter= UdmPairIsFrequent2(W0, W1, prm) ? '+' : '-';
      udm_snprintf(word_pair_key, sizeof(word_pair_key),
                   "%s%c%s", W0->word, delimiter, W1->word);
      break;
    }
    case 1:
      udm_snprintf(word_pair_key, sizeof(word_pair_key),
                 "%.*s-%.*s", 1, W0->word, 1, W1->word);
      nbytes= udm_snprintf((char*) beg, sizeof(buf),
                            "%s-%s", W0->word + 1, W1->word + 1);
      beg+= nbytes + 1;
      break;
    case 2:
    {
      int delimiter= UdmPairIsFrequent2(W0, W1, prm) ? '+' : '-';
      udm_snprintf(word_pair_key, sizeof(word_pair_key),
                 "%.*s%c%.*s", 1, W0->word, delimiter, 1, W1->word);
      nbytes= udm_snprintf((char*) beg, sizeof(buf),
                            "%s%c%s", W0->word + 1, delimiter, W1->word + 1);
      beg+= nbytes + 1;
      break;
    }
    case 4:
    {
      udmcrc32_t crc32;
      crc32= UdmCRC32UpdateStr(0, W0->word);
      crc32= UdmCRC32UpdateStr(crc32, W1->word);
      
#if 0
      udm_snprintf(word_pair_key, sizeof(word_pair_key),
                 "%.*s-%.*s", 1, W0->word, 1, W1->word);
#else
      word_pair_key[0]= W0->word[0];
      word_pair_key[1]= '-';
      word_pair_key[2]= W1->word[0];
      word_pair_key[3]= '\0';
#endif
      udm_put_int4(crc32, beg);
      beg+= 2;
      break;
    }
  }

  word_seed= UdmStrHash32(word_pair_key) >> 8 & BLOB_CACHE_SIZE;      
  
  nintags= mindiff;
/*
  nintags= 1;
  if (!(nbytes= udm_coord_put(mindiff, beg, end)))
    return UDM_ERROR;
  beg+= nbytes;
*/
/*
  nintags= 2;
  if (!(nbytes= udm_coord_put(npairs, beg, end)))
    return UDM_ERROR;
  beg+= nbytes;
*/
  if (0)
  fprintf(stderr, "[%d:%d] %d/%d  %s\n",
          url_id, secno, mindiff, npairs, word_pair_key);
  return  UdmBlobCacheAdd(&cache[word_seed],
                          url_id, secno, word_pair_key,
                          nintags, (char*) buf, beg - buf);
}


static int
UdmBlob2BlobV2ConvertDistancesForSection(UDM_BLOB_CACHE *cache,
                                         UDM_WORDLIST *WL,
                                         UDM_WORDPAIRLIST *PL,
                                         urlid_t url_id, size_t secno,
                                         UDM_WORDPAIR_PARAM *prm)
{
  int rc= UDM_OK;
  size_t i;
  udm_timer_t ticks= UdmStartTimer();
  
  if (!WL->nwords)
    return UDM_OK;
  
  UdmWordPairListReset(PL);
  /*
  for (i= 0; i < WL->nwords; i++)
  {
    fprintf(stderr, "[%d:%d:%d]%s\n", url_id, WL->Word[i].secno, i, WL->Word[i].word);
    UDM_ASSERT(secno == WL->Word[i].secno || (!WL->Word[i].secno && !WL->Word[i].word));
  }
  */

  if (UDM_OK != (rc= UdmWordListToWordPairList(WL, PL, prm)))
    goto ex;
  timer_blob_pair_to_pl+= UdmStartTimer() - ticks;

  if (!PL->nitems)
    goto ex;
  
  ticks= UdmStartTimer();
  UdmSort(PL->Item, PL->nitems, sizeof(UDM_WORDPAIR),
          (udm_qsort_cmp) cmp_wordpair_by_words_pos_diff);
  timer_blob_pair_sort_word+= UdmStartTimer() - ticks;

  /* Group pairs by words */
  ticks= UdmStartTimer();
  for (i= 0; i < PL->nitems; i++)
  {
    UDM_WORDPAIR *P= &PL->Item[i];
    size_t j;
    udm_pos_t mindiff= P->diff;

    for (j= i + 1; j < PL->nitems; j++)
    {
      UDM_WORDPAIR *E= &PL->Item[j];
      if (P->word0offset != E->word0offset ||
          P->word1offset != E->word1offset)
      {
        break;
      }
      else
      {
        if (mindiff > P->diff)
          mindiff= P->diff;
      }
    }
    UdmBlobCacheAddPair(cache, url_id, secno, WL, prm, P, mindiff, j - i);

    i= j - 1; /* +1 will be done in the loop header */
  }
  timer_blob_pair_to_cache+= UdmStartTimer() - ticks;
  
ex:
  return rc;
}


static int
UdmBlob2BlobV2ConvertDistances(UDM_BLOB_CACHE *cache,
                               UDM_WORDLISTLIST *WLL,
                               UDM_WORDPAIRLIST *PL,
                               urlid_t url_id,
                               UDM_WORDPAIR_PARAM *prm)
{
  size_t i;
  for (i= 0; i < 256; i++)
  {
    int rc;
    if (UDM_OK != UdmBlob2BlobV2ConvertDistancesForSection(cache,
                                                           &WLL->Item[i],
                                                           PL,
                                                           url_id, i, prm))
      return rc;
  }
  return UDM_OK;
}


typedef struct udm_word_stat
{
  udm_pos_t minpos;
  udm_pos_t maxpos;
  udm_pos_t seclen;
  udm_pos_t ncoords; /* Without seclen marker */
} UDM_WORD_STAT;



#define WSIZE 1024
static int
UdmWordListAddAtPosition(UDM_WORDLIST *WL, const char *word, 
                         size_t secno, size_t pos, size_t minpos,
                         size_t hash, size_t seclen_marker)
{
  UDM_WORD *W;
  if(pos >= WL->mwords)
  {
    size_t mwords= pos + WSIZE;
    WL->Word= (UDM_WORD *) UdmRealloc(WL->Word, mwords * sizeof(UDM_WORD));
    bzero(&WL->Word[WL->mwords], (mwords - WL->mwords) * sizeof(UDM_WORD));
    WL->mwords= mwords;
  }
  W= &WL->Word[pos];
  W->word= (char*) UdmStrdup(word);
  W->pos= minpos;
  W->secno= secno;
  W->hash= hash;
  W->seclen_marker= seclen_marker;
  if (pos + 1 > WL->nwords)
    WL->nwords= pos + 1;
  return UDM_OK;
}


static int
UdmBlobCoordsToWordList(UDM_WORDLIST *WL, const char *word, size_t secno,
                        UDM_WORD_STAT *st,
                        const char *s, const char *e)
{
  int rc= UDM_OK;
  size_t pos;
  size_t hash= (st->ncoords > 255) ? 255 : st->ncoords;
  
  /* Put all coordinates into WL */
  for (pos= 0, st->ncoords= 0, st->minpos= 0, st->maxpos= 0, st->seclen= 0;
       s < e && *s ; st->ncoords++)
  {
    size_t nbytes, delta;
    if (!(nbytes= udm_coord_get(&delta, (const unsigned char *) s,
                               (const unsigned char *) e)))
      break;
    s+= nbytes;
    if (!st->ncoords)
      st->seclen= st->minpos= st->maxpos= delta;

    if (st->ncoords)
    {
      /*
        Put coord from the previous iteration,
        to avoid seclen markers
      */
      UDM_ASSERT(pos > 0);
      if (UDM_OK != (rc= UdmWordListAddAtPosition(WL, word, secno,
                                                  pos, st->minpos, hash, 0)))
        break;
    }

    pos+= delta;
    st->maxpos= st->seclen;
    st->seclen= pos;
  }

  if (st->ncoords)
    st->ncoords--; /* Don't count seclen marker */

  return rc;
}


static size_t
UdmBlobPackWordStat(char *buf, size_t bufsize, UDM_WORD_STAT *st, size_t *nintags)
{
  size_t nbytes, inverted_density;
  unsigned char *beg= (unsigned char*) buf;
  unsigned char *end= beg + bufsize;
  
  if ((inverted_density= st->seclen / st->ncoords) < 256)
  {
    /*
      For words with good density store inverted density only,
      no minpos, maxpos
    */
    nintags[0]= 1;
    if (!(nbytes= udm_coord_put(inverted_density, beg, end)))
      return 0;
    return nbytes;
  }
  
  if (!(nbytes= udm_coord_put(st->minpos, beg, end)))
    return 0;
  beg+= nbytes;

  if (st->ncoords == 1)
  {
    /* Don't put maxpos if min=max */
    UDM_ASSERT(st->minpos == st->maxpos);
    nintags[0]= 3;
  }
  else
  {
    nintags[0]= 4;
    if (!(nbytes= udm_coord_put(st->maxpos - st->minpos, beg, end)))
     return 0;
    beg+= nbytes;
  }

  if (!(nbytes= udm_coord_put(st->seclen - st->maxpos, beg, end)))
    return 0;
  beg+= nbytes;
  
  if (!(nbytes= udm_coord_put(st->ncoords, beg, end)))
    return 0;
  beg+= nbytes;
  return beg - (unsigned char*) buf;
}


int
UdmBlobCacheWrite(UDM_AGENT *A, UDM_DB *db,
                  UDM_BLOB_CACHE *cache,
                  const char *table, int use_deflate)
{
  size_t w;
  int rc= UDM_OK;
  UDM_DSTR buf, qbuf, zbuf;
  int use_multi_insert= (db->DBType == UDM_DB_MYSQL) &&
                        UdmVarListFindBool(&db->Vars, "MultiInsert", 0);
  /* Whether to use a single Prepare for multiple Execute
    Ibase doesn't work for some reasons.
    Requires implicit transaction ID.
    Perhaps a bug in sql-ibase.c
  */
  int use_bind= !use_multi_insert &&
                 (db->flags & UDM_SQL_HAVE_BIND_BINARY) &&
                 (db->DBType != UDM_DB_IBASE);
  int auto_prepare= (db->DBType == UDM_DB_IBASE);
  int use_compr= UdmVarListFindBool(&db->Vars, "Compr", 0);
  udm_timer_t ticks;
  
  if (!cache->nwords)
    return(UDM_OK);

  ticks= UdmStartTimer();
  UdmBlobCacheSort(cache);
  timer_blob_cache_sort+= UdmStartTimer() - ticks;

  UdmDSTRInit(&buf, 8192);
  UdmDSTRInit(&qbuf, 8192);
  UdmDSTRInit(&zbuf, 8192);
  
  if (use_bind && UDM_OK != (rc= UdmBlobWriteWordPrepare(db, table)))
    goto end;
  
  for (w= 0; w < cache->nwords; )
  {
    UDM_BLOB_CACHE_WORD *word= &cache->words[w];
    ticks= UdmStartTimer();
    w+= UdmBlobCachePackOneWordForBdict(cache, w, &buf, use_compr);
    timer_blob_cache_pack+= UdmStartTimer() - ticks;
    
    ticks= UdmStartTimer();
    if (UDM_OK != (rc= UdmBlobWriteWordCmpr(db, table,
                                            word->word, word->secno,
                                            buf.data, buf.size_data, &qbuf,
                                            use_deflate ? &zbuf : NULL, 0,
                                            auto_prepare, use_multi_insert)))
      goto end;
    timer_blob_cache_send+= UdmStartTimer() - ticks;
    
    UdmDSTRReset(&buf); 
  }

  if (use_bind && UDM_OK != (rc= UdmSQLStmtFree(db)))
    goto end;

  if (use_multi_insert && qbuf.size_data)
  {
    ticks= UdmStartTimer();
    if (UDM_OK != (rc= UdmSQLQuery(db, NULL, qbuf.data)))
    {
      fprintf(stderr, "FAIL\n");
      goto end;
    }
    timer_blob_cache_send+= UdmStartTimer() - ticks;
  }

end:
  UdmDSTRFree(&zbuf);
  UdmDSTRFree(&qbuf);
  UdmDSTRFree(&buf);
  return rc;
}


static int
UdmBlob2BlobWriteCache(UDM_AGENT *Indexer, UDM_DB *db,
                       UDM_BLOB_CACHE *cache,
                       const char *wtable, int use_deflate, size_t *srows)
{
  size_t trows, i;
  int tr= (db->flags & UDM_SQL_HAVE_TRANSACT) ? 1 : 0;
  int rc= UDM_OK;

  if ((tr && UDM_OK != (rc= UdmSQLBegin(db))))
    goto ret;
  for (trows= 0, i= 0; i <= BLOB_CACHE_SIZE; i++)
  {
    if (cache[i].nwords)
    {
      (*srows)+= cache[i].nwords;
      trows+= cache[i].nwords;
      if (UDM_OK != (rc= UdmBlobCacheWrite(Indexer, db, &cache[i], wtable, use_deflate)))
         goto ret;
    }
    UdmBlobCacheFree(&cache[i]);
    if (tr && trows > 16*1024)
    {
      if (UDM_OK != (rc= UdmSQLCommit(db)) ||
          UDM_OK != (rc= UdmSQLBegin(db)))
        goto ret;
      trows= 0;
    }
  }
  if (tr && UDM_OK != (rc= UdmSQLCommit(db)))
    goto ret;

ret:
  return rc;
}


static int
UdmBlob2BlobV2ConvertOneColumn(UDM_SQLRES *SQLRes,
                               UDM_BLOB_CACHE *cache,
                               urlid_t url_id,
                               UDM_WORDLISTLIST *WLL,
                               UDM_VAR **SectionByID,
                               UDM_WORDPAIR_PARAM *prm,
                               size_t rownum,
                               size_t column_number)
{
  int rc= UDM_OK;
  const char *s= UdmSQLValue(SQLRes, rownum, column_number);
  const char *e= s + UdmSQLLen(SQLRes, rownum, column_number);
  udm_timer_t ticks= UdmStartTimer();
  
  while (s < e)
  {
    const char *word= s;

    while (s < e && *s)
      s++;
    if (++s >= e) /* Skip 0x00 */
      break;
    while (s < e)
    {
      unsigned char secno= (unsigned char) *s++;
      int add_sec= !SectionByID[secno] ||
                   !(SectionByID[secno]->flags & UDM_VARFLAG_NOINDEX);
      size_t coord_len, word_seed;
      UDM_WORD_STAT st;
      /* TODO: remove double loop */
      udm_coord_statistics(s, e, &coord_len, &st.ncoords);
      if (!add_sec)
      {
        s+= coord_len;
        goto skip_markers;
      }

      if (UDM_OK != (rc= UdmBlobCoordsToWordList(&WLL->Item[secno],
                                                 word, secno, &st, s , e)))
        goto ex;
      /*
      fprintf(stderr, "urlid=%d secno=%d min=%d max=%d seclen=%d ncoords=%d dens=%.3f %s\n",
              url_id, secno, st.minpos, st.maxpos, st.seclen, st.ncoords, ((float)st.ncoords)/st.seclen, word);
      */

      word_seed= UdmStrHash32(word) >> 8 & BLOB_CACHE_SIZE;
      if (st.ncoords < prm->ncoordslimit)
      {
        /* Put full coords */
        UdmBlobCacheAdd(&cache[word_seed],
                        url_id, secno, word,
                        st.ncoords + 1, /* +1 for seclen marker */
                        s, coord_len);
      }
      else
      {
        /* Put simplified coords */
        size_t nintags, buflen;
        char buf[256];
        buflen= UdmBlobPackWordStat(buf, sizeof(buf), &st, &nintags);
        UdmBlobCacheAdd(&cache[word_seed],
                        url_id, secno, word, nintags, buf, buflen);
        if (0)
        fprintf(stderr, "url_id=%d secno=%d idens=%d (%d/%d) ncoords=%d word=%s\n",
                url_id, secno, st.seclen / st.ncoords, st.seclen, st.ncoords, nintags, word);
        
      }
      s+= coord_len;

skip_markers:
      if (s++ >= e) /* Skip end-of-section marker */
        break;
      
      if (s >= e)
        break;
      if (*s == '\0')
      {
        s++; /* Skip end-of-chunk marker */
        break;
      }
    }
  }
  timer_blob_words_cache_add+= UdmStartTimer() - ticks;
ex:
  return rc;
}


static int
UdmBlob2BlobV2ConvertWords(UDM_AGENT *A, UDM_DB *db, 
                           UDM_URLDATALIST *URLList,
                           UDM_SQLRES *SQLRes,
                           UDM_BLOB_CACHE *cache,
                           udm_uint8 *nbytes_statistics,
                           const char *wtable,
                           int use_deflate,
                           size_t *srows)
{
  size_t rownum, nrows= UdmSQLNumRows(SQLRes);
  UDM_VAR *SectionByID[256];
  UDM_WORDLISTLIST WLL;
  udm_timer_t ticks= UdmStartTimer();
  int rc= UDM_OK;
  UDM_WORDPAIR_PARAM prm;
  UDM_WORDPAIRLIST PL;
  
  prm.maxdist= UdmVarListFindInt(&A->Conf->Vars, "MaxDist", 2);
  prm.maxdist_often= UdmVarListFindInt(&A->Conf->Vars, "MaxDistOften", 1);
  prm.ncoordslimit= UdmVarListFindInt(&A->Conf->Vars, "NCoordsLimit", 16);
  prm.mode= UdmVarListFindInt(&A->Conf->Vars, "WordPairMode", 4);
  
  UdmWordListListInit(&WLL);
  UdmWordPairListInit(&PL);
  UdmSectionsSortedByID(&A->Conf->Sections, SectionByID, 256);

  for (rownum= 0; rownum < nrows; rownum ++)
  {
    size_t column_number;
    urlid_t url_id= UDM_ATOI(UdmSQLValue(SQLRes, rownum, 0));

    if (!UdmURLDataListSearch(URLList, url_id))
      continue;

    /* TODO: don't allocate words, pointers to SQLRes data are enough */
    for (column_number=0 ; column_number <= 0x1F; column_number++)
    {

      if (UDM_OK != (rc= UdmBlob2BlobV2ConvertOneColumn(SQLRes,
                                                        cache, url_id,
                                                        &WLL, SectionByID,
                                                        &prm, rownum,
                                                        column_number + 1)))
       goto ex;
    }
    if (UDM_OK != (rc= UdmBlob2BlobV2ConvertDistances(cache, &WLL, &PL, url_id, &prm)))
      break;
    UdmWordListListReset(&WLL);
  }
  timer_blob_cache_conv+= (UdmStartTimer() - ticks);

  if (UDM_OK != (rc= UdmBlob2BlobWriteCache(A, db,
                                            cache, wtable,
                                            use_deflate, srows)))
    goto ex;

ex:  
  UdmWordListListFree(&WLL);
  UdmWordPairListFree(&PL);
  return rc;
}


static const char *
UdmBlobStateCondition(UDM_AGENT *A)
{
  return
    UdmVarListFindBool(&A->Conf->Vars, "delta", 0) ? "state=1" : "state>0";
}


static int
UdmBlob2BlobConsequent(UDM_AGENT *Indexer, UDM_DB *db,
                       UDM_URLDATALIST *URLList,
                       UDM_BLOB_CACHE *cache, const char *wtable,
                       int use_deflate)
{
  size_t t;
  int rc= UDM_OK;
  size_t srows= 0;
  udm_uint8 nbytes= 0;
  const char *state_cond= UdmBlobStateCondition(Indexer);
  
  for (t= 0; t <= 0x1f; t++)
  {
    size_t nrows;
    UDM_SQLRES SQLRes;
    char buf[128];
    udm_timer_t ticks= UdmStartTimer();
    UdmLog(Indexer, UDM_LOG_DEBUG, "Loading intag%02X", t);
    udm_snprintf(buf, sizeof(buf),
                 "SELECT url_id,intag%02X FROM bdicti WHERE %s", t, state_cond);
    if (UDM_OK != (rc= UdmSQLQuery(db, &SQLRes, buf)))
      goto ret;
    nrows= UdmSQLNumRows(&SQLRes);
    timer_blob_cache_load+= (UdmStartTimer() - ticks);
    UdmLog(Indexer, UDM_LOG_DEBUG, "Loading intag%02X done, %d rows, %.2f sec",
                                   t, nrows, UdmStopTimer(&ticks));
    
    ticks= UdmStartTimer();
    UdmLog(Indexer, UDM_LOG_ERROR, "Converting intag%02X", t);
    UdmBlob2BlobConvertOneColumn(Indexer, db, URLList, &SQLRes, 1, cache, &nbytes);
    UdmLog(Indexer, UDM_LOG_DEBUG,
           "Converting intag%02X done, %.2f sec", t, UdmStopTimer(&ticks));

    ticks= UdmStartTimer();
    UdmLog(Indexer, UDM_LOG_DEBUG, "Writing intag%02X", t);

    if (UDM_OK != (rc= UdmBlob2BlobWriteCache(Indexer, db,
                        cache, wtable, use_deflate, &srows)))
      goto ret;
    
    /* Can't free SQLRes earlier, "cache" has pointers to it */
    UdmSQLFree(&SQLRes);
    UdmLog(Indexer, UDM_LOG_DEBUG,
           "Writing intag%02X done, %.2f sec", t, UdmStopTimer(&ticks));
  }
  UdmLog(Indexer, UDM_LOG_ERROR, "Total converted: %d records, %llu bytes", srows, nbytes);
ret:
  return rc;
}



#define BDICTI_WORD_COLUMNS \
  "intag00,intag01,intag02,intag03," \
  "intag04,intag05,intag06,intag07," \
  "intag08,intag09,intag0A,intag0B," \
  "intag0C,intag0D,intag0E,intag0F," \
  "intag10,intag11,intag12,intag13," \
  "intag14,intag15,intag16,intag17," \
  "intag18,intag19,intag1A,intag1B," \
  "intag1C,intag1D,intag1E,intag1F "


static int
UdmBlob2BlobParallelQuery(UDM_AGENT *Indexer, UDM_DB *db, UDM_SQLRES *SQLRes,
                          const char *state_cond,
                          urlid_t minid, urlid_t maxid)
{
  int rc;
  char buf[512];
  udm_timer_t ticks= UdmStartTimer();
  UdmLog(Indexer, UDM_LOG_ERROR, "Loading documents %d-%d", minid, maxid);
  udm_snprintf(buf, sizeof(buf),
               "SELECT url_id,"
                       "intag00,intag01,intag02,intag03,"
                       "intag04,intag05,intag06,intag07,"
                       "intag08,intag09,intag0A,intag0B,"
                       "intag0C,intag0D,intag0E,intag0F,"
                       "intag10,intag11,intag12,intag13,"
                       "intag14,intag15,intag16,intag17,"
                       "intag18,intag19,intag1A,intag1B,"
                       "intag1C,intag1D,intag1E,intag1F "
               "FROM bdicti WHERE %s AND url_id>=%d AND url_id<%d",
               state_cond, minid, maxid);
  if (UDM_OK != (rc= UdmSQLQuery(db, SQLRes, buf)))
    goto ex;
  UdmLog(Indexer, UDM_LOG_DEBUG, "Loading documents done, %d rows, %.2f sec",
                                  UdmSQLNumRows(SQLRes), UdmStopTimer(&ticks));
ex:
  timer_blob_cache_load+= UdmStartTimer() - ticks;
  return rc;
}


static int
UdmBlob2BlobParallel(UDM_AGENT *Indexer, UDM_DB *db,
                     UDM_URLDATALIST *URLList,
                     UDM_BLOB_CACHE *cache, const char *wtable,
                     int use_deflate, size_t step)
{
  int rc= UDM_OK;
  size_t srows= 0, id;
  udm_uint8 nbytes= 0;
  size_t min_id= URLList->Item[0].url_id;
  size_t max_id= URLList->Item[URLList->nitems - 1].url_id;
  const char *state_cond= UdmBlobStateCondition(Indexer);
  int NewBlob= UdmVarListFindBool(&Indexer->Conf->Vars, "NewBlob", 0);

  for (id= min_id; id <= max_id; id+= step)
  {
    size_t nrows, t;
    UDM_SQLRES SQLRes;

    if (UDM_OK != (rc= UdmBlob2BlobParallelQuery(Indexer, db, &SQLRes,
                                                 state_cond, id, id + step)))
      goto ret;
    
    if (NewBlob)
    {
      fprintf(stderr, "New=yes\n");
      if (UDM_OK != (rc= UdmBlob2BlobV2ConvertWords(Indexer, db, URLList,
                                                    &SQLRes,
                                                    cache, &nbytes,
                                                    wtable, use_deflate,
                                                    &srows)))
        goto ret;
      goto sql_free;
    }

    nrows= UdmSQLNumRows(&SQLRes);
    
    for (t= 0; nrows > 0 && t <= 0x1F; t++)
    {
      udm_timer_t ticks= UdmStartTimer();
      UdmLog(Indexer, UDM_LOG_ERROR, "Converting intag%02X", t);
      UdmBlob2BlobConvertOneColumn(Indexer, db, URLList, &SQLRes, t+1, cache, &nbytes);
      UdmLog(Indexer, UDM_LOG_DEBUG,
             "Converting intag%02X done, %.2f sec", t, UdmStopTimer(&ticks));

      ticks= UdmStartTimer();
      UdmLog(Indexer, UDM_LOG_DEBUG, "Writing intag%02X", t);

      if (UDM_OK != (rc= UdmBlob2BlobWriteCache(Indexer, db,
                          cache, wtable, use_deflate, &srows)))
        goto ret;

      UdmLog(Indexer, UDM_LOG_DEBUG,
             "Writing intag%02X done, %.2f sec", t, UdmStopTimer(&ticks));
    }
sql_free:
    /* Can't free SQLRes earlier, "cache" has pointers to it */
    UdmSQLFree(&SQLRes);
  }
  UdmLog(Indexer, UDM_LOG_ERROR, "Total converted: %d records, %llu bytes", srows, nbytes);
ret:
  return rc;
}


static int
UdmTruncateDictBlob(UDM_AGENT *Indexer,UDM_DB *db)
{
  int rc;
  if(UDM_OK != (rc= UdmSQLTableTruncateOrDelete(db, "bdicti")) ||
     UDM_OK != (rc= UdmSQLTableTruncateOrDelete(db, "bdict")))
    return rc;
  return UDM_OK;
}


static int
UdmBlob2BlobSQL(UDM_AGENT *Indexer, UDM_DB *db, UDM_URLDATALIST *URLList)
{
  size_t i, use_deflate= 0;
  int rc;
  char buf[128], wtable[64];
  UDM_BLOB_CACHE cache[BLOB_CACHE_SIZE + 1];
  int tr= (db->flags & UDM_SQL_HAVE_TRANSACT) ? 1 : 0;
  int tr_truncate= tr && (db->DBType != UDM_DB_SYBASE);
  udm_timer_t ticks;
  int step= UdmVarListFindInt(&db->Vars, "step", 0);
  int disable_keys= UdmVarListFindBool(&db->Vars, "DisableKeys", 0);
  int delta= UdmVarListFindBool(&Indexer->Conf->Vars, "delta", 0);
  
#ifdef HAVE_ZLIB
  if (UdmVarListFindBool(&db->Vars, "deflate", UDM_DEFAULT_DEFLATE))
  {
    UdmLog(Indexer, UDM_LOG_DEBUG, "Using deflate");
    use_deflate= 1;
  }
  else
    UdmLog(Indexer, UDM_LOG_DEBUG, "Not using deflate");
#endif
  /* Get table to write to */
  if (UDM_OK != (rc= UdmBlobGetWTable(Indexer, db, wtable, sizeof(wtable))))
    return rc;
  /* Lock tables for MySQL */
  if (db->DBType == UDM_DB_MYSQL)
  {
    if (db->version >= 40000 && disable_keys)
    {
      sprintf(buf, "ALTER TABLE %s DISABLE KEYS", wtable);
      if (UDM_OK != UdmSQLQuery(db, NULL, buf))
        goto ret;
    }
    udm_snprintf(buf, sizeof(buf), "LOCK TABLES bdicti READ, %s WRITE", wtable);
    if (UDM_OK != (rc= UdmSQLQuery(db, NULL, buf)))
      return rc;
  }

  /* Initialize blob cache */
  for (i= 0; i <= BLOB_CACHE_SIZE; i++)
    UdmBlobCacheInit(&cache[i]);

  /* Delete old words from bdict */
  if ((tr_truncate && UDM_OK != (rc= UdmSQLBegin(db))) ||
      UDM_OK != (rc= UdmSQLTableTruncateOrDelete(db, wtable)) ||
      (tr_truncate && UDM_OK != (rc= UdmSQLCommit(db))))
    goto ret;

  /* Convert words */
  
  
  if (UDM_OK != (rc= step ? UdmBlob2BlobParallel(Indexer, db, URLList,
                                                 cache, wtable,
                                                 use_deflate, step)
                          : UdmBlob2BlobConsequent(Indexer, db, URLList,
                                                   cache, wtable,
                                                   use_deflate)))
    goto ret;
  /* Free blob cache  */
  for (i= 0; i <= BLOB_CACHE_SIZE; i++)
    UdmBlobCacheFree(&cache[i]);

  UdmLog(Indexer, UDM_LOG_DEBUG, "Loading words: %.2f", (float) timer_blob_cache_load / 1000);
  UdmLog(Indexer, UDM_LOG_DEBUG, "Converting words: %.2f", (float) timer_blob_cache_conv / 1000);
  if (timer_blob_words_cache_add ||
      timer_blob_pair_sort_word ||
      timer_blob_pair_to_pl ||
      timer_blob_pair_to_cache)
  {  
    UdmLog(Indexer, UDM_LOG_DEBUG, "Conv.Words-Cache  : %.2f", (float) timer_blob_words_cache_add / 1000);
    UdmLog(Indexer, UDM_LOG_DEBUG, "Conv.Pairs-SortW  : %.2f", (float) timer_blob_pair_sort_word / 1000);
    UdmLog(Indexer, UDM_LOG_DEBUG, "Conv.Pairs-PL     : %.2f", (float) timer_blob_pair_to_pl / 1000);
    UdmLog(Indexer, UDM_LOG_DEBUG, "Conv.Pairs-Cache  : %.2f", (float) timer_blob_pair_to_cache / 1000);
  }
  UdmLog(Indexer, UDM_LOG_DEBUG, "Sorting words: %.2f", (float) timer_blob_cache_sort / 1000);
  UdmLog(Indexer, UDM_LOG_DEBUG, "Packing words: %.2f", (float) timer_blob_cache_pack / 1000);
  UdmLog(Indexer, UDM_LOG_DEBUG, "Sending words: %.2f", (float) timer_blob_cache_send / 1000);
  if (db->DBType == UDM_DB_MYSQL)
  {
    if (UDM_OK != (rc= UdmSQLQuery(db, NULL, "UNLOCK TABLES")))
      return rc;
    udm_snprintf(buf, sizeof(buf), "LOCK TABLES bdicti WRITE, %s WRITE", wtable);
    if (UDM_OK != (rc= UdmSQLQuery(db, NULL, buf)))
      return rc;
  }
  
  /* Put timestamp */
  if ((tr && (UDM_OK != (rc= UdmSQLBegin(db)))) ||
      (UDM_OK != (rc= UdmBlobWriteTimestamp(Indexer, db, wtable))) ||
      (tr && (UDM_OK != (rc= UdmSQLCommit(db)))))
    goto ret;
  
  
  /* Clean bdicti */
  if (!delta)
  {
    ticks= UdmStartTimer();
    UdmLog(Indexer, UDM_LOG_DEBUG, "Cleaning up table 'bdicti'");
    if ((tr && UDM_OK != (rc= UdmSQLBegin(db))) ||
        UDM_OK != (rc= UdmSQLQuery(db, NULL, "DELETE FROM bdicti WHERE state=0")) ||
        UDM_OK != (rc= UdmSQLQuery(db, NULL, "UPDATE bdicti SET state=2")) ||
        (tr && UDM_OK != (rc= UdmSQLCommit(db))))
      goto ret;
    UdmLog(Indexer, UDM_LOG_DEBUG,
           "Cleaning up table 'bdicti' done, %.2f sec", UdmStopTimer(&ticks));
  }
  
  
  if (db->DBType == UDM_DB_MYSQL)
  {
    UdmSQLQuery(db, NULL, "UNLOCK TABLES");
    if (db->version >= 40000 && disable_keys)
    {
      ticks= UdmStartTimer();
      UdmLog(Indexer, UDM_LOG_EXTRA, "Enabling SQL indexes");
      sprintf(buf, "ALTER TABLE %s ENABLE KEYS", wtable);
      UdmSQLQuery(db, NULL, buf);
      UdmLog(Indexer, UDM_LOG_DEBUG,
             "Enabling SQL indexes done, %.2f sec", UdmStopTimer(&ticks));
    }
  }

  /* Convert URL */
  ticks= UdmStartTimer();
  UdmLog(Indexer, UDM_LOG_ERROR, "Converting url data");
  if ((tr && UDM_OK != (rc= UdmSQLBegin(db))) ||
      UDM_OK != (rc= UdmBlobWriteURL(Indexer, db, wtable, use_deflate)) ||
      (tr && UDM_OK != (rc= UdmSQLCommit(db))))
    return rc;
  UdmLog(Indexer, UDM_LOG_DEBUG,
         "Converting URL data done, %.2f sec", UdmStopTimer(&ticks));

  /* Switch to new blob table */
  UdmLog(Indexer, UDM_LOG_ERROR, "Switching to new blob table.");
  rc= UdmBlobSetTable(Indexer, db);
  return rc;

ret:
  for (i= 0; i <= BLOB_CACHE_SIZE; i++)
    UdmBlobCacheFree(&cache[i]);
  if (db->DBType == UDM_DB_MYSQL)
    UdmSQLQuery(db, NULL, "UNLOCK TABLES");
  return rc;
}


int
UdmBlobWriteURL(UDM_AGENT *A, UDM_DB *db, const char *table, int use_deflate)
{
  const char *url;
  int use_zint4= UdmVarListFindBool(&db->Vars, "zint4", UDM_DEFAULT_ZINT4);
  UDM_DSTR buf;
  UDM_DSTR r, s, l, p, z, *pz= use_deflate ? &z : NULL;
  UDM_SQLRES SQLRes;
  int rc= UDM_OK;
  UDM_PSTR row[4];
  size_t pop_rank_not_null_count= 0, nrows;
  udm_timer_t ticks= UdmStartTimer();

  /* Need this if "indexer -Erewriteurl" */
  UdmSQLBuildWhereCondition(A->Conf, db);
  url= (db->from && db->from[0]) ? "url." : "";

  UdmDSTRInit(&buf, 8192);
  UdmDSTRInit(&r, 8192);
  UdmDSTRInit(&s, 8192);
  UdmDSTRInit(&l, 8192);
  UdmDSTRInit(&p, 8192);
  UdmDSTRInit(&z, 8192);

  UdmDSTRAppendf(&buf,
              "SELECT %srec_id, site_id, last_mod_time, pop_rank "
              "FROM url%s%s%s "
              "ORDER BY %srec_id",
              url, db->from, db->where[0] ? " WHERE " : "", db->where, url);
  rc= UdmSQLExecDirect(db, &SQLRes, buf.data);
  UdmDSTRReset(&buf);
  if (rc != UDM_OK) goto ex;

  for (nrows= 0; db->sql->SQLFetchRow(db, &SQLRes, row) == UDM_OK; nrows++)
  {
    double pop_rank= UDM_ATOF(row[3].val);
    UdmDSTRAppendINT4(&r, UDM_ATOI(row[0].val));
    UdmDSTRAppendINT4(&s, UDM_ATOI(row[1].val));
    UdmDSTRAppendINT4(&l, UDM_ATOI(row[2].val));
    /* FIXME: little and big endian incompatibility: */
    UdmDSTRAppend(&p, (char *)&pop_rank, sizeof(pop_rank));
    if (pop_rank != 0)
      pop_rank_not_null_count++;
  }
  UdmSQLFree(&SQLRes);
  UdmLog(A, UDM_LOG_DEBUG, "Loading basic URL data %d rows done: %.2f sec",
         nrows, UdmStopTimer(&ticks));
  
  if (use_zint4)
  {
    size_t i, nrec_ids= r.size_data / 4;
    urlid_t *rec_id= (urlid_t *)r.data;
    char *zint4_buf= UdmMalloc(nrec_ids * 5 + 5);
    UDM_ZINT4_STATE zint4_state;
    if (! zint4_buf)
    {
      rc= UDM_ERROR;
      goto ex;
    }
    udm_zint4_init(&zint4_state, zint4_buf);
    for (i= 0; i < nrec_ids; i++)
      udm_zint4(&zint4_state, rec_id[i]);
    udm_zint4_finalize(&zint4_state);
    UdmDSTRReset(&r);
    UdmDSTRAppendCompressionType(&r, UDM_BLOB_COMP_ZINT4);
    UdmDSTRAppend(&r, (char *)zint4_state.buf,
                  zint4_state.end - zint4_state.buf);
  }

  if (pz)
    UdmDSTRRealloc(pz, p.size_data + 8 + 1);

  UdmDSTRAppendf(&buf, "DELETE FROM %s WHERE word IN ('#rec_id', '#site_id', '#last_mod_time', '#pop_rank')", table);
  if (UDM_OK != (rc= UdmSQLQuery(db, NULL, buf.data)))
    goto ex;
  UdmDSTRReset(&buf);

  if (UDM_OK != (rc= UdmBlobWriteWordCmpr(db, table, "#rec_id", 0, r.data,
                                          r.size_data, &buf, pz, use_zint4, 1, 0)) ||
      UDM_OK != (rc= UdmBlobWriteWordCmpr(db, table, "#site_id", 0, s.data,
                                          s.size_data, &buf, pz, 0, 1, 0)) ||
      UDM_OK != (rc= UdmBlobWriteWordCmpr(db, table, "#last_mod_time", 0,
                                          l.data, l.size_data, &buf, pz, 0, 1, 0)))
    goto ex;
  
  /* Write pop_rank only if we have some non-empty value */
  if (pop_rank_not_null_count > 0 &&
      UDM_OK != (rc= UdmBlobWriteWordCmpr(db, table, "#pop_rank", 0, p.data,
                                          p.size_data, &buf, pz, 0, 1, 0)))
    goto ex;

  UdmLog(A, UDM_LOG_DEBUG, "Writing basic URL data done: %.2f", UdmStopTimer(&ticks));
  
  if (UDM_OK != (rc= UdmBlobWriteLimitsInternal(A, db, table, use_deflate)) ||
      UDM_OK != (rc= UdmBlobWriteTimestamp(A, db, table)))
    goto ex;

ex:
  UdmDSTRFree(&buf);
  UdmDSTRFree(&r);
  UdmDSTRFree(&s);
  UdmDSTRFree(&l);
  UdmDSTRFree(&p);
  UdmDSTRFree(&z);
  return rc;
}


/*
  Unpack URL data from all packed records:
  '#rec_id'
  '#site_id'
  '#last_mod_time'
  '#pop_rank'
  
*/
static size_t
UdmURLDataUnpackFull(UDM_URLDATALIST *DataList, size_t nrows,
                     const char *rec_id_str,
                     const char *site_id_str,
                     const char *last_mod_time_str,
                     const char *pop_rank_str)
{
  size_t i, j;
  for (j= 0, i= 0; i < nrows; i++)
  {
    urlid_t rec_id= udm_get_int4(rec_id_str);
    rec_id_str+= 4;
    
    if (rec_id == DataList->Item[j].url_id)
    {
      UDM_URLDATA *D= &DataList->Item[j];
      if (site_id_str)
        D->site_id= udm_get_int4(&site_id_str[i*4]);
      if (pop_rank_str)
        D->pop_rank= *((const double *) &pop_rank_str[i*8]);
      if (last_mod_time_str)
        D->last_mod_time= udm_get_int4(&last_mod_time_str[i*4]);
      j++;
      if (j == DataList->nitems)
        break;
    }
  }
  return j;
}


/*
  Unpack rec_id from '#rec_id' record.
*/
static size_t
UdmURLDataUnpackRecID(UDM_AGENT *A, UDM_URLDATALIST *DataList, size_t nrows,
                      const char *rec_id_str)
{
  /* Need only rec_id */
  UDM_URLDATA *Data= DataList->Item;
  size_t j, i, skip= 0, ncoords= DataList->nitems;

  for (j = 0, i = 0; i < nrows; i++)
  {
    urlid_t rec_id= udm_get_int4(rec_id_str);
    while (rec_id > Data[j].url_id && j < ncoords)
    {
      skip++;
      j++;
    }

    if (rec_id == Data[j].url_id)
    {
      j++;
      if (j == ncoords) break;
    }
    rec_id_str+= 4;
  }
  if (j < ncoords)
  {
    skip+= (ncoords - j);
    UdmLog(A, UDM_LOG_DEBUG,
           "Warning: %d out of %d coords didn't have URL data", skip, DataList->nitems);
    j= DataList->nitems;
  }
  return j;
}


int
UdmLoadURLDataFromBdict(UDM_AGENT *A,
                        UDM_URLDATALIST *DataList,
                        UDM_DB *db,
                        size_t num_best_rows, int flags)
{
  int rc;
  char qbuf[4*1024], table[64];
  UDM_SQLRES SQLres;
  const char *rec_id_str= NULL;
  const char *site_id_str= NULL;
  const char *last_mod_time_str= NULL;
  const char *pop_rank_str= NULL;
  size_t rec_id_len= 0, site_id_len= 0;
  UDM_DSTR buf, rec_id_buf, site_id_buf, pop_rank_buf, last_mod_time_buf;
  UDM_PSTR row[2];
  size_t last_mod_time_len= 0, pop_rank_len= 0;
  size_t nrecs_expected= 1, nrecs_loaded= 0;
  udm_timer_t ticks= UdmStartTimer();
  int need_pop_rank= (flags & UDM_URLDATA_POP);
  int need_last_mod_time= (flags & UDM_URLDATA_LM);
  int group_by_site= (flags & UDM_URLDATA_SITE);
  int need_site_id= (flags & (UDM_URLDATA_SITE | UDM_URLDATA_SITE_RANK));

  UdmBlobGetRTable(A, db, table, sizeof(table));
  
  UdmDSTRInit(&buf, 64);
  UdmDSTRAppendSTR(&buf, "'#rec_id'");
  if (need_pop_rank)
  {
    UdmDSTRAppendSTR(&buf, ",'#pop_rank'");
    nrecs_expected++;
  }
  if (need_last_mod_time)
  {
    UdmDSTRAppendSTR(&buf, ",'#last_mod_time'");
    nrecs_expected++;
  }
  if (need_site_id)
  {
    UdmDSTRAppendSTR(&buf, ",'#site_id'");
    nrecs_expected++;
  }

  /* Check that DataList is not empty and is sorted by url_id */
  UDM_ASSERT(num_best_rows);
  UDM_ASSERT(DataList->Item[0].url_id <= DataList->Item[num_best_rows-1].url_id);

  UdmLog(A,UDM_LOG_DEBUG,"Trying to load URL data from bdict");
  udm_snprintf(qbuf, sizeof(qbuf), "SELECT word, intag FROM %s WHERE word IN (%s)", table, buf.data);
  UdmDSTRFree(&buf);

  
  if (UDM_OK != (rc= UdmSQLExecDirect(db, &SQLres, qbuf)))
  {
    UdmLog(A,UDM_LOG_DEBUG,"Couldn't run a query on bdict");
    return(rc);
  }

  UdmDSTRInit(&rec_id_buf, 4096);
  UdmDSTRInit(&site_id_buf, 4096);
  UdmDSTRInit(&pop_rank_buf, 4096);
  UdmDSTRInit(&last_mod_time_buf, 4096);

  while (db->sql->SQLFetchRow(db, &SQLres, row) == UDM_OK)
  {
    if (!strcmp(row[0].val, "#rec_id"))
    {
      rec_id_len= row[1].len;
      rec_id_str= UdmBlobModeInflateOrAlloc(A, &rec_id_buf, "#rec_id",
                                            row[1].val, &rec_id_len);
      nrecs_loaded++;
    }
    else if (!strcmp(row[0].val, "#site_id"))
    {
      site_id_len= row[1].len;
      site_id_str= UdmBlobModeInflateOrAlloc(A, &site_id_buf, "#site_id",
                                             row[1].val, &site_id_len);
      nrecs_loaded++;
    }
    else if (!strcmp(row[0].val, "#last_mod_time"))
    {
      last_mod_time_len= row[1].len;
      last_mod_time_str= UdmBlobModeInflateOrAlloc(A, &last_mod_time_buf,
                                                  "#last_mod_time",
                                                  row[1].val, &last_mod_time_len);
      nrecs_loaded++;
    }
    else if (!strcmp(row[0].val, "#pop_rank"))
    {
      pop_rank_len= row[1].len;
      pop_rank_str= UdmBlobModeInflateOrAlloc(A, &pop_rank_buf, "#pop_rank",
                                             row[1].val, &pop_rank_len);
      nrecs_loaded++;
    }
  }
  
  UdmLog(A, UDM_LOG_DEBUG, "Fetch from bdict done: %.2f", UdmStopTimer(&ticks));

  if (need_pop_rank && !pop_rank_str)
  {
    /*
      All pop_rank values were 0 at "indexer -Eblob" time.
      Use 0 as pop_rank values.
    */
    UdmLog(A, UDM_LOG_DEBUG, "Warning: s=R is requested, but '#pop_rank' record not found");
    UdmLog(A, UDM_LOG_DEBUG, "Perhaps you forgot to run 'indexer -n0 -R' before running 'indexer -Eblob'");
    need_pop_rank= 0;
  }
  
  if (rec_id_str && rec_id_len &&
      (site_id_str || ! need_site_id) &&
      (last_mod_time_str || ! need_last_mod_time) &&
      (pop_rank_str || ! need_pop_rank))
  {
    size_t j, nrows= rec_id_len / 4;

    ticks= UdmStartTimer();
    UdmLog(A, UDM_LOG_DEBUG, "Unpacking URL Data %d rows", nrows);
    if (need_pop_rank || need_last_mod_time ||
        (!group_by_site && need_site_id) /* GroupBySite=rank */)
    {
      /* Need pop_rank or last_mod_time */
      j= UdmURLDataUnpackFull(DataList, nrows, rec_id_str,
                              need_site_id ? site_id_str : NULL,
                              need_last_mod_time ? last_mod_time_str : NULL,
                              need_pop_rank ? pop_rank_str : NULL);

    }
    else if (group_by_site && need_site_id)
    {
      /* Need rec_id and site_id */
      if (UDM_OK != UdmURLDataListGroupBySiteUsingHash(A, DataList,
                                                      rec_id_str, rec_id_len,
                                                      site_id_str, site_id_len))
      {
        j= DataList->nitems;
        group_by_site= 1;
      }
      else
      {
        j= DataList->nitems;
        group_by_site= 0;
      }
    }
    else
    {
      /* Need only rec_id */
      j= UdmURLDataUnpackRecID(A, DataList, nrows, rec_id_str);
    }

    UdmLog(A, UDM_LOG_DEBUG, "Unpacking URL Data done: %.02f", UdmStopTimer(&ticks));

    if (j != DataList->nitems)
    {
      UdmLog(A,UDM_LOG_DEBUG,"Expected to load %d URLs, loaded %d", DataList->nitems, j);
      UdmLog(A,UDM_LOG_DEBUG,"Couldn't load URL data from bdict");
      goto load_from_url;
    }

    rc= group_by_site ?
        UdmURLDataListGroupBySiteUsingSort(A, DataList, db) : UDM_OK;
    goto ret;
  }
  else
  {
    UdmLog(A,UDM_LOG_DEBUG,"There is no URL data in bdict");
  }
  
load_from_url:
  rc= UdmLoadURLDataFromURL(A, DataList, db, num_best_rows, flags);
  
ret:
  UdmSQLFree(&SQLres);
  UdmDSTRFree(&rec_id_buf);
  UdmDSTRFree(&site_id_buf);
  UdmDSTRFree(&pop_rank_buf);
  UdmDSTRFree(&last_mod_time_buf);
  return rc;
}

/*
  Write limits, but don't COMMIT and don't write timestamp
*/
int
UdmBlobWriteLimitsInternal(UDM_AGENT *A, UDM_DB *db,
                           const char *table, int use_deflate)
{
  UDM_VARLIST *Vars= &A->Conf->Vars;
  UDM_VAR *Var;
  UDM_DSTR l, buf;
  int rc= UDM_OK;
  
  UdmDSTRInit(&l, 8192);
  UdmDSTRInit(&buf, 8192);
  for (Var= Vars->Var; Var < Vars->Var + Vars->nvars; Var++)
  {
    size_t i, ndocs;
    char qbuf[128];
    char lname[64];
    UDM_URLID_LIST list;
    UDM_URL_INT4_LIST UserScore;
    int is_score= 0;
    udm_timer_t ticks;

    if (!strncasecmp(Var->name, "Limit.", 6))
      udm_snprintf(lname, sizeof(lname), "#limit#%s", Var->name + 6);
    else if (!strncasecmp(Var->name, "Order.", 6))
      udm_snprintf(lname, sizeof(lname), "#order#%s", Var->name + 6);
    else if ((is_score= !strncasecmp(Var->name, "Score.", 6)))
      udm_snprintf(lname, sizeof(lname), "#score#%s", Var->name + 6);
    else
      continue;
    UdmLog(A, UDM_LOG_DEBUG, "Writing '%s'", lname);

    bzero((void*) &list, sizeof(list));
    bzero((void*) &UserScore, sizeof(UserScore));
    
    if (UDM_OK != (rc= is_score ? 
                       UdmUserScoreListLoad(A, db, &UserScore, Var->val) :
                       UdmLoadSlowLimit(A, db, &list, Var->val)))
      goto ret;
    
    ticks= UdmStartTimer();
    
    if (!strncasecmp(Var->name, "Limit.", 6))
      UdmURLIdListSort(&list);
    
    UdmDSTRReset(&buf);
    UdmDSTRReset(&l);
    ndocs= is_score ? UserScore.nitems : list.nurls;
    for (i= 0; i < ndocs; i++)
    {
      if (is_score)
      {
        UDM_URL_INT4 *item= &UserScore.Item[i];
        char ch= item->param;
        UdmDSTRAppendINT4(&l, item->url_id);
        UdmDSTRAppend(&l, &ch, 1);
      }
      else
      {
        /* Limit */
        UdmDSTRAppendINT4(&l, list.urls[i]);
      }
    }

    udm_snprintf(qbuf, sizeof(qbuf), "DELETE FROM %s WHERE word=('%s')", table, lname);
    if(UDM_OK != (rc= UdmSQLQuery(db, NULL, qbuf)))
      goto ret;

    if (l.size_data)
    {
      if (UDM_OK != (rc= UdmBlobWriteWordCmpr(db, table, lname, 0, l.data,
                                              l.size_data, &buf, NULL, 0, 1, 0)))
        goto ret;
    }

    UDM_FREE(list.urls);
    UDM_FREE(UserScore.Item);
    UdmLog(A, UDM_LOG_DEBUG, "%d documents written to '%s': %.2f",
                              ndocs, lname, UdmStopTimer(&ticks));
  }
ret:
  UdmDSTRFree(&l);
  UdmDSTRFree(&buf);
  return rc;
}


/*
  Name is already escaped here, using UdmSQLEscStrSimple().
*/
static int
UdmBlobLoadFastURLLimitByFullName(UDM_AGENT *A,
                                  UDM_DB *db,
                                  const char *ename,
                                  UDM_URLID_LIST *buf)
{
  int rc= UDM_OK;
  UDM_SQLRES SQLRes;
  char qbuf[256], tablename[64], exclude;
  size_t nrows, nurls, i, row;
  
  exclude= buf->exclude;
  bzero((void*)buf, sizeof(*buf));
  buf->exclude= exclude;
  
  UdmBlobGetRTable(A, db, tablename, sizeof(tablename));
  udm_snprintf(qbuf, sizeof(qbuf),
               "SELECT intag FROM %s WHERE word LIKE '%s'", tablename, ename);
  if (UDM_OK != (rc= UdmSQLQuery(db, &SQLRes, qbuf)))
   goto ret;

  if (! (nrows= UdmSQLNumRows(&SQLRes)))
  {
    buf->empty= 1;
    goto ret;
  }
  nurls= 0;
  for (row= 0; row < nrows; row++)
    nurls+= UdmSQLLen(&SQLRes, row, 0) / 4;

  if (!(buf->urls= UdmMalloc(sizeof(urlid_t) * nurls)))
    goto ret;

  for (row= 0; row < nrows; row++)
  {
    const char *src= UdmSQLValue(&SQLRes, row, 0);
    nurls= UdmSQLLen(&SQLRes, row, 0) / 4;
    if (src && nurls)
      for (i = 0; i < nurls; i++, src+= 4)
        buf->urls[buf->nurls++]= (urlid_t) udm_get_int4(src);
  }
  UdmURLIdListSort(buf);
  
ret:
  UdmSQLFree(&SQLRes);
  return rc;
}


int
UdmBlobLoadFastURLLimit(UDM_AGENT *A, UDM_DB *db,
                        const char *name, UDM_URLID_LIST *buf)
{
  char ename[130], ename2[130];
  size_t namelen= strlen(name);
  if (namelen > 64)
    return UDM_OK;
  UdmSQLEscStrSimple(db, ename, name, namelen);
  udm_snprintf(ename2, sizeof(ename2), "#limit#%s", ename);
  return UdmBlobLoadFastURLLimitByFullName(A, db, ename2, buf);
}


static int
UdmBlobLoadFastOrderOrFastScore(UDM_AGENT *A, UDM_DB *db, UDM_SQLRES *SQLRes,
                                const char *prefix, const char *name)
{
  char qbuf[256], ename[256], tablename[64];
  size_t namelen= strlen(name);
  bzero((void*) SQLRes, sizeof(*SQLRes));
  if (namelen > 64)
    return UDM_OK;
  UdmSQLEscStrSimple(db, ename, name, namelen); /* Escape order name */
  UdmBlobGetRTable(A, db, tablename, sizeof(tablename));
  udm_snprintf(qbuf, sizeof(qbuf),
               "SELECT intag FROM %s WHERE word LIKE '#%s#%s'",
               tablename, prefix, ename);
  return UdmSQLQuery(db, SQLRes, qbuf);
}


static int
UdmBlobUnpackFastOrder(UDM_URL_INT4_LIST *List,
                       UDM_SQLRES *SQLRes,
                       size_t record_size)
{
  size_t nrows, nurls, row, param;
  int rc= UDM_OK;

  bzero((void*)List, sizeof(*List));
   
  if (!(nrows= UdmSQLNumRows(SQLRes)))
    goto ret;

  nurls= 0;
  for (row= 0; row < nrows; row++)
    nurls+= UdmSQLLen(SQLRes, row, 0) / record_size;

  if (!(List->Item= UdmMalloc(sizeof(UDM_URL_INT4) * nurls)))
  {
    rc= UDM_ERROR;
    goto ret;
  }
  
  for (param= 0x7FFFFFFF, row= 0; row < nrows; row++)
  {
    const char *src= UdmSQLValue(SQLRes, row, 0);
    nurls= UdmSQLLen(SQLRes, row, 0) / record_size;
    if (src && nurls)
    {
      size_t i;
      for (i= 0; i < nurls; i++, src+= record_size)
      {
        UDM_URL_INT4 *Item= &List->Item[List->nitems++];
        Item->url_id= (urlid_t) udm_get_int4(src);
        if (record_size == 5)
          Item->param= src[4];
        else
          Item->param= --param;
      }
    }
  }
  if (List->nitems > 1)
    UdmSort(List->Item, List->nitems, sizeof(UDM_URL_INT4), (udm_qsort_cmp) UdmCmpURLID);
ret:
  return rc;
}


int
UdmBlobLoadFastOrder(UDM_AGENT *A, UDM_DB *db,
                     UDM_URL_INT4_LIST *List, const char *name)
{
  int rc= UDM_OK;
  UDM_SQLRES SQLRes;

  if (UDM_OK != (rc= UdmBlobLoadFastOrderOrFastScore(A, db, &SQLRes, "order", name)) ||
      UDM_OK != (rc= rc= UdmBlobUnpackFastOrder(List, &SQLRes, 4)))
    goto ret;

ret:
  UdmSQLFree(&SQLRes);
  return rc;
}


int
UdmBlobLoadFastScore(UDM_AGENT *A, UDM_DB *db,
                     UDM_URL_INT4_LIST *List, const char *name)
{
  int rc= UDM_OK;
  UDM_SQLRES SQLRes;
  
  if (UDM_OK != (rc= UdmBlobLoadFastOrderOrFastScore(A, db, &SQLRes, "score", name)) ||
      UDM_OK != (rc= rc= UdmBlobUnpackFastOrder(List, &SQLRes, 5)))
    goto ret;
  
ret:
  UdmSQLFree(&SQLRes);
  return rc;
}


static int
UdmWordStatCreateBlob(UDM_AGENT *A, UDM_DB *db)
{
  char qbuf[128], tablename[64], expr[64];
  UdmBlobGetTableForRewrite(A, db, tablename, sizeof(tablename));
  switch(db->DBType)
  {
    case UDM_DB_ORACLE8:
      udm_snprintf(expr, sizeof(expr), "lengthb(intag)");
      break;
    case UDM_DB_SQLITE3:
      udm_snprintf(expr, sizeof(expr), "length(intag)");
      break;
    case UDM_DB_MONETDB:
      udm_snprintf(expr, sizeof(expr), "length(cast(intag as text))");
      break;
    case UDM_DB_MSSQL:
      udm_snprintf(expr, sizeof(expr), "datalength(intag)");
      break;
    default:
      udm_snprintf(expr, sizeof(expr), "octet_length(intag)");
  }
  udm_snprintf(qbuf, sizeof(qbuf),
               "SELECT word, sum(%s) FROM %s GROUP BY word",
               expr, tablename);
  return UdmWordStatQuery(A, db, qbuf);
}


/*
  Dump word information to stdout.
*/
static int
UdmDumpWordInfoOneDocBlob(UDM_AGENT *A, UDM_DB *db, UDM_DOCUMENT *Doc)
{
  size_t i;
  int rc;
  char buf[512];
  urlid_t url_id=  UdmVarListFindInt(&Doc->Sections, "ID", 0);
  UDM_SQLRES SQLRes;
  UDM_PSTR Chunks[32];
  UDM_DSTR qbuf;
  size_t total_length;
  udm_snprintf(buf, sizeof(buf),
               "SELECT " BDICTI_WORD_COLUMNS "FROM bdicti WHERE url_id=%d",
               url_id);
  if (UDM_OK != (rc= UdmSQLQuery(db, &SQLRes, buf)))
    return rc;
  
  if (UdmSQLNumRows(&SQLRes) != 1)
    goto ret;
  
  for (total_length= 0, i= 0; i < 32; i++)
  {
    Chunks[i].val= (char*) UdmSQLValue(&SQLRes, 0, i);
    Chunks[i].len= UdmSQLLen(&SQLRes, 0, i);
    total_length+= Chunks[i].len;
  }
  UdmDSTRInit(&qbuf, 256);
  UdmStoreWordBlobUsingEncoding(db, 0, total_length, Chunks, &qbuf);
  printf("%s;\n", qbuf.data);
  UdmDSTRFree(&qbuf);
ret:
  UdmSQLFree(&SQLRes);
  return rc;
}


UDM_DBMODE_HANDLER udm_dbmode_handler_blob=
{
  "blob",
  UdmBlob2BlobSQL,
  UdmStoreWordsBlob,
  UdmTruncateDictBlob,
  UdmDeleteWordsFromURLBlob,
  UdmFindWordBlob,
  UdmDumpWordInfoOneDocBlob,
  UdmWordStatCreateBlob,
};

#endif /* HAVE_SQL */
