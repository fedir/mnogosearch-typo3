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
#include "udm_env.h"
#include "udm_utils.h"
#include "udm_unicode.h"
#include "udm_unidata.h"
#include "udm_word.h"
#include "udm_synonym.h"
#include "udm_conf.h"


void UdmSynonymListInit(UDM_SYNONYMLIST * List)
{
     bzero((void*)List, sizeof(*List));
}


/*
  Returns the number of separators in a complex word.
  Needed for complex synonyms.
  
  TODO: currently doesn't work with extra spaces: "aaa   bbb"
*/
size_t
UdmMultiWordPhraseLength(const char *s)
{
  size_t res;
  for (res= 0; *s; s++)
  {
    if (*s == ' ')
      res++;
  }
  return res;
}

static void
AddSynonym(UDM_SYNONYMLIST *Syn, char *first, char *second, int origin)
{
  UDM_SYNONYM *trg= &Syn->Synonym[Syn->nsynonyms];
  size_t phrase_length= UdmMultiWordPhraseLength(first);
  if (Syn->max_phrase_length < phrase_length)
    Syn->max_phrase_length= phrase_length;
  trg->p= strdup(first);
  trg->s= strdup(second);
  /*
    We don't allow multi-word synonyms to get into loop
    at this point - for performance purposes.
  */
  trg->origin= phrase_length ? UDM_WORD_ORIGIN_SYNONYM_FINAL : origin;
  Syn->nsynonyms++;
}


#define UDM_SYN_MODE_ONEWAY    0
#define UDM_SYN_MODE_ROUNDTRIP 1
#define UDM_SYN_MODE_RETURN    2


