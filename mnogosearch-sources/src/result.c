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
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#include "udm_common.h"
#include "udm_word.h"
#include "udm_doc.h"
#include "udm_utils.h"
#include "udm_result.h"
#include "udm_parsehtml.h"
#include "udm_parsexml.h"
#include "udm_vars.h"

#include "udm_db.h" /* for UdmURLAction */


UDM_RESULT *UdmResultInit(UDM_RESULT *Res)
{
  if(!Res)
  {
    Res= (UDM_RESULT*)UdmMalloc(sizeof(UDM_RESULT));
    bzero((void*)Res, sizeof(UDM_RESULT));
    Res->freeme=1;
  }
  else
  {
    bzero((void*)Res, sizeof(UDM_RESULT));
  }
  Res->ItemList.items= (UDM_STACK_ITEM*)UdmMalloc(UDM_MAXSTACK * sizeof(UDM_STACK_ITEM));
  Res->ItemList.mitems= UDM_MAXSTACK;
  return Res;
}

void __UDMCALL UdmResultFree(UDM_RESULT * Res)
{
  size_t i;
  if(!Res)return;
  UDM_FREE(Res->ItemList.items);
  if (Res->URLData.Item)
  {
    for (i= 0; i < Res->URLData.nitems; i++)
    {
      UDM_FREE(Res->URLData.Item[i].url);
      UDM_FREE(Res->URLData.Item[i].section);
    } 
    UdmFree(Res->URLData.Item);
  }
  UdmWideWordListFree(&Res->WWList);
  if(Res->Doc)
  {
    for(i=0;i<Res->num_rows;i++)
    {
      UdmDocFree(&Res->Doc[i]);
    }
    UdmFree(Res->Doc);
  }
  if(Res->freeme)
  {
    UDM_FREE(Res);
  }
  else
  {
    bzero((void*)Res, sizeof(*Res));
  }
  return;
}


int UdmResultFromTextBuf(UDM_RESULT *R,char *buf){
	size_t	num_rows=0;
	char	*tok,*lt;
	
	for(tok = udm_strtok_r(buf,"\r\n",&lt); tok; tok = udm_strtok_r(NULL,"\r\n",&lt)) {
		if(!memcmp(tok,"<DOC",4)){
			UDM_DOCUMENT	D;
			UdmDocInit(&D);
			UdmDocFromTextBuf(&D,tok);
			R->Doc=(UDM_DOCUMENT*)UdmRealloc(R->Doc,sizeof(UDM_DOCUMENT)*(R->num_rows+1));
			R->Doc[R->num_rows]=D;
			R->num_rows++;
		}else
		if(!memcmp(tok,"<WRD",4)){
			size_t		i;
			UDM_HTMLTOK	tag;
			const char	*htok,*last;
			UDM_WIDEWORD	*W;
			
			R->WWList.Word=(UDM_WIDEWORD*)UdmRealloc(R->WWList.Word,sizeof(R->WWList.Word[0])*(R->WWList.nwords+1));
			W=&R->WWList.Word[R->WWList.nwords];
			bzero((void*)W, sizeof(*W));
			
			UdmHTMLTOKInit(&tag);
			htok=UdmHTMLToken(tok,&last,&tag);
			
			for(i=0;i<tag.ntoks;i++){
				size_t  nlen=tag.toks[i].nlen;
				size_t  vlen=tag.toks[i].vlen;
				char	*name = UdmStrndup(tag.toks[i].name,nlen);
				char	*data = UdmStrndup(tag.toks[i].val,vlen);
				if(!strcmp(name,"word")){
					W->word = (char*)UdmStrdup(data);
				}else
				if(!strcmp(name,"order")){
					W->order=atoi(data);
				}else
				if(!strcmp(name,"count")){
					W->count=atoi(data);
				}else
				if(!strcmp(name,"origin")){
					W->origin=atoi(data);
				}
				UDM_FREE(name);
				UDM_FREE(data);
			}
			R->WWList.nwords++;
		}else{
			size_t		i;
			UDM_HTMLTOK	tag;
			const char	*htok,*last;
			
			UdmHTMLTOKInit(&tag);
			htok=UdmHTMLToken(tok,&last,&tag);
			
			for(i=0;i<tag.ntoks;i++){
				size_t  nlen=tag.toks[i].nlen;
				size_t  vlen=tag.toks[i].vlen;
				char	*name = UdmStrndup(tag.toks[i].name,nlen);
				char	*data = UdmStrndup(tag.toks[i].val,vlen);
				if(!strcmp(name,"first")){
					R->first=atoi(data);
				}else
				if(!strcmp(name,"last")){
					R->last=atoi(data);
				}else
				if(!strcmp(name,"count")){
					R->total_found=atoi(data);
				}else
				if(!strcmp(name,"rows")){
					num_rows=atoi(data);
				}
				UDM_FREE(name);
				UDM_FREE(data);
			}
		}
	}
	return UDM_OK;
}


