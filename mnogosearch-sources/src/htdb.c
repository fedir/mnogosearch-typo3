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

#ifdef HAVE_SQL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#ifdef WIN32
#include <time.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_db.h"
#include "udm_url.h"
#include "udm_log.h"
#include "udm_proto.h"
#include "udm_vars.h"
#include "udm_hrefs.h"
#include "udm_db_int.h"
#include "udm_sqldbms.h"
#include "udm_indexer.h"
#include "udm_textlist.h"
#include "udm_parsehtml.h"


/***********************************************************/
/*  HTDB stuff:  Indexing of database content              */
/***********************************************************/

#define MAXNPART 32

static void include_params(UDM_DB *db,const char *src,char *path,char *dst,
                           size_t start, int limit)
{
  size_t nparts= 0;
  char *part[MAXNPART];
  char *lt;
  
  part[0]= udm_strtok_r(path, "/", &lt);
  while (part[nparts] && nparts < MAXNPART)
  {
    part[++nparts]= udm_strtok_r(NULL, "/", &lt);
  }
  
  for (*dst='\0'; *src; )
  {
    if(*src == '\\')
    {
      *dst++= src[1];
      *dst= '\0';
      src+= 2;
      continue;
    }
    if (*src == '$')
    {
      int i= atoi(++src)-1;
      
      while((*src>='0')&&(*src<='9'))src++;
      if ((i >= 0) && (i < nparts))
      {
        UdmUnescapeCGIQuery(dst,part[i]);
        while(*dst)dst++;
      }
      continue;
    }
    *dst++=*src++;
    *dst='\0';
    continue;
  }
  
  if (limit)
  {
    switch (db->DBType)
    {
      case UDM_DB_MYSQL:
        sprintf(dst, " LIMIT %d,%d", start, limit);
        break;
      case UDM_DB_PGSQL:
      default:
        sprintf(dst, " LIMIT %d OFFSET %d", limit, start);
        break;
    }
  }
}


static int
UdmHTDBCreateHTTPResponse(UDM_AGENT *Indexer,
                          UDM_DOCUMENT *Doc,
                          UDM_SQLRES *SQLres)
{
  char *to;
  size_t i; 
  for (to= Doc->Buf.buf, i= 0; i < UdmSQLNumCols(SQLres); i++)
  {
    size_t len;
    const char *from;
    if (i > 0)
    {
      memcpy(to, "\r\n", 2);
      to+= 2;
    }
    len= UdmSQLLen(SQLres, 0, i);
    from= UdmSQLValue(SQLres, 0, i);
    if (len == 1 && *from == ' ')
    {
      /*
         Sybase+unixODBC returns space character instead
         of an empty string executing this query:
           SELECT '' FROM t1;
      */
    }
    else
    {
      memcpy(to, from, len);
      to+= len;
    }
  }
  *to= '\0';
  return UDM_OK;
}


static void
UdmRemoveWiki(char *str, char *strend)
{
  for ( ; str < strend ; str++)
  {
    if (*str == '[')
    {
      int smcount= 0;
      for (*str++= ' ' ; str < strend ; str++)
      {
        if (*str == ']')
        {
          *str++= ' ';
          break;
        }
        if (*str == '[')
          UdmRemoveWiki(str, strend);
        if (*str == ':')
        {
          *str= ' ';
          smcount++;
        }
        if (smcount < 2)
          *str= ' ';
      }
    }
  }
}