__C_LINK int __UDMCALL UdmSynonymListLoad(UDM_ENV * Env,const char * filename)
{
  FILE         *f;
  char         str[512];
  char         lang[64]="";
  UDM_CHARSET  *cs=NULL;
  UDM_CONV     file_lcs;
  int          mode= UDM_SYN_MODE_ROUNDTRIP;
  int          origin= UDM_WORD_ORIGIN_SYNONYM;
  UDM_UNIDATA  *unidata= Env->unidata;
  UDM_SYNONYMLIST Synonyms;
  size_t       lineno= 0;
  
  UdmSynonymListInit(&Synonyms);
  
  if(!(f=fopen(filename,"r")))
  {
    udm_snprintf(Env->errstr,sizeof(Env->errstr)-1,"Can't open synonyms file '%s'",filename);
    return UDM_ERROR;
  }
  
  for (lineno= 1; fgets(str,sizeof(str),f); lineno++)
  {
    if(str[0]=='#'||str[0]==' '||str[0]=='\t'||str[0]=='\r'||str[0]=='\n')continue;
    
    if(!strncmp(str,"Charset:",8))
    {
      char * lasttok;
      char * charset;
      
      if((charset = udm_strtok_r(str + 8, " \t\n\r", &lasttok)))
      {
        if(!(cs=UdmGetCharSet(charset)))
        {
          udm_snprintf(Env->errstr, sizeof(Env->errstr),
                       "Unknown charset '%s' in synonyms file '%s'", charset, filename);
          fclose(f);
          return UDM_ERROR;
        }
        UdmConvInit(&file_lcs,cs,Env->lcs,UDM_RECODE_HTML_NONASCII);
      }
    }
    else if(!strncmp(str,"Language:",9))
    {
      char *lasttok, *l;
      if((l = udm_strtok_r(str + 9, " \t\n\r", &lasttok)))
        strncpy(lang,l,sizeof(lang)-1);
    }
    else if (!strncasecmp(str, "Mode:", 5))
    {
      char *lasttok, *l;
      for (l= udm_strtok_r(str + 5, " ,\t\n\r", &lasttok) ; l;
           l= udm_strtok_r(NULL, " ,\t\n\r", &lasttok))
      {
        if (!strcasecmp(l, "oneway"))
        {
          mode= UDM_SYN_MODE_ONEWAY;
          continue;
        }
        else if (!strcasecmp(l, "reverse") || !strcasecmp(l, "roundtrip"))
        {
          mode= UDM_SYN_MODE_ROUNDTRIP;
          continue;
        }
        else if (!strcasecmp(l, "return"))
        {
          mode= UDM_SYN_MODE_RETURN;
          continue;
        }
        else if (!strcasecmp(l, "recursive"))
        {
          origin= UDM_WORD_ORIGIN_SYNONYM;
          continue;
        }
        else if (!strcasecmp(l, "final"))
        {
          origin= UDM_WORD_ORIGIN_SYNONYM_FINAL;
          continue;
        }
        else
        {
          udm_snprintf(Env->errstr, sizeof(Env->errstr),
                       "Bad Mode command in synonym file %s:%d",
                       filename, lineno);
          fclose(f);
          return UDM_ERROR;
        }
      }
    }
    else
    {
      char      *av[255], tmp[512];
      size_t   ac, i, j, len;
      
      if(!cs)
      {
        udm_snprintf(Env->errstr,sizeof(Env->errstr)-1,
                     "No Charset command in synonyms file '%s'",filename);
        fclose(f);
        return UDM_ERROR;
      }
      if(!lang[0])
      {
        udm_snprintf(Env->errstr,sizeof(Env->errstr)-1,
                     "No Language command in synonyms file '%s'",filename);
        fclose(f);
        return UDM_ERROR;
      }
      
      len= UdmConv(&file_lcs, tmp, sizeof(tmp), str, strlen(str));
      tmp[len]= '\0';
      UdmStrToLower(unidata, cs, tmp, len);
      
      if ((ac= UdmGetArgs(tmp, av, 255)) < 2)
        continue;

      for (i = 0; i < (mode == UDM_SYN_MODE_RETURN ? ac - 1 : 1) ; i++)
      {
        for (j = i + 1; j < ac; j++)
        {
          if((Synonyms.nsynonyms + 1) >= Synonyms.msynonyms)
          {
            Synonyms.msynonyms+= 64;
            {
              size_t nbytes= sizeof(UDM_SYNONYM)*Synonyms.msynonyms;
              Synonyms.Synonym= (UDM_SYNONYM*)UdmRealloc(Synonyms.Synonym, nbytes);
            }
          }

          /* Add direct order */
          if (mode == UDM_SYN_MODE_ONEWAY || mode == UDM_SYN_MODE_ROUNDTRIP)
            AddSynonym(&Synonyms, av[i], av[j], origin);
          
          /* Add reverse order */
          if (mode == UDM_SYN_MODE_RETURN || mode == UDM_SYN_MODE_ROUNDTRIP)
            AddSynonym(&Synonyms, av[j], av[i], origin);
        }
      }
    }
  }
  fclose(f);
  udm_snprintf(Synonyms.fname, sizeof(Synonyms.fname), "%s", filename);
  udm_snprintf(Synonyms.cset, sizeof(Synonyms.cset), "%s", cs->name);
  udm_snprintf(Synonyms.lang, sizeof(Synonyms.lang), "%s", lang);
  UdmSynonymListListAdd(&Env->Synonym, &Synonyms);
  return UDM_OK;
}

void UdmSynonymListFree(UDM_SYNONYMLIST * List)
{
  size_t i;
     
  for(i=0;i<List->nsynonyms;i++)
  {
    UdmFree(List->Synonym[i].p);
    UdmFree(List->Synonym[i].s);
  }
  UDM_FREE(List->Synonym);
}

static int cmpsyn(const void * v1,const void * v2)
{
  const char *s1= ((const UDM_SYNONYM*)v1)->p;
  const char *s2= ((const UDM_SYNONYM*)v2)->p;
  return strcmp(s1, s2);
}

__C_LINK void __UDMCALL UdmSynonymListSort(UDM_SYNONYMLIST * List)
{
  if(List->nsynonyms)
    UdmSort(List->Synonym,List->nsynonyms,sizeof(UDM_SYNONYM),&cmpsyn);
}


static void
UdmWideWordListAddSynonym(UDM_WIDEWORDLIST *Res,
                          const UDM_WIDEWORD *pattern,
                          const UDM_SYNONYM *syn)
{
  UDM_WIDEWORD ww= *pattern;
  ww.origin= syn->origin;
  UdmWideWordListAddLike(Res, &ww, syn->s);
}



