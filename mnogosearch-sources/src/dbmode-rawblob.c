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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_db.h"
#include "udm_log.h"
#include "udm_hash.h"
#include "udm_searchtool.h"
#include "udm_word.h"
#include "udm_db_int.h"
#include "udm_sqldbms.h"


/*
  UDM_RAWBLOB_SEARCH mode:
  Search in raw blob mode for small databases:
  direct search from "bdicti" without having to
  run "indexer -Eblob":
  load the records with state=1 (new) or state=2 (converted).

  UDM_RAWBLOB_DELTA mode (for LiveUpdates):
  - Load the records with state=1 (new).
  - The records with state=0 (deleted) will be loaded
  as an excluding limit for "bdict".
*/
static int
UdmFindWordRawBlobInternal(UDM_FINDWORD_ARGS *args, int flags)
{
  char qbuf[4096];
  udm_timer_t ticks;
  UDM_SQLRES SQLRes;
  UDM_URLCRDLIST CoordList;
  int intagnum= UdmStrHash32(args->Word.word) & 0x1f;
  size_t rownum, nrows;
  int rc;
  const char *state_cmp= (flags & UDM_RAWBLOB_DELTA) ? "=" : ">=";
  UDM_URLID_LIST *urls= (flags & UDM_RAWBLOB_DELTA) ?
                      &args->live_update_active_urls :
                      &args->urls;
  UDM_URL_CRD CoordTemplate;
  bzero(&CoordTemplate, sizeof(CoordTemplate));
  
  if (urls->empty)
  {
    UdmLog(args->Agent, UDM_LOG_DEBUG, "Not searching 'bdicti': Live URL limit is empty");
    return UDM_OK;
  }
  
  
  ticks= UdmStartTimer();
  UdmLog(args->Agent, UDM_LOG_DEBUG, "Start fetching from bdicti");
  
  if (*args->where)
  {
    udm_snprintf(qbuf, sizeof(qbuf),"\
SELECT d.url_id,d.intag%02X FROM bdicti d,url%s \
WHERE d.state%s1 AND url.rec_id=d.url_id AND %s",
                 intagnum, args->db->from, state_cmp, args->where);
  }
  else
  {
    udm_snprintf(qbuf, sizeof(qbuf), 
                 "SELECT url_id, intag%02X FROM bdicti WHERE state%s1",
                 intagnum, state_cmp);
  }

  if(UDM_OK != (rc= UdmSQLQuery(args->db, &SQLRes, qbuf)))
    return rc;
  nrows= UdmSQLNumRows(&SQLRes);
  
  bzero((void*) &CoordList, sizeof(CoordList));
  for (rownum= 0; rownum < nrows; rownum++)
  {
    CoordList.acoords+= UdmSQLLen(&SQLRes, rownum, 1);
  }
  CoordList.Coords= (UDM_URL_CRD*)UdmMalloc((CoordList.acoords) * sizeof(UDM_URL_CRD));
  
  for (rownum= 0; rownum < nrows; rownum ++)
  {
    UDM_PSTR row[2];
    size_t pos= 0;
    row[0].val= (char *) UdmSQLValue(&SQLRes, rownum, 0);
    row[0].len= UdmSQLLen(&SQLRes, rownum, 0);
    row[1].val= (char*) UdmSQLValue(&SQLRes, rownum, 1);
    row[1].len= UdmSQLLen(&SQLRes, rownum, 1);
    CoordTemplate.url_id= UDM_ATOI(row[0].val);
    if (urls->nurls)
    {
      void *found= UdmBSearch(&CoordTemplate.url_id, urls->urls, urls->nurls,
                              sizeof(urlid_t), (udm_qsort_cmp)UdmCmpURLID);
      if (found && urls->exclude)
        continue;
      if (!found && !urls->exclude)
        continue;
    }

    /*
      TODO: substring search, numeric search
    */
    
    while (pos < row[1].len)
    {
      char *word= &row[1].val[pos];
      while (pos < row[1].len && row[1].val[pos])
        pos++;
      if (++pos >= row[1].len)
        break;
      while (pos < row[1].len)
      {
        char *intag= &row[1].val[++pos];
        size_t lintag;
        while (pos < row[1].len && row[1].val[pos])
          pos++;
        lintag= row[1].val + pos - intag;
        CoordTemplate.secno= (unsigned char)row[1].val[pos];
        if ((!args->Word.secno || args->Word.secno == CoordTemplate.secno) &&
            !strcmp(args->Word.word, word) &&
            args->wf[CoordTemplate.secno])
        {
          CoordTemplate.num= args->Word.order;
          UdmCoordListMultiUnpack(&CoordList, &CoordTemplate,
                                  (unsigned char*) intag, lintag,
                                   args->save_section_size);
        }
        if (++pos >= row[1].len || ! row[1].val[pos])
        {
          pos++;
          break;
        }
      }
    }
  }
  UdmSQLFree(&SQLRes);
  if (CoordList.ncoords)
  {
    args->Word.count+= CoordList.ncoords;
    UdmURLCRDListSortByURLThenSecnoThenPos(&CoordList);
    UdmURLCRDListListAddWithSort2(args, &CoordList);
  }
  else
  {
    UdmFree(CoordList.Coords);
  }
  UdmLog(args->Agent, UDM_LOG_DEBUG,
         "Stop fetching from bdicti\t%.2f %d coords found",
         UdmStopTimer(&ticks), (int) CoordList.ncoords);
  return UDM_OK;
}


int
UdmFindWordRawBlobDelta(UDM_FINDWORD_ARGS *args)
{
  return UdmFindWordRawBlobInternal(args, UDM_RAWBLOB_DELTA);
}


static int
UdmFindWordRawBlobSearch(UDM_FINDWORD_ARGS *args)
{
  return UdmFindWordRawBlobInternal(args, UDM_RAWBLOB_SEARCH);
}


UDM_DBMODE_HANDLER udm_dbmode_handler_rawblob=
{
  "rawblob",
  NULL,                      /* ToBlob            */
  NULL,                      /* StoreWords        */
  NULL,                      /* Truncate          */
  NULL,                      /* DeleteWordFromURL */
  UdmFindWordRawBlobSearch,  /* FindWord          */
  NULL,                      /* DumpWordInfo      */
  NULL,                      /* WordStatCreate    */
};

#endif /* HAVE_SQL */
