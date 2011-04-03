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

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_stopwords.h"


/*
UDM_STOPWORD
*UdmStopListFind(UDM_STOPLIST *List, const char *word, const char *lang)
{
  int low= 0;
  int high= List->nstopwords - 1;
  
  if(!List->StopWord)
    return(0);
  
  while (low <= high)
  {
    int middle = (low + high) / 2;
    int match = strcmp(List->StopWord[middle].word,word);
    if (match < 0)
    {
      low = middle + 1;
    }
    else if (match > 0)
    {
      high = middle - 1;
    }
    else
    {
      if (lang==NULL || *lang=='\0' || 
          !strncasecmp(List->StopWord[middle].lang, lang, 
                       strlen(List->StopWord[middle].lang)))
        return(&List->StopWord[middle]);
      return NULL;
    }
  }
  return NULL;
}
*/

static int
cmpstop(const void *s1,const void *s2)
{
  return(strcmp(((const UDM_STOPWORD*)s1)->word,((const UDM_STOPWORD*)s2)->word));
}


static void
UdmStopListInit(UDM_STOPLIST *List)
{
  bzero((void*) List, sizeof(UDM_STOPLIST));
}


void
UdmStopListSort(UDM_STOPLIST *List)
{
  /* Sort stoplist to run binary search later */
  UdmSort(List->StopWord,List->nstopwords,sizeof(UDM_STOPWORD),cmpstop);
}


int
UdmStopListAdd(UDM_STOPLIST *List,UDM_STOPWORD * stopword)
{
  size_t j;
  
  /* 
    If the word is already in list, we will not add it again
    But mark it as "international word", i.e. a word without language
    It will allow to avoid troubles with language guesser
  */
  
  for (j= 0; j < List->nstopwords; j++)
  {
    if (!strcmp(List->StopWord[j].word, stopword->word))
    {
      UDM_FREE(List->StopWord[j].lang);
      List->StopWord[j].lang= (char*)UdmStrdup("");
      return 0;
    }
  }
  
  List->StopWord= (UDM_STOPWORD *)UdmRealloc(List->StopWord,(List->nstopwords+1)*sizeof(UDM_STOPWORD));
  List->StopWord[List->nstopwords].word= (char*)UdmStrdup(stopword->word);
  List->StopWord[List->nstopwords].lang= (char*)UdmStrdup(stopword->lang?stopword->lang:"");
  List->nstopwords++;
  
  return 1;
}


void
UdmStopListFree(UDM_STOPLIST *List)
{
  size_t i;
  for(i= 0; i < List->nstopwords; i++)
  {
    UDM_FREE(List->StopWord[i].word);
    UDM_FREE(List->StopWord[i].lang);
  }
  UDM_FREE(List->StopWord);
  List->nstopwords= 0;
}


__C_LINK int __UDMCALL
UdmStopListLoad(UDM_ENV *Conf,const char *fname)
{
  char str[1024];
  char *lasttok, *lwrd, *charset=NULL;
  FILE *stopfile;
  UDM_STOPWORD stopword;
  UDM_CHARSET *cs= NULL;
  UDM_CONV cnv;
  UDM_STOPLIST StopList;
  
  UdmStopListInit(&StopList);
  
  if (!(stopfile=fopen(fname,"r")))
  {
    sprintf(Conf->errstr,"Can't open stopwords file '%s' (%s)", fname, strerror(errno));
    return UDM_ERROR;
  }
  if ((lwrd = (char*)UdmMalloc(Conf->WordParam.max_word_len + 1)) == NULL)
    return UDM_ERROR;
  
  bzero((void*)&stopword, sizeof(stopword));
  
  while (fgets(str,sizeof(str),stopfile))
  {
    if(!str[0])continue;
    if(str[0]=='#')continue;
    
    if(!strncmp(str,"Charset:",8))
    {
      UDM_FREE(charset);
      charset= udm_strtok_r(str + 8, " \t\n\r", &lasttok);
      if(charset)
        charset= (char*)UdmStrdup(charset);
    }
    else if (!strncmp(str,"Language:",9))
    {
      UDM_FREE(stopword.lang);
      stopword.lang= udm_strtok_r(str + 9, " \t\n\r", &lasttok);
      if(stopword.lang)
        stopword.lang= (char*)UdmStrdup(stopword.lang);
    }
    else if ((stopword.word= udm_strtok_r(str, "\t\n\r", &lasttok)))
    {
      
      if(!cs)
      {
        if(!charset)
        {
          sprintf(Conf->errstr,"No charset definition in stopwords file '%s'", fname);
          UDM_FREE(stopword.lang);
          UDM_FREE(lwrd);
          return UDM_ERROR;
        }
        else
        {
          if(!(cs= UdmGetCharSet(charset)))
          {
            sprintf(Conf->errstr,"Unknown charset '%s' in stopwords file '%s'", charset,fname);
            UDM_FREE(stopword.lang);
            UDM_FREE(charset);
            UDM_FREE(lwrd);
            return UDM_ERROR;
          }
          UdmConvInit(&cnv,cs,Conf->lcs,UDM_RECODE_HTML);
        }
      }
      
      UdmConv(&cnv, lwrd, Conf->WordParam.max_word_len, stopword.word, strlen(stopword.word) + 1);
      lwrd[Conf->WordParam.max_word_len]= '\0';
      stopword.word= lwrd;
      UdmStopListAdd(&StopList, &stopword);
    }
  }
  fclose(stopfile);
  UdmStopListSort(&StopList);
  udm_snprintf(StopList.lang, UDM_STOPLIST_LANGLEN, "%s", stopword.lang);
  udm_snprintf(StopList.cset, UDM_STOPLIST_CSETLEN, "%s", charset);
  udm_snprintf(StopList.fname,UDM_STOPLIST_FILELEN, "%s", fname);
  UDM_FREE(stopword.lang);
  UDM_FREE(charset);
  UDM_FREE(lwrd);
  return UdmStopListListAdd(&Conf->StopWord, &StopList);
}


void
UdmStopListListInit(UDM_STOPLISTLIST *Lst)
{
  bzero((void*)Lst, sizeof(Lst[0]));
}


void
UdmStopListListFree(UDM_STOPLISTLIST *Lst)
{
  size_t i;
  for (i= 0; i < Lst->nitems; i++)
    UdmStopListFree(&Lst->Item[i]);
  UDM_FREE(Lst->Item);
}


int
UdmStopListListAdd(UDM_STOPLISTLIST *Lst, UDM_STOPLIST *Item)
{
  size_t nbytes= (Lst->nitems + 1) * sizeof(UDM_STOPLIST);
  if (!(Lst->Item= (UDM_STOPLIST*) UdmRealloc(Lst->Item, nbytes)))
    return UDM_ERROR;
  Lst->Item[Lst->nitems++]= Item[0];
  return UDM_OK;
}


UDM_STOPWORD*
UdmStopListListFind(UDM_STOPLISTLIST *SLL,
                    const char *word, const char *lang)
{
  UDM_STOPWORD Key;
  size_t i;
  char tmp[128];
  Key.word= tmp;

  udm_snprintf(tmp, sizeof(tmp), "%s", word);
  
  for (i= 0; i < SLL->nitems; i++)
  {
    UDM_STOPLIST *SL= &SLL->Item[i];
    UDM_STOPWORD *S;
    if (lang && lang[0] && strcmp(SL->lang, lang))
      continue;
    if ((S= UdmBSearch(&Key, SL->StopWord, SL->nstopwords, sizeof(UDM_STOPWORD), cmpstop)))
      return S;
  }
  
  return NULL;
}