int UdmResultToTextBuf(UDM_RESULT *R,char *buf,size_t len){
	char	*end=buf;
	size_t	i;
	
	end+=sprintf(end,"<RES\ttotal=\"%d\"\trows=\"%d\"\tfirst=\"%d\"\tlast=\"%d\">\n", R->total_found, R->num_rows, R->first, R->last);
	
	for (i = 0; i< R->WWList.nwords; i++) {
		UDM_WIDEWORD	*W=&R->WWList.Word[i];
		end+=sprintf(end,"<WRD\tword=\"%s\"\torder=\"%d\"\tcount=\"%d\"\torigin=\"%d\">\n",
			W->word,W->order,W->count,W->origin);
	}
	
	for(i=0;i<R->num_rows;i++){
		UDM_DOCUMENT	*D=&R->Doc[i];
		size_t		nsec;
		
		for(nsec=0;nsec<D->Sections.nvars;nsec++)
			D->Sections.Var[nsec].section=1;
		
		UdmDocToTextBuf(D,end,len-1);
		end+=strlen(end);
		*end='\n';
		end++;
	}
	return UDM_OK;
}


/*****************************************************************/

/*
  This function should probably be moved to var.c
*/
static int
UdmVarListReplaceOrAppendStrn(UDM_VARLIST *vars, const char *name,
                              const char *val, size_t len, int hl)
{
  UDM_VAR *v= UdmVarListFind(vars, name);
  
  if (!v)
  {
    UdmVarListReplaceStr(vars, name, "");
    v= UdmVarListFind(vars, name);
    if (!v)
      return UDM_ERROR;
  }
  
  /* Append new piece */
  if (hl)
  {
    v->flags|= UDM_VARFLAG_HL;
    UdmVarAppendStrn(v, "\2", 1);
  }
  UdmVarAppendStrn(v, val, len);
  if (hl)
    UdmVarAppendStrn(v, "\3", 1);
  return UDM_OK;
}


/*****************************************************************/
#define UDM_DF_SIZE 64

typedef struct
{
  int state;
  UDM_AGENT *Agent;
  UDM_WIDEWORD WW;
  UDM_DOCUMENT Doc;
  UDM_RESULT *Res;
  UDM_CHARSET *cs;
  uint4       score;
  uint4       per_site;
  urlid_t     site_id;
  char date_format[UDM_DF_SIZE];
} RES_PARSER_DATA;


static int
PD_ReplaceOrAppendStrn_hl0(RES_PARSER_DATA *D,
                        const char *name,
                        const char *val, size_t len)
{
  return UdmVarListReplaceOrAppendStrn(&D->Doc.Sections, name, val, len, 0);
}


static int
PD_ReplaceOrAppendStrn_hl1(RES_PARSER_DATA *D,
                           const char *name,
                           const char *val, size_t len)
{
  return UdmVarListReplaceOrAppendStrn(&D->Doc.Sections, name, val, len, 1);
}


static int
PD_ReplaceStrn(RES_PARSER_DATA *D,
               const char *name,
               const char *val, size_t len)
{
  return UdmVarListReplaceStrn(&D->Doc.Sections, name, val, len);
}


