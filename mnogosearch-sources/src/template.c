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
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>

#include "udmsearch.h"
#include "udm_db.h"
#include "udm_http.h"
#include "udm_parsehtml.h"
#include "udm_host.h"
#include "udm_contentencoding.h"
#include "udm_utils.h"
#include "udm_wild.h"

/******************* Template functions ********************/


static size_t out_string(FILE * stream, char * dst, size_t dst_len, const char * src)
{
  if(src)
  {
    if(stream)
      fputs(src,stream);
    if(dst)
    {
      strncat(dst, src, dst_len - 1);
      return(strlen(src));
    }
  }
  return(0);
}

static char * HiLightDup(const char * src,const char * beg, const char * end)
{
  size_t len=1;
  size_t blen=strlen(beg);
  size_t elen=strlen(end);
  const char * s;
  char  * res, *d;
  
  for(s=src;*s;s++)
  {
    switch(*s)
    {
      case '\2':
        len+=blen;
        break;
      case '\3':
        len+=elen;
        break;
      default:
        len++;
    }
  }
  res = (char*)UdmMalloc(len);
  for(s=src,d=res;*s;s++)
  {
    switch(*s)
    {
      case '\2':
        strcpy(d,beg);
        d+=blen;
        break;
      case '\3':
        strcpy(d,end);
        d+=elen;
        break;
      default:
        *d=*s;
        d++;
    }
  }
  *d='\0';
  return(res);
}


static char *UdmConvDup(const char *src,
                        UDM_CHARSET *from, 
                        UDM_CHARSET *to, int fl)
{
  UDM_CONV cnv;
  size_t len;
  char *dst;
  len= strlen(src);
  dst= (char*) UdmMalloc(len * 12 + 1);
  UdmConvInit(&cnv, from, to, fl);
  UdmConv(&cnv, dst, len * 12 + 1, src, len + 1);
  return dst;
}


#define UDM_TEMPLATE_BASE64 0xB64

size_t UdmTemplatePrintVar(UDM_ENV *Env, 
                           FILE *stream, char *dst, size_t dstlen,
                           const char *value, int type,
                           const char *HlBeg,
                           const char *HlEnd)
{
  char *eval= NULL;
  size_t res= 0;
  
  switch(type)
  {
    case '(': 
#ifndef NO_ADVANCE_CONV
      eval = UdmRemoveHiLightDup(value);
      res= out_string(stream, dst, dstlen, eval);
#else
      res= out_string(stream, dst, dstlen, value);
#endif
      break;
    case '&':
      {
#ifndef NO_ADVANCE_CONV
        char *cval= UdmConvDup(value, Env->bcs, Env->bcs, UDM_RECODE_HTML);
#else
        char *cval= UdmConvDup(value, Env->lcs, Env->bcs, UDM_RECODE_HTML);
#endif
        eval= HiLightDup(cval,HlBeg,HlEnd);
        UDM_FREE(cval);
      }
      res= out_string(stream, dst, dstlen, eval);
      break;
    case '^':
      eval=HiLightDup(value,HlBeg,HlEnd);
      res= out_string(stream, dst, dstlen, eval);
      break;
    case '%':
      eval= (char*)UdmMalloc(strlen(value) * 3 + 1);
      UdmEscapeURL(eval,value);
      res= out_string(stream, dst, dstlen, eval);
      break;
    case UDM_TEMPLATE_BASE64:
      {
        size_t len= strlen(value);
        eval= (char*)UdmMalloc(BASE64_LEN(len));
        udm_base64_encode(value, eval, len);
        res= out_string(stream, dst, dstlen, eval);
      }
  }
  UDM_FREE(eval);
  return res;
}


/*
  Html-aware LEFT() function:
  Finds byte length of "pos" characters in a safe way,
  not to break entities like &#1234; or &acute; in the middle.
*/
static size_t
UdmHtmlStrLeft(const char *str, size_t length, size_t pos)
{
  const char *p= str + pos;
  const char *b;
  
  if (pos > length)
    return length;
  
  /*
    '&' '#' alnum* ';'
  */
  
  for (b= (p > str && *p == ';') ? p - 1 : p ;
       isalnum(*b) && b > str ; b--) /* nop */;
  
  /* Check if we found a valid entity beginning */
  if (*b == '&' || (*b == '#' && b > str && *--b == '&'))
  {
    const char *strend= str + length;
    const char *e;
    for (e= (p < strend && (*p == '#' || *p == '&')) ? p + 1 : p ;
         e < strend && isalnum(*e) ; e++) /* nop */;
    if (e < strend && *e == ';')
      p= b;
  }
  return p - str;
}


static size_t PrintTextTemplate(UDM_AGENT *A, FILE *stream, char *dst, 
                                size_t dst_len, UDM_VARLIST *vars,
                                const char *templ,
                                const char *HlBeg,
                                const char *HlEnd)
{
  const char * s;
  size_t dlen=0;
  
  if (dst)
    *dst= '\0'; /* out_string uses strcat */
  for(s=templ; (*s) && ((stream) || (dlen < dst_len)); s++)
  {
    int type= 0, neg= 0;
    char *value=NULL, *newvalue = NULL;
    char empty[]="";
    size_t maxlen=0;
    size_t curlen=0;

    if(*s=='$')
    {
      const char * vbeg=NULL, * vend;
      int pcount=0;
      
      if(!strncmp(s,"$(",2))
      {
        vbeg=s+2;
        type='(';
      }
      else if(!strncmp(s,"$%(",3))
      {
        vbeg=s+3;
        type='%';
      }
      else if(!strncmp(s,"$&(",3))
      {
        vbeg=s+3;
        type='&';
      }
      else if(!strncmp(s,"$^(",3))
      {
        vbeg=s+3;
        type='^';
      }
      else if (!strncasecmp(s, "$base64(", 8))
      {
        vbeg= s + 8;
        type= UDM_TEMPLATE_BASE64;
      }
      
      for (vend=s; vend[0]; vend++)
      {
        if(vend[0]=='(')
          pcount++;
        if(vend[0]==')' && (--pcount)==0)
          break;
      }
      
      if(type && vend[0])
      {
        UDM_VAR * var;
        size_t len;
        char name[100]="";
        char * sem;
        
        len=(vend-vbeg);
        if(len>=sizeof(name))len=sizeof(name)-1;
        strncpy(name,vbeg,len);name[len]='\0';
        if((sem=strchr(name,':')))
        {
          *sem++= 0;
          maxlen= atoi((neg= (*sem == '-')) ? sem + 1 : sem);
        }
        
        if ((A->doccount == 0) && !strcasecmp(name, "ndocs"))
        {
          UdmURLAction(A, NULL, UDM_URL_ACTION_DOCCOUNT);
          UdmVarListReplaceInt(vars, "ndocs", A->doccount);
        }

        if((var=UdmVarListFind(vars,name)))
        {
          switch(type)
          {
            case '&':
            case '^': 
              value= UdmStrdup(var->val);
              break;
            default:  value= var->val ? UdmRemoveHiLightDup(var->val) : NULL;
          }
          if(!value)
            value= empty;
        }
        else
        {
          value= empty;
        }
        s=vend;
      }else
      {
        type=0;
      }
    }
    if(!value)
      value=empty;
    curlen=strlen(value);
    
    if((curlen>maxlen)&&(maxlen>0))
    {
      if ((newvalue= (char*) UdmMalloc(maxlen + 8)))
      {
        if (!neg)
        {
          char *c2, *c3;
          size_t pos= UdmHtmlStrLeft(value, curlen, maxlen);
          strncpy(newvalue, value, pos);
          newvalue[pos] = '\0';
          c2 = strrchr(newvalue, '\2');
          c3 = strrchr(newvalue, '\3');
          if ((c2 != NULL) && ((c3 == NULL) || (c2 > c3)))
          {
            strcpy(newvalue + pos, "\3...");
          }
          else
          {
            strcpy(newvalue + pos, "...");
          }
        }
        else
        {
          char *c2, *c3;
          size_t pos= UdmHtmlStrLeft(value, curlen, curlen - maxlen);
          size_t len= curlen - pos;
          size_t offs= 0;
          c2= strchr(value + pos, '\2');
          c3= strchr(value + pos, '\3');
          if (c3 && (!c2 || c2 > c3))
          {
            *newvalue= '\2';
            offs= 1;
          }
          memcpy(newvalue + offs, value + pos, len);
          newvalue[offs + len]= '\0';
        }
        if (value != empty) UDM_FREE(value);
        value= newvalue;
      }
    }
    
    if (type)
    {
      dlen+= UdmTemplatePrintVar(A->Conf, stream, dst + dlen, dst_len - dlen, 
                                 value, type, HlBeg, HlEnd);
    }
    else
    {
      if((stream)&&(*s))fputc(*s,stream);
      if(dst)
      {
        dst[dlen++]= *s;
        dst[dlen]= '\0';
      }
    }
    
    if (value != empty) UDM_FREE(value);
  }
  return dlen;
}