static int
UdmHTDBProcessNonHTTPResponse(UDM_AGENT *A,
                              UDM_DOCUMENT *D,
                              UDM_SQLRES *SQLRes)
{
  UDM_TEXTITEM Item;
  UDM_DSTR tbuf;
  int rc= UDM_OK, status= 200;
  size_t row, nrows, ncols= UdmSQLNumCols(SQLRes), length= 0;
  char dbuf[128]= "";
  bzero((void*)&Item, sizeof(Item));
  UdmDSTRInit(&tbuf, 1024);
  
  for (row=0, nrows= UdmSQLNumRows(SQLRes); row < nrows; row++)
  {
    size_t col;
    for (col= 0; col  < ncols; col++)
    {
      UDM_VAR *Sec;
      const char *sqlname= SQLRes->Fields[col].sqlname;
      const char *sqlval= UdmSQLValue(SQLRes, row, col);
      Item.section_name= SQLRes->Fields[col].sqlname;
      if ((Sec= UdmVarListFind(&D->Sections, Item.section_name)))
      {
        if (Sec->flags & UDM_VARFLAG_HTMLSOURCE)
        {
          UDM_HTMLTOK tag;
          const char *htok, *last;
          UdmHTMLTOKInit(&tag);
          for (htok= UdmHTMLToken(sqlval, &last, &tag);
               htok;
               htok = UdmHTMLToken(NULL, &last, &tag))
          {
            if (tag.type == UDM_HTML_TXT &&
                !tag.script && !tag.comment && !tag.style)
            {
              UdmDSTRReset(&tbuf);
              if (Sec->flags & UDM_VARFLAG_WIKI)
                UdmRemoveWiki((char*) htok, (char*) last);
              UdmDSTRAppend(&tbuf, htok, last - htok);
              Item.str= tbuf.data;
              Item.section= Sec->section;
              Item.section_name= Sec->name;
              UdmTextListAdd(&D->TextList, &Item);
            }
          }
        }
        else
        {
          Item.str= (char*) sqlval;
          Item.section= Sec->section;
          UdmTextListAdd(&D->TextList, &Item);
        }
        length+= UdmSQLLen(SQLRes, row, col);
      }
      if (!strcasecmp(sqlname, "status"))
        status= atoi(sqlval);
      else if (!strcasecmp(sqlname, "last_mod_time"))
      {
        int last_mod_time= atoi(sqlval);
        strcpy(dbuf, "Last-Modified: ");
        UdmTime_t2HttpStr(last_mod_time, dbuf + 15);
      }
    }
  }
  
  UdmDSTRFree(&tbuf);
  
  D->Buf.content_length= length;
  sprintf(D->Buf.buf, "HTTP/1.0 %d %s\r\n"
                      "Content-Type: mnogosearch/htdb\r\n%s%s\r\n",
                      status, UdmHTTPErrMsg(status),
                      dbuf[0] ? dbuf : "", dbuf[0] ? "\r\n" : "");
  return rc;
}


