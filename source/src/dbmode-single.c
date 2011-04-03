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
#include "udm_url.h"
#include "udm_vars.h"
#include "udm_searchtool.h"
#include "udm_log.h"
#include "udm_hash.h"
#include "udm_doc.h"
#include "udm_word.h"
#include "udm_indexer.h"


#ifdef HAVE_SQL


#define OLD_SINGLE_FORMAT 0

#if OLD_SINGLE_FORMAT
#define UDM_WRDCOORD(p,w)  ((((unsigned int)(p))<<16)+(((unsigned int)(w))<<8))
#define UDM_WRDSEC(c)      ((((unsigned int)(c))>>8)&0xFF)
#define UDM_WRDPOS(c)      (((unsigned int)(c))>>16)
#else
#define UDM_WRDCOORD(p,s)  ((((unsigned int)(s))<<24)+(unsigned int)(p))
#define UDM_WRDSEC(c)      (((unsigned int)(c))>>24)
#define UDM_WRDPOS(c)      (((unsigned int)(c))&0x001FFFFF)
#endif


int
UdmAddOneCoord(UDM_URLCRDLIST *CoordList,
               urlid_t url_id, uint4 coord, udm_wordnum_t num)
{
  UDM_URL_CRD *C;
  if (CoordList->ncoords == CoordList->acoords)
  {
    UDM_URL_CRD *tmp;
    size_t newsize= CoordList->acoords ? CoordList->acoords * 2 : 1024;
    tmp= UdmRealloc(CoordList->Coords, newsize * sizeof(UDM_URL_CRD));
    if (! tmp)
    {
      return(UDM_ERROR);
    }
    CoordList->Coords= tmp;
    CoordList->acoords= newsize;
  }
  C= &CoordList->Coords[CoordList->ncoords];
  C->url_id= url_id;
  C->pos= UDM_WRDPOS(coord);
  C->num= num;
  C->secno= UDM_WRDSEC(coord);
  C->seclen= 0;
  CoordList->ncoords++;
  return UDM_OK;
}


static int
UdmDeleteWordsFromURLSingle(UDM_AGENT *Indexer, UDM_DB *db, urlid_t url_id)
{
  char qbuf[512];
  udm_snprintf(qbuf, sizeof(qbuf), "DELETE FROM dict WHERE url_id=%d", url_id);
  return UdmSQLQuery(db, NULL, qbuf);
}


static int
StoreWordsSingle(UDM_AGENT *Indexer, UDM_DB *db, UDM_DOCUMENT *Doc)
{
  size_t  i;
  char  qbuf[256]="";
  time_t  stmp;
  int  rc=UDM_OK;
  urlid_t  url_id = UdmVarListFindInt(&Doc->Sections, "ID", 0);
  const char      *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";

  if (UDM_OK != (rc= UdmDeleteWordsFromURLSingle(Indexer, db, url_id)))
    return rc;
  
  if (UdmVarListFindInt(&Indexer->Conf->Vars, "SaveSectionSize", 1))
  {
    if (UDM_OK!= (rc= UdmWordListSaveSectionSize(Doc)))
      return rc;
  }
  
  stmp=time(NULL);
  
  /*
    Don't need to delete words here,
    they are deleted in UdmStoreWords().
  */

 
  /* Insert new words */
  if(db->DBType==UDM_DB_MYSQL)
  {
    if(Doc->Words.nwords)
    {
      size_t nstored=0;
      
      while(nstored < Doc->Words.nwords)
      {
        char * qb,*qe;
        size_t step=1024;
        size_t mlen=1024;
        size_t rstored = 0;

        qb=(char*)UdmMalloc(mlen);
        strcpy(qb,"INSERT INTO dict (word,url_id,intag) VALUES ");
        qe=qb+strlen(qb);

        for(i=nstored;i<Doc->Words.nwords;i++)
        {
          size_t len=qe-qb;
          if(!Doc->Words.Word[i].secno)
          { 
            nstored++; 
            continue;
          }
          rstored++;
        
          /* UDM_MAXWORDSIZE+100 should be enough */
          if((len + Indexer->Conf->WordParam.max_word_len + 100) >= mlen)
          {
            mlen+=step;
            qb=(char*)UdmRealloc(qb,mlen);
            qe=qb+len;
          }
          
          if(i>nstored)*qe++=',';

          if(db->DBMode==UDM_DBMODE_SINGLE)
          {
            *qe++='(';
            *qe++='\'';
            strcpy(qe,Doc->Words.Word[i].word);
            while(*qe)qe++;
            *qe++='\'';
            *qe++=',';
            qe+=sprintf(qe,"%d,%d",url_id,
                        UDM_WRDCOORD(Doc->Words.Word[i].pos,
                                     Doc->Words.Word[i].secno) /*+ 
                                     Doc->Words.Word[i].seclen_marker*/);
            *qe++=')';
            *qe='\0';
          }
          if(qe>qb+UDM_MAX_MULTI_INSERT_QSIZE)
            break;
        }
        nstored = i + 1;
        rc = (rstored > 0) ? UdmSQLQuery(db, NULL, qb) : UDM_OK;
        UDM_FREE(qb);
        if(rc!=UDM_OK) goto unlock_StoreWordsSingle;
      }
    }
  }else{
    for(i=0;i<Doc->Words.nwords;i++)
    {
      if(!Doc->Words.Word[i].secno)continue;
        
      if(db->DBMode==UDM_DBMODE_SINGLE)
      {
        sprintf(qbuf,"INSERT INTO dict (url_id,word,intag) VALUES(%s%i%s,'%s',%d)", qu, url_id, qu, 
          Doc->Words.Word[i].word,
          UDM_WRDCOORD(Doc->Words.Word[i].pos, Doc->Words.Word[i].secno) /*+
          Doc->Words.Word[i].seclen_marker*/);
      }
      if(UDM_OK!=(rc=UdmSQLQuery(db,NULL,qbuf)))
        goto unlock_StoreWordsSingle;
    }
  }
unlock_StoreWordsSingle:
  return rc;
}