#if 0
static char *
GetHtmlTok(const char *src, const char **last, UDM_HTMLTOK *t, size_t *length)
{
  const char *beg= UdmHTMLToken(src, last, t);
  if (beg)
  {
    *length= last[0] - beg;
    return UdmStrndup(beg, *length);
  }
  *length= 0;
  return NULL;
}
#endif


#define UDM_TMPL_IF		1
#define UDM_TMPL_IFLIKE		2
#define UDM_TMPL_ELSEIF		3
#define UDM_TMPL_ELSELIKE	4
#define UDM_TMPL_ELSE		5
#define UDM_TMPL_ENDIF		6
#define UDM_TMPL_COPY		7
#define UDM_TMPL_SET		8
#define UDM_TMPL_INCLUDE	9
#define UDM_TMPL_CMP		10
#define UDM_TMPL_WLD		11
#define UDM_TMPL_JE		12
#define UDM_TMPL_JNE		13
#define UDM_TMPL_JMP		14
#define UDM_TMPL_PRINT		15
#define UDM_TMPL_TAG		16
#define UDM_TMPL_IFNOT		17
#define UDM_TMPL_INC		18
#define UDM_TMPL_DEC		19
#define UDM_TMPL_WHILENOT	20
#define UDM_TMPL_WHILE		21
#define UDM_TMPL_ENDWHILE	22
#define UDM_TMPL_PUT		23
#define UDM_TMPL_STRBCUT	24
#define UDM_TMPL_STRPCASE	25
#define UDM_TMPL_STRLEFT	26
#define UDM_TMPL_IFCS		27
#define UDM_TMPL_CMPCS		28
#define UDM_TMPL_URLDECODE	29
#define UDM_TMPL_HTMLENCODE	30
#define UDM_TMPL_UDMENV		31
#define UDM_TMPL_UDMRESULT	32
#define UDM_TMPL_METHOD		33
#define UDM_TMPL_STRLCASE	34
#define UDM_TMPL_ADD		35
#define UDM_TMPL_SUB		36
#define UDM_TMPL_MUL		37
#define UDM_TMPL_EREG		38
#define UDM_TMPL_IFLE		39
#define UDM_TMPL_IFGE		40
#define UDM_TMPL_IFGT		41
#define UDM_TMPL_IFLT		42
#define UDM_TMPL_FOR		43
#define UDM_TMPL_ENDFOR		44

struct udm_template_st;

typedef struct udm_tmpl_names_st
{
  const char *name;
  size_t len;
  int cmdnum;
  size_t nargs;
  int (*exec)(struct udm_template_st *tmpl);
} UDM_TMPL_CMD;


#define TMPL_MAXARG 4

typedef struct udm_tmpl_cmd_st
{
  int cmdnum;
  char *arg[TMPL_MAXARG];
  char *method;
  size_t jump;
  UDM_TMPL_CMD *cmd;
} UDM_TMPL_PRGITEM;


typedef struct udm_tmpl_prg_st
{
  size_t nitems;
  size_t mitems;
  size_t curr;
  int reg;
  UDM_TMPL_PRGITEM *Items;
  char errstr[128];
} UDM_TMPL_PRG;


typedef struct udm_template_st
{
  UDM_AGENT *Agent;
  FILE *stream;
  UDM_VARLIST *vars;
  char *HlBeg;
  char *HlEnd;
  UDM_TMPL_PRG prg;
  char *dst;
  size_t dstlen;
} UDM_TEMPLATE;


static int TemplateCompare(UDM_TEMPLATE *T)
{
  UDM_AGENT *Agent= T->Agent;
  UDM_VARLIST *vars= T->vars;
  UDM_TMPL_PRG *p= &T->prg;  
  UDM_TMPL_PRGITEM *it= &p->Items[p->curr];
  char *nhvar=UdmRemoveHiLightDup(UdmVarListFindStr(vars,it->arg[0],""));
  size_t vurlen = 256 + 4 * strlen(it->arg[1]);
  char  *vurl = (char*)UdmMalloc(vurlen);
  vurl[0]= '\0';
  PrintTextTemplate(Agent, NULL, vurl, vurlen, vars, it->arg[1], T->HlBeg, T->HlEnd);
  
  switch (it->cmdnum)
  {
    case UDM_TMPL_CMP:   p->reg= strcasecmp(nhvar, vurl); break;
    case UDM_TMPL_CMPCS: p->reg= strcmp(nhvar, vurl); break;
    case UDM_TMPL_IFLE:  p->reg= !(atof(nhvar) <= atof(vurl)); break;
    case UDM_TMPL_IFGE:  p->reg= !(atof(nhvar) >= atof(vurl)); break;
    case UDM_TMPL_IFLT:  p->reg= !(atof(nhvar) < atof(vurl)); break;
    case UDM_TMPL_IFGT:  p->reg= !(atof(nhvar) > atof(vurl)); break;
    default:
      p->reg= UdmWildCaseCmp(nhvar, vurl);
  }
  UdmFree(nhvar);
  UdmFree(vurl);
  return UDM_OK;
}