int UdmHTDBGet(UDM_AGENT *Indexer,UDM_DOCUMENT *Doc)
{
  char    *qbuf;
  char    *end=Doc->Buf.buf;
  UDM_SQLRES  SQLres;
  UDM_URL    realURL;
  UDM_DB      dbnew, *db= &dbnew;
  const char  *url=UdmVarListFindStr(&Doc->Sections,"URL","");
  const char  *htdblist=UdmVarListFindStr(&Doc->Sections,"HTDBList","");
  const char  *htdbdoc=UdmVarListFindStr(&Doc->Sections,"HTDBDoc","");
  const char  *htdbaddr = UdmVarListFindStr(&Doc->Sections, "HTDBAddr", NULL);
  int    usehtdburlid = UdmVarListFindInt(&Indexer->Conf->Vars, "UseHTDBURLId", 0);
  int    rc = UDM_OK;
  
  Doc->Buf.buf[0]=0;
  UdmURLInit(&realURL);
  UdmURLParse(&realURL,url);
  if ((qbuf = (char*)UdmMalloc(4 * 1024 + strlen(htdblist) + strlen(htdbdoc))) == NULL) return UDM_ERROR;
  *qbuf = '\0';
  
  
  if (htdbaddr)
  {
    UdmDBInit(&dbnew);
    if (UDM_OK != (rc= UdmDBSetAddr(db, htdbaddr, UDM_OPEN_MODE_READ)))
    {
      UdmLog(Indexer, UDM_LOG_ERROR, "%s HTDB address: %s",
             rc == UDM_UNSUPPORTED ? "Unsupported" : "Invalid", htdbaddr);
      return UDM_ERROR;
    }
  }
  else
  {
    if (Indexer->Conf->dbl.nitems != 1)
    {
      UdmLog(Indexer, UDM_LOG_ERROR, "HTDB cannot work with multiple DBAddr without HTDBAddr");
      return UDM_ERROR;
    }
    db= &Indexer->Conf->dbl.db[0];
  }

  if(realURL.filename != NULL)
  {
    char real_path[1024]="";
    
    udm_snprintf(real_path,sizeof(real_path)-1,"%s%s",realURL.path,realURL.filename);
    real_path[sizeof(real_path)-1]='\0';
    include_params(db,htdbdoc, real_path, qbuf, 0, 0);
    UdmLog(Indexer, UDM_LOG_DEBUG, "HTDBDoc: %s\n",qbuf);
    if(UDM_OK != (rc = UdmSQLQuery(db, &SQLres, qbuf)))
    {
      goto HTDBexit;
    }
    if(UdmSQLNumRows(&SQLres)==1)
    {
      if (!strncmp(UdmSQLValue(&SQLres,0,0),"HTTP/",5))
        UdmHTDBCreateHTTPResponse(Indexer, Doc, &SQLres);
      else
        UdmHTDBProcessNonHTTPResponse(Indexer, Doc, &SQLres);
    }
    else
    {
      sprintf(Doc->Buf.buf,"HTTP/1.0 404 Not Found\r\n\r\n");
    }
    UdmSQLFree(&SQLres);
  }
  else
  {
    size_t  i, start = 0;
    urlid_t  url_id = UdmVarListFindInt(&Doc->Sections, "ID", 0);
    const size_t  htdblimit = UdmVarListFindUnsigned(&Doc->Sections, "HTDBLimit", 0);
    int  done = 0, hops=UdmVarListFindInt(&Doc->Sections,"Hops",0);


    sprintf(Doc->Buf.buf,"HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n<HTML><BODY>\n");
    end=UDM_STREND(Doc->Buf.buf);
    strcpy(end,"</BODY></HTML>\n");

    while (!done)
    {
      size_t nrows;
      include_params(db,htdblist, realURL.path, qbuf, start, htdblimit);
      UdmLog(Indexer, UDM_LOG_DEBUG, "HTDBList: %s\n",qbuf);
      if(UDM_OK != (rc = UdmSQLQuery(db, &SQLres, qbuf)))
      goto HTDBexit;

      nrows= UdmSQLNumRows(&SQLres);
      done= htdblimit ? (htdblimit != nrows) : 1;
      start+= nrows;

      for(i = 0; i < nrows; i++)
      {
      UDM_HREF Href;
      UdmHrefInit(&Href);
      Href.referrer=url_id;
      Href.hops=hops+1;
      Href.url = (char*)UdmStrdup(UdmSQLValue(&SQLres,i,0));
      Href.method=UDM_METHOD_GET;
      Href.rec_id = usehtdburlid ? atoi(Href.url) : 0;
      UdmHrefListAdd(&Doc->Hrefs,&Href);
      UDM_FREE(Href.url);
      }
      UdmSQLFree(&SQLres);
      UdmDocStoreHrefs(Indexer, Doc);
      UdmHrefListFree(&Doc->Hrefs);
      UdmStoreHrefs(Indexer);
    }
  }
  end = UDM_STREND(Doc->Buf.buf);
  Doc->Buf.size=end-Doc->Buf.buf;
HTDBexit:
  if (db == &dbnew)
    UdmDBFree(db);
  UdmURLFree(&realURL);
  UDM_FREE(qbuf);
  return rc;
}

#endif /* HAVE_SQL */