int
UdmStoreCrossWords(UDM_AGENT * Indexer,UDM_DOCUMENT *Doc,UDM_DB *db)
{
  UDM_DOCUMENT  U;
  size_t    i;
  char    qbuf[1024];
  const char  *lasturl="scrap";
  const char      *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";
  urlid_t    referrer = UdmVarListFindInt(&Doc->Sections, "ID", 0);
  urlid_t    childid = 0;
  int    rc=UDM_OK;
  UDM_HREF        Href;
  UDM_URL         docURL;
  
  UdmDocInit(&U);
  bzero((void*)&Href, sizeof(Href));
  UdmVarListReplaceInt(&Doc->Sections, "Referrer-ID", referrer);
  if(UDM_OK!=(rc=UdmDeleteCrossWordFromURL(Indexer,&U,db)))
  {
    UdmDocFree(&U);
    return rc;
  }
  
  if(Doc->CrossWords.ncrosswords==0)
  {
    UdmDocFree(&U);
    return rc;
  }
  
  UdmURLInit(&docURL);
  UdmURLParse(&docURL, UdmVarListFindStr(&Doc->Sections, "URL", ""));
  for(i=0;i<Doc->CrossWords.ncrosswords;i++)
  {
    if(!Doc->CrossWords.CrossWord[i].weight)continue;
    if(strcmp(lasturl,Doc->CrossWords.CrossWord[i].url))
    {
      Href.url = (char*)UdmStrdup(Doc->CrossWords.CrossWord[i].url);
      UdmConvertHref(Indexer, &docURL, &Doc->Spider, &Href);
      UdmVarListReplaceStr(&U.Sections, "URL", Href.url);
      UdmVarListReplaceInt(&U.Sections, "URL_ID", UdmStrHash32(Href.url));
      if(UDM_OK != (rc= UdmFindURL(Indexer, &U, db)))
      {
        UdmDocFree(&U);
        UdmURLFree(&docURL);
        return rc;
      }
      childid = UdmVarListFindInt(&U.Sections,"ID",0);
      lasturl=Doc->CrossWords.CrossWord[i].url;
      UDM_FREE(Href.url);
    }
    Doc->CrossWords.CrossWord[i].referree_id=childid;
  }
  
  /* Begin transacttion/lock */
  if (db->DBDriver == UDM_DB_MYSQL)
  {
    sprintf(qbuf,"LOCK TABLES crossdict WRITE");
    if (UDM_OK != (rc= UdmSQLQuery(db,NULL,qbuf)))
      goto free_ex;
  }
  else
  {
    /* Don't start transaction here - 
       this function is called inside a transaction
    */
  }
  
  /* Insert new words */
  for(i=0;i<Doc->CrossWords.ncrosswords;i++)
  {
    if(Doc->CrossWords.CrossWord[i].weight && Doc->CrossWords.CrossWord[i].referree_id)
    {
      int weight=UDM_WRDCOORD(Doc->CrossWords.CrossWord[i].pos,Doc->CrossWords.CrossWord[i].weight);
      sprintf(qbuf,"INSERT INTO crossdict (ref_id,url_id,word,intag) VALUES(%s%i%s,%s%i%s,'%s',%d)",
        qu, referrer, qu, qu, Doc->CrossWords.CrossWord[i].referree_id, qu,
        Doc->CrossWords.CrossWord[i].word, weight);
      if(UDM_OK!=(rc=UdmSQLQuery(db,NULL,qbuf)))
      {
        UdmDocFree(&U);
        goto unlock_UdmStoreCrossWords;
      }
    }
  }

unlock_UdmStoreCrossWords:

  if (db->DBDriver == UDM_DB_MYSQL)
  {
    sprintf(qbuf,"UNLOCK TABLES");
    rc=UdmSQLQuery(db,NULL,qbuf);
  }
  else
  {
    /*
      Don't do commit here - 
      this function is called inside a transaction
    */
  }

free_ex:
  UdmDocFree(&U);
  UdmURLFree(&docURL);
  return rc;
}


