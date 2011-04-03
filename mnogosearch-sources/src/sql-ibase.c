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
#if HAVE_IBASE
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
#ifdef WIN32
#include <process.h>
#endif

#include <ibase.h>

#define SQL_VARCHAR(len) struct {short vary_length; char vary_string[(len)+1];}
typedef struct
{
  short  len;
  char  str[1];
} UDM_IBASE_VARY;

#define UDM_IBASE_MAX_BIND_PARAM UDM_SQL_MAX_BIND_PARAM

typedef struct
{
  isc_db_handle DBH;        /* database handle    */
  ISC_STATUS    status[20]; /* status vector      */
  isc_tr_handle tr_handle;  /* transaction handle */ 
  isc_stmt_handle query_handle; /* query handle   */
  int autocommit;
  long query_type;          /* SELECT or UPDATE, etc */
  XSQLDA in_sqlda;
  XSQLVAR dummy_vars[UDM_IBASE_MAX_BIND_PARAM]; /* Must be after in_sqlda */
  size_t long_data_lengths[UDM_IBASE_MAX_BIND_PARAM]; /* For data >64K */
} UDM_IB;


static int
UdmIBStmtFree(UDM_DB *db)
{
  UDM_IB *ib= (UDM_IB*) db->specific;
  if (isc_dsql_free_statement(ib->status, &ib->query_handle, DSQL_drop))
  {
    db->errcode= 1;
    return UDM_ERROR;
  }
  return UDM_OK;
}


static void
UdmIBaseDisplayError(UDM_DB *db, const char *fname)
{
  char *s= db->errstr;
  UDM_IB *ib= (UDM_IB*) db->specific;
  ISC_STATUS *ibstatus= ib->status;

  s+= sprintf(s, "%s: ", fname);
  while(isc_interprete(s ,&ibstatus))
  {
    strcat(s," ");
    s=db->errstr+strlen(db->errstr);
  }
}

static int
UdmIBaseConnect(UDM_DB *db)
{
  char dpb_buffer[256], *dpb, *e;
  const char *p;
  int dpb_length, len;
  char connect_string[256];
  const char* DBUser= UdmVarListFindStr(&db->Vars,"DBUser",NULL);
  const char* DBPass= UdmVarListFindStr(&db->Vars,"DBPass",NULL);
  const char* DBHost= UdmVarListFindStr(&db->Vars, "DBHost", "localhost");
  UDM_IB *ib= (UDM_IB*) malloc(sizeof(UDM_IB));
  bzero(ib, sizeof(*ib));
  db->specific= (void*) ib;
  
  ib->autocommit= 1;
  dpb = dpb_buffer;
  *dpb++ = isc_dpb_version1;

  if (DBUser != NULL && (len = strlen(DBUser)))
  {
    *dpb++ = isc_dpb_user_name;
    *dpb++ = len;
    for (p = DBUser; *p;)
      *dpb++ = *p++;
  }
  if (DBPass != NULL && (len = strlen(DBPass)))
  {
    *dpb++ = isc_dpb_password;
    *dpb++ = len;
    for (p = DBPass; *p;)
      *dpb++ = *p++;
  }
  /*
  if (charset != NULL && (len = strlen(charset))) {
    *dpb++ = isc_dpb_lc_ctype;
    *dpb++ = strlen(charset);
    for (p = charset; *p;) {
      *dpb++ = *p++;
    }
  }
#ifdef isc_dpb_sql_role_name
  if (role != NULL && (len = strlen(role))) {
    *dpb++ = isc_dpb_sql_role_name;
    *dpb++ = strlen(role);
    for (p = role; *p;) {
      *dpb++ = *p++;
    }
  }
#endif
  */

  dpb_length = dpb - dpb_buffer;
  
  if(strcmp(DBHost,"localhost"))
    udm_snprintf(connect_string,sizeof(connect_string)-1,"%s:%s",DBHost, db->DBName);
  else
    udm_snprintf(connect_string,sizeof(connect_string)-1,"%s",db->DBName);
  
  /* Remove possible trailing slash */
  e= connect_string+strlen(connect_string);
  if (e>connect_string && e[-1]=='/')
    e[-1]='\0';
  
#ifdef DEBUG_SQL
  fprintf(stderr, "SQL Connect to: '%s'\n",connect_string);
#endif  
  if(isc_attach_database(ib->status, strlen(connect_string), connect_string,
                         &(ib->DBH), dpb_length, dpb_buffer))
  {
    db->errcode=1;
    return UDM_ERROR;
  }
  
  /*
    http://www.firebirdfaq.org/faq223/
    To obtain the server version, use isc_info_svc_server_version() API func.
    It does not work for Firebird Classic 1.0, so if you don't get an answer
    you'll know it's Firebird Classic 1.0 or InterBase Classic 6.0.
    Otherwise it returns a string like this:
      LI-V2.0.0.12748 Firebird 2.0
    or...
      LI-V1.5.3.4870 Firebird 1.5
  
    If you use Firebird 2.1 (and higher), you can also use:
      SELECT rdb$get_context('SYSTEM', 'ENGINE_VERSION')
      from rdb$database;   
  */
  return UDM_OK;
}