static int TemplateFunc2(UDM_TEMPLATE *T)
{
  UDM_AGENT *Agent= T->Agent;
  UDM_VARLIST *vars= T->vars;
  UDM_TMPL_PRG *p= &T->prg;
  UDM_TMPL_PRGITEM *it= &p->Items[p->curr];
  size_t vurlen = 256 + 4 * strlen(it->arg[1]);
  char  *vurl = (char*)UdmMalloc(vurlen);
  vurl[0]= '\0';
  PrintTextTemplate(Agent, NULL, vurl, vurlen, vars, it->arg[1], T->HlBeg, T->HlEnd);
  
  if (it->cmdnum == UDM_TMPL_STRPCASE)
  {
    char *v;
    for (v= vurl; v[0]; v++)
    {
      /* This will probably need full Unicode support in the future */
      v[0]= v == vurl ? toupper(v[0]) : tolower(v[0]);
    }
  }
  else if (it->cmdnum == UDM_TMPL_STRLCASE)
  {
    char *v;
    for (v= vurl; v[0]; v++)
    {
      v[0]= tolower(v[0]);
    }
  }
  else if (it->cmdnum == UDM_TMPL_STRBCUT)
  {
    if ((vurlen= strlen(vurl)) > 1)
      memmove(vurl, vurl+1, vurlen); /* including trailing null */
    else
      vurl[0]= '\0';
  }
  else if (it->cmdnum == UDM_TMPL_STRLEFT)
  {
    vurl[1]= '\0';
  }
  else if (it->cmdnum == UDM_TMPL_URLDECODE)
  {
    UdmUnescapeCGIQuery(vurl, vurl);
  }
  else if (it->cmdnum == UDM_TMPL_HTMLENCODE)
  {
    size_t len= strlen(vurl);
    size_t enclen= len * 6 + 1;
    char *vurl_encoded= (char*) UdmMalloc(enclen); 
    UdmHTMLEncode(vurl_encoded, enclen, vurl, len + 1);
    UdmFree(vurl);
    vurl= vurl_encoded;
  }
  else if (it->cmdnum == UDM_TMPL_EREG)
  {
    if (it->arg[3] && it->arg[2])
    {
      char errstr[128];
      size_t len= (strlen(vurl)+strlen(it->arg[2])+strlen(it->arg[3])) * 10;
      UDM_MATCH M;
      UDM_MATCH_PART P[10];
      UdmMatchInit(&M);
      M.pattern= it->arg[3];
      M.match_type= UDM_MATCH_REGEX;
      if (!UdmMatchComp(&M, errstr, sizeof(errstr)) &&
          !UdmMatchExec(&M, vurl, strlen(vurl), NULL, 10, P))
      {
        char *res= (char*) UdmMalloc(len);
        res[0]= '\0';
        UdmMatchApply(res, len, vurl, it->arg[2], &M, 10, P);
        UdmFree(vurl);
        vurl= res;
      }
      else
        vurl[0]= '\0';
      M.pattern= 0;
      UdmMatchFree(&M);
    }
    else
      vurl[0]= '\0';
  }

  UdmVarListReplaceStr(vars,it->arg[0], vurl);
  UdmFree(vurl);
  return UDM_OK;
}


static int TemplateSetOrPut(UDM_TEMPLATE *T)
{
  UDM_TMPL_PRGITEM *it= &T->prg.Items[T->prg.curr];
  size_t vurlen = 256 + 4 * strlen(it->arg[1]);
  char  *vurl = (char*)UdmMalloc(vurlen);
  vurl[0]= '\0';
  PrintTextTemplate(T->Agent, NULL, vurl, vurlen, T->vars, it->arg[1], T->HlBeg, T->HlEnd);
  if (it->cmdnum == UDM_TMPL_SET)
    UdmVarListReplaceStr(T->vars,it->arg[0], vurl);
  else
    UdmVarListAddStr(T->vars,it->arg[0], vurl);
  UdmFree(vurl);
  return UDM_OK;  
}


static int TemplateNum2(UDM_TEMPLATE *T)
{
  UDM_TMPL_PRGITEM *it= &T->prg.Items[T->prg.curr];
  size_t vurlen = 256 + 4 * strlen(it->arg[1]);
  char  *vurl = (char*)UdmMalloc(vurlen);
  int val0= UdmVarListFindInt(T->vars, it->arg[0], 0); 
  int val1;
  vurl[0]= '\0';
  PrintTextTemplate(T->Agent, NULL, vurl, vurlen, T->vars, it->arg[1], T->HlBeg, T->HlEnd);
  val1= atoi(vurl);
  switch (it->cmdnum)
  {
    case UDM_TMPL_MUL: val0*= val1; break;
    case UDM_TMPL_ADD: val0+= val1; break;
    case UDM_TMPL_SUB: val0-= val1; break;
  }
  UdmFree(vurl);
  UdmVarListReplaceInt(T->vars,it->arg[0], val0);
  return UDM_OK;  
}


static int TemplateCopy(UDM_TEMPLATE *T)
{
  UDM_TMPL_PRGITEM *it= &T->prg.Items[T->prg.curr];
  const char *val;
  size_t vurlen = 256 + 4 * strlen(it->arg[1]);
  char  *vurl = (char*)UdmMalloc(vurlen);
  vurl[0]= '\0';
  PrintTextTemplate(T->Agent, NULL, vurl, vurlen, T->vars, it->arg[1], T->HlBeg, T->HlEnd);
  val= UdmVarListFindStr(T->vars, vurl, "");
  UdmVarListReplaceStr(T->vars,it->arg[0],val);
  UdmFree(vurl);
  return UDM_OK;
}



static int TemplateIncOrDec(UDM_TEMPLATE *tmpl)
{
  UDM_TMPL_PRGITEM *it= &tmpl->prg.Items[tmpl->prg.curr];
  int val= UdmVarListFindInt(tmpl->vars, it->arg[0], 0);
  switch (it->cmdnum)
  {
    case UDM_TMPL_INC: val++; break;
    case UDM_TMPL_DEC: val--; break;
  }
  UdmVarListReplaceInt(tmpl->vars, it->arg[0], val);
  return UDM_OK;
}


static int CreateArg(UDM_TEMPLATE *tmpl, const char *name, const char *arg)
{
  size_t vallen = 4 * 256 + 4 * strlen(arg);
  char  *val = (char*)UdmMalloc(vallen);
  val[0]= '\0';
  PrintTextTemplate(tmpl->Agent, NULL, val, vallen, tmpl->vars,
                    arg, tmpl->HlBeg, tmpl->HlEnd);
  UdmVarListReplaceStr(tmpl->vars, name, val);
  UdmFree(val);
  return UDM_OK;
}

static int TemplateMethod(UDM_TEMPLATE *tmpl)
{
  UDM_TMPL_PRGITEM *it= &tmpl->prg.Items[tmpl->prg.curr];
  UDM_VAR *var; /* We cannot call UdmVarListFind because
                   UdmVarListReplace will change it */
  UDM_VAR *args[2];
  size_t nargs= 0;
  if (it->arg[1])
  {
    CreateArg(tmpl, "arg1", it->arg[1]);
  }
  if (it->arg[2])
  {
    CreateArg(tmpl, "arg2", it->arg[2]);
  }
  if (it->arg[1])
    args[nargs++]= UdmVarListFind(tmpl->vars, "arg1");
  if (it->arg[2])
    args[nargs++]= UdmVarListFind(tmpl->vars, "arg2");
  var= UdmVarListFind(tmpl->vars, it->arg[0]);
  if (var && it->method)
  {
    UdmVarListInvokeMethod(tmpl->vars, var, it->method, args, nargs);
  }
  if (it->arg[1])
    UdmVarListDel(tmpl->vars, "arg1");
  if (it->arg[2])
    UdmVarListDel(tmpl->vars, "arg2");
  return UDM_OK;
}


/* 
  FIXME: add support to include into buffer
  FIXME: currently stream only is supported
*/

static int TemplateInclude(UDM_TEMPLATE *T)
{
  UDM_AGENT *Agent= T->Agent;
  UDM_VARLIST *vars= T->vars;
  UDM_TMPL_PRG *p= &T->prg;  
  UDM_TMPL_PRGITEM *it= &p->Items[p->curr];
  const char *tag_content= it->arg[1];

  if (!Agent)
    return UDM_OK;
  

  if(it->arg[1])
  {
    UDM_DOCUMENT Inc;
    size_t vurlen = 256 + 4 * strlen(tag_content);
    char  *vurl = (char*)UdmMalloc(vurlen);
    PrintTextTemplate(Agent, NULL, vurl, vurlen, vars, it->arg[1], T->HlBeg, T->HlEnd);

    UdmDocInit(&Inc);
        
    if (UdmGetURLSimple(Agent, &Inc, vurl) == UDM_OK)
    {
      if (Inc.Buf.content)
      {
        if(T->stream)
        {
          fprintf(T->stream,"%s",Inc.Buf.content);
        }
        else
        {
          /* FIXME: add printing to string */
        }
      }
    }
    UdmDocFree(&Inc);
    UDM_FREE(vurl);
  }
  return UDM_OK;
}

