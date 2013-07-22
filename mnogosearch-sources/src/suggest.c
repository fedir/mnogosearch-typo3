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
#include <math.h>

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

/******* "indexer -Ewordstat" - word statistics for suggestions *************/

#ifdef HAVE_SQL

int
UdmWordStatQuery(UDM_AGENT *A, UDM_DB *db, const char *src)
{
  int rc;
  UDM_SQLRES SQLRes;
  size_t row, rows;
  
  if(UDM_OK!= (rc= UdmSQLQuery(db, &SQLRes, src)))
    return rc;
  
  rows=UdmSQLNumRows(&SQLRes);
  for(row=0 ; row < rows ; row++)
  {
    const char *word;
    int count;
    size_t wordlen;
    char snd[UDM_MAXWORDSIZE];
    char insert[64 + 2 * UDM_MAXWORDSIZE];
    /*
      Skip words that are longer than UDM_MAXWORDSIZE.
    */
    if ((wordlen= UdmSQLLen(&SQLRes, row, 0)) > sizeof(snd))
      continue;
    word= UdmSQLValue(&SQLRes, row, 0);
    count= UDM_ATOI(UdmSQLValue(&SQLRes, row, 1));
    UdmSoundex(A->Conf->lcs, snd, sizeof(snd), word, wordlen);
    if (snd[0])
    {
      udm_snprintf(insert, sizeof(insert),
                   "INSERT INTO wrdstat (word, snd, cnt) VALUES ('%s','%s',%d)",
                   word, snd, count);
      if (UDM_OK!= (rc= UdmSQLQuery(db, NULL, insert)))
        return rc;
    }
  }
  UdmSQLFree(&SQLRes);
  return UDM_OK;
}


int
UdmWordStatCreate(UDM_AGENT *A, UDM_DOCUMENT *D, UDM_DB *db)
{
  int rc;

  UdmLog(A, UDM_LOG_ERROR, "Calculating word statistics");

  if (UDM_OK != (rc= UdmSQLTableTruncateOrDelete(db, "wrdstat")))
    return rc;
  
  UDM_ASSERT(db->dbmode_handler->WordStatCreate != NULL);
  rc= db->dbmode_handler->WordStatCreate(A, db);
  UdmLog(A, UDM_LOG_ERROR, "Word statistics done");
  return rc;
}


/********************  Suggest ************************************/

static float
UdmUCharArrayCorrelation(unsigned char *a, unsigned char *b, size_t size)
{
  float Asum= 0;
  float Bsum= 0;
  float ABsum= 0;
  size_t i;

  for (i= 0; i < size; i++)
  {
    Asum+=  (float) a[i] * (float) a[i];
    Bsum+=  (float) b[i] * (float) b[i];
    ABsum+= (float) a[i] * (float) b[i];
  }
  return ABsum / sqrt(Asum) / sqrt(Bsum);
}


static void
UdmUCharArrayFillByteStatistics(unsigned char *a, size_t size,
                                const unsigned char *src, size_t srclen)
{
  bzero((void*) a, size);
  for ( ; srclen ; src++, srclen--)
  {
    a[((unsigned char) *src) % size]++;
  }
}


static int
WordProximity(UDM_CHARSET *cs,
              const char *s1, size_t len1,
              const char *s2, size_t len2, int scale)
{
  size_t max= len1 > len2 ? len1 : len2;
  size_t min= len1 < len2 ? len1 : len2;
  if ((max - min)*3 > max)  /* Not more than 1/3 of length difference */
    return 0;
  {
    unsigned char a[256];
    unsigned char b[256];
    UdmUCharArrayFillByteStatistics(a, sizeof(a), (unsigned const char *) s1, len1);
    UdmUCharArrayFillByteStatistics(b, sizeof(b), (unsigned const char *) s2, len2);
    return (size_t) (UdmUCharArrayCorrelation(a, b, sizeof(a)) * scale);
  }
  return (1000 * min) / (max ? max : 1);
}


