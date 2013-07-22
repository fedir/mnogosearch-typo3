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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_db.h"
#include "udm_hash.h"
#include "udm_sqldbms.h"
#include "udm_log.h"
#include "udm_vars.h"
#include "udm_word.h"
#include "udm_searchtool.h"

#ifdef HAVE_SQL

/********************* MultiCache stuff *************************/
UDM_MULTI_CACHE *UdmMultiCacheInit (UDM_MULTI_CACHE *cache)
{
  size_t i;

  if (! cache)
  {
    cache = UdmMalloc(sizeof(UDM_MULTI_CACHE));
    if (! cache) return(NULL);
    cache->free = 1;
  }
  else
  {
    cache->free = 0;
  }
  cache->nrecs = 0;

  for (i = 0; i <= MULTI_DICTS; i++)
  {
    cache->tables[i].nurls = 0;
    cache->tables[i].urls = NULL;
  }

  cache->nurls = 0;
  cache->urls = NULL;

  return(cache);
}

void UdmMultiCacheFree (UDM_MULTI_CACHE *cache)
{
  size_t w, s, u, t;

  if (! cache) return;
  for (t = 0; t <= MULTI_DICTS; t++)
  {
    UDM_MULTI_CACHE_TABLE *table = &cache->tables[t];
    for (u = 0; u < table->nurls; u++)
    {
      UDM_MULTI_CACHE_URL *url = &table->urls[u];
      for (s = 0; s < url->nsections; s++)
      {
        UDM_MULTI_CACHE_SECTION *section = &url->sections[s];
        for (w = 0; w < section->nwords; w++) {
          UDM_MULTI_CACHE_WORD *word = &section->words[w];
          UdmFree(word->word);
          UdmFree(word->intags);
        }
        UdmFree(section->words);
      }
      UdmFree(url->sections);
    }
    UdmFree(table->urls);
    cache->tables[t].nurls = 0;
    cache->tables[t].urls = NULL;
  }

  UdmFree(cache->urls);
  cache->nurls = 0;
  cache-> urls = NULL;
  cache->nrecs = 0;

  if (cache->free) UdmFree(cache);
}


static int mccmpword(uint4 *a, uint4 *b)
{
  if (*a > *b) return 1;
  if (*a < *b) return -1;
  return 0;
}


static size_t UdmMultiCacheAddWord (UDM_MULTI_CACHE_WORD *cache, uint4 coord)
{
  uint4 *tmp;

  if (! cache) return(0);
  tmp = UdmRealloc(cache->intags, (cache->nintags + 1) * sizeof(uint4));
  if (! tmp) return(0);
  cache->intags = tmp;
  cache->intags[cache->nintags] = coord;
  cache->nintags++;
  UdmSort(cache->intags, cache->nintags, sizeof(uint4), (udm_qsort_cmp)mccmpword);
  return(1);
}

static size_t UdmMultiCacheAddSection (UDM_MULTI_CACHE_SECTION *cache, UDM_WORD *word)
{
  size_t i;
  uint4 coord = word->pos & 0x1FFFFF;

  if (! cache) return(0);

  for (i = 0; i < cache->nwords; i++)
  {
    if (! strcmp(cache->words[i].word, word->word))  break;
  }

  if (i == cache->nwords)
  {
    UDM_MULTI_CACHE_WORD *tmp;
    tmp = UdmRealloc(cache->words, (cache->nwords + 1) * sizeof(UDM_MULTI_CACHE_WORD));
    if (! tmp) return(0);
    cache->words = tmp;
    cache->words[cache->nwords].word = (char *)UdmStrdup(word->word);
    cache->words[cache->nwords].nintags = 0;
    cache->words[cache->nwords].intags = NULL;
    cache->nwords++;
  }

  return(UdmMultiCacheAddWord(&cache->words[i], coord));
}

