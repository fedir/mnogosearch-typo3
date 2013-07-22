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
#include <sys/types.h>

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_env.h"
#include "udm_parser.h"
#include "udm_robots.h"
#include "udm_host.h"
#include "udm_hrefs.h"
#include "udm_server.h"
#include "udm_url.h"
#include "udm_proto.h"
#include "udm_alias.h"
#include "udm_log.h"
#include "udm_match.h"
#include "udm_stopwords.h"
#include "udm_guesser.h"
#include "udm_vars.h"
#include "udm_synonym.h"
#include "udm_doc.h"
#include "udm_result.h"
#include "udm_spell.h"
#include "udm_db.h"
#include "udm_db_int.h"
#include "udm_chinese.h"
#include "udm_mutex.h"

/**************************** DBAddr ***********************************/
void
UdmEnvSetDirs(UDM_ENV *Env)
{
  char dir[256];
  UdmGetDir(dir, sizeof(dir), UDM_DIRTYPE_CONF);
  UdmVarListReplaceStr(&Env->Vars, "ConfDir", dir);

  UdmGetDir(dir, sizeof(dir), UDM_DIRTYPE_SHARE);
  UdmVarListReplaceStr(&Env->Vars, "ShareDir", dir);

  UdmGetDir(dir, sizeof(dir), UDM_DIRTYPE_VAR);
  UdmVarListReplaceStr(&Env->Vars, "VarDir", dir);

  UdmGetDir(dir, sizeof(dir), UDM_DIRTYPE_VAR);
  UdmVarListReplaceStr(&Env->Vars, "TmpDir", dir);
}


__C_LINK UDM_ENV * __UDMCALL
UdmEnvInit(UDM_ENV *Conf)
{

  if (!Conf)
  {
    Conf=(UDM_ENV *)UdmMalloc(sizeof(UDM_ENV));
    bzero((void*)Conf, sizeof(*Conf));
    Conf->freeme=1;
  }
  else
  {
    bzero((void*)Conf, sizeof(*Conf));
  }
  
  Conf->WordParam.min_word_len=1;
  Conf->WordParam.max_word_len=32;
  Conf->url_number=0x7FFFFFFF;
  Conf->lcs=UdmGetCharSet("latin1");
  Conf->bcs=UdmGetCharSet("latin1");
  Conf->unidata= udm_unidata_default;
  
  return(Conf);
}


__C_LINK void __UDMCALL
UdmEnvFree(UDM_ENV * Env)
{
#ifdef MECAB
  mecab_destroy (Env->mecab);
#endif
  UdmRobotListFree(&Env->Robots);
  UdmHrefListFree(&Env->Hrefs);
  UdmResultFree(&Env->Targets);
  UdmParserListFree(&Env->Parsers);
  UdmStopListListFree(&Env->StopWord);
  UdmHostListFree(&Env->Hosts);
  
  UdmMatchListFree(&Env->MimeTypes);
  UdmMatchListFree(&Env->Encodings);
  UdmMatchListFree(&Env->Aliases);
  UdmMatchListFree(&Env->ReverseAliases);
  UdmMatchListFree(&Env->Filters);
  UdmMatchListFree(&Env->SectionFilters);
  UdmMatchListFree(&Env->SectionMatch);
  UdmMatchListFree(&Env->SectionHdrMatch);
  UdmMatchListFree(&Env->SectionGsrMatch);
    
  UdmSynonymListListFree(&Env->Synonym);
  UdmVarListFree(&Env->Sections);
  UdmVarListFree(&Env->XMLEnterHooks);
  UdmVarListFree(&Env->XMLLeaveHooks);
  UdmVarListFree(&Env->XMLDataHooks);
  UdmVarListFree(&Env->Cookies);
  UdmLangMapListSave(&Env->LangMaps);
  UdmLangMapListFree(&Env->LangMaps);
  UdmServerListFree(&Env->Servers);
  UdmSpellListListFree(&Env->Spells);
  UdmAffixListListFree(&Env->Affixes);
  UdmVarListFree(&Env->Vars);
  UdmChineseListFree(&Env->Chi);
  UdmChineseListFree(&Env->Thai);
  UdmDBListFree(&Env->dbl);
  if(Env->freeme)UDM_FREE(Env);
}


__C_LINK char * __UDMCALL
UdmEnvErrMsg(UDM_ENV * Conf)
{
  size_t  i;
  UDM_DB *db;

  for(i = 0; i<Conf->dbl.nitems; i++){
    db = &Conf->dbl.db[i];
    if (db->errcode) {
      char *oe = (char*)UdmStrdup(Conf->errstr);
      udm_snprintf(Conf->errstr, 2048, "DB err: %s - %s", db->errstr, oe);
      UDM_FREE(oe);
    }
  }
  return(Conf->errstr);
}

__C_LINK int __UDMCALL UdmSetLockProc(UDM_ENV * Conf,
                      __C_LINK void (*proc)(UDM_AGENT *A,
                                            int command, int type,
                                            const char *f,int l))
{
  Conf->LockProc= proc;
  return 0;
}


int
UdmEnvPrepare(UDM_ENV *Env)
{
#ifdef MECAB
   if (!Env->mecab)
   {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
     static char *mecab_argv[]=
     {
       (char *) "mecab",
       (char *) "-F%m\" \"",
       (char *) "-E\" \"",
       (char *) "-B\" \"",
       NULL
     };
#pragma GCC diagnostic pop

     if (!(Env->mecab= mecab_new (4, mecab_argv)))
     {
       udm_snprintf(Env->errstr, sizeof(Env->errstr) - 1,
                    "mecab_new failed: %s", mecab_strerror(Env->mecab));
       return UDM_ERROR;
     }
   }
#endif

  if(Env->Spells.nitems && Env->Affixes.nitems)
  {
    char *err= Env->errstr;
    int flags= strcasecmp(UdmVarListFindStr(&Env->Vars,
                                            "IspellUsePrefixes","no"),"no");
    flags= flags ? 0 : UDM_SPELL_NOPREFIX;
    if (UdmSpellListListLoad(&Env->Spells, err, 128) ||
        UdmAffixListListLoad(&Env->Affixes, flags, err, 128))
    return UDM_ERROR;
  }
  UdmSynonymListListSortItems(&Env->Synonym);
  return UDM_OK;
}


int
UdmEnvDBListAdd(UDM_ENV *Env, const char *dbaddr, int mode)
{
  int rc;
  if(UDM_OK != (rc= UdmDBListAdd(&Env->dbl, dbaddr, mode)))
  {
    udm_snprintf(Env->errstr, sizeof(Env->errstr), "%s", Env->dbl.errstr);
    rc=UDM_ERROR;
  }
  return rc;
}
