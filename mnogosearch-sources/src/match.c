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

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_wild.h"
#include "udm_match.h"
#include "udm_vars.h"


static int
IsSimpleWildEnding(const char *p)
{
  if (*p++ != '*' || !*p)
    return 0;
  for (; *p; p++)
  {
    if (*p == '*' || *p =='?')
      return 0;
  }
  return 1;
}


int UdmMatchComp(UDM_MATCH *Match,char *errstr,size_t errstrsize)
{
  int flag=REG_EXTENDED;
  int err;
  
  errstr[0]='\0';
  Match->pattern_length= strlen(Match->pattern);
  
  switch(Match->match_type)
  {
    case UDM_MATCH_REGEX:
      Match->reg = (regex_t*)UdmRealloc(Match->reg, sizeof(regex_t));
      bzero((void*)Match->reg, sizeof(regex_t));
      if(Match->case_sense == UDM_CASE_INSENSITIVE)
        flag|=REG_ICASE;
      if((err=regcomp(Match->reg, Match->pattern, flag)))
      {
        regerror(err, Match->reg, errstr, errstrsize);
        UDM_FREE(Match->reg);
        return(1);
      }
      break;
    case UDM_MATCH_WILD:
      if (!(Match->flags & UDM_MATCH_FLAG_SKIP_OPTIMIZATION) &&
          IsSimpleWildEnding(Match->pattern))
      {
        /* Convert simple UDM_MATCH_WILD cases to UDM_MATCH_END */
        memmove(Match->pattern, Match->pattern + 1, Match->pattern_length);
        Match->match_type= UDM_MATCH_END;
        Match->pattern_length--;
      }
      break;
    case UDM_MATCH_BEGIN:
    case UDM_MATCH_END:
    case UDM_MATCH_SUBSTR:
    case UDM_MATCH_FULL:
      break;
    default:
      udm_snprintf(errstr,errstrsize,"Unknown match type '%d'",Match->match_type);
      return(1);
  }
  return 0;
}

UDM_MATCH *UdmMatchInit(UDM_MATCH *M)
{
  bzero((void*)M,sizeof(*M));
  return M;
}

void UdmMatchFree(UDM_MATCH * Match)
{
  UDM_FREE(Match->pattern);
  UDM_FREE(Match->arg);
  UDM_FREE(Match->arg1);
  UDM_FREE(Match->section);
  if(Match->reg)
  {
    regfree((regex_t*)Match->reg);
    UDM_FREE(Match->reg);
  }
}

#define UDM_NSUBS 10