static size_t UdmMultiCacheAddURL (UDM_MULTI_CACHE_URL *cache, UDM_WORD *word)
{
  size_t i;
  unsigned char secno= word->secno;

  if (! cache) return(0);

  for (i = 0; i < cache->nsections; i++)
    if (cache->sections[i].secno == secno) break;

  if (i == cache->nsections)
  {
    UDM_MULTI_CACHE_SECTION *tmp;
    tmp = UdmRealloc(cache->sections, (cache->nsections + 1) * sizeof(UDM_MULTI_CACHE_SECTION));
    if (! tmp) return(0);
    cache->sections = tmp;
    cache->sections[cache->nsections].secno = secno;
    cache->sections[cache->nsections].nwords = 0;
    cache->sections[cache->nsections].words = NULL;
    cache->nsections++;
  }

  return(UdmMultiCacheAddSection(&cache->sections[i], word));
}

static size_t UdmMultiCacheAddTable (UDM_MULTI_CACHE_TABLE *cache, urlid_t url_id, unsigned char reindex, UDM_WORD *word)
{
  size_t i;

  if (! cache) return(0);
  for (i = 0; i < cache->nurls; i++)
  {
    if (cache->urls[i].url_id == url_id) break;
  }

  if (i == cache->nurls)
  {
    UDM_MULTI_CACHE_URL *tmp;
    tmp = UdmRealloc(cache->urls, (cache->nurls + 1) * sizeof(UDM_MULTI_CACHE_URL));
    if (! tmp) return(0);
    cache->urls = tmp;
    cache->urls[cache->nurls].url_id = url_id;
    cache->urls[cache->nurls].reindex = reindex;
    cache->urls[cache->nurls].nsections = 0;
    cache->urls[cache->nurls].sections = NULL;
    cache->nurls++;
  }

  return(UdmMultiCacheAddURL(&cache->urls[i], word));
}

size_t UdmMultiCacheAdd(UDM_MULTI_CACHE *cache, urlid_t url_id, unsigned char reindex, UDM_WORD *word)
{
  udmhash32_t table = UdmStrHash32(word->word) & MULTI_DICTS;
  size_t i;
  
  if (! cache) return(0);

  cache->nrecs++;

  if (reindex)
  {
    for (i = 0; i < cache->nurls; i++)
      if (cache->urls[i] == url_id) break;

    if (i == cache->nurls)
    {
      urlid_t *tmp;
      tmp = UdmRealloc(cache->urls, (cache->nurls + 1) * sizeof(urlid_t));
      if (! tmp) return(0);
      cache->urls = tmp;
      cache->urls[cache->nurls] = url_id;
      cache->nurls++;
    }
  }

  return UdmMultiCacheAddTable(&cache->tables[table], url_id, reindex, word);
}



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


static size_t
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



int
UdmCoordListMultiUnpack(UDM_URLCRDLIST *CoordList,
                             UDM_URL_CRD *CoordTemplate,
                             const unsigned char *intag, size_t lintag,
                             int save_section_size)
{
  const unsigned char *s, *e;
  size_t last= 0;
  size_t ncoords0= CoordList->ncoords;
  
  for (s= intag, e= intag + lintag; e > s; )
  {
    UDM_URL_CRD *Crd= &CoordList->Coords[CoordList->ncoords];
    size_t crd;
    size_t nbytes= udm_coord_get(&crd, s, e);
    if (!nbytes) break;
    last+= crd;
    Crd->url_id= CoordTemplate->url_id;
    Crd->pos= last;
    Crd->secno= CoordTemplate->secno;
    Crd->num= CoordTemplate->num;
    Crd->seclen= 0;
    CoordList->ncoords++;
    s+= nbytes;
  }
  
  if (save_section_size)
  {
    uint4 seclen= CoordList->Coords[--CoordList->ncoords].pos;
    for ( ; ncoords0 < CoordList->ncoords; ncoords0++)
      CoordList->Coords[ncoords0].seclen= seclen;
  }
  return UDM_OK;
}


#define COORD_MAX_LEN 4