static void
UdmIBaseClose(UDM_DB*db)
{
  UDM_IB *ib= (UDM_IB*) db->specific;
  if(db->connected)
  {
    if (isc_detach_database(ib->status, &(ib->DBH)))
    {
      db->errcode=1;
    }
    UDM_FREE(db->specific);
  }
}


static int
UdmIBPrepare(UDM_DB *db, const char *query)
{
  UDM_IB *ib= (UDM_IB*) db->specific;
  char    query_info[] = { isc_info_sql_stmt_type };
  char    info_buffer[18];
  
  ib->query_handle= NULL;
  ib->in_sqlda.sqln= 0;
  ib->in_sqlda.sqld= 0;
  ib->in_sqlda.version= 1;

  if(!ib->tr_handle)
  {
    if (isc_start_transaction(ib->status, &ib->tr_handle, 1,
                              &(ib->DBH), 0, NULL))
    {
      db->errcode=1;
      UdmIBaseDisplayError(db, "isc_start_transaction");
      return UDM_ERROR;
    }
    ib->autocommit=1;
  }
  else
  {
    ib->autocommit=0;
  }
  
  
  if (isc_dsql_allocate_statement(ib->status, &ib->DBH, &ib->query_handle))
  {
    db->errcode=1; 
    UdmIBaseDisplayError(db, "isc_dsql_allocate_statement");
    return UDM_ERROR;
  }
  if (isc_dsql_prepare(ib->status, &ib->tr_handle, &ib->query_handle,
                       0, query, 1, NULL))
  {
    UdmIBaseDisplayError(db, "isc_dsql_prepare");
    db->errcode=1; 
    return UDM_ERROR;
  }
  if (!isc_dsql_sql_info(ib->status, &ib->query_handle,
                         sizeof(query_info), query_info,
                         sizeof(info_buffer), info_buffer))
  {
    short l;
    l = (short) isc_vax_integer((char ISC_FAR *) info_buffer + 1, 2);
    ib->query_type = isc_vax_integer((char ISC_FAR *) info_buffer + 3, l);
  }
  
  return UDM_OK;
}