static int
UdmFindWordSingleInternal(UDM_FINDWORD_ARGS *args,
                          UDM_URLCRDLIST *CoordList,
                          const char *table, int join)
{
  char qbuf[4096];
  UDM_SQLRES SQLRes;
  size_t numrows, i, curnum;
  int rc;

  if (*args->where)
  {
    udm_snprintf(qbuf, sizeof(qbuf) - 1,"\
SELECT %s.url_id,%s.intag FROM %s, url%s \
WHERE %s.word%s AND url.rec_id=%s.url_id AND %s",
    table, table, table, args->db->from,
    table, args->cmparg, table, args->where);
  }
  else if (!join)
  {
    udm_snprintf(qbuf, sizeof(qbuf) - 1,
      "SELECT url_id,intag FROM %s WHERE word%s", table, args->cmparg);
  }
  else
  {

    udm_snprintf(qbuf,sizeof(qbuf)-1,"\
SELECT url_id,intag FROM %s,url WHERE %s.word%s AND url.rec_id=%s.url_id",
       table, table, args->cmparg, table);
  }
  
  if(UDM_OK != (rc= UdmSQLQuery(args->db, &SQLRes, qbuf)))
    return rc;
  
  if ((numrows= UdmSQLNumRows(&SQLRes)))
  {
    CoordList->Coords= (UDM_URL_CRD*) UdmMalloc(numrows*sizeof(UDM_URL_CRD));
    CoordList->acoords= numrows;
  }
  
  /* Add new found word to the list */
  for(curnum= 0, i= 0; i < numrows; i++)
  {
    uint4 coord= atoi(UdmSQLValue(&SQLRes, i, 1));
    uint4 section= UDM_WRDSEC(coord);
    uint4 weight= args->wf[section];
    
    if(weight && (!args->Word.secno || args->Word.secno == section))
    {
      UDM_URL_CRD *Coord= &CoordList->Coords[curnum];
      Coord->url_id= UDM_ATOI(UdmSQLValue(&SQLRes, i, 0));
      Coord->pos= UDM_WRDPOS(coord);
      Coord->secno= UDM_WRDSEC(coord);
      Coord->num= args->Word.order & 0xFF;
      Coord->seclen= 0;
      curnum++;
    }
  }
  CoordList->ncoords= curnum;
  UdmSQLFree(&SQLRes);
  UdmURLCRDListSortByURLThenSecnoThenPos(CoordList);
  return rc;
}