static size_t PrintTagTemplate(UDM_AGENT *Agent,FILE *stream,
                               char *dst,size_t dst_len,
                               UDM_VARLIST *vars,const char *tok,
                               const char *HlBeg, 
                               const char *HlEnd)
{
  char * opt;
  UDM_HTMLTOK ltag, *tag = &ltag;
  const char *last;
  UDM_VAR * var=NULL;
  char * vname = NULL, *value = NULL;
  size_t i, res = 0;
  
  opt = (char*)UdmMalloc(strlen(tok) + 200);
  UdmHTMLTOKInit(tag);
  UdmHTMLToken(tok, &last, tag);
  sprintf(opt, "<");

  for (i = 0; i < ltag.ntoks; i++)
  {
    const char *space= (i == 0) ? "" : " ";
    if (ISTAG(i, "selected") && ltag.toks[i].vlen)
    {
      vname = UdmStrndup(ltag.toks[i].val, ltag.toks[i].vlen);
    }
    else if (ISTAG(i, "value"))
    {
      value = UdmStrndup(ltag.toks[i].val, ltag.toks[i].vlen);
      sprintf(UDM_STREND(opt), "%svalue=\"%s\"", space, value);
    }
    else
    {
      char *tname = UdmStrndup(ltag.toks[i].name, ltag.toks[i].nlen);
      if (ltag.toks[i].vlen)
      {
        char *tval = UdmStrndup(ltag.toks[i].val, ltag.toks[i].vlen);
        sprintf(UDM_STREND(opt), "%s%s=\"%s\"", space, tname, tval);
        UDM_FREE(tval);
      }
      else
      {
         sprintf(UDM_STREND(opt), "%s%s", space, tname);
      }
      UDM_FREE(tname);
    }
  }

  if(vname)
  {
    var= UdmVarListFindWithValue(vars, UdmTrim(vname, "$&()"), value ? value:"");
  }

  sprintf(UDM_STREND(opt), "%s>", var ? " selected=\"selected\"":"");

  if (vname) { UDM_FREE(vname); }
  if (value) { UDM_FREE(value); }

  res = PrintTextTemplate(Agent, stream, dst, dst_len, vars, opt, HlBeg, HlEnd);
  UDM_FREE(opt);
  return res;
}


static int TemplateTagOrText(UDM_TEMPLATE *tmpl)
{
  size_t tmplen;
  UDM_TMPL_PRGITEM *it= &tmpl->prg.Items[tmpl->prg.curr];

  switch (it->cmdnum)
  {
      case UDM_TMPL_PRINT:
        
        tmplen= PrintTextTemplate(tmpl->Agent, tmpl->stream,
                                  tmpl->dst , tmpl->dstlen, 
                                  tmpl->vars, it->arg[0],
                                  tmpl->HlBeg, tmpl->HlEnd);
        break;
      
      case UDM_TMPL_TAG:
        tmplen= PrintTagTemplate(tmpl->Agent, tmpl->stream,
                                 tmpl->dst, tmpl->dstlen,
                                 tmpl->vars, it->arg[0],
                                 tmpl->HlBeg, tmpl->HlEnd);
        break;
  }
  tmpl->dst+= tmplen;
  tmpl->dstlen-= tmplen;
  return UDM_OK;
}

static int TemplateCreateEnv(UDM_TEMPLATE *tmpl)
{
  UDM_TMPL_PRGITEM *it= &tmpl->prg.Items[tmpl->prg.curr];
  if (it->arg[1] && !strcasecmp(it->arg[1], "Default"))
  {
    UDM_VAR arg, *args= &arg;
    bzero((void*)&arg, sizeof(arg));
    arg.val= (char*) tmpl->Agent->Conf;
    return UdmVarListCreateObject(tmpl->vars, it->arg[0], UDM_VAR_ENV, &args, 1);
  }
  return UdmVarListCreateObject(tmpl->vars, it->arg[0], UDM_VAR_ENV, NULL, 0);
}

static int TemplateCreateResult(UDM_TEMPLATE *tmpl)
{
  UDM_TMPL_PRGITEM *it= &tmpl->prg.Items[tmpl->prg.curr];
  UDM_VAR *var[2];
  var[0]= UdmVarListFind(tmpl->vars, it->arg[1]);
  UdmVarListReplaceStr(tmpl->vars, "Result", it->arg[2]);
  var[1]= UdmVarListFind(tmpl->vars, "Result");
  return UdmVarListCreateObject(tmpl->vars, it->arg[0], UDM_VAR_RESULT, var, 1);
}


static char
*UdmHTMLTokAttrDup(UDM_HTMLTOK *tag,
                   const char *name,
                   const char *def)
{
  size_t toks;
  for (toks= 0; toks < tag->ntoks; toks++)
  {
    if (tag->toks[toks].name &&
        !strncasecmp(tag->toks[toks].name, name, tag->toks[toks].nlen))
    {
      if (tag->toks[toks].val && tag->toks[toks].vlen)
        return UdmStrndup(tag->toks[toks].val, tag->toks[toks].vlen);
      return def ? UdmStrdup(def) : NULL;
    }
  }
  return def ? UdmStrdup(def) : NULL;
}


UDM_TMPL_CMD tnames[]=
{
  {"IFLIKE",   6, UDM_TMPL_IFLIKE   , 0, NULL },
  {"IFNOT",    5, UDM_TMPL_IFNOT    , 0, NULL },
  {"IFCS",     4, UDM_TMPL_IFCS     , 0, NULL },
  {"IFLE",     4, UDM_TMPL_IFLE     , 0, NULL },
  {"IFGE",     4, UDM_TMPL_IFGE     , 0, NULL },
  {"IFGT",     4, UDM_TMPL_IFGT     , 0, NULL },
  {"IFLT",     4, UDM_TMPL_IFLT     , 0, NULL },
  {"IF",       2, UDM_TMPL_IF       , 0, NULL },
  {"ELSEIF",   6, UDM_TMPL_ELSEIF   , 0, NULL },
  {"ELIF",     4, UDM_TMPL_ELSEIF   , 0, NULL },
  {"ELIKE",    5, UDM_TMPL_ELSELIKE , 0, NULL },
  {"ELSELIKE", 8, UDM_TMPL_ELSELIKE , 0, NULL },
  {"ELSE",     4, UDM_TMPL_ELSE     , 0, NULL },
  {"ENDIF",    5, UDM_TMPL_ENDIF    , 0, NULL },
  {"/IF",      3, UDM_TMPL_ENDIF    , 0, NULL },
  {"WHILENOT", 8, UDM_TMPL_WHILENOT , 0, NULL },
  {"WHILE",    5, UDM_TMPL_WHILE    , 0, NULL },
  {"ENDWHILE", 8, UDM_TMPL_ENDWHILE , 0, NULL },
  {"FOR",      3, UDM_TMPL_FOR      , 0, NULL },
  {"ENDFOR",   6, UDM_TMPL_ENDFOR   , 0, NULL },
  
  {"COPY",     4, UDM_TMPL_COPY     , 2, TemplateCopy },
  {"SET",      3, UDM_TMPL_SET      , 2, TemplateSetOrPut },
  {"PUT",      3, UDM_TMPL_PUT      , 2, TemplateSetOrPut },
  {"INCLUDE",  7, UDM_TMPL_INCLUDE  , 2, TemplateInclude },
  {"INC",      3, UDM_TMPL_INC      , 1, TemplateIncOrDec },
  {"DEC",      3, UDM_TMPL_DEC      , 1, TemplateIncOrDec },
  {"MUL",      3, UDM_TMPL_MUL      , 2, TemplateNum2 },
  {"ADD",      3, UDM_TMPL_ADD      , 2, TemplateNum2 },
  {"SUB",      3, UDM_TMPL_SUB      , 2, TemplateNum2 },
  {"STRBCUT",  7, UDM_TMPL_STRBCUT  , 2, TemplateFunc2 },
  {"STRPCASE", 8, UDM_TMPL_STRPCASE , 2, TemplateFunc2 }, 
  {"STRLCASE", 8, UDM_TMPL_STRLCASE , 2, TemplateFunc2 }, 
  {"STRLEFT",  7, UDM_TMPL_STRLEFT  , 2, TemplateFunc2 },
  {"URLDECODE",9, UDM_TMPL_URLDECODE, 2, TemplateFunc2 },
  {"HTMLENCODE",10, UDM_TMPL_HTMLENCODE, 2, TemplateFunc2 },
  {"UDMENV",   6, UDM_TMPL_UDMENV   , 1, TemplateCreateEnv },
  {"UDMRESULT",9, UDM_TMPL_UDMRESULT, 2, TemplateCreateResult },
  {"EREG",     4, UDM_TMPL_EREG     , 2, TemplateFunc2 },
  { NULL,      0, 0                 , 0, NULL }
};