static int
UdmIBExecInternal(UDM_DB *db, UDM_SQLRES *res, int free_stmt)
{
  UDM_IB *ib= (UDM_IB*) db->specific;
  ISC_STATUS  status[20]; /* To not override db->status */

  /* Find out what kind of query is to be executed */
  if (ib->query_type == isc_info_sql_stmt_select ||
      ib->query_type == isc_info_sql_stmt_select_for_upd)
  {
    XSQLDA *osqlda=NULL;
    long fetch_stat;
    int i;
    short   sqlind_array[128];
    
    /* Select, need to allocate output sqlda and and prepare it for use */
    osqlda = (XSQLDA *) UdmXmalloc(XSQLDA_LENGTH(0));
    osqlda->sqln = 0;
    osqlda->version = SQLDA_VERSION1;
    
    /* Fetch column information */
    if (isc_dsql_describe(ib->status, &ib->query_handle, 1, osqlda))
    {
      UDM_FREE(osqlda);
      db->errcode=1;
      isc_rollback_transaction(status, &ib->tr_handle);
      return UDM_ERROR;
    }
    
    if (osqlda->sqld)
    {
      osqlda = (XSQLDA *) UdmXrealloc(osqlda, XSQLDA_LENGTH(osqlda->sqld));
      osqlda->sqln = osqlda->sqld;
      osqlda->version = SQLDA_VERSION1;
      if (isc_dsql_describe(ib->status, &ib->query_handle, 1, osqlda))
      {
        UDM_FREE(osqlda);
        db->errcode=1;
        isc_rollback_transaction(status, &ib->tr_handle);
        return UDM_ERROR;
      }
    }
    
    if (!res)
    {
      db->errcode=1;
      udm_snprintf(db->errstr, sizeof(db->errstr)-1,
                   "ibase_query with empty 'res' returned rows");
      return UDM_ERROR;
    }
    
    res->nCols = osqlda->sqld;
    res->Fields= (UDM_SQLFIELD*)UdmXmalloc(res->nCols*sizeof(UDM_SQLFIELD));
    
    for (i = 0; i < osqlda->sqld; i++)
    {
      XSQLVAR *var= &osqlda->sqlvar[i];
      int coltype= osqlda->sqlvar[i].sqltype & ~1;
      
      var->sqlind= &sqlind_array[i];
      var->sqlind[0]= 0;
      res->Fields[i].sqlname= (char*)UdmStrdup(var->aliasname[0] ? var->aliasname : var->sqlname);
      res->Fields[i].sqllen= var->sqllen;
      
      switch(coltype)
      {
        case SQL_SHORT:
          var->sqldata = (char*)UdmMalloc(sizeof(short));
          break;
        case SQL_LONG:
          var->sqldata = (char*)UdmMalloc(sizeof(long));
          break;
        case SQL_FLOAT:
          var->sqldata = (char*)UdmMalloc(sizeof(float));
          break;
        case SQL_DOUBLE:
          var->sqldata = (char*)UdmMalloc(sizeof(double));
          break;
        case SQL_DATE:
        case SQL_BLOB:
        case SQL_ARRAY:
          var->sqldata = (char*)UdmMalloc(sizeof(ISC_QUAD));
          break;
        case SQL_TEXT:
          var->sqldata = (char*)UdmMalloc((size_t)(osqlda->sqlvar[i].sqllen));
          break;
        case SQL_VARYING:
          osqlda->sqlvar[i].sqldata = (char*)UdmMalloc((size_t)(osqlda->sqlvar[i].sqllen+sizeof(short)));
          break;
      }
    }
    
    if (isc_dsql_execute(ib->status, &ib->tr_handle, &ib->query_handle, 1, NULL))
    {
      UDM_FREE(osqlda);
      db->errcode=1;
      isc_rollback_transaction(status, &ib->tr_handle);
      return UDM_ERROR;
    }
    
    while ((fetch_stat= isc_dsql_fetch(ib->status, &ib->query_handle, 1, osqlda)) == 0)
    {
      res->Items= (UDM_PSTR *)UdmRealloc(res->Items,(res->nRows+1)*(res->nCols)*sizeof(UDM_PSTR));
      
      for(i=0;i<osqlda->sqld; i++)
      {
        UDM_IBASE_VARY *vary;
        char shortdata[64];
        XSQLVAR *var=osqlda->sqlvar+i;
        char *p=NULL;
        size_t len;
        
        if(*var->sqlind==-1) /* NULL data */
        {
          p= (char*)UdmStrdup("");
          len= 0;
        }
        else
        switch(var->sqltype & ~1)
        {
          case SQL_TEXT:
            p=(char*)UdmMalloc((size_t)(var->sqllen+1));
            strncpy(p,(char*)var->sqldata,(size_t)(var->sqllen));
            p[var->sqllen]='\0';
            len= var->sqllen;
            break;
          case SQL_VARYING:
            vary=(UDM_IBASE_VARY*)var->sqldata;
            p=(char*)UdmMalloc((size_t)(vary->len+1));
            strncpy(p,vary->str,(size_t)(vary->len));
            p[vary->len]='\0';
            len= vary->len;
            break;
          case SQL_LONG:
            len= sprintf(shortdata,"%ld",*(long*)(var->sqldata));
            p = (char*)UdmStrdup(shortdata);
            break;
          case SQL_SHORT:
            len= sprintf(shortdata,"%d",*(short*)(var->sqldata));
            p = (char*)UdmStrdup(shortdata);
            break;
          case SQL_FLOAT:
            len= sprintf(shortdata,"%f",*(float*)(var->sqldata));
            p = (char*)UdmStrdup(shortdata);
            break;
          case SQL_DOUBLE:
            len= sprintf(shortdata,"%f",*(double*)(var->sqldata));
            p = (char*)UdmStrdup(shortdata);
            break;
          case SQL_BLOB:
            {
              isc_blob_handle blob_handle= NULL;
              unsigned short blob_seg_len, blob_segment_size= 32*1024;
              char blob_items[]= {isc_info_blob_total_length };
              char res_buffer[20], *res_buffer_ptr;
              size_t blob_length= 0;
              
              len= 0;
              p= NULL;
              
              /* Open the blob with the fetched blob_id. */
              if (isc_open_blob(status, &ib->DBH, &ib->tr_handle,
                                &blob_handle, (ISC_QUAD*)var->sqldata))
              {
                /* isc_print_status(status); */
                len= sprintf(shortdata, "<isc_open_blob failed>");
                p= (char*) UdmStrdup(shortdata);
                break;
              }
              
              /* Get blob info */
              isc_blob_info(status, &blob_handle,
                            sizeof(blob_items), blob_items,
                            sizeof(res_buffer), res_buffer);
              if (status[0] == 1 && status[1])
              {
                /* isc_print_status(status);*/
                len= sprintf(shortdata, "<isc_blob_info failed>");
                p= (char*) UdmStrdup(shortdata);
                goto blob_close;
              }
              
              /* Parse blob info data */
              for (res_buffer_ptr= res_buffer; *res_buffer_ptr != isc_info_end; )
              {
                char item= *res_buffer_ptr++;
                if (item == isc_info_blob_total_length)
                {
                  int length= isc_vax_integer(res_buffer_ptr, 2);
                  res_buffer_ptr+= 2;
                  blob_length= isc_vax_integer(res_buffer_ptr, length);
                  res_buffer_ptr+= length;
                }
              }
              
              p= (char*) UdmMalloc(blob_length + 1);
              
              while (isc_get_segment(status, &blob_handle, &blob_seg_len,
                                     blob_segment_size, p + len) == 0 ||
                     status[1] == isc_segment)
              {
                len+= blob_seg_len;
              }
              
              p[len]= '\0';
              
              if (status[1] != isc_segstr_eof)
              {
                len= sprintf(shortdata, "<isc_get_segment failed>");
                p= (char*) UdmStrdup(shortdata);
                break;
              }
blob_close:
              if (isc_close_blob(status, &blob_handle))
              {
                len= sprintf(shortdata, "<isc_close_blob failed>");
                p= (char*) UdmStrdup(shortdata);
                break;
              }
            }
            break;
          default:
            len= sprintf(shortdata,"Unknown SQL type %d", var->sqltype & ~1);
            p= (char*)UdmStrdup(shortdata);
            break; 
        }
        /*UdmRTrim(p," ");*/
        res->Items[res->nRows*res->nCols+i].val= p;
        res->Items[res->nRows*res->nCols+i].len= len;
      }
      res->nRows++;
      if (db->res_limit && res->nRows >= db->res_limit)
      {
        fetch_stat = 100L;
        break;
      }
    }
    
    db->res_limit= 0;
    
    /* Free fetch buffers */
    for (i = 0; i < osqlda->sqld; i++)
      UDM_FREE(osqlda->sqlvar[i].sqldata);

    UDM_FREE(osqlda);
    
    if (fetch_stat != 100L)
    {
      db->errcode=1; 
      isc_rollback_transaction(status, &ib->tr_handle);
      return UDM_ERROR;
    }
  }
  else
  {
    /* Not select */
    if (isc_dsql_execute(ib->status, &ib->tr_handle, &ib->query_handle, 1,
                         ib->in_sqlda.sqln ? &ib->in_sqlda : NULL))
    {
      UdmIBaseDisplayError(db, "isc_dsql_execute");
      ib->autocommit= 1;
      db->errcode=1;
      isc_rollback_transaction(status, &ib->tr_handle);
      if (free_stmt)
        UdmIBStmtFree(db);
      return UDM_ERROR;
    }
  }
  
  if (free_stmt && UDM_OK != UdmIBStmtFree(db))
  {
    db->errcode=1; 
    isc_rollback_transaction(status, &ib->tr_handle);
    return UDM_ERROR;
  }

  if(ib->autocommit)
  {
    if (isc_commit_transaction(ib->status, &ib->tr_handle))
    {
      db->errcode=1; 
      ib->tr_handle=NULL;
      return UDM_ERROR;
    }
    ib->tr_handle=NULL;
  }
  
  return UDM_OK;
}


