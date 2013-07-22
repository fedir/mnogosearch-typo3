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
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>

#include "udm_dirent.h"
#include "udm_common.h"
#include "udm_hash.h"
#include "udm_guesser.h"
#include "udm_utils.h"
#include "udm_unicode.h"
#include "udm_log.h"
#include "udm_vars.h"
#include "udm_mutex.h"

/* This should be last include */
#ifdef DMALLOC
#include "dmalloc.h"
#endif

/*#define DEBUG_GUESSER*/

typedef struct ct_match_st
{
  int offs_min; /* We'll use negative offsets for end-of-file offsets */
  int offs_max; /* in the future.                                     */
  const char *arg;
  size_t length;
  const char *content_type;
} UDM_CTMATCH;

static UDM_CTMATCH udm_magic[]=
{
  {0, 30, "<!DOCTYPE HTML", 14, "text/html"},
  {0, 30, "<!doctype html", 14, "text/html"},
  {0, 30, "<HEAD",          5,  "text/html"},
  {0, 30, "<head",          5,  "text/html"},
  {0, 30, "<TITLE",         6,  "text/html"},
  {0, 30, "<title",         6,  "text/html"},
  {0, 30, "<HTML",          5,  "text/html"},
  {0, 30, "<html",          5,  "text/html"},
  {0, 30, "<!--",           4,  "text/html"},
  {0, 30, "<H1",            3,  "text/html"},
  {0, 30, "<h1",            3,  "text/html"},
  {0, 30, "<BASE",          5,  "text/html"},
  {0, 30, "<base",          5,  "text/html"},
  {0,  1,   UDM_CSTR_WITH_LEN("{\\rtf"), "application/rtf"},
  {0,  1,   UDM_CSTR_WITH_LEN("Received: "), "message/rfc822"},
  {0, 1024, UDM_CSTR_WITH_LEN("\nReceived: "), "message/rfc822"},
  {0, 1024, UDM_CSTR_WITH_LEN("\nFrom: "),     "message/rfc822"},
  {0, 1024, UDM_CSTR_WITH_LEN("\nTo: "),       "message/rfc822"},
  {0, 1024, UDM_CSTR_WITH_LEN("\nSubject: "),  "message/rfc822"},
  {0, 4096, "\x00word/document.xml", 18, "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
  {0, 1024, NULL, 0, NULL}
};

const char *UdmGuessContentType(const char *begin, size_t length,
                                const char *default_type)
{
  UDM_CTMATCH *m;
  const char *beg, *end, *cur;
  for (m= udm_magic; m->content_type; m++)
  {
    beg= begin + m->offs_min;
    end= begin + length - m->length;
    if (end > begin + m->offs_max)
      end= begin + m->offs_max;
    for (cur= beg; cur < end; cur++)
    {
      if (!memcmp(cur, m->arg, m->length))
        return m->content_type;
    }
  }

  if (default_type)
    return default_type;

  /* Check if text/plain */
  end= length > 128 ? begin + 128 : begin + length;
  for (cur= begin; cur < end; cur++)
  {
    if (((unsigned char)cur[0]) < 9)
      return "application/unknown";
  }
  return "text/plain";
}


static UDM_LANGMAP *FindLangMap(UDM_LANGMAPLIST *L, const char *lang, const char *charset, const char *filename) {
  UDM_LANGMAP *o = NULL;
  register size_t i;

  for(i = 0; i < L->nmaps; i++) {
    if(
       (!strcasecmp(L->Map[i].charset, charset))
       &&
       (!strcasecmp(L->Map[i].lang, lang))
       ) {
      return &L->Map[i];
    }
  }

  if(L->nmaps == 0){
    o = L->Map = (UDM_LANGMAP*)UdmMalloc(sizeof(UDM_LANGMAP));
  }else{
    L->Map = (UDM_LANGMAP*)UdmRealloc(L->Map, (L->nmaps + 1) * sizeof(UDM_LANGMAP));
    o = &L->Map[L->nmaps];
  }
  if (o == NULL || L->Map == NULL) {
    fprintf(stderr,
            "Can't alloc/realloc for language map (%s, %s), nmaps: %d (%d)",
            lang, charset, (int) L->nmaps + 1, (int) ((L->nmaps + 1) * sizeof(UDM_LANGMAP)));
    return NULL;
  }
  bzero((void*)o, sizeof(UDM_LANGMAP));
  for (i = 0; i <= UDM_LM_HASHMASK; i++) o->memb[i].index = i;
  o->charset = (char*)UdmStrdup(charset);
  o->lang = (char*)UdmStrdup(lang);
  o->filename = (filename == NULL) ? NULL : (char*)UdmStrdup(filename);
  L->nmaps++;
  return o;
}

__C_LINK int __UDMCALL UdmLoadLangMapFile(UDM_LANGMAPLIST *L, const char * filename) {
     FILE * f;
     char str[1000];
     char *Ccharset = NULL, *Clanguage = NULL;
     UDM_LANGMAP *Cmap = NULL;
     UDM_CHARSET * cs;

     f=fopen(filename,"r");
     if(!f){
          fprintf(stderr, "Can't open LangMapFile '%s'\n", filename);
          return UDM_ERROR;
     }
     while(fgets(str,sizeof(str),f)){
          if(str[0]=='#'||str[0]==' '||str[0]=='\t')continue;

          if(!strncmp(str,"Charset:",8)){
               
               char * charset, * lasttok;
               UDM_FREE(Ccharset);
               if((charset = udm_strtok_r(str+8," \t\n\r",&lasttok))){
                       const char *canon = UdmCharsetCanonicalName(charset);
                    if (canon) {
                      Ccharset = (char*)UdmStrdup(canon);
                    } else {
                      fprintf(stderr, "Charset: %s in %s not supported\n", charset, filename);
                      return UDM_ERROR;
                    }
               }
          }else
          if(!strncmp(str,"Language:",9)){
               char * lang, *lasttok;
               UDM_FREE(Clanguage);
               if((lang = udm_strtok_r(str+9," \t\n\r",&lasttok))){
                    Clanguage = (char*)UdmStrdup(lang);
               }
          }else{
               char *s;
               int count;
               
               if(!(s=strchr(str,'\t')))continue;
               if(Clanguage == NULL) {
                 fprintf(stderr, "No language definition in LangMapFile '%s'\n", filename);
                 return UDM_ERROR;
               }

               if(Ccharset == NULL) {
                 fprintf(stderr, "No charset definition in LangMapFile '%s'\n", filename);
                 return UDM_ERROR;
               }
               if(!(cs = UdmGetCharSet(Ccharset))) {
                 fprintf(stderr, "Unknown charset '%s' in LangMapFile '%s'\n", Ccharset, filename);
                 return UDM_ERROR;
               }
               if (Cmap == NULL) {
		 Cmap = FindLangMap(L, Clanguage, Ccharset, filename);
		 UdmSort(Cmap->memb, UDM_LM_HASHMASK + 1, sizeof(UDM_LANGITEM), (udm_qsort_cmp)UdmLMcmpIndex);
	       }
               if (Cmap == NULL) return UDM_ERROR;
               *s='\0';

               if(((count=atoi(s+1))==0)||(strlen(str)<1)||(strlen(str)>UDM_LM_MAXGRAM))
                    continue;

               for(s=str;*s;s++){
                    if(*s=='_')*s=' ';
               }
               if(*str){
                    register int hindex;
                    hindex = ((unsigned int)UdmStrHash32(str)) & UDM_LM_HASHMASK;
                    Cmap->memb[hindex].count += count;
                    strcpy(Cmap->memb[hindex].str, str);
               }
          }
     }
     fclose(f);
     UDM_FREE(Clanguage);
     UDM_FREE(Ccharset);
     
     if (Cmap != NULL) UdmPrepareLangMap(Cmap);
     return UDM_OK;
}


void UdmLangMapListFree(UDM_LANGMAPLIST *List){
     size_t i;
     
     for(i=0;i<List->nmaps;i++){
          UDM_FREE(List->Map[i].charset);
          UDM_FREE(List->Map[i].lang);
          UDM_FREE(List->Map[i].filename);
     }
     UDM_FREE(List->Map);
     List->nmaps = 0;
}

void UdmLangMapListSave(UDM_LANGMAPLIST *List) {
     size_t i, j, minv;
     FILE *out;
     UDM_LANGMAP *Cmap;
     char name[128];
     
     for(i = 0; i < List->nmaps; i++) {
       Cmap = &List->Map[i];
       if (Cmap->needsave) {
         if (Cmap->filename == NULL) {
           udm_snprintf(name, 128, "%s.%s.lm", Cmap->lang, Cmap->charset);
           if ((out = fopen(name, "w")) == NULL) continue;
         } else {
           if ((out = fopen(Cmap->filename, "w")) == NULL) continue;
         }
         fprintf(out, "#\n");
         fprintf(out, "# Autoupdated.\n");
         fprintf(out, "#\n\n");
         fprintf(out, "Language: %s\n", Cmap->lang);
         fprintf(out, "Charset:  %s\n", Cmap->charset);
         fprintf(out, "\n\n");
         UdmSort(Cmap->memb, UDM_LM_HASHMASK + 1, sizeof(UDM_LANGITEM), (udm_qsort_cmp)UdmLMcmpCount);
	 minv = (Cmap->memb[UDM_LM_TOPCNT - 1].count > 1000) ? 1000 : Cmap->memb[UDM_LM_TOPCNT - 1].count;
         for(j = 0; j < UDM_LM_TOPCNT; j++) {
           Cmap->memb[j].count -= Cmap->memb[UDM_LM_TOPCNT - 1].count - minv;
         }
         for(j = 0; j < UDM_LM_TOPCNT; j++) {
          char * s;
          if(!Cmap->memb[j].count) break;
          
          for(s = Cmap->memb[j].str; *s; s++)
               if(*s == ' ') *s='_';
          
          fprintf(out, "%s\t%d\n", Cmap->memb[j].str, (int) Cmap->memb[j].count);
         }
         fclose(out);
       }
     }
}

void UdmBuildLangMap(UDM_LANGMAP * map,const char * text,size_t textlen, int StrFlag){
     const char * end=text+textlen;
     int prevb=' ';

     for(;text<=end;text++){
          char buf[UDM_LM_MAXGRAM+3];
          size_t buflen=0;
          const char * t;
          int code;
          int prev=0;

          code=(unsigned char)(*text);
          if(code<' ')continue;
          if((code==' ')&&(prevb==' '))continue;
          prevb=code;

          t=text;
          for(buflen=0;buflen<UDM_LM_MAXGRAM;buflen++){
               int hindex;

               for(;t<=end;t++){
                    code=(unsigned char)(*t);
                    if(code<' ')continue;
                    if((code==' ')&&(prev==' '))continue;
                    prev=code;
                    break;
               }
               if(t>end)break;
               t++;

               buf[buflen]=code;
/*               if (buflen == 0) continue;*/
               buf[buflen+1]='\0';

               hindex = UdmHash32(buf, buflen + 1);
               hindex=((unsigned int)hindex ) & UDM_LM_HASHMASK;
               map->memb[hindex].count++;

#ifdef DEBUG_GUESSER
               /* Print collision */
               if(map->memb[hindex].str[0]){
                    int res;
                    res=strcmp(map->memb[hindex].str,buf);
                    if(res){
                         printf("Coll %04X '%s' '%s'\n",hindex,map->memb[hindex].str,buf);
                         strcpy(map->memb[hindex].str,buf);
                    }
               }
#endif
               if (StrFlag) strcpy(map->memb[hindex].str, buf);
          }
     }
}


int UdmLMstatcmp(const void * i1, const void * i2){
  register const UDM_MAPSTAT *s1 = (const UDM_MAPSTAT *)i1;
  register const UDM_MAPSTAT *s2 = (const UDM_MAPSTAT *)i2;
  if (s1->hits > s2->hits) return -1;
  if (s1->hits < s2->hits) return 1;
  if (s1->miss < s2->miss) return -1;
  if (s1->miss > s2->miss) return 1;

  return 0;
}

int UdmLMcmpCount(UDM_LANGITEM *m1, UDM_LANGITEM *m2) {
     if (m2->count > m1->count) return(1);
     if (m2->count < m1->count) return(-1);
     return(0);
}

int UdmLMcmpIndex(UDM_LANGITEM *m1, UDM_LANGITEM *m2) {
     if (m1->index > m2->index) return(1);
     if (m1->index < m2->index) return(-1);
     return(0);
}

void UdmPrepareLangMap(UDM_LANGMAP * map){

     UdmSort(map->memb, UDM_LM_HASHMASK + 1, sizeof(UDM_LANGITEM), (udm_qsort_cmp)UdmLMcmpCount);
     UdmSort(map->memb, UDM_LM_TOPCNT, sizeof(UDM_LANGITEM), (udm_qsort_cmp)UdmLMcmpIndex);
}


void UdmCheckLangMap(UDM_LANGMAP * map0, UDM_LANGMAP * map1, UDM_MAPSTAT *stat, size_t InfMiss) {
     register int i;
     UDM_LANGITEM *HIT;

     stat->hits = stat->miss = 0;
     for (i = 0; i < UDM_LM_TOPCNT; i++) {
       if ( (HIT = UdmBSearch(&map1->memb[i], map0->memb, UDM_LM_TOPCNT, sizeof(UDM_LANGITEM), (udm_qsort_cmp)UdmLMcmpIndex)) == NULL) {
	 stat->miss++;
       } else {
	 stat->hits += UDM_LM_TOPCNT - abs(HIT - map0->memb);
       }
       if (stat->miss > InfMiss) break;
     }
#if 0
     printf("Hit: %d Miss:%d '%s''%s'\n",stat->hits,stat->miss,map0->lang,map0->charset);
#endif
}


int  UdmGuessCharSet(UDM_AGENT *Indexer, UDM_DOCUMENT * Doc,UDM_LANGMAPLIST *List,UDM_LANGMAP *LangMap){
     size_t i;
     UDM_MAPSTAT * mapstat = NULL;
     const char *lang = UdmVarListFindStr(&Doc->Sections, "Content-Language", "");
     const char *meta_lang = UdmVarListFindStr(&Doc->Sections, "Meta-Language", "");
     const char *server_charset = UdmVarListFindStr(&Doc->Sections, "Server-Charset", "");
     const char *meta_charset = UdmVarListFindStr(&Doc->Sections, "Meta-Charset", "");
     const char *charset = UdmVarListFindStr(&Doc->Sections, "RemoteCharset", "");
     size_t InfMiss = UDM_LM_TOPCNT + 1;
     int have_server_lang = (*lang != '\0');
     int use_meta, update_lm;
     
     UDM_GETLOCK(Indexer,UDM_LOCK_CONF);
     use_meta = !strcasecmp(UdmVarListFindStr(&Indexer->Conf->Vars, "GuesserUseMeta", "yes"), "yes");
     update_lm = !strcasecmp(UdmVarListFindStr(&Indexer->Conf->Vars, "LangMapUpdate", "no"), "yes");
     UDM_RELEASELOCK(Indexer,UDM_LOCK_CONF);

     if (!List->nmaps)
     {
       /*
         Choose the first non-empty charset in this order:
         - server_charset, i.e. the one from HTTP headers
         - meta_charset, from <META NAME="Content-Type">,
           or <?xml encoding=""?>
         - RemoteCharset, the default one for this server
       */
       if (*server_charset)
         charset= server_charset;
       else if (use_meta && *meta_charset)
         charset= meta_charset;
     }
     else if (*charset == '\0')
     {
       if (*server_charset != '\0' && (!use_meta ||
                                       *meta_charset == '\0' ||
                                       !strcasecmp(server_charset, meta_charset) ))
         charset= server_charset;
       else if (use_meta && *server_charset == '\0' && *meta_charset != '\0')
         charset= meta_charset;
     }
     UdmVarListReplaceStr(&Doc->Sections, "Charset", charset);

     if (*lang == '\0') {
       if (*meta_lang != '\0' && use_meta) UdmVarListReplaceStr(&Doc->Sections, "Content-Language", lang = meta_lang);
     }
     
     /* 
          TODO for Guesser
          
          There are three sources to detect charset:
          1. HTTP header:  Content-Type: ... charset=XXX
          2. <META HTTP-EQUIV="Content-Type" Contnet="... charset=YYY">
          3. ZZZ[i] - array of guessed charsets in mapstat[]
          good(ZZZ[n]) means that guesser returned good results for n.

          Now we may have various combinations:
          Simple situations, non-guessed and guessed charsets
          seem to be the same. At least one of non-guessed
          charset is the same with the best guessed charset
          and guessed charset gave good results:

          1. XXX==YYY==ZZZ[0],      good(ZZZ[0]). Take XXX value.
          2. No YYY, XXX=ZZZ[0] and good(ZZZ[0]). Take XXX value.
          3. No XXX, YYY=ZZZ[0] and good(ZZZ[0]). Take YYY value.
          4. XXX<>YYY, XXX==ZZZ[0], good(ZZZ[0]). Take XXX value.
          5. XXX<>YYY, YYY==ZZZ[0], good(ZZZ[0]). Take XXX value.
               4 and 5 seem to be webmaster mistake.

          There are the same fith situations when ZZZ[x] is still good 
          enough, but it is not the best one, i.e. x>0 
          Choose charset in the same way.
     */

     if ((*charset == '\0' || *lang == '\0') && List->nmaps)
     {

       /* Prepare document langmap */
       UdmPrepareLangMap(LangMap);

       /* Allocate memory for comparison statistics */
       if ((mapstat=(UDM_MAPSTAT *)UdmMalloc((List->nmaps + 1) * sizeof(UDM_MAPSTAT))) == NULL) {
	 UdmLog(Indexer, UDM_LOG_ERROR,
	        "Can't alloc momory for UdmGuessCharSet (%d bytes)",
	        (int) (List->nmaps*sizeof(UDM_MAPSTAT)));
	 return UDM_ERROR;
       }
     
       UDM_GETLOCK(Indexer,UDM_LOCK_CONF);
       for(i=0;i<List->nmaps;i++){
          mapstat[i].map=&List->Map[i];
          if ((*charset == '\0') && (*lang == '\0')) {
            UdmCheckLangMap(&List->Map[i], LangMap, &mapstat[i], InfMiss);
          } else if ((*charset) && 
                  (!strcasecmp(mapstat[i].map->charset, charset) || !strcasecmp(mapstat[i].map->charset, meta_charset))  ) {
            UdmCheckLangMap(&List->Map[i], LangMap, &mapstat[i], InfMiss);
          } else if ((*lang) && !strncasecmp(mapstat[i].map->lang, lang, strlen(mapstat[i].map->lang) )) {
            UdmCheckLangMap(&List->Map[i], LangMap, &mapstat[i], InfMiss);
          } else {
            mapstat[i].hits = 0;
	    mapstat[i].miss = UDM_LM_TOPCNT + 1;
          }
          if (mapstat[i].miss < InfMiss) InfMiss = mapstat[i].miss;
       }
       UDM_RELEASELOCK(Indexer,UDM_LOCK_CONF);

       /* Sort statistics in quality order */
       if (List->nmaps > 1) UdmSort(mapstat, List->nmaps, sizeof(UDM_MAPSTAT), &UdmLMstatcmp);

       /* Display results, best is shown first */

       for (i = 0; i < ((List->nmaps < 5) ? List->nmaps : 5); i++)
         UdmLog(Indexer,UDM_LOG_EXTRA, "Guesser: %dh:%dm %s-%s",
                (int) mapstat[i].hits, (int) mapstat[i].miss,
                mapstat[i].map->lang, mapstat[i].map->charset);


       if (*server_charset != '\0' || *meta_charset != '\0')
	 for(i=0;i<List->nmaps;i++){

	   if (mapstat[i].map->lang && (*lang != '\0') && (!strncasecmp(mapstat[i].map->lang, lang, strlen(mapstat[i].map->lang) ))) {
	     if(mapstat[i].map->charset && !strcasecmp(mapstat[i].map->charset, server_charset)) {
	       UdmVarListReplaceStr(&Doc->Sections, "Charset", charset = mapstat[i].map->charset);
	     } else if(mapstat[i].map->charset && !strcasecmp(mapstat[i].map->charset, meta_charset)) {
	       UdmVarListReplaceStr(&Doc->Sections, "Charset", charset = mapstat[i].map->charset);
	     }
	   } else {
	     if(mapstat[i].map->charset && !strcasecmp(mapstat[i].map->charset, server_charset)) {
	       UdmVarListReplaceStr(&Doc->Sections, "Charset", charset = mapstat[i].map->charset);
               UdmVarListReplaceStr(&Doc->Sections, "Content-Language", lang = mapstat[i].map->lang);
	     } else if(mapstat[i].map->charset && !strcasecmp(mapstat[i].map->charset, meta_charset)) {
	       UdmVarListReplaceStr(&Doc->Sections, "Charset", charset = mapstat[i].map->charset);
               UdmVarListReplaceStr(&Doc->Sections, "Content-Language", lang = mapstat[i].map->lang);
	     }
	   }
	   if (*charset != '\0') break;
	 }
          
       for(i=0;i<List->nmaps;i++){

          if ((*lang != '\0') && (*charset != '\0')) break;
          if(mapstat[i].map->lang && *lang == '\0'/* && (*charset == '\0' || (!strcasecmp(mapstat[i].map->charset, charset)))*/ ){
               UdmVarListReplaceStr(&Doc->Sections, "Content-Language", lang = mapstat[i].map->lang);
          }
          if (mapstat[i].map->charset && *charset == '\0' && (!strcmp(lang, mapstat[i].map->lang)) ) {
               UdmVarListReplaceStr(&Doc->Sections, "Charset", charset = mapstat[i].map->charset);
          }
#if 0
          fprintf(stderr, "Guesser: %dh:%dm %s-%s\n",mapstat[i].hits, mapstat[i].miss,mapstat[i].map->lang,mapstat[i].map->charset);
#endif
       }
       if (List->nmaps > 0 && mapstat[0].map->charset && (*charset == '\0') ) {
	     UdmVarListReplaceStr(&Doc->Sections, "Charset", charset = mapstat[0].map->charset);
       }
       if (List->nmaps > 0 && mapstat[0].map->lang && (*lang == '\0') ) {
             UdmVarListReplaceStr(&Doc->Sections, "Content-Language", lang = mapstat[0].map->lang);
       }
     }

     if (have_server_lang && (*server_charset != '\0' || (*meta_charset != '\0' && use_meta ))) {
       if (update_lm && List->nmaps) {
         UDM_LANGMAP *Cmap;

	 UDM_GETLOCK(Indexer,UDM_LOCK_CONF);
         if ((Cmap = FindLangMap(&Indexer->Conf->LangMaps, lang, charset, NULL)) != NULL) {
	   UdmSort(Cmap->memb, UDM_LM_HASHMASK + 1, sizeof(UDM_LANGITEM), (udm_qsort_cmp)UdmLMcmpIndex);
           for (i = 0; i < UDM_LM_HASHMASK+1; i++) {
	     if (Cmap->memb[i].count == 0) {
	       strcpy(Cmap->memb[i].str, LangMap->memb[i].str);
	     }
	     Cmap->memb[i].count += LangMap->memb[i].count;
           }
           UdmPrepareLangMap(Cmap);
           Cmap->needsave = 1;
	   UDM_RELEASELOCK(Indexer,UDM_LOCK_CONF);
           UdmLog(Indexer, UDM_LOG_EXTRA, "Lang map: %s.%s updated", Cmap->lang, Cmap->charset);
         } else {
	   UDM_RELEASELOCK(Indexer,UDM_LOCK_CONF);
	 }
       }
     }
     UDM_FREE(mapstat);
     return UDM_OK;
}