UDM_TMPL_CMD udm_tmpl_set=
{"SET", 3, UDM_TMPL_SET, 2, TemplateSetOrPut};

UDM_TMPL_CMD udm_tmpl_inc=
{"INC", 3, UDM_TMPL_INC, 1, TemplateIncOrDec};

UDM_TMPL_CMD udm_tmpl_txt=
{"TXT", 3, UDM_TMPL_PRINT, 1, TemplateTagOrText };

UDM_TMPL_CMD udm_tmpl_tag=
{"TAG", 3, UDM_TMPL_TAG, 1, TemplateTagOrText };

UDM_TMPL_CMD udm_tmpl_cmp=
{"CMP", 3, UDM_TMPL_CMP, 2, TemplateCompare };

UDM_TMPL_CMD udm_tmpl_cmplt=
{"CMPLT", 5, UDM_TMPL_IFLT, 2, TemplateCompare };

UDM_TMPL_CMD udm_tmpl_cmple=
{"CMPLE", 5, UDM_TMPL_IFLE, 2, TemplateCompare };

UDM_TMPL_CMD udm_tmpl_cmpgt=
{"CMPGT", 5, UDM_TMPL_IFGT, 2, TemplateCompare };

UDM_TMPL_CMD udm_tmpl_cmpge=
{"CMPGE", 5, UDM_TMPL_IFGE, 2, TemplateCompare };

UDM_TMPL_CMD udm_tmpl_cmpcs=
{"CMPCS", 5, UDM_TMPL_CMPCS, 2, TemplateCompare };

UDM_TMPL_CMD udm_tmpl_wld=
{"CMPCS", 5, UDM_TMPL_WLD, 2, TemplateCompare };

UDM_TMPL_CMD udm_tmpl_method=
{"METHOD", 6, UDM_TMPL_METHOD, 3, TemplateMethod };


static UDM_TMPL_CMD *udm_tmpl_cmpcmd(int cmdnum)
{
  switch (cmdnum)
  {
    case UDM_TMPL_IFCS: return &udm_tmpl_cmpcs;
    case UDM_TMPL_IFLT: return &udm_tmpl_cmplt;
    case UDM_TMPL_IFLE: return &udm_tmpl_cmple;
    case UDM_TMPL_IFGT: return &udm_tmpl_cmpgt;
    case UDM_TMPL_IFGE: return &udm_tmpl_cmpge;  
    default: return &udm_tmpl_cmp;
  }
}

static UDM_TMPL_CMD *HtmlTemplateCmd(const char *str, const char *strend)
{
  UDM_TMPL_CMD *t;
  size_t len;
  for (len= 0; str+len < strend && isalnum(str[len]) ; len++);
  for (t= tnames; t->name; t++)
  {
    if (t->len == len && !strncasecmp(str, t->name, len))
      return t;
  }
  return NULL;
}


typedef struct udm_tmpl_compile_stack_item_st
{
  size_t beg;
  size_t jmp;
  size_t cmd; /* IF FOR WHILE */
} UDM_TMPL_COMPILE_STACK_ITEM;


typedef struct udm_tmpl_compile_stack_st
{
  size_t nitems;
  size_t mitems;
  UDM_TMPL_COMPILE_STACK_ITEM *Items;
} UDM_TMPL_COMPILE_STACK;


static void CompileStackInit(UDM_TMPL_COMPILE_STACK *st)
{
  bzero((void*) st, sizeof(*st));
}


static int CompileStackPush(UDM_TMPL_COMPILE_STACK *st, 
                            UDM_TMPL_COMPILE_STACK_ITEM *item)
{
  if (st->nitems >= st->mitems)
  {
    size_t sz;
    st->mitems+= 16;
    sz= st->mitems * sizeof(UDM_TMPL_COMPILE_STACK_ITEM);
    st->Items= (UDM_TMPL_COMPILE_STACK_ITEM *) UdmRealloc(st->Items, sz);
    if (!st->Items)
      return UDM_ERROR;
  }
  st->Items[st->nitems]= item[0];
  st->nitems++;
  return UDM_OK;
}


static int CompileStackPop(UDM_TMPL_COMPILE_STACK *st)
{
  if (!st->nitems)
    return UDM_ERROR;
  st->nitems--;
  return UDM_OK;
}


static void CompileStackFree(UDM_TMPL_COMPILE_STACK *st)
{
  UDM_FREE(st->Items);
}


static void HtmlTemplatePrgInit(UDM_TMPL_PRG *prg)
{
  bzero((void*)prg, sizeof(*prg));
}


static int HtmlTemplatePrgAdd(UDM_TMPL_PRG *prg, UDM_TMPL_PRGITEM *item)
{
  if (prg->nitems >= prg->mitems)
  {
    prg->mitems+= 64;
    prg->Items= (UDM_TMPL_PRGITEM*) UdmRealloc(prg->Items,
                                               prg->mitems*sizeof(*item));
    if (!prg->Items)
      return UDM_ERROR;
  }
  prg->Items[prg->nitems]= item[0];
  prg->nitems++;
  return UDM_OK;
}


static void HtmlTemplatePrgFree(UDM_TMPL_PRG *prg)
{
  size_t i;
  for (i=0 ; i < prg->nitems; i++)
  {
    UDM_FREE(prg->Items[i].arg[0]);
    UDM_FREE(prg->Items[i].arg[1]);
    UDM_FREE(prg->Items[i].arg[2]);
    UDM_FREE(prg->Items[i].arg[3]);
    UDM_FREE(prg->Items[i].method);
  }
  UDM_FREE(prg->Items);
}