/*
  Create a BLOB, write data to it, and return BLOB id
*/
static int
UdmIBCreateBlob(UDM_IB *ib, const char *blob_data, size_t blob_length,
                ISC_QUAD *blob_id)
{
  isc_blob_handle blob_handle= NULL;
  unsigned short seg_max_length = 32*1024;
  
  if (isc_create_blob(ib->status, &ib->DBH, &ib->tr_handle, &blob_handle, blob_id))
    return UDM_ERROR;

  while (blob_length)
  {
    unsigned short blob_seg_length= blob_length > seg_max_length ?
                                    seg_max_length : blob_length;
    if (isc_put_segment(ib->status, &blob_handle, blob_seg_length, blob_data))
      return UDM_ERROR;
    blob_data+= blob_seg_length;
    blob_length-= blob_seg_length;
  }
  
  if (isc_close_blob(ib->status, &blob_handle))
    return UDM_ERROR;
  
  return UDM_OK;
}


static int
UdmIBExec(UDM_DB *db)
{
  UDM_IB *ib= (UDM_IB*) db->specific;
  ISC_QUAD blob_ids[UDM_IBASE_MAX_BIND_PARAM];
  size_t i;
  for (i= 0; i < ib->in_sqlda.sqln; i++)
  {
    XSQLVAR *var=&ib->in_sqlda.sqlvar[i];
    if ((var->sqltype & ~1) == SQL_BLOB)
    {
      size_t blob_length= ib->long_data_lengths[i];
      if (UDM_OK != UdmIBCreateBlob(ib, var->sqldata, blob_length, &blob_ids[i]))
      {
        UdmIBaseDisplayError(db, "UdmIBCreateBlob");
        db->errcode= 1;
        return UDM_ERROR;
      }
      var->sqldata= (char*) &blob_ids[i];
      var->sqllen= sizeof(ISC_QUAD);
    }
  }
  
  return UdmIBExecInternal(db, NULL, 0);
}