static int
PD_ReplaceLastModified(RES_PARSER_DATA *D,
                       const char *name,
                       const char *val, size_t len)
{
  time_t last_mod_time;
  char tmp[64];
  len= len >= sizeof(tmp) ? sizeof(tmp) - 1 : len;
  memcpy(tmp, val, len);
  tmp[len]= '\0';
  last_mod_time= UdmHttpDate2Time_t(tmp);
  UdmVarListReplaceInt(&D->Doc.Sections, "Last-Modified-Timestamp", (int) last_mod_time);
  if ((len= strftime(tmp, sizeof(tmp)-1, D->date_format, localtime(&last_mod_time))))
    tmp[len]= '\0';
  else
    UdmTime_t2HttpStr(last_mod_time, tmp);
  UdmVarListReplaceStr(&D->Doc.Sections, "Last-Modified", tmp);
  return UDM_OK;
}


typedef int
(*section_handler)(RES_PARSER_DATA *D,
                   const char *name,
                   const char *val, size_t len);




#define RES_MISC 0

struct udm_res_section_st
{
  int        state;
  size_t     length;
  const char *str;
  const char *section_name;
  section_handler handler;
};

#define UDM_RESSEC_TOTAL_RESULTS       100

#define UDM_RESSEC_WORD                1000
#define UDM_RESSEC_WORD_ID             1001
#define UDM_RESSEC_WORD_ORDER          1002
#define UDM_RESSEC_WORD_COUNT          1003
#define UDM_RESSEC_WORD_ORIGIN         1004
#define UDM_RESSEC_WORD_WEIGHT         1005
#define UDM_RESSEC_WORD_MATCH          1006
#define UDM_RESSEC_WORD_SECNO          1007
#define UDM_RESSEC_WORD_PHRLEN         1008
#define UDM_RESSEC_WORD_PHRPOS         1009
#define UDM_RESSEC_WORD_WORD           1010

#define UDM_RESSEC_ITEM                2000
#define UDM_RESSEC_ITEM_DESCR          2002
#define UDM_RESSEC_ITEM_SCORE          2005

#define UDM_RESSEC_ITEM_PERSITE        2011
#define UDM_RESSEC_ITEM_SITE_ID        2012
#define UDM_RESSEC_ITEM_DESCR_B        2014

#define WSL "rss.channel.mnoGoSearch:WordStatList"
#define WSI "rss.channel.mnoGoSearch:WordStatList.mnoGoSearch:WordStatItem"
#define WSI_LEN 61
#define RCI(x) ("rss.channel.item." x)

static struct udm_res_section_st res_sec[]=
{
  {UDM_RESSEC_WORD,                  20, "result.wordinfo.word",        NULL, NULL},
  {UDM_RESSEC_WORD_ID,               23, "result.wordinfo.word.id",     NULL, NULL},
  {UDM_RESSEC_WORD_ORDER,            26, "result.wordinfo.word.order",  NULL, NULL},
  {UDM_RESSEC_WORD_COUNT,            26, "result.wordinfo.word.count",  NULL, NULL},
  {UDM_RESSEC_WORD_ORIGIN,           27, "result.wordinfo.word.origin", NULL, NULL},
  {UDM_RESSEC_WORD_WEIGHT,           27, "result.wordinfo.word.weight", NULL, NULL},
  {UDM_RESSEC_WORD_MATCH,            26, "result.wordinfo.word.match",  NULL, NULL},
  {UDM_RESSEC_WORD_SECNO,            26, "result.wordinfo.word.secno",  NULL, NULL},
  {UDM_RESSEC_WORD_PHRLEN,           27, "result.wordinfo.word.phrlen", NULL, NULL},
  {UDM_RESSEC_WORD_PHRPOS,           27, "result.wordinfo.word.phrpos", NULL, NULL},
  {UDM_RESSEC_TOTAL_RESULTS,         19, "result.totalResults",         NULL, NULL},
  