static int HtmlTemplatePrgAddFunc(UDM_TMPL_PRG *prg,
                                  UDM_TMPL_CMD *cmd,
                                  UDM_HTMLTOK *tag)
{
  UDM_TMPL_PRGITEM i;
  i.cmdnum= cmd->cmdnum;
  i.cmd= cmd;

  i.arg[0]= UdmHTMLTokAttrDup(tag, "Name", "");
  if (!(i.arg[1]= UdmHTMLTokAttrDup(tag, "Content", NULL)))
    i.arg[1]= UdmHTMLTokAttrDup(tag, "From", "");
  if (!(i.arg[2]= UdmHTMLTokAttrDup(tag, "Result", NULL)))
    i.arg[2]= UdmHTMLTokAttrDup(tag, "To", "");
  i.arg[3]= UdmHTMLTokAttrDup(tag, "Match", "");
  
  i.method= NULL;
  i.jump= 0;
  return HtmlTemplatePrgAdd(prg, &i);
}


static int HtmlTemplatePrgAdd1Arg(UDM_TMPL_PRG *prg, 
                                  UDM_TMPL_CMD *cmd,
                                  const char *str,
                                  const char *strend)
{
  UDM_TMPL_PRGITEM i;
  i.cmdnum= cmd->cmdnum;
  i.cmd= cmd;
  i.arg[0]= str ? UdmStrndup(str, strend - str) : NULL;
  i.arg[1]= NULL;
  i.arg[2]= NULL;
  i.arg[3]= NULL;
  i.method= NULL;
  i.jump= 0;
  return HtmlTemplatePrgAdd(prg, &i);
}


static int HtmlTemplatePrgAddJmp(UDM_TMPL_PRG *prg, int cmdnum, int addr)
{
  UDM_TMPL_PRGITEM i;
  i.cmdnum= cmdnum;
  i.arg[0]= NULL;
  i.arg[1]= NULL;
  i.arg[2]= NULL;
  i.arg[3]= NULL;
  i.method= NULL;
  i.jump= addr;
  return HtmlTemplatePrgAdd(prg, &i);
}

static void HtmlTemplateErrorUnexpectedSym(UDM_TMPL_PRG *prg, const char *sym)
{
  udm_snprintf(prg->errstr, sizeof(prg->errstr), "Unexpected '%s'", sym);
}


static int HtmlTemplatePrgAddInvokeMethod(UDM_TMPL_PRG *prg,
                                          UDM_TMPL_COMPILE_STACK *st,
                                          UDM_HTMLTOK *tag,
                                          const char *str,
                                          const char *strend)
{
  if (tag->ntoks > 1)
  {
    UDM_TMPL_PRGITEM i;
    i.arg[0]= UdmHTMLTokAttrDup(tag, "Name", "");
    if (i.arg[0][0])
    {
      i.arg[1]= UdmHTMLTokAttrDup(tag, "Content", "");
      i.arg[2]= UdmHTMLTokAttrDup(tag, "Result", "");
      i.arg[3]= UdmHTMLTokAttrDup(tag, "Match", "");
      i.cmdnum= UDM_TMPL_METHOD;
      i.cmd= &udm_tmpl_method;
      i.method= UdmStrndup(tag->toks[0].name+1,tag->toks[0].nlen-1);
      i.jump= 0;
      HtmlTemplatePrgAdd(prg, &i);
      return UDM_OK;
    }
    UdmFree(i.arg[0]);
  }

  /* Just print special tags:  <!DOCTYPE ... > */
  HtmlTemplatePrgAdd1Arg(prg, &udm_tmpl_txt, str, strend);
  return UDM_OK;
}

static int HtmlTemplateCompileCommand(UDM_TMPL_PRG *prg,
                                      UDM_TMPL_COMPILE_STACK *st,
                                      UDM_HTMLTOK *tag,
                                      UDM_TMPL_CMD *cmd)
{
  UDM_TMPL_COMPILE_STACK_ITEM sitem;
  int rc= UDM_OK;

  switch (cmd->cmdnum)
  {
    case UDM_TMPL_WHILE:
    case UDM_TMPL_IF:
    case UDM_TMPL_IFCS:
    case UDM_TMPL_IFLE:
    case UDM_TMPL_IFGE:
    case UDM_TMPL_IFGT:
    case UDM_TMPL_IFLT:
      sitem.beg= prg->nitems;
      sitem.jmp= prg->nitems + 1;
      sitem.cmd= cmd->cmdnum == UDM_TMPL_WHILE ? UDM_TMPL_WHILE : UDM_TMPL_IF;
      CompileStackPush(st, &sitem);
      HtmlTemplatePrgAddFunc(prg, udm_tmpl_cmpcmd(cmd->cmdnum), tag);
      HtmlTemplatePrgAddJmp(prg, UDM_TMPL_JNE, 0);
      break;
    
    case UDM_TMPL_WHILENOT:
    case UDM_TMPL_IFNOT:
      sitem.beg= prg->nitems;
      sitem.jmp= prg->nitems + 1;
      sitem.cmd= cmd->cmdnum == UDM_TMPL_WHILENOT ? UDM_TMPL_WHILE : UDM_TMPL_IF;
      CompileStackPush(st, &sitem);
      HtmlTemplatePrgAddFunc(prg, &udm_tmpl_cmp, tag);
      HtmlTemplatePrgAddJmp(prg, UDM_TMPL_JE, 0);
      break;
    
    case UDM_TMPL_FOR:
      HtmlTemplatePrgAddFunc(prg, &udm_tmpl_set, tag);
      sitem.beg= prg->nitems;
      sitem.jmp= prg->nitems + 1;
      sitem.cmd= UDM_TMPL_FOR;
      CompileStackPush(st, &sitem);
      break;
      
    case UDM_TMPL_IFLIKE:
      sitem.beg= prg->nitems;
      sitem.jmp= prg->nitems + 1;
      sitem.cmd= UDM_TMPL_IF;
      CompileStackPush(st, &sitem);
      HtmlTemplatePrgAddFunc(prg, &udm_tmpl_wld, tag);
      HtmlTemplatePrgAddJmp(prg, UDM_TMPL_JNE, 0);
      break;
    
    case UDM_TMPL_ELSEIF:
      if (!st->nitems || st->Items[st->nitems-1].cmd != UDM_TMPL_IF)
      {
        HtmlTemplateErrorUnexpectedSym(prg, "<!ELSEIF>");
        rc= UDM_ERROR; 
        goto ret;
      }
      HtmlTemplatePrgAddJmp(prg, UDM_TMPL_JMP, 0);
      prg->Items[st->Items[st->nitems-1].jmp].jump= prg->nitems;
      st->Items[st->nitems-1].jmp= prg->nitems + 1;
      HtmlTemplatePrgAddFunc(prg, &udm_tmpl_cmp, tag);
      HtmlTemplatePrgAddJmp(prg, UDM_TMPL_JNE, 0);
      break;
    
    case UDM_TMPL_ELSELIKE:
      if (!st->nitems || st->Items[st->nitems-1].cmd != UDM_TMPL_IF)
      {
        HtmlTemplateErrorUnexpectedSym(prg, "<!ELSELIKE>");
        rc= UDM_ERROR; 
        goto ret;
      }
      HtmlTemplatePrgAddJmp(prg, UDM_TMPL_JMP, 0);
      prg->Items[st->Items[st->nitems-1].jmp].jump= prg->nitems;
      st->Items[st->nitems-1].jmp= prg->nitems + 1;
      HtmlTemplatePrgAddFunc(prg, &udm_tmpl_wld, tag);
      HtmlTemplatePrgAddJmp(prg, UDM_TMPL_JNE, 0);
      break;
    
    case UDM_TMPL_ELSE:
      if (!st->nitems || st->Items[st->nitems-1].cmd != UDM_TMPL_IF)
      {
        HtmlTemplateErrorUnexpectedSym(prg, "<!ELSE>");
        rc= UDM_ERROR; 
        goto ret;
      }
      HtmlTemplatePrgAddJmp(prg, UDM_TMPL_JMP, 0);
      prg->Items[st->Items[st->nitems-1].jmp].jump= prg->nitems;
      break;
    
    case UDM_TMPL_ENDFOR:
      if (!st->nitems || st->Items[st->nitems-1].cmd != UDM_TMPL_FOR)
      {
        HtmlTemplateErrorUnexpectedSym(prg, "<!ENDFOR>");
        rc= UDM_ERROR;
        goto ret;
      }
      {
        UDM_TMPL_PRGITEM *FromItem= &prg->Items[st->Items[st->nitems-1].beg-1];
        UDM_TMPL_PRGITEM N;

        bzero((void*)&N, sizeof(N));
        N.cmdnum= UDM_TMPL_INC;
        N.cmd= &udm_tmpl_inc;
        N.arg[0]= UdmStrdup(FromItem->arg[0]);
        HtmlTemplatePrgAdd(prg, &N);

        bzero((void*)&N, sizeof(N));
        N.cmdnum= UDM_TMPL_IFGT;
        N.cmd= &udm_tmpl_cmp;
        N.arg[0]= UdmStrdup(FromItem->arg[0]);
        N.arg[1]= UdmStrdup(FromItem->arg[2]);
        HtmlTemplatePrgAdd(prg, &N);
        
        HtmlTemplatePrgAddJmp(prg, UDM_TMPL_JNE, st->Items[st->nitems-1].beg);
      }
      CompileStackPop(st);
      break;
    
    case UDM_TMPL_ENDWHILE:
      if (!st->nitems || st->Items[st->nitems-1].cmd != UDM_TMPL_WHILE)
      {
        HtmlTemplateErrorUnexpectedSym(prg, "<!ENDWHILE>");
        rc= UDM_ERROR; 
        goto ret;
      }
      HtmlTemplatePrgAddJmp(prg, UDM_TMPL_JMP, st->Items[st->nitems-1].beg);
      prg->Items[st->Items[st->nitems-1].jmp].jump= prg->nitems;
      CompileStackPop(st);
      break;
    
    case UDM_TMPL_ENDIF:
      if (!st->nitems || st->Items[st->nitems-1].cmd != UDM_TMPL_IF)
      {
        HtmlTemplateErrorUnexpectedSym(prg, "<!ENDIF>");
        rc= UDM_ERROR; 
        goto ret;
      }
      /*
        Now we know the address if <!ENDIF>
        We should pass through <!IF> and all <!ELSEIF>
        and set correct JMP addresses.
      */
      {
        size_t i;
        for (i= st->Items[st->nitems-1].beg; i < prg->nitems; i++)
        {
          if (prg->Items[i].jump == 0 &&
              (prg->Items[i].cmdnum == UDM_TMPL_JMP ||
               prg->Items[i].cmdnum == UDM_TMPL_JE  ||
               prg->Items[i].cmdnum == UDM_TMPL_JNE))
            prg->Items[i].jump= prg->nitems; 
        }
      }
      CompileStackPop(st);
      break;
    
    default:
      HtmlTemplatePrgAddFunc(prg, cmd, tag);
      break;
  }
  
ret:
  return rc;
}