/*
  Encode intag:
  0 - hex encoding
  1 - hex encoding with '0x' prefix
  2 - PgSQL bytea encoding
*/
char*
UdmMultiCachePutIntagUsingEncoding(UDM_MULTI_CACHE_WORD *cache,
                                   UDM_DB *db, char type)
{
  size_t c;
  size_t len;
  char *_;
  unsigned char buf[COORD_MAX_LEN];
  unsigned char *bufend = buf + sizeof(buf);
  size_t nbytes;
  size_t last;
  size_t multiply= (type == 2) ? 3*5 : 3*2;
  size_t alloced_length;

  if (! cache->nintags) return(NULL);

  /* Allocate maximum possible length. */
  _ = UdmMalloc((alloced_length= cache->nintags * multiply + 3));
  if (! _) return(NULL);

  if (type == 1)
  {
    strcpy(_, "0x");
    len= 2;
  }
  else
  {
    *_ = 0;
    len = 0;
  }

  last = 0;
  for (c = 0; c < cache->nintags; c++)
  {
    if (cache->intags[c] < last)
    {
      UdmFree(_);
      return(NULL);
    }

    nbytes = udm_coord_put(cache->intags[c] - last, buf, bufend);
    if (! nbytes)
    {
      UdmFree(_);
      return(NULL);
    }

    if (type == 2)
    {
      len+= UdmSQLBinEscStr(db, _ + len, alloced_length, (char*) buf, nbytes);
      _[len]= '\0'; /* Check if really necessary - possibly  already done */
    }
    else
    {
      len+= UdmHexEncode(_ + len, (char*) buf, nbytes);
    }

    last = cache->intags[c];
  }

  return(_);
}


char*
UdmMultiCachePutIntagUsingRaw(UDM_MULTI_CACHE_WORD *cache)
{
  size_t c;
  size_t len = 0;
  char *_;
  unsigned char buf[COORD_MAX_LEN];
  unsigned char *bufend = buf + sizeof(buf);
  size_t nbytes;
  size_t last;

  if (! cache->nintags) return(NULL);

  /* Allocate maximum possible length. */
  _ = UdmMalloc(cache->nintags * COORD_MAX_LEN + 1);
  if (! _) return(NULL);

  last = 0;
  for (c = 0; c < cache->nintags; c++)
  {
    if (cache->intags[c] < last)
    {
      UdmFree(_);
      return(NULL);
    }

    nbytes = udm_coord_put(cache->intags[c] - last, buf, bufend);
    if (! nbytes)
    {
      UdmFree(_);
      return(NULL);
    }

    memcpy(_ + len, buf, nbytes);
    len += nbytes;

    last = cache->intags[c];
  }
  _[len] = 0;

  return(_);
}

#define UDM_THREADINFO(A,s,m)	if(A->Conf->ThreadInfo)A->Conf->ThreadInfo(A,s,m)


static int
UdmWordCacheWriteUsingBind(UDM_DB *db, int seed, urlid_t url_id,
                           int secno, const char *word,
                           char *coord, size_t coord_len)
{
  int rc;
  int BindType= UDM_SQLTYPE_LONGVARBINARY;
  char qbuf[128];
  
  if (db->DBType == UDM_DB_IBASE)
    BindType= UDM_SQLTYPE_VARCHAR;
  
  /*
    DBType   Data type        Native ODBC   BindType
    -------  ---------------- ------ ----   
    Cache    VARCHAR(15000)   n/a    n/a    LONGVARBINARY
    DB2      BLOB             n/a    n/a    LONGVARBINARY
    Ibase    VARCHAR(32000)   passed passed VARCHAR
    Mimer    VARCHAR(15000)   passed passed LONGVARBINARY
    MSSQL    VARBINARY(4000)  n/a    n/a    
    MySQL    BLOB             passed passed LONGVARBINARY
    Oracle   RAW(2000)        passed passed LONGVARBINARY
    PgSQL    ByteA            passed failed 
    SQLite   BLOB             passed n/a    LONGVARBINARY
    SQLite3  BLOB             passed n/a    LONGVARBINARY
    Sybase   VARBINARY(4000)  n/a    n/a    LONGVARBINARY
    Virtuoso VARBINARY(4000)  n/a    n/a    LONGVARBINARY
  */
  
  udm_snprintf(qbuf, sizeof(qbuf),
               "INSERT INTO dict%02X "
               "(url_id,secno,word,intag) "
               "VALUES (%d,%d,'%s',%s)",
               seed, url_id, secno, word,
               UdmSQLParamPlaceHolder(db, 1));
  if (UDM_OK != (rc= UdmSQLPrepare(db, qbuf)))
    return rc;

  if (UDM_OK != (rc= UdmSQLBindParameter(db, 1, coord, coord_len, BindType)))
    return rc;

  if(UDM_OK != (rc= UdmSQLExecute(db)) ||
     UDM_OK != (rc= UdmSQLStmtFree(db)))
    return rc;
  
  return rc;
}