/*
  Traverse through a synonym list starting
  from "medium" towards the beginning or the
  end of the list and adding all synonyms
  equal to "word".
  "medium" must be a valid pointer to some word
  in the list.
*/
static void
UdmWideWordListAddSynonymIterate(UDM_WIDEWORDLIST *Res,
                                 const UDM_SYNONYMLIST *List,
                                 const UDM_SYNONYM *medium,
                                 const UDM_WIDEWORD *pattern,
                                 const char *word,
                                 int direction)
{
  UDM_SYNONYM *first= List->Synonym;
  UDM_SYNONYM *last= List->Synonym + List->nsynonyms;
  for ( ; medium >= first && medium < last; medium+= direction)
  {
    if (strcmp(word, medium->p))
      break;
    UdmWideWordListAddSynonym(Res, pattern, medium);
  }
}


UDM_WIDEWORDLIST *
UdmSynonymListFind(UDM_WIDEWORDLIST *Res,
                   const UDM_SYNONYMLIST *List,
                   const UDM_WIDEWORD *wword)
{
  UDM_SYNONYM syn, *res;
  
  /* Quickly skip empty lists, and skip final synonyms */
  if (!List->nsynonyms || wword->origin == UDM_WORD_ORIGIN_SYNONYM_FINAL)
    return NULL;

  syn.p= wword->word;
  
  if((res= UdmBSearch(&syn,List->Synonym,List->nsynonyms,sizeof(UDM_SYNONYM),&cmpsyn)))
  {
    size_t nnorm,i;
    
    /* Find first and last synonym */
    UdmWideWordListAddSynonymIterate(Res, List, res, wword, wword->word, -1);
    UdmWideWordListAddSynonymIterate(Res, List, res, wword, wword->word, +1);
    
    /* Now find each of them in reverse order */
    nnorm=Res->nwords;
    for(i=0; i < nnorm; i++)
    {
      UDM_WIDEWORD *ww= &Res->Word[i];
      /* Skip final synonyms */
      if (ww->origin == UDM_WORD_ORIGIN_SYNONYM_FINAL)
        continue;
      syn.p= ww->word;
      res= UdmBSearch(&syn,List->Synonym,List->nsynonyms,sizeof(UDM_SYNONYM),&cmpsyn);
      
      if(res)
      {
        /* Find first and last synonym */
        UdmWideWordListAddSynonymIterate(Res, List, res, wword, syn.p, -1);
        /* Note, "ww" is not valid here anymore, realloc could happen */
        UdmWideWordListAddSynonymIterate(Res, List, res, wword, syn.p, +1);
      }
    }
  }
  return Res;
}


void
UdmSynonymListListInit(UDM_SYNONYMLISTLIST *Lst)
{
  bzero((void*)Lst, sizeof(Lst[0]));
}


void
UdmSynonymListListFree(UDM_SYNONYMLISTLIST *Lst)
{
  size_t i;
  for (i= 0; i < Lst->nitems; i++)
    UdmSynonymListFree(&Lst->Item[i]);
  UDM_FREE(Lst->Item);
}


int
UdmSynonymListListAdd(UDM_SYNONYMLISTLIST *Lst, UDM_SYNONYMLIST *Item)
{
  size_t nbytes= (Lst->nitems + 1) * sizeof(UDM_SYNONYMLIST);
  if (!(Lst->Item= (UDM_SYNONYMLIST*) UdmRealloc(Lst->Item, nbytes)))
    return UDM_ERROR;
  Lst->Item[Lst->nitems++]= Item[0];
  return UDM_OK;
}


__C_LINK void __UDMCALL UdmSynonymListListSortItems(UDM_SYNONYMLISTLIST *List)
{
  size_t i;
  for (i= 0; i < List->nitems; i++)
    UdmSynonymListSort(&List->Item[i]);
}


UDM_WIDEWORDLIST*
UdmSynonymListListFind(const UDM_SYNONYMLISTLIST *SLL, UDM_WIDEWORD *word)
{
  size_t i;
  UDM_WIDEWORDLIST *Res= (UDM_WIDEWORDLIST *) UdmMalloc(sizeof(*Res));
  UdmWideWordListInit(Res);

  UDM_ASSERT(word->origin >= UDM_WORD_ORIGIN_QUERY &&
             word->origin <= UDM_WORD_ORIGIN_COLLATION);

  for (i= 0; i < SLL->nitems; i++)
    UdmSynonymListFind(Res, &SLL->Item[i], word);
  
  if (!Res->nwords)
  {
    UdmWideWordListFree(Res);
    UdmFree(Res);
    return NULL;
  }
  return Res;
}