static int
UdmFindWordSingle(UDM_FINDWORD_ARGS *args)
{
  int rc;
  UDM_URLCRDLIST CoordList;

  bzero(&CoordList, sizeof(CoordList));

  if (UDM_OK != (rc= UdmFindWordSingleInternal(args, &CoordList, "dict", 0)))
    return rc;

  if (args->save_section_size && CoordList.ncoords)
  {
    UDM_URL_CRD *Crd= CoordList.Coords;
    UDM_URL_CRD *End= CoordList.Coords + CoordList.ncoords;
    UDM_URL_CRD *To= CoordList.Coords;
    UDM_URL_CRD *Prev= CoordList.Coords;
    urlid_t prev_url_id;
    int prev_secno;
    
    for (prev_url_id= Crd->url_id, prev_secno= Crd->secno ; Crd < End; Crd++)
    {
      UDM_URL_CRD *Next= Crd + 1;
      if (Next == End ||
          Next->url_id != prev_url_id ||
          Next->secno  != prev_secno)
      {
        for ( ; Prev < To; Prev++)
        {
          Prev->seclen= Crd->pos;
        }
        if (Next < End)
        {
          prev_url_id= Next->url_id;
          prev_secno= Next->secno;
          Prev= To;
        }
      }
      else
        *To++= *Crd;
    }
    CoordList.ncoords= To - CoordList.Coords;
  }
  if (args->urls.nurls)
    UdmApplyFastLimit(&CoordList, &args->urls);
  if (CoordList.ncoords)
  {
    args->Word.count= CoordList.ncoords;
    UdmURLCRDListListAddWithSort2(args, &CoordList);
  }
  return(UDM_OK);
}


int
UdmFindCrossWord(UDM_FINDWORD_ARGS *args)
{
  UDM_URLCRDLIST CoordList;
  char cmparg[256];
  int rc;

  bzero((void*) &CoordList, sizeof(CoordList));

  UdmBuildCmpArgSQL(args->db, args->Word.match, args->Word.word,
                    cmparg, sizeof(cmparg));
  args->cmparg= cmparg;

  if (UDM_OK != (rc= UdmFindWordSingleInternal(args, &CoordList,
                                               "crossdict", 1)))
    return rc;

  if (args->urls.nurls)
    UdmApplyFastLimit(&CoordList, &args->urls);
  if (CoordList.ncoords)
  {
    UdmURLCRDListListAddWithSort2(args, &CoordList);
    args->Word.count= CoordList.ncoords;
  }
  return UDM_OK;
}


int UdmTruncateCrossDict(UDM_AGENT * Indexer,UDM_DB *db)
{
  return UdmSQLTableTruncateOrDelete(db, "crossdict");
}


int
UdmDeleteCrossWordFromURL(UDM_AGENT * Indexer,UDM_DOCUMENT *Doc, UDM_DB *db)
{
  char  qbuf[1024];
  urlid_t  url_id = UdmVarListFindInt(&Doc->Sections, "ID", 0);
  urlid_t  referrer_id  =UdmVarListFindInt(&Doc->Sections, "Referrer-ID", 0);
  int  rc=UDM_OK;
  const char      *qu = (db->DBType == UDM_DB_PGSQL) ? "'" : "";
  
  if(url_id)
  {
    sprintf(qbuf,"DELETE FROM crossdict WHERE url_id=%s%i%s", qu, url_id, qu);
    if(UDM_OK!=(rc=UdmSQLQuery(db,NULL,qbuf)))
      return rc;
  }
  if(referrer_id)
  {
    sprintf(qbuf,"DELETE FROM crossdict WHERE ref_id=%s%i%s", qu, referrer_id, qu);
    rc=UdmSQLQuery(db,NULL,qbuf);
  }
  return rc;
}


static int
UdmTruncateDictSingle(UDM_AGENT *Indexer,UDM_DB *db)
{
  return UdmSQLTableTruncateOrDelete(db, "dict");
}


