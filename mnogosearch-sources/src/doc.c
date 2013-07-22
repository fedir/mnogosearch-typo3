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
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#include "udm_common.h"
#include "udm_doc.h"
#include "udm_utils.h"
#include "udm_doc.h"
#include "udm_word.h"
#include "udm_crossword.h"
#include "udm_hrefs.h"
#include "udm_guesser.h"
#include "udm_vars.h"
#include "udm_textlist.h"
#include "udm_parsehtml.h"
#include "udm_xmalloc.h"
#include "udm_url.h"

UDM_DOCUMENT * __UDMCALL UdmDocInit(UDM_DOCUMENT * Doc)
{
  if (!Doc)
  {
    Doc= (UDM_DOCUMENT*) UdmMalloc(sizeof(UDM_DOCUMENT));
    bzero((void*)Doc, sizeof(UDM_DOCUMENT));
    Doc->freeme=1;
  }
  else
  {
    bzero((void*)Doc, sizeof(UDM_DOCUMENT));
  }
  Doc->Spider.read_timeout= UDM_READ_TIMEOUT;
  Doc->Spider.doc_timeout= UDM_DOC_TIMEOUT;
  Doc->Spider.net_error_delay_time= UDM_DEFAULT_NET_ERROR_DELAY_TIME;
  Doc->connp.connp= &Doc->connp2;
  UdmURLInit(&Doc->CurURL);
  return Doc;
}


void __UDMCALL UdmDocFree(UDM_DOCUMENT *Result)
{
  if(!Result)return;
  UDM_FREE(Result->Buf.buf);
  UDM_FREE(Result->connp.hostname);
  UDM_FREE(Result->connp.user);
  UDM_FREE(Result->connp.pass);
  
  UdmHrefListFree(&Result->Hrefs);
  UdmWordListFree(&Result->Words);
  UdmCrossListFree(&Result->CrossWords);
  UdmVarListFree(&Result->RequestHeaders);
  UdmVarListFree(&Result->Sections);
  UdmTextListFree(&Result->TextList);
  
  UdmURLFree(&Result->CurURL);

  if(Result->freeme)
  {
    UDM_FREE(Result);
  }
  else
  {
    bzero((void*)Result, sizeof(UDM_DOCUMENT));
  }
}


#define nonul(x)  ((x)?(x):"")

int UdmDocToTextBuf(UDM_DOCUMENT * Doc,char *textbuf,size_t len)
{
  size_t  i;
  char  *end;
  
  textbuf[0]='\0';
  
  udm_snprintf(textbuf, len, "<DOC");
  end=textbuf+strlen(textbuf);
  
  for(i=0;i<Doc->Sections.nvars;i++)
  {
    UDM_VAR  *S=&Doc->Sections.Var[i];
    if(!S->name || !S->val ||!S->val[0])
      continue;
    if(!S->section && 
       strcasecmp(S->name,"ID") &&
       strcasecmp(S->name,"URL") &&
       strcasecmp(S->name,"Status") &&
       strcasecmp(S->name,"Content-Type") &&
       strcasecmp(S->name,"Content-Length") &&
       strcasecmp(S->name,"Content-Language") &&
       strcasecmp(S->name,"Last-Modified") &&
       strcasecmp(S->name,"Tag") &&
       strcasecmp(S->name,"Category"))
      continue;
    
    udm_snprintf(end, len - (end - textbuf), "\t%s=\"%s\"", S->name, S->val);
    end = end + strlen(end);
  }
  if (len - (end - textbuf) > 1) strcpy(end, ">");
  return UDM_OK;
}


int __UDMCALL UdmDocFromTextBuf(UDM_DOCUMENT * Doc,const char *textbuf)
{
  const char  *htok, *last;
  UDM_HTMLTOK  tag;
  size_t    i;
  
  if (textbuf == NULL) return UDM_OK;
  
  UdmHTMLTOKInit(&tag);
  
  htok=UdmHTMLToken(textbuf,&last,&tag);
  
  if(!htok || tag.type!=UDM_HTML_TAG)
    return UDM_OK;
  
  for(i=1;i<tag.ntoks;i++)
  {
    size_t  nlen=tag.toks[i].nlen;
    size_t  vlen=tag.toks[i].vlen;
    char  *name = UdmStrndup(tag.toks[i].name,nlen);
    char  *data = UdmStrndup(tag.toks[i].val,vlen);
    UDM_VAR  Sec;
    bzero((void*)&Sec, sizeof(Sec));
    Sec.name=name;
    Sec.val=data;
    UdmVarListReplace(&Doc->Sections, &Sec);
    UDM_FREE(name);
    UDM_FREE(data);
  }
  return 0;
}
