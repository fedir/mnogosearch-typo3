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
#include <string.h>

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_vars.h"
#include "udm_searchtool.h"


static int
UdmWWListWordInfo(UDM_VARLIST *Vars, UDM_WIDEWORDLIST *WWList)
{
  size_t  len, i, j, wsize;
  char  *wordinfo= NULL, *end;
  size_t  corder= (size_t)-1, ccount= 0;
  int have_suggestions= 0;
  
  for(len = i = 0; i < WWList->nwords; i++) 
    len += WWList->Word[i].len + 64;
  
  wsize= (1+len)*sizeof(char);
  wordinfo= (char*) UdmMalloc(wsize);
  *wordinfo= '\0';
  
  UdmVarListAddInt(Vars, "nwords", WWList->nwords);

  for(i = 0; i < WWList->nwords; i++)
  {
    char name[32], count[32];
    if (WWList->Word[i].origin == UDM_WORD_ORIGIN_QUERY ||
        WWList->Word[i].origin == UDM_WORD_ORIGIN_SPELL ||
        WWList->Word[i].origin == UDM_WORD_ORIGIN_SYNONYM ||
        WWList->Word[i].origin == UDM_WORD_ORIGIN_SYNONYM_FINAL ||
        WWList->Word[i].origin == UDM_WORD_ORIGIN_COLLATION)
    {
      if(wordinfo[0])
        strcat(wordinfo,", ");
      sprintf(UDM_STREND(wordinfo)," %s : %d",
              WWList->Word[i].word, (int) WWList->Word[i].count);
      sprintf(count, "%d", (int) WWList->Word[i].count);
    }
    else if (WWList->Word[i].origin == UDM_WORD_ORIGIN_STOP)
    {
      if(wordinfo[0])
        strcat(wordinfo,", ");
      sprintf(UDM_STREND(wordinfo)," %s : stopword", WWList->Word[i].word);
      strcpy(count, "stopword");
    }
    sprintf(name, "word%d.word", (int) i);
    UdmVarListAddStr(Vars, name, WWList->Word[i].word);
    sprintf(name, "word%d.count", (int) i);
    UdmVarListAddStr(Vars, name, count);
    sprintf(name, "word%d.doccount", (int) i);
    UdmVarListAddInt(Vars, name, WWList->Word[i].doccount);
    sprintf(name, "word%d.order", (int) i);
    UdmVarListAddInt(Vars, name, WWList->Word[i].order);
    sprintf(name, "word%d.origin", (int) i);
    UdmVarListAddInt(Vars, name, WWList->Word[i].origin);
    sprintf(name, "word%d.weight", (int) i);
    UdmVarListAddInt(Vars, name, WWList->Word[i].weight);
  }
  
  UdmVarListReplaceStr(Vars, "WE", wordinfo);
  
  *wordinfo = '\0';
  for(i = 0; i < WWList->nwords; i++)
  {
    corder= WWList->Word[i].order;
    ccount= 0;
    for(j= 0; j < WWList->nwords; j++)
      if (WWList->Word[j].order == corder)
        ccount += WWList->Word[j].count;
    if (WWList->Word[i].origin == UDM_WORD_ORIGIN_STOP)
    {
      sprintf(UDM_STREND(wordinfo),"%s%s : stopword", (*wordinfo) ? ", " : "",  WWList->Word[i].word);
    }
    else if (WWList->Word[i].origin == UDM_WORD_ORIGIN_QUERY)
    {
      sprintf(UDM_STREND(wordinfo),"%s%s : %d / %d", 
              (*wordinfo) ? ", " : "",
              WWList->Word[i].word,
              (int) WWList->Word[i].count, (int) ccount);
    }
  }
  UdmVarListReplaceStr(Vars, "W", wordinfo);
  
  *wordinfo= '\0';
  end= wordinfo;
  for (i= 0; i < WWList->nwords; i++)
  {
    UDM_WIDEWORD *Wi= &WWList->Word[i];
    UDM_WIDEWORD *Wb= NULL;
   
    if (Wi->origin == UDM_WORD_ORIGIN_QUERY)
    {
      if (Wi->count > 0)
      {
        Wb= Wi;
      }
      else
      {
        ccount= 0;
        for (j= 0; j < WWList->nwords; j++)
        {
          UDM_WIDEWORD *Wj= &WWList->Word[j];
          if (Wj->origin == UDM_WORD_ORIGIN_SUGGEST &&
              Wj->order == Wi->order && Wj->count > ccount)
          {
            ccount= WWList->Word[j].count;
            Wb= Wj;
            have_suggestions= 1;
          }
        }
      }
    }
    else if (Wi->origin == UDM_WORD_ORIGIN_STOP)
    {
      Wb= Wi;
    }
    
    if (Wb)
    {
      sprintf(end, "%s%s", wordinfo[0] ? " " : "", Wb->word);
      end= end + strlen(end);
    }
  }
  
  if (have_suggestions)
    UdmVarListReplaceStr(Vars, "WS", wordinfo);
  UDM_FREE(wordinfo);
  return UDM_OK;
}


int
UdmResWordInfo(UDM_ENV *Env, UDM_RESULT *Res)
{
  return UdmWWListWordInfo(&Env->Vars, &Res->WWList);
}