static int
UdmSingle2BlobSQL(UDM_AGENT *Indexer, UDM_DB *db, UDM_URLDATALIST *URLList)
{
  int rc;
  char buf[128], wtable[64];
  UDM_PSTR row[3];
  UDM_SQLRES SQLRes;
  size_t t, u, s, w;
  UDM_BLOB_CACHE bcache;
  UDM_MULTI_CACHE mcache;
  urlid_t url_id;
  UDM_WORD words;
  int tr= (db->flags & UDM_SQL_HAVE_TRANSACT) ? 1 : 0;
  
  if (tr && UDM_OK != (rc= UdmSQLBegin(db)))
    return rc;
  
  if (UDM_OK != (rc= UdmBlobGetWTable(Indexer, db, wtable, sizeof(wtable))))
    return rc;

  /* Delete old words from bdict */
  rc= UdmSQLTableTruncateOrDelete(db, wtable);
  if (rc != UDM_OK)
  {
    return(rc);
  }

  if (db->DBType == UDM_DB_MYSQL)
  {
    udm_snprintf(buf, sizeof(buf), "LOCK TABLES dict WRITE, %s WRITE", wtable);
    rc = UdmSQLQuery(db, NULL, buf);
    if (rc != UDM_OK)
    {
      return(rc);
    }
  }

  udm_snprintf(buf, sizeof(buf), "SELECT url_id, word, intag FROM dict");
  rc= UdmSQLExecDirect(db, &SQLRes, buf);
  if (rc != UDM_OK)
  {
    return(rc);
  }

  UdmMultiCacheInit(&mcache);
  while (db->sql->SQLFetchRow(db, &SQLRes, row) == UDM_OK)
  {
    int coord;
    url_id = UDM_ATOI(row[0].val);
    words.word = row[1].val;
    coord= UDM_ATOI(row[2].val);
    words.secno= UDM_WRDSEC(coord);
    words.pos= UDM_WRDPOS(coord);
    words.hash= 0;
    UdmMultiCacheAdd(&mcache, url_id, 0, &words);
  }
  UdmSQLFree(&SQLRes);
  UdmBlobCacheInit(&bcache);
  for (t = 0; t <= MULTI_DICTS; t++)
  {
    UDM_MULTI_CACHE_TABLE *table = &mcache.tables[t];
    for (u = 0; u < table->nurls; u++)
    {
      UDM_MULTI_CACHE_URL *url = &table->urls[u];
      if (!UdmURLDataListSearch(URLList, url->url_id))
        continue;
      for (s = 0; s < url->nsections; s++)
      {
        UDM_MULTI_CACHE_SECTION *section = &url->sections[s];
        for (w = 0; w < section->nwords; w++)
	{
          UDM_MULTI_CACHE_WORD *word= &section->words[w];
          char *intag= UdmMultiCachePutIntagUsingRaw(word);
          UdmBlobCacheAdd(&bcache, url->url_id, section->secno, word->word,
                          word->nintags, intag, strlen(intag));
          UDM_FREE(intag);
	}
      }
    }
  }
  rc= UdmBlobCacheWrite(Indexer, db, &bcache, wtable, 0);
  UdmBlobCacheFree(&bcache);
  UdmMultiCacheFree(&mcache);

  if (rc != UDM_OK)
  {
    return(rc);
  }

  if (db->DBType == UDM_DB_MYSQL)
  {
    rc = UdmSQLQuery(db, NULL, "UNLOCK TABLES");
    if (rc != UDM_OK)
    {
      return(rc);
    }
  }

  if (UDM_OK != (rc= UdmBlobWriteTimestamp(Indexer, db, wtable)))
    return rc;
    
  UdmLog(Indexer, UDM_LOG_ERROR, "Converting url.");
  if (UDM_OK != (rc= UdmBlobWriteURL(Indexer, db, wtable, 0)))
    return rc;
  
  UdmLog(Indexer, UDM_LOG_ERROR, "Switching to new blob table.");
  UdmBlobSetTable(Indexer, db);
  
  if (tr && UDM_OK != (rc= UdmSQLCommit(db)))
    return rc;
  
  return UDM_OK;
}


static int
UdmWordStatCreateSingle(UDM_AGENT *A, UDM_DB *db)
{
  char qbuf[128];
  sprintf(qbuf, "SELECT word, count(*) FROM dict GROUP BY word");
  return UdmWordStatQuery(A, db, qbuf);
}


static int
UdmDumpWordInfoOneDocSingle(UDM_AGENT *A, UDM_DB *db, UDM_DOCUMENT *Doc)
{
  return UDM_OK;
}


UDM_DBMODE_HANDLER udm_dbmode_handler_single=
{
  "single",
  UdmSingle2BlobSQL,
  StoreWordsSingle,
  UdmTruncateDictSingle,
  UdmDeleteWordsFromURLSingle,
  UdmFindWordSingle,
  UdmDumpWordInfoOneDocSingle,
  UdmWordStatCreateSingle
};


#endif /* HAVE_SQL */