static int HtmlTemplateCompile(UDM_TMPL_PRG *prg, const char *src)
{
  const char *lt;
  const char *tok;
  UDM_TMPL_COMPILE_STACK st;
  int rc= UDM_OK;

  UDM_HTMLTOK HtmlTok;
  UdmHTMLTOKInit(&HtmlTok);
  CompileStackInit(&st);
  
  
  for (tok= UdmHTMLToken(src, &lt, &HtmlTok) ;
       tok ;
       tok= UdmHTMLToken(NULL , &lt, &HtmlTok))
  {
    if (tok[0]=='<' && tok[1] == '!' && isalpha(tok[2]))
    {
      UDM_TMPL_CMD *cmd;
      if ((cmd= HtmlTemplateCmd(tok+2, lt)))
        rc= HtmlTemplateCompileCommand(prg, &st, &HtmlTok, cmd);
      else if (tok[2] == '-') /* Comment */
        rc= HtmlTemplatePrgAdd1Arg(prg, &udm_tmpl_txt, tok, lt);
      else
        rc= HtmlTemplatePrgAddInvokeMethod(prg, &st, &HtmlTok, tok, lt);
    }
    else if(!(strncasecmp(tok,"<OPTION",7)))
    {
      rc= HtmlTemplatePrgAdd1Arg(prg, &udm_tmpl_tag, tok, lt);
    }
    else if(!(strncasecmp(tok,"<INPUT",6)))
    {
      rc= HtmlTemplatePrgAdd1Arg(prg, &udm_tmpl_tag, tok, lt);
    }
    else
    {
      rc= HtmlTemplatePrgAdd1Arg(prg, &udm_tmpl_txt, tok, lt);
    }
    if (rc != UDM_OK)
      break;
  }
  
  if (rc == UDM_OK && st.nitems)
  {
    const char *missing;
    if (st.Items[st.nitems-1].cmd == UDM_TMPL_IF)
      missing= "ENDIF";
    else if (st.Items[st.nitems-1].cmd == UDM_TMPL_FOR)
      missing= "ENDFOR";
    else if (st.Items[st.nitems-1].cmd == UDM_TMPL_WHILE)
      missing= "ENDWHILE";
    else
      missing= "ENDUNKNOWN";
    udm_snprintf(prg->errstr, sizeof(prg->errstr), "Missing %s", missing);
    rc= UDM_ERROR;
  }
  CompileStackFree(&st);
  return rc;
}


static void HtmlTemplateProgItemPrint(UDM_TMPL_PRGITEM *it)
{
  if (it->jump)
    printf("SOMEJMP %d\n", it->jump);
  else
  switch (it->cmd->nargs)
  {
    case 0:
      printf("%s\n", it->cmd->name);
      break;
    case 1:
      printf("%s '%s'\n", it->cmd->name, it->arg[0]);
      break;
    case 2:
      printf("%s '%s' '%s'\n", it->cmd->name, it->arg[0], it->arg[1]);
      break;
    case 3:
      printf("%s '%s' '%s' '%s'\n", it->cmd->name, it->arg[0], it->arg[1], it->arg[2]);
      break;
  }
}


#if 1
static int HtmlTemplateProgPrint(UDM_TMPL_PRG *p)
{
  size_t i;
  
  for (i=0 ; i < p->nitems ; i++)
  {
    UDM_TMPL_PRGITEM *it= &p->Items[i];
    printf("%04X ", i);
    HtmlTemplateProgItemPrint(it);
  }
  return UDM_OK;
}
#endif




static void PrintHtmlTemplate(UDM_TEMPLATE *tmpl, const char *template)
{
  HtmlTemplatePrgInit(&tmpl->prg);
  
  if (UDM_OK != HtmlTemplateCompile(&tmpl->prg, template))
  {
    if (tmpl->stream)
      fprintf(tmpl->stream, "Template error: %s\n", tmpl->prg.errstr);
    goto ret;
  }
  
  if (0)
    HtmlTemplateProgPrint(&tmpl->prg);
  
  for (tmpl->prg.curr= 0; tmpl->prg.curr < tmpl->prg.nitems; )
  {
    UDM_TMPL_PRGITEM *it= &tmpl->prg.Items[tmpl->prg.curr];
    
    switch (it->cmdnum)
    {
      case UDM_TMPL_JNE:
        if (tmpl->prg.reg)
          tmpl->prg.curr= it->jump;
        else
          tmpl->prg.curr++;
        continue;
      
      case UDM_TMPL_JE:
        if (!tmpl->prg.reg)
          tmpl->prg.curr= it->jump;
        else
          tmpl->prg.curr++;
        continue;
      
      case UDM_TMPL_JMP:
        tmpl->prg.curr= it->jump;
        continue;
      
      default:
        it->cmd->exec(tmpl);
        tmpl->prg.curr++;
    }
  }

ret:
  HtmlTemplatePrgFree(&tmpl->prg);
}