static int
sql_ibase_query(UDM_DB *db, UDM_SQLRES *res, const char *query)
{
  int     rc= UDM_OK;
  UDM_IB  *ib;  

  if(!db->connected)
  {
    UdmIBaseConnect(db);
    if(db->errcode)
    {
      UdmIBaseDisplayError(db, "UdmIBaseConnect");
      db->errcode=1;
      return UDM_ERROR;
    }
    else
    {
      db->connected=1;
    }
  }
  
  ib= (UDM_IB*) db->specific;
  
  if(!strcmp(query,"BEGIN"))
  {
    if(!ib->tr_handle)
    {
      if (isc_start_transaction(ib->status, &ib->tr_handle, 1,
                                &(ib->DBH), 0, NULL))
      {
        db->errcode=1;
        UdmIBaseDisplayError(db, "isc_start_transaction");
        rc= UDM_ERROR;
      }
      ib->autocommit= 0;
    }
    else
    {
      ib->autocommit= 1;
      db->errcode=1;
      udm_snprintf(db->errstr,sizeof(db->errstr)-1,"Wrong call order: begin");
      rc= UDM_ERROR;
    }
    return rc;
  }
  
  if(!strcmp(query,"COMMIT"))
  {
    if(ib->tr_handle)
    {
      if (isc_commit_transaction(ib->status, &ib->tr_handle))
      {
        db->errcode=1;
        UdmIBaseDisplayError(db, "isc_commit_transaction");
        rc= UDM_ERROR;
      }
      ib->tr_handle=NULL;
      ib->autocommit= 1;
    }
    else
    {
      ib->autocommit= 1;
      db->errcode=1;
      udm_snprintf(db->errstr,sizeof(db->errstr)-1,"Wrong call order: commit");
      rc= UDM_ERROR;
    }
    return rc;
  }
  
  if (UdmIBPrepare(db, query))
    return UDM_ERROR;

  if (UdmIBExecInternal(db, res, 1))
    return UDM_ERROR;
  
  return rc;
}