int
UdmResSuggest(UDM_AGENT *A, UDM_DB *db, UDM_RESULT *Res, size_t dbnum)
{
  int rc= UDM_OK;
  size_t i, nwords;
  UDM_WIDEWORDLIST *WWList= &Res->WWList;
  UDM_CONV lcs_uni;
  
  UdmLog(A, UDM_LOG_DEBUG, "Generating suggestion list");
  UdmConvInit(&lcs_uni, A->Conf->lcs, &udm_charset_sys_int, UDM_RECODE_HTML);

  for (i=0, nwords= WWList->nwords; i < nwords; i++)
  {
    UDM_WIDEWORD *W= &WWList->Word[i];
    if (W->origin == UDM_WORD_ORIGIN_QUERY && !W->count)
    {
      char snd[32];
      char qbuf[128];
      UDM_SQL_TOP_CLAUSE Top;
      UDM_SQLRES SQLRes;
      UDM_WIDEWORD sg;
      size_t row, rows;
      size_t Wlen= W->len; /* W is not valid after UdmWideWordListAdd() */
      size_t Word= W->order;
      size_t phrpos= W->phrpos;
      size_t count_sum;
      const char *Wword= W->word;
      
      UdmSQLTopClause(db, 100, &Top);
      
      UdmSoundex(A->Conf->lcs, snd, sizeof(snd), W->word, W->len);
      UdmLog(A, UDM_LOG_DEBUG, "Suggest for '%s': '%s'", W->word, snd);
      udm_snprintf(qbuf, sizeof(qbuf),
                   "SELECT %sword, cnt FROM wrdstat WHERE snd='%s'%s"
                   " ORDER by cnt DESC%s",
                    Top.top, snd, Top.rownum, Top.limit);
      
      if(UDM_OK!= (rc= UdmSQLQuery(db, &SQLRes, qbuf)))
        return rc;
      
      rows=UdmSQLNumRows(&SQLRes);
      UdmLog(A, UDM_LOG_DEBUG, "%d suggestions found", (int) rows);
      if (rows > 0)
      {
        UdmLog(A, UDM_LOG_DEBUG, "<%s>: <%s>/<%s>/<%s>/<%s>", 
               "word", "count", "count_weight", "proximity_weight",
               "final_weight");
      }
      bzero((void*)&sg, sizeof(sg));
      
      for (count_sum= 0, row= 0; row < rows; row++)
      {
        count_sum+= UDM_ATOI(UdmSQLValue(&SQLRes, row, 1));
      }
      if (!count_sum)
        count_sum= 1;

      for(row=0 ; row < rows ; row++)
      {
        size_t nbytes, proximity_weight, count_weight, final_weight;
        int tmp[128];
        int word_count_factor= 63;
        int word_proximity_factor= 256 - word_count_factor;
        
        sg.word= (char*) UdmSQLValue(&SQLRes, row, 0);
        sg.count= UDM_ATOI(UdmSQLValue(&SQLRes, row, 1));
        sg.len= UdmSQLLen(&SQLRes, row, 0);

        count_weight= (word_count_factor * sg.count) / count_sum;
        proximity_weight= WordProximity(A->Conf->lcs,
                                        Wword, Wlen,
                                        sg.word, sg.len,
                                        word_proximity_factor);
        final_weight= (count_weight + proximity_weight);
        UdmLog(A, UDM_LOG_DEBUG, "%s: %d/%d/%d/%d", 
               sg.word, (int) sg.count, (int) count_weight,
               (int) proximity_weight, (int) final_weight);
        sg.count= final_weight;

        nbytes= (sg.len + 1) * sizeof(int);
        if (nbytes < sizeof(tmp))
        {
          sg.order= Word;
          sg.phrpos= phrpos;
          sg.origin= UDM_WORD_ORIGIN_SUGGEST;
          UdmWideWordListAdd(WWList, &sg); /* W is not valid after this */
        }
      }
      UdmSQLFree(&SQLRes);
    }
  }
  return rc;
}

#endif