int UdmWordCacheWrite (UDM_AGENT *Indexer, UDM_DB *db, size_t limit)
{
  size_t i;
  size_t in_num, in_limit= UdmVarListFindInt(&Indexer->Conf->Vars, "URLSelectCacheSize", URL_DELETE_CACHE);
  int LastLocked = 0;
  int rc;
  UDM_WORD_CACHE *cache = &db->WordCache;
  UDM_DSTR buf, qbuf;
  UDM_MULTI_CACHE_WORD mintag;
  size_t aintags = 0;
  char arg[128];

  if (cache->nbytes <= limit) return(UDM_OK);
  UdmLog(Indexer, UDM_LOG_ERROR,
         "Writing words (%d words, %d bytes%s).",
         (int) cache->nwords, (int) cache->nbytes, limit ? "" : ", final");

  UDM_THREADINFO(Indexer, "Starting tnx", "");
  if(UDM_OK!=(rc=UdmSQLBegin(db)))
  {
    UdmWordCacheFree(cache);
    return(rc);
  }

  UdmDSTRInit(&buf, 8192);
  UdmDSTRInit(&qbuf, 8192);
  mintag.intags = NULL;
  mintag.word = NULL;
  
  /* Sort IN list */
  if (cache->nurls)
    UdmSort(cache->urls, cache->nurls, sizeof(urlid_t), (udm_qsort_cmp)UdmCmpURLID);

  /* Remove expired words */
  for (i= 0, in_num= 0; i < cache->nurls; i++)
  {
    if (buf.size_data) UdmDSTRAppend(&buf, ",", 1);
    UdmDSTRAppendf(&buf, "'%d'", cache->urls[i]);

    /* Don't allow too long IN list */
    if (++in_num >= in_limit || (i + 1) == cache->nurls)
    {
      int dictno;
      for (dictno= 0; dictno <= MULTI_DICTS; dictno++)
      {
        udm_snprintf(arg, sizeof(arg), "dict%02X", dictno);
        UDM_THREADINFO(Indexer, "Deleting", arg);
        UdmDSTRReset(&qbuf);
        UdmDSTRAppendf(&qbuf, "DELETE FROM dict%02X WHERE url_id IN (%s)", dictno, buf.data);
        if(UDM_OK != (rc= UdmSQLQuery(db,NULL,qbuf.data)))
          goto unlock_UdmStoreWordsMulti;
      }
      UdmDSTRReset(&buf);
      UdmDSTRReset(&qbuf);
      in_num= 0;
    }
  }

  UdmDSTRReset(&buf);
  UdmDSTRReset(&qbuf);

  if (cache->nwords)
    UdmWordCacheSort(cache);

  for (i = 0; i < cache->nwords;)
  {
    UDM_WORD_CACHE_WORD *cword = &cache->words[i];
    unsigned char seed = cword->seed;
    char *word = cword->word;
    urlid_t url_id = cword->url_id;
    unsigned char secno = cword->secno;

    mintag.nintags = 0;

    do
    {
      if (mintag.nintags == aintags)
      {
        uint4 *itmp;
        itmp = UdmRealloc(mintag.intags, (aintags + 256) * sizeof(uint4));
	if (! itmp)
	{
	  goto unlock_UdmStoreWordsMulti;
	}
	mintag.intags = itmp;
	aintags += 256;
      }
      mintag.intags[mintag.nintags]= cword->pos;
      mintag.nintags++;

      i++;
      if (i == cache->nwords) break;
      cword = &cache->words[i];
    }
    while (seed == cword->seed &&
	url_id == cword->url_id &&
        secno == cword->secno &&
	! strcmp(word, cword->word));

    udm_snprintf(arg, sizeof(arg), "dict%02X", seed);
    UDM_THREADINFO(Indexer, "Writing", arg);

    if (db->flags & UDM_SQL_HAVE_BIND_BINARY)
    {
      char *intag= UdmMultiCachePutIntagUsingRaw(&mintag);
      if (!intag)
        continue;

      if (UDM_OK != (rc= UdmWordCacheWriteUsingBind(db, seed, url_id,
                           secno, word, intag, strlen(intag))))
        goto unlock_UdmStoreWordsMulti;
      UdmFree(intag);
    }
    else if (db->DBType == UDM_DB_MYSQL)
    {
      char *intag= UdmMultiCachePutIntagUsingEncoding(&mintag, db, 1);
      if (! intag) continue;

      if (buf.size_data)
      {
        UdmDSTRAppendf(&buf, ",(%d, %d, '%s', %s)",
	               url_id, secno, word, intag);
      } else {
        UdmDSTRAppendf(&buf, "INSERT INTO dict%02X (url_id,secno,word,intag) VALUES(%d,%d,'%s',%s)",
                       seed, url_id, secno, word, intag);
      }
      UdmFree(intag);
      if (seed != cword->seed || i == cache->nwords)
      {
        if (LastLocked <= seed)
	{
          if (LastLocked) UdmSQLQuery(db, NULL, "UNLOCK TABLES");
	  LastLocked = seed;
	  UdmDSTRAppendf(&qbuf, "LOCK TABLES dict%02X WRITE", LastLocked);
          for (LastLocked++; LastLocked <= MULTI_DICTS; LastLocked++)
          {
	    if (LastLocked - seed == 0x10) break;
            UdmDSTRAppendf(&qbuf, ",dict%02X WRITE", LastLocked);
          }
          UdmSQLQuery(db, NULL, qbuf.data);
	  UdmDSTRReset(&qbuf);
	}

        if (buf.size_data)
        {
          if(UDM_OK!=(rc=UdmSQLQuery(db,NULL,buf.data)))
          {
            goto unlock_UdmStoreWordsMulti;
          }
	  UdmDSTRReset(&buf);
        }
      }
    }
    else
    {
      const char *quot, *x, *castb, *caste;
      char *intag;
      
      if (db->DBType == UDM_DB_ORACLE8 ||
          db->DBType == UDM_DB_DB2 ||
          db->DBType == UDM_DB_MONETDB)
        intag= UdmMultiCachePutIntagUsingEncoding(&mintag, db, 0);
      else if (db->DBType == UDM_DB_MSSQL ||
               db->DBType == UDM_DB_SYBASE ||
               db->DBType == UDM_DB_ACCESS)
        intag= UdmMultiCachePutIntagUsingEncoding(&mintag, db, 1);
      else if (db->DBType == UDM_DB_PGSQL)
        intag= UdmMultiCachePutIntagUsingEncoding(&mintag, db, 2);
      else
      {
        if ((intag= UdmMultiCachePutIntagUsingRaw(&mintag)))
        {
          char *tmp= UdmSQLEscStrAlloc(db, intag, strlen(intag)); /* Data */
          UdmFree(intag);
          intag= tmp;
        }
      }
      if (! intag) continue;

      if (db->DBType == UDM_DB_MSSQL ||
          db->DBType == UDM_DB_SYBASE ||
          db->DBType == UDM_DB_ACCESS)
        quot="";
      else
        quot="'";

      if (db->DBType == UDM_DB_DB2)
      {
        x="X";
        castb="CAST(";
        caste=" AS BLOB)";
      }
      else
      {
        x="";
        castb="";
        caste="";
      }

      UdmDSTRAppendf(&buf,
        "INSERT INTO dict%02X (url_id,secno,word,intag) VALUES(%d,%d,'%s',%s%s%s%s%s%s)",
        seed, url_id, secno, word, castb,x,quot,intag,quot,caste);
      UdmFree(intag);
      if(UDM_OK!=(rc=UdmSQLQuery(db,NULL,buf.data))) goto unlock_UdmStoreWordsMulti;
      UdmDSTRReset(&buf);
    }
  }

unlock_UdmStoreWordsMulti:
  UDM_FREE(mintag.intags);
  UdmDSTRFree(&buf);
  UdmDSTRFree(&qbuf);
  UDM_THREADINFO(Indexer, "Committing tnx", "");

  if (LastLocked && rc == UDM_OK)
    rc = UdmSQLQuery(db, NULL, "UNLOCK TABLES");

  if(rc==UDM_OK)
    rc=UdmSQLCommit(db);

  UdmWordCacheFree(&db->WordCache);
  UdmLog(Indexer, UDM_LOG_ERROR, "The words are written successfully.%s", limit ? "" : " (final)");
  return(rc);
}


