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
#include <time.h>

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_agent.h"
#include "udm_hrefs.h"
#include "udm_result.h"



__C_LINK UDM_AGENT * __UDMCALL
UdmAgentInit(UDM_AGENT *result,UDM_ENV * Env, int handle)
{
#ifdef USE_TRACE
  char filename[PATH_MAX];
#endif

  if (!result)
  {
    result=(UDM_AGENT*)UdmMalloc(sizeof(UDM_AGENT));
    bzero((void*)result, sizeof(*result));
    result->freeme=1;
  }
  else
  {
    bzero((void*)result, sizeof(*result));
  }
  
  time(&result->start_time);
  result->Conf= Env;
  result->handle= handle; /* Handle is used in multi-threaded version */
  result->action= UDM_OK;
  result->LangMap= (UDM_LANGMAP*)UdmMalloc(sizeof(UDM_LANGMAP));
  bzero((void*) result->LangMap, sizeof(UDM_LANGMAP));

#ifdef USE_TRACE
  udm_snprintf(filename, sizeof(filename), "%s/udm_agent.%d.trace",
               UdmVarListFindStr(&Env->Vars, "TmpDir", UDM_TMP_DIR), handle);
  result->TR= fopen(filename, "w");
  if (!result->TR)
  {
    fprintf(stderr, "Cannot open %s for writing, tracing to stderr\n",filename);
    fprintf(stderr, "If you don't want this message again,\n");
    fprintf(stderr, "Please make this file writtable, or\n");
    fprintf(stderr, "compile mnogosearch without --enable-trace\n");
    result->TR= stderr;
  }
#endif
  return(result);
}


__C_LINK void __UDMCALL
UdmAgentFree(UDM_AGENT *Indexer)
{
  int i;
  if(!Indexer)return;
#ifdef USE_TRACE
  if (Indexer->TR && Indexer->TR != stderr)
    fclose(Indexer->TR);
#endif
  UdmResultFree(&Indexer->Indexed);
  UDM_FREE(Indexer->LangMap);
  for(i= 0; i < UDM_FINDURL_CACHE_SIZE; i++)
    UDM_FREE(Indexer->UdmFindURLCache[i]);
  for(i= 0; i < UDM_SERVERID_CACHE_SIZE; i++)
    UDM_FREE(Indexer->ServerIdCache[i]);
  if(Indexer->freeme)
    UDM_FREE(Indexer);
}


__C_LINK void __UDMCALL
UdmAgentSetAction(UDM_AGENT *Indexer,int action)
{
  Indexer->action = action;
}


void
UdmAgentSetTask(UDM_AGENT *Indexer, const char *task)
{
  Indexer->State.task= task;
  Indexer->State.start_time= time(0);
}