int UdmMatchExec(UDM_MATCH * Match,
                 const char *string, size_t string_length,
                 const char *net_string,
                 size_t nparts,UDM_MATCH_PART * Parts)
{
  size_t    i;
  int    res=0;
  regmatch_t  subs[UDM_NSUBS];
  const char  *se;

  UdmDebugEnter();
  
  switch(Match->match_type)
  {
    case UDM_MATCH_REGEX:
      if(nparts>UDM_NSUBS)nparts=UDM_NSUBS;
      res=regexec((regex_t*)Match->reg,string,nparts,subs,0);
      if(res)
      {
        for(i=0;i<nparts;i++)Parts[i].beg=Parts[i].end=-1;
      }
      else
      {
        for(i=0;i<nparts;i++)
        {
          Parts[i].beg=subs[i].rm_so;
          Parts[i].end=subs[i].rm_eo;
        }
      }
      break;
    case UDM_MATCH_WILD:
      for(i=0;i<nparts;i++)Parts[i].beg=Parts[i].end=-1;
      if(Match->case_sense == UDM_CASE_INSENSITIVE)
      {
        res=UdmWildCaseCmp(string,Match->pattern);
      }
      else
      {
        res=UdmWildCmp(string,Match->pattern);
      }
      break;
    case UDM_MATCH_SUBNET:
      for(i = 0; i < nparts; i++) Parts[i].beg = Parts[i].end = -1;
      if(Match->case_sense == UDM_CASE_INSENSITIVE)
      {
        res = UdmWildCaseCmp(net_string, Match->pattern);
      }
      else
      {
        res = UdmWildCmp(net_string, Match->pattern);
      }
      break;
    case UDM_MATCH_BEGIN:
      for(i=0;i<nparts;i++)Parts[i].beg=Parts[i].end=-1;
      if(Match->case_sense == UDM_CASE_INSENSITIVE)
      {
        res=strncasecmp(Match->pattern, string, Match->pattern_length);
      }
      else
      {
        res=strncmp(Match->pattern, string, Match->pattern_length);
      }
      break;
    case UDM_MATCH_FULL:
      for(i=0;i<nparts;i++)Parts[i].beg=Parts[i].end=-1;
      if(Match->case_sense == UDM_CASE_INSENSITIVE)
      {
        res=strcasecmp(Match->pattern,string);
      }
      else
      {
        res=strcmp(Match->pattern,string);
      }
      break;
    case UDM_MATCH_END:
      for(i=0;i<nparts;i++)Parts[i].beg=Parts[i].end=-1;
      if(string_length < Match->pattern_length)
      {
        res= 1;
        break;
      }
      se= string + string_length - Match->pattern_length;
      if(Match->case_sense == UDM_CASE_INSENSITIVE)
      {
        res=strcasecmp(Match->pattern,se);
      }
      else
      {
        res=strcmp(Match->pattern,se);
      }
      break;

    case UDM_MATCH_SUBSTR:
    default:
      for(i=0;i<nparts;i++)Parts[i].beg=Parts[i].end=-1;
      res=0;
  }
  if (Match->nomatch) res = !res;
  UdmDebugReturn(res);
}


static int
UdmMatchSubNo(const char *str)
{
  if (*str == '$' && str[1] >= '0' && str[1] <= '9')
    return str[1] - '0';
  return -1;
}

static size_t
UdmMatchPartLength(UDM_MATCH_PART *P)
{
  return ((P->beg > -1) && (P->end > P->beg)) ? P->end - P->beg : 0;
}


/*
  Calc length which will be required for UdmMatchRegexApply(),
  including traling '\0' character.
*/
static size_t
UdmMatchRegexApplyLength(const char *repl, size_t nparts, UDM_MATCH_PART *Parts)
{
  size_t length;
  for (length= 0; *repl; repl++)
  {
    int sub;
    if ((sub= UdmMatchSubNo(repl)) >= 0)
    {
      size_t part_length= UdmMatchPartLength(&Parts[sub]);
      length+= part_length;
      repl++;
    }
    else
    {
      length++;
    }
  }
  return length + 1;
}


/*
  Apply "regex" type of match parts.
  previously generated by UdmMatchExec result.
  res    - where to put result to
  size   - size of res, must be at least 1 byte - for trailing '\0'.
  string - source string
  rpl    - replacement pattern, with variables \0, \1, \2, ... \9
  nparts - how many parts were found by UdmMatchExec
  Parts  - the parts themself
  
  RETURNS - result length.
*/
static size_t
UdmMatchRegexApply(char *res, size_t size,
                   const char *string,const char *rpl,
                   size_t nparts, UDM_MATCH_PART *Parts)
{
  char *dst= res, *dstend= res + size - 1;
  const char *repl= rpl;
  
  if (!res)
    return UdmMatchRegexApplyLength(rpl, nparts, Parts);
  
  UDM_ASSERT(size>0);
  
  while (*repl && dst < dstend)
  {
    int sub;
    if ((sub= UdmMatchSubNo(repl)) >= 0)
    {
      size_t part_length;
      if((part_length= UdmMatchPartLength(&Parts[sub])))
      {
        size_t bytes_left= dstend - dst;
        if (bytes_left <= part_length)
          part_length= bytes_left;
        memcpy(dst, string + Parts[sub].beg, part_length);
        dst+= part_length;
      }
      repl+= 2;
    }
    else
    {
      *dst++= *repl++;
    }
  }
  *dst='\0';
  return dst - res;
}