void __UDMCALL UdmTemplatePrint(UDM_AGENT * Agent,
                                FILE *stream, char *dst, size_t dst_len,
                                UDM_VARLIST *vars, UDM_VARLIST *tm,
                                const char *w)
{
  size_t  t;
  size_t  matches=0;
  size_t  format=(size_t)UdmVarListFindInt(vars,"o",0);
  UDM_VAR  *First=NULL;
  UDM_TEMPLATE tmpl;
  tmpl.Agent= Agent;
  tmpl.stream= stream;
  tmpl.vars= vars;
  tmpl.HlBeg= UdmRemoveHiLightDup(UdmVarListFindStr(vars,"HlBeg",""));
  tmpl.HlEnd= UdmRemoveHiLightDup(UdmVarListFindStr(vars,"HlEnd",""));
  tmpl.dst= dst;
  tmpl.dstlen= dst_len;
  
  if(dst)*dst='\0';
  for(t=0;t<tm->nvars;t++)
  {
    if(!strcasecmp(w,tm->Var[t].name))
    {
      if(!First)First=&tm->Var[t];
      if(matches==format)
      {
        PrintHtmlTemplate(&tmpl, tm->Var[t].val);
        goto ret;
      }
      matches++;
    }
  }
  if (First) PrintHtmlTemplate(&tmpl, First->val);
ret:
  UDM_FREE(tmpl.HlBeg);
  UDM_FREE(tmpl.HlEnd);
  return;
}


static int ParseHl (UDM_VARLIST *v, const char *str)
{
  const char *p;
  const char *e;
  char *s;

  for (p = str; *p; p++) if (! isspace(*p)) break;

  if (! strncasecmp(p, "HlBeg", 5))
  {
    p += 5;
    while (*p && isspace(*p)) p++;
    if ((*p == '\'' || *p == '"') && (e = strrchr(p + 1, *p))) s = UdmStrndup(p + 1, e - p - 1);
    else s = UdmStrdup(p);
    UdmVarListReplaceStr(v, "HlBeg", s);
    UdmFree(s);
  }
  else if (! strncasecmp(p, "HlEnd", 5))
  {
    p += 5;
    while (*p && isspace(*p)) p++;
    if ((*p == '\'' || *p == '"') && (e = strrchr(p + 1, *p))) s = UdmStrndup(p + 1, e - p - 1);
    else s = UdmStrdup(p);
    UdmVarListReplaceStr(v, "HlEnd", s);
    UdmFree(s);
  }
  else
  {
    return UDM_ERROR;
  }

  return UDM_OK;
}

/* Load template  */
int UdmTemplateLoad(UDM_AGENT *Agent,
                    const char * tname,
                    UDM_VARLIST *tmpl)
{
  UDM_ENV *Env = Agent->Conf;
  UDM_CFG Cfg;
  UDM_SERVER Srv;
  FILE    *file;
  char    str[1024];
  char    ostr[1024];
  const char  *dbaddr=NULL;
  int    variables=0, rc= UDM_OK;
  char    cursection[128]="";
  char    nameletter[]=
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789._";
  UDM_DSTR dstr;
  UdmDSTRInit(&dstr, 16*1024);
  UdmServerInit(&Srv);
  bzero((void*)&Cfg, sizeof(Cfg));
  Cfg.Indexer = Agent;
  Cfg.level = 0;
  Cfg.flags = UDM_FLAG_LOAD_LANGMAP + UDM_FLAG_SPELL;
  Cfg.Srv = &Srv;

  if(!(file=fopen(tname,"r")))
  {
    udm_snprintf(Env->errstr,sizeof(Env->errstr)-1,"Unable to open template '%s': %s",tname,strerror(errno));
    rc= UDM_ERROR;
    goto ret;
  }
  
  while(fgets(str,sizeof(str)-1,file))
  {
    char  *s;
    
    str[sizeof(str)-1]='\0';
    strcpy(ostr,str);
    
    s=UdmTrim(str," \r\n");
    
    if(!strcasecmp(s,"<!--variables"))
    {
      variables=1;
      continue;
    }
    
    if(!strcmp(s,"-->") && variables)
    {
      variables=0;
      continue;
    }
    
    if(variables)
    {
      int r;
      if(!*s)continue;
      if(*s=='#')continue;

      if(UDM_OK!=(r=ParseHl(&Env->Vars, s)) && UDM_OK!=(r=UdmEnvAddLine(&Cfg,s)))
      {
        rc= r;
        goto ret;
      }
      continue;
    }
    
    if(!memcmp(s,"<!--",4))
    {
      char *e;
      
      for(e=s+4;(*e)&&(strchr(nameletter,*e)||(*e=='/'));e++);
      
      if(!strcmp(e,"-->"))
      {
        *e='\0';
        s+=4;
        
        if(s[0]=='/')
        {
          if(!strcasecmp(s+1,cursection) && cursection[0])
          {
            UDM_VAR *I;
            UdmVarListAdd(tmpl, NULL);
            I=&tmpl->Var[tmpl->nvars-1];
            I->name = (char*)UdmStrdup(cursection);
            I->val = (char*) UdmStrdup(dstr.size_data ? dstr.data : "");
            cursection[0]='\0';
            UdmDSTRReset(&dstr);
            continue;
          }
        }
        else if(s[1])
        {
          strncpy(cursection,s,sizeof(cursection));
          cursection[sizeof(cursection)-1]='\0';
          continue;
        }
      }
    }
    
    if(!cursection[0])
      continue;
    
    UdmDSTRAppend(&dstr, ostr, strlen(ostr));
  }
  fclose(file);
  UdmVarListReplaceLst(&Env->Vars, &Srv.Vars, NULL, "*");
  UdmServerFree(&Srv);
    
  if (UDM_OK != UdmEnvPrepare(Env))
  {
    rc= UDM_ERROR;
    goto ret;
  }
  
#ifdef HAVE_SQL
  if(Env->dbl.nitems == 0)
  {
    if (UDM_OK != (rc= UdmDBListAdd(&Env->dbl,
                                    dbaddr= "mysql://localhost/mnogosearch",
                                    UDM_OPEN_MODE_READ)))
    {
      sprintf(Env->errstr,"%s DBAddr: '%s'",
              rc == UDM_UNSUPPORTED ? "Unsupported" : "Invalid",
             (!dbaddr)?"NULL":dbaddr);
      rc= UDM_ERROR;
      goto ret;
    }
  }
#endif
  if(Env->dbl.nitems == 0)
  {
    if (UDM_OK!= (rc= UdmDBListAdd(&Env->dbl, dbaddr=  "searchd://localhost/", UDM_OPEN_MODE_READ)))
    {
      sprintf(Env->errstr,"Invalid DBAddr: '%s'", (!dbaddr) ? "NULL" : dbaddr);
      rc= UDM_ERROR;
      goto ret;
    }
  }

ret:  
  UdmDSTRFree(&dstr);
  return rc;
}