  {UDM_RESSEC_WORD,                  WSI_LEN,     WSI,            NULL, NULL},
  {UDM_RESSEC_WORD_ID,               WSI_LEN + 3, WSI ".id",      NULL, NULL},
  {UDM_RESSEC_WORD_WORD,             WSI_LEN + 5, WSI ".word",    NULL, NULL},
  {UDM_RESSEC_WORD_ORDER,            WSI_LEN + 6, WSI ".order",   NULL, NULL},
  {UDM_RESSEC_WORD_COUNT,            WSI_LEN + 6, WSI ".count",   NULL, NULL},
  {UDM_RESSEC_WORD_ORIGIN,           WSI_LEN + 7, WSI ".origin",  NULL, NULL},
  {UDM_RESSEC_WORD_WEIGHT,           WSI_LEN + 7, WSI ".weight",  NULL, NULL},
  {UDM_RESSEC_WORD_MATCH,            WSI_LEN + 6, WSI ".match",   NULL, NULL},
  {UDM_RESSEC_WORD_SECNO,            WSI_LEN + 6, WSI ".secno",   NULL, NULL},
  {UDM_RESSEC_WORD_PHRLEN,           WSI_LEN + 6, WSI ".phrlen",  NULL, NULL},
  {UDM_RESSEC_WORD_PHRPOS,           WSI_LEN + 6, WSI ".phrpos",  NULL, NULL},

  {UDM_RESSEC_TOTAL_RESULTS,         35, "rss.channel.openSearch:totalResults"},
  
  {UDM_RESSEC_ITEM,                  16, "rss.channel.item",                NULL, NULL},

  {UDM_RESSEC_ITEM_SCORE,            22, "rss.channel.item.score",          NULL, NULL},
  {UDM_RESSEC_ITEM_PERSITE,          24, "rss.channel.item.persite",        NULL, NULL},
  {UDM_RESSEC_ITEM_SITE_ID,          23, "rss.channel.item.siteid",         NULL, NULL},

  /* Duplicate: */
  {0, 24, RCI("updated"),         "Last-Modified",    PD_ReplaceLastModified},
  {0, 24, RCI("pubDate"),         "Last-Modified",    PD_ReplaceLastModified},
  {0, 30, RCI("last-modified"),   "Last-Modified",    PD_ReplaceLastModified},

  {0, 22, RCI("title"),           "title",            PD_ReplaceOrAppendStrn_hl0},
  {0, 24, RCI("title.b"),         "title",            PD_ReplaceOrAppendStrn_hl1},
  
  /* Duplicate: */
  {0, 21, RCI("body"),            "body",             PD_ReplaceOrAppendStrn_hl0},
  {0, 28, RCI("description"),     "body",             PD_ReplaceOrAppendStrn_hl0},
  
  {0, 30, RCI("description.b"),   "body",             PD_ReplaceOrAppendStrn_hl1},
  
  /* Duplicate: */
  {0, 21, RCI("link"),            "url",              PD_ReplaceStrn},
  {0, 20, RCI("url"),             "url",              PD_ReplaceStrn},
  
  {0, 19, RCI("id"),              "id",               PD_ReplaceStrn}, 
  {0, 31, RCI("content-length"),  "Content-Length",   PD_ReplaceStrn},
  {0, 29, RCI("content-type"),    "Content-Type",     PD_ReplaceStrn},
  {0, 23, RCI("cached"),          "stored_href",      PD_ReplaceStrn},
  
  /* Duplicate: */
  {0, 33, RCI("CachedCopyBase64"),"CachedCopyBase64", PD_ReplaceStrn},
  {0, 31, RCI("cached-content"),  "CachedCopyBase64", PD_ReplaceStrn}, 
  
  {0, 20, RCI("tag"),             "tag",              PD_ReplaceStrn},
  {0, 22, RCI("crc32"),           "crc32",            PD_ReplaceStrn},
  {0, 24, RCI("charset"),         "charset",          PD_ReplaceStrn},
  {0, 23, RCI("status"),          "status",           PD_ReplaceStrn},

  {0, 0, NULL, NULL, NULL}
};