static int
UdmStoreWordsMulti(UDM_AGENT *Indexer, UDM_DB *db, UDM_DOCUMENT *Doc)
{
  int rc= UDM_OK;
  urlid_t  url_id= UdmVarListFindInt(&Doc->Sections, "ID", 0);
  unsigned char PrevStatus= UdmVarListFindInt(&Doc->Sections, "PrevStatus", 0) ? 1 : 0;

  /*
    Don't need to delete words here.
    DBMode=multi deletes words from multiple
    documents in a portion, when doing UdmWordCacheWrite()
  */

  if (UdmVarListFindInt(&Indexer->Conf->Vars, "SaveSectionSize", 1))
  {
    if (UDM_OK!= (rc= UdmWordListSaveSectionSize(Doc)))
      return rc;
  }
  
  if (PrevStatus) UdmWordCacheAddURL(&db->WordCache, url_id);

  rc= UdmWordCacheAddWordList(&db->WordCache, &Doc->Words, url_id);  
  return(rc);
}


static int
UdmMultiAddCoords(UDM_FINDWORD_ARGS *args, UDM_SQLRES *SQLRes)
{
  size_t i;
  size_t numrows;
  UDM_URLCRDLIST CoordList;
  UDM_URL_CRD CoordTemplate;

  bzero((void*)&CoordList, sizeof(CoordList));
  bzero((void*)&CoordTemplate, sizeof(CoordTemplate));
  CoordTemplate.num= args->Word.order;
  
  numrows= UdmSQLNumRows(SQLRes);

  for (i= 0; i < numrows; i++)
  {
    size_t tmp = UdmSQLLen(SQLRes, i, 2);
    if (tmp) CoordList.acoords += tmp;
    else CoordList.acoords += strlen(UdmSQLValue(SQLRes, i, 2));
  }
  CoordList.Coords= (UDM_URL_CRD*)UdmMalloc((CoordList.acoords) * sizeof(UDM_URL_CRD));

  for (i = 0; i < numrows; i++)
  {
    size_t lintag= UdmSQLLen(SQLRes, i, 2);
    const unsigned char *intag= (const unsigned char *)UdmSQLValue(SQLRes, i, 2);
    
    CoordTemplate.url_id= UDM_ATOI(UdmSQLValue(SQLRes, i, 0));
    CoordTemplate.secno= UDM_ATOI(UdmSQLValue(SQLRes, i, 1));
    
    if (!lintag)
      lintag= strlen((const char *)intag); /* FIXME: Check UdmSQLLen */
    
    if (args->wf[CoordTemplate.secno])
    {
      UdmCoordListMultiUnpack(&CoordList, &CoordTemplate, 
                              intag, lintag,
                              args->save_section_size);
    }
  }
  
  if (args->urls.nurls)
    UdmApplyFastLimit(&CoordList, &args->urls);
  if (CoordList.ncoords)
  {
    /*
      We have to sort here, because DBMode=multi
      returns data unsorted.
    */
    UdmURLCRDListSortByURLThenSecnoThenPos(&CoordList);
    UdmURLCRDListListAddWithSort2(args, &CoordList);
  }
  else
  {
    UdmFree(CoordList.Coords);
  }
  args->Word.count= CoordList.ncoords;
  return UDM_OK;
}