static int
UdmIBaseQuery(UDM_DB *db, UDM_SQLRES *res, const char *query)
{
  int rc;
  db->errcode= 0;
    
  if (res)
  {
    bzero((void*) res, sizeof(UDM_SQLRES));
    res->db= db;
  }

  if(UDM_OK != (rc= sql_ibase_query(db,res,query)))
  {
    if(strstr(db->errstr,"uplicat") || strstr(db->errstr,"UNIQUE"))
    {
      db->errcode=0;
      rc= UDM_OK;
    }
    else
    {
      /*strcat(db->errstr," ");
      strcat(db->errstr,query);*/
    }
    return rc;
  }
  return rc;
}


static int
UdmIBBegin(UDM_DB *db)
{
  int rc= UdmIBaseQuery(db,NULL,"BEGIN");
  db->commit_fl = 1;
  return rc;
}


static int
UdmIBCommit(UDM_DB *db)
{
  int rc= UdmIBaseQuery(db,NULL,"COMMIT");
  db->commit_fl = 1;
  return rc;
}


static int
UdmSQLType2IBType(int udm_type)
{
  switch (udm_type)
  {
    case UDM_SQLTYPE_INT32:         return SQL_LONG;
    case UDM_SQLTYPE_LONGVARBINARY: return SQL_BLOB;
  }
  return SQL_TEXT;
}


static int short flag0= 0;


static int
UdmIBBind(UDM_DB *db, int position, const void *data, int size, int type)
{
  UDM_IB *ib= (UDM_IB*) db->specific;
  XSQLVAR *var= &ib->in_sqlda.sqlvar[position - 1];
  if (ib->in_sqlda.sqln < position)
  {
    ib->in_sqlda.sqln= position;
    ib->in_sqlda.sqld= position;
  }
  var->sqldata= (char*) data;
  var->sqllen= size;
  var->sqltype= UdmSQLType2IBType(type) + 1;
  var->sqlind= &flag0;
  ib->long_data_lengths[position - 1]= (size_t) size;
  return UDM_OK;
}


UDM_SQLDB_HANDLER udm_sqldb_ibase_handler =
{
  UdmSQLEscStrGeneric,
  UdmIBaseQuery,
  UdmIBaseConnect,
  UdmIBaseClose,
  UdmIBBegin,
  UdmIBCommit,
  UdmIBPrepare,
  UdmIBBind,
  UdmIBExec,
  UdmIBStmtFree,
  UdmSQLFetchRowSimple,
  UdmSQLStoreResultSimple,
  UdmSQLFreeResultSimple,
  UdmIBaseQuery,
  NULL, /* RenameTable    */
  NULL, /* CopyStrucuture */
  UdmSQLLockOrBeginGeneric,
  UdmSQLUnlockOrCommitGeneric,
};

#endif