size_t
UdmMatchApply(char *res, size_t size,
              const char *string,const char *rpl,
              UDM_MATCH *Match,
              size_t nparts, UDM_MATCH_PART *Parts)
{
  int    len=0;
  if(!size)return 0;
  
  switch(Match->match_type)
  {
    case UDM_MATCH_REGEX:
      len= UdmMatchRegexApply(res, size, string, rpl, nparts, Parts);
      break;
      
    case UDM_MATCH_BEGIN:
      len = udm_snprintf(res,size-1,"%s%s",rpl,string+strlen(Match->pattern));
      break;
      
    default:
      *res='\0';
      len=0;
      break;
  }
  return len;
}



int UdmMatchListAdd(UDM_AGENT *A, UDM_MATCHLIST *L, UDM_MATCH *M,
                    char *err, size_t errsize, int ordre)
{
  UDM_MATCH  *N;
  
  L->Match=(UDM_MATCH *)UdmRealloc(L->Match,(L->nmatches+1)*sizeof(UDM_MATCH));
  N=&L->Match[L->nmatches++];
  UdmMatchInit(N);
  N->pattern = (char*)UdmStrdup(M->pattern);
  N->match_type=M->match_type;
  N->case_sense=M->case_sense;
  N->nomatch=M->nomatch;
  N->flags= M->flags;
  N->section = M->section ? (char *)UdmStrdup(M->section) : NULL;
  N->arg = M->arg ? (char*)UdmStrdup(M->arg) : NULL;
  N->arg1 = M->arg1 ? (char*)UdmStrdup(M->arg1) : NULL;

  return UdmMatchComp(N, err, errsize);
}


UDM_MATCHLIST *UdmMatchListInit(UDM_MATCHLIST *L)
{
  bzero((void*)L,sizeof(*L));
  return L;
}

void UdmMatchListFree(UDM_MATCHLIST *L)
{
  size_t i;
  for(i=0;i<L->nmatches;i++)
  {
    UdmMatchFree(&L->Match[i]);
  }
  L->nmatches=0;
  UDM_FREE(L->Match);
}

UDM_MATCH * __UDMCALL UdmMatchListFind(UDM_MATCHLIST *L, const char *str,
                                       size_t nparts,UDM_MATCH_PART *Parts)
{
  size_t i, slen= strlen(str);
  for(i=0;i<L->nmatches;i++)
  {
    UDM_MATCH *M=&L->Match[i];
    if(!UdmMatchExec(M, str, slen, str, nparts, Parts))
      return M;
  }
  return NULL;
}

UDM_MATCH * __UDMCALL UdmMatchSectionListFind(UDM_MATCHLIST *L,
                                              UDM_DOCUMENT *Doc,
                                              size_t nparts,
                                              UDM_MATCH_PART *Parts)
{
  size_t i;

  for(i=0;i<L->nmatches;i++)
  {
    UDM_MATCH *M=&L->Match[i];
    const char *str = UdmVarListFindStr(&Doc->Sections, M->section, "");
    size_t slen= strlen(str);
    if(!UdmMatchExec(M, str, slen, str, nparts, Parts))
      return M;
  }
  return NULL;
}


const char *UdmMatchTypeStr(int m)
{
  switch(m)
  {
    case UDM_MATCH_REGEX:  return  "Regex";
    case UDM_MATCH_WILD:  return  "Wild";
    case UDM_MATCH_BEGIN:  return  "Begin";
    case UDM_MATCH_END:  return  "End";
    case UDM_MATCH_SUBSTR:  return  "SubStr";
    case UDM_MATCH_FULL:  return  "Full";
    case UDM_MATCH_SUBNET:  return  "Subnet";
  }
  return "<Unknown Match Type>";
}