static int
UdmFindWordMulti(UDM_FINDWORD_ARGS *args)
{
  char qbuf[4096], secno[64]= "";
  UDM_SQLRES SQLRes;
  int tnum, tmin, tmax;
  udm_timer_t ticks;
  int rc;

  if (args->Word.match != UDM_MATCH_FULL)
  {
    /* This is for substring search!  */
    /* In Multi mode: we have to scan */
    /* almost all tables except those */
    /* with to short words            */
      
    tmin= 0;
    tmax= MULTI_DICTS;
  }
  else
  {
    tmin= tmax= UdmStrHash32(args->Word.word) & MULTI_DICTS;
  }

  if (args->Word.secno)
    udm_snprintf(secno, sizeof(secno),
                 " AND dict.secno=%d", (int) args->Word.secno);

  for(tnum= tmin; tnum <= tmax; tnum++)
  {
    if (*args->where)
    {
      udm_snprintf(qbuf, sizeof(qbuf) - 1,"\
SELECT dict.url_id,dict.secno,dict.intag \
FROM dict%02X dict, url%s \
WHERE dict.%s \
AND url.rec_id=dict.url_id AND %s%s",
        tnum, args->db->from, args->cmparg, args->where, secno);
    }
    else
    {
      udm_snprintf(qbuf, sizeof(qbuf) - 1,
                   "SELECT url_id,secno,intag FROM dict%02X dict WHERE %s%s",
                   tnum, args->cmparg, secno);
    }

    if (UDM_OK != (rc= UdmSQLQuery(args->db, &SQLRes, qbuf)))
      return rc;

    UdmLog(args->Agent, UDM_LOG_DEBUG, "Start UdmMultiAddCoords");
    ticks= UdmStartTimer();
    UdmMultiAddCoords(args, &SQLRes);
    UdmLog(args->Agent, UDM_LOG_DEBUG, "Stop UdmMultiAddCoords\t%.2f", UdmStopTimer(&ticks));
    UdmSQLFree(&SQLRes);
  }
  return UDM_OK;
}