static struct udm_res_section_st *res_sec_find(const char *attr, size_t len)
{
  struct udm_res_section_st *s;
  for (s= res_sec; s->str; s++)
  {
    if (len == s->length && !strncasecmp(attr, s->str, len))
      return s;
  }
  return NULL;
}

                    
static int
ResFromXMLEnter(UDM_XML_PARSER *parser, const char *name, size_t l)
{
  RES_PARSER_DATA *D = parser->user_data;
  struct udm_res_section_st *st= res_sec_find(parser->attr,
                                              parser->attrend - parser->attr);
  D->state= st ? st->state : 0;
  if (D->state == UDM_RESSEC_WORD)
  {
    UdmWideWordInit(&D->WW);
    D->WW.origin= UDM_WORD_ORIGIN_QUERY;
  }
  if (D->state == UDM_RESSEC_ITEM)
  {
    char dbuf[128];
    UdmDocInit(&D->Doc);
    snprintf(dbuf, 128, "%.5f", (float) 0);
    UdmVarListReplaceStr(&D->Doc.Sections, "Pop_Rank", dbuf);
  }
  return(UDM_XML_OK);
}


static int
ResFromXMLAddDocHook(UDM_XML_PARSER *parser)
{
  RES_PARSER_DATA *D= parser->user_data;
  size_t nbytes;
  D->Res->URLData.nitems++;
  D->Res->num_rows++;
  nbytes= D->Res->num_rows * sizeof(UDM_DOCUMENT);
  D->Res->Doc= (UDM_DOCUMENT*) UdmRealloc(D->Res->Doc, nbytes);
  D->Res->Doc[D->Res->num_rows-1]= D->Doc;
  bzero((void*)&D->Doc, sizeof(UDM_DOCUMENT));
  
  nbytes= D->Res->num_rows * sizeof(UDM_URLDATA);
  D->Res->URLData.Item= (UDM_URLDATA*) UdmRealloc(D->Res->URLData.Item, nbytes);
  bzero((void*)&D->Res->URLData.Item[D->Res->num_rows-1], sizeof(UDM_URLDATA));
  D->Res->URLData.Item[D->Res->num_rows-1].url_id= D->Res->URLData.nitems-1;
  D->Res->URLData.Item[D->Res->num_rows-1].score= D->score;
  D->Res->URLData.Item[D->Res->num_rows-1].per_site= D->per_site;
  D->Res->URLData.Item[D->Res->num_rows-1].site_id= D->site_id;
  
  D->score= 0;
  D->per_site= 0;
  D->site_id= 0;
  return UDM_OK;
}

static int
ResFromXMLAddDocHookImport(UDM_XML_PARSER *parser)
{
  RES_PARSER_DATA *Data= parser->user_data;
  UDM_DOCUMENT *D= &Data->Doc;
  UdmURLAction(Data->Agent, D, UDM_URL_ACTION_RESTOREDATA);
  UdmVarListFree(&D->Sections);
  return UDM_OK;
}

static int
ResFromXMLLeave(UDM_XML_PARSER *parser, const char *name, size_t l)
{
  RES_PARSER_DATA *D= parser->user_data;
  struct udm_res_section_st *st= res_sec_find(parser->attr,
                                              parser->attrend - parser->attr);
  D->state= st ? st->state : 0;

  if (D->state == UDM_RESSEC_WORD)
  {
    if (!D->WW.word)
    {
      D->WW.word= UdmStrdup("<empty>");
      D->WW.len= 7;
    }
    UdmWideWordListAddForStat(&D->Res->WWList, &D->WW);
    UdmWideWordFree(&D->WW);
  }
  if (D->state == UDM_RESSEC_ITEM)
  {
    if (D->Res)
      ResFromXMLAddDocHook(parser);
    else
      ResFromXMLAddDocHookImport(parser);
  }
  /* fprintf(stderr, "leave: len=%d '%s'\n", l, name);*/
  return(UDM_XML_OK);
}


static double
udm_strntod(const char *s, size_t len)
{
  char tmp[64];
  len= len >= sizeof(tmp) ? sizeof(tmp) - 1 : len;
  memcpy(tmp, s, len);
  tmp[len]= 0;
  return atof(tmp);
}