static int
UdmDeleteWordsFromURLMulti(UDM_AGENT *Indexer, UDM_DB *db, urlid_t url_id)
{
  int i;
  char qbuf[512];
  for(i= 0; i <= MULTI_DICTS; i++)
  {
    int rc;
    udm_snprintf(qbuf, sizeof(qbuf),
                 "DELETE FROM dict%02X WHERE url_id=%d", i, url_id);
    if (UDM_OK != (rc= UdmSQLQuery(db, NULL, qbuf)))
      return rc;
  }
  return UDM_OK;
}


static int
UdmTruncateDictMulti(UDM_AGENT *Indexer,UDM_DB *db)
{
  int rc= UDM_OK;
  int i;
  for(i= 0 ; i <= MULTI_DICTS; i++)
  {
    char tablename[32];
    sprintf(tablename, "dict%02X", i);
    if(UDM_OK != (rc= UdmSQLTableTruncateOrDelete(db, tablename)))
      return rc;
  }
  return UDM_OK;
}


static int
UdmMulti2BlobSQL(UDM_AGENT *Indexer, UDM_DB *db, UDM_URLDATALIST *URLList)
{
  size_t i, use_deflate= 0;
  int t, rc;
  UDM_SQLRES SQLRes;
  char buf[128], wtable[64];
  size_t srows = 0;
  UDM_BLOB_CACHE cache[BLOB_CACHE_SIZE + 1];
  urlid_t url_id;
  unsigned char secno;
  char *intag;
  const char *word;
  size_t nintags;
  udmhash32_t word_seed;
  UDM_PSTR row[4];

#ifdef HAVE_ZLIB
  if (UdmVarListFindBool(&db->Vars, "deflate", UDM_DEFAULT_DEFLATE))
  {
     UdmLog(Indexer, UDM_LOG_DEBUG, "Using deflate");
     use_deflate= 1;
  }
  else
  {
     UdmLog(Indexer, UDM_LOG_DEBUG, "Not using deflate");
  }
#endif
  
  
  if (UDM_OK != (rc= UdmBlobGetWTable(Indexer, db, wtable, sizeof(wtable))))
    return rc;

  /* Delete old words from bdict */
  rc = UdmSQLTableTruncateOrDelete(db, wtable);
  if (rc != UDM_OK)
  {
    return(rc);
  }

  for (i = 0; i <= BLOB_CACHE_SIZE; i++)
  {
    UdmBlobCacheInit(&cache[i]);
  }

  for (t = 0; t <= MULTI_DICTS; t++)
  {
    if (db->DBType == UDM_DB_MYSQL)
    {
      udm_snprintf(buf, sizeof(buf), "LOCK TABLES dict%02X WRITE, %s WRITE", t, wtable);
      rc = UdmSQLQuery(db, NULL, buf);
      if (rc != UDM_OK)
      {
        return(rc);
      }
    }

    UdmLog(Indexer, UDM_LOG_DEBUG, "Loading dict%02X", t);
    udm_snprintf(buf, sizeof(buf), "SELECT url_id, secno, word, intag FROM dict%02X", t);
    rc= UdmSQLExecDirect(db, &SQLRes, buf);
    if (rc != UDM_OK)
    {
      return(rc);
    }

    UdmLog(Indexer, UDM_LOG_ERROR, "Converting dict%02X", t);
    while (db->sql->SQLFetchRow(db, &SQLRes, row) == UDM_OK)
    {
      url_id = UDM_ATOI(row[0].val);
      if (!UdmURLDataListSearch(URLList, url_id))
        continue;
      secno = (unsigned char)UDM_ATOI(row[1].val);
      word = row[2].val;
      intag = row[3].val;
      nintags = udm_coord_len(intag);
      word_seed = UdmStrHash32(word ? word : "") >> 8 & MULTI_DICTS;
      UdmBlobCacheAdd(&cache[word_seed],
                      url_id, secno, word, nintags, intag, row[3].len);
    }
    UdmLog(Indexer, UDM_LOG_DEBUG, "Writing dict%02X", t);
    for (i = 0; i <= BLOB_CACHE_SIZE; i++)
    {
      srows += cache[i].nwords;
      rc= UdmBlobCacheWrite(Indexer, db, &cache[i], wtable, use_deflate);
      UdmBlobCacheFree(&cache[i]);
      if (rc != UDM_OK)
        return rc;
    }
    UdmSQLFree(&SQLRes);

    if (db->DBType == UDM_DB_MYSQL)
    {
      rc = UdmSQLQuery(db, NULL, "UNLOCK TABLES");
      if (rc != UDM_OK)
      {
        return(rc);
      }
    }
  }

  UdmLog(Indexer, UDM_LOG_ERROR, "Total records converted: %d", (int) srows);
  if (UDM_OK != (rc= UdmBlobWriteTimestamp(Indexer, db, wtable)))
    return rc;

  UdmLog(Indexer, UDM_LOG_ERROR, "Converting url.");
  if (UDM_OK != (rc= UdmBlobWriteURL(Indexer, db, wtable, use_deflate)))
    return rc;

  UdmLog(Indexer, UDM_LOG_ERROR, "Switching to new blob table.");
  rc= UdmBlobSetTable(Indexer, db);
  return rc;
}


static int
UdmWordStatCreateMulti(UDM_AGENT *A, UDM_DB *db)
{
  int rc, i;
  
  for (i=0; i <= 0xFF; i++)
  {
    char qbuf[128];
    UdmLog(A, UDM_LOG_EXTRA, "Processing table %02X", i);

    sprintf(qbuf, "SELECT word, count(*) FROM dict%02X GROUP BY word", i);
    if (UDM_OK != (rc= UdmWordStatQuery(A, db, qbuf)))
      return rc;
  }
  return UDM_OK;
}



static int
UdmDumpWordInfoOneDocMulti(UDM_AGENT *A, UDM_DB *db, UDM_DOCUMENT *Doc)
{
  return UDM_OK;
}


UDM_DBMODE_HANDLER udm_dbmode_handler_multi=
{
  "multi",
  UdmMulti2BlobSQL,
  UdmStoreWordsMulti,
  UdmTruncateDictMulti,
  UdmDeleteWordsFromURLMulti,
  UdmFindWordMulti,
  UdmDumpWordInfoOneDocMulti,
  UdmWordStatCreateMulti,
};

#endif /* HAVE_SQL */