static int ResFromXMLValue(UDM_XML_PARSER *parser, const char *s, size_t len)
{
  RES_PARSER_DATA *D= parser->user_data;
  struct udm_res_section_st *st= res_sec_find(parser->attr,
                                              parser->attrend - parser->attr);
  if (!st)
  {
    /* Add user defined tags */
    if (!strncasecmp(parser->attr, "rss.channel.item.", 17))
      UdmVarListReplaceStrn(&D->Doc.Sections, parser->attr + 17, s, len);
    return UDM_XML_OK;
  }
  
  if (st->handler)
  {
    st->handler(D, st->section_name, s, len);
    return UDM_XML_OK;
  }
  D->state= st->state;
  switch (D->state)
  {
    case  UDM_RESSEC_WORD         :
      UdmFree(D->WW.word);
      D->WW.word= UdmStrndup(s, len);
      D->WW.len= len;
      break;
    case  UDM_RESSEC_WORD_WORD    :
      UdmFree(D->WW.word);
      D->WW.word= UdmStrndup(s, len);
      D->WW.len= len;
      break;
    case  UDM_RESSEC_WORD_ID      :
      break;
    case  UDM_RESSEC_WORD_ORDER   :
      D->WW.order= atoi(s);
      break;
    case  UDM_RESSEC_WORD_COUNT   :
      D->WW.count= atoi(s);
      break;
    case  UDM_RESSEC_WORD_ORIGIN  :
      D->WW.origin= atoi(s);
      break;
    case  UDM_RESSEC_WORD_WEIGHT  :
      D->WW.weight= atoi(s);
      break;
    case  UDM_RESSEC_WORD_MATCH   :
      D->WW.match= atoi(s);
      break;
    case  UDM_RESSEC_WORD_SECNO   :
      D->WW.secno= atoi(s);
      break;
    case  UDM_RESSEC_WORD_PHRLEN  :
      D->WW.phrlen= atoi(s);
      break;
    case  UDM_RESSEC_WORD_PHRPOS  :
      D->WW.phrpos= atoi(s);
      break;
    case UDM_RESSEC_ITEM_PERSITE:
      D->per_site= udm_strntod(s, len);
      break;
    case UDM_RESSEC_ITEM_SITE_ID:
      D->site_id= udm_strntod(s, len);
      UdmVarListReplaceStrn(&D->Doc.Sections, "Site_id", s, len);
      break;
    case UDM_RESSEC_ITEM_SCORE:
      D->score= udm_strntod(s, len) * 1000 + 0.5;
      break;
    case UDM_RESSEC_TOTAL_RESULTS:
      D->Res->total_found= atoi(s);
      break;
  }
  /*fprintf(stderr, "UdmXMLValue: st=%d '%.*s' name='%s'\n", D->state, len, s, parser->attr);*/
  return(UDM_XML_OK);
}


int
UdmResultFromXML(UDM_AGENT *A, UDM_RESULT *Res,
                 const char *str, size_t length, UDM_CHARSET *cs)
{
  int res= UDM_OK;
  RES_PARSER_DATA Data;
  UDM_XML_PARSER parser;
  const char *date_format= UdmVarListFindStr(&A->Conf->Vars, "DateFormat",
                                             "%a, %d %b %Y, %X %Z");
  UdmXMLParserCreate(&parser);
  parser.flags |= UDM_XML_SKIP_TEXT_NORMALIZATION;
  bzero(&Data, sizeof(Data));
  Data.Agent= A;
  Data.Res= Res;
  Data.cs= cs;
  udm_snprintf(Data.date_format, UDM_DF_SIZE, "%s", date_format);

  UdmXMLSetUserData(&parser, &Data);
  UdmXMLSetEnterHandler(&parser, ResFromXMLEnter);
  UdmXMLSetLeaveHandler(&parser, ResFromXMLLeave);
  UdmXMLSetValueHandler(&parser, ResFromXMLValue);

  if (UdmXMLParser(&parser, str, length) == UDM_XML_ERROR)
  {
    char err[256];    
    udm_snprintf(err, sizeof(err), 
                 "XML parsing error: %s at line %d pos %d\n",
                  UdmXMLErrorString(&parser),
                  UdmXMLErrorLineno(&parser),
                  UdmXMLErrorPos(&parser));
    res= UDM_ERROR;
  }

  UdmXMLParserFree(&parser);
  return res;
}
