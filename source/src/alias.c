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
#include "udm_alias.h"
#include "udm_match.h"
#include "udm_log.h"


int
UdmAliasProg(UDM_AGENT *Indexer,
             const char *alias_prog,
             const char *argument,
             char *res, size_t rsize)
{
  FILE    *aprog;
  char    *cmd;
  char    *arg = NULL;
  char    *ares=NULL;
  char    *args[1];
  const char  *a;
  char    *c;
  size_t          arg_len, cmd_len;
  
  arg= (char*)UdmMalloc(arg_len= (2 * strlen(argument) + 1));
  if (arg == NULL)
    return UDM_ERROR;
  cmd= (char*)UdmMalloc(cmd_len= (arg_len + 2 * strlen(alias_prog) + 1));
  if (cmd == NULL)
  {
    UDM_FREE(arg);
    return UDM_ERROR;
  }


  /* Escape command argument */
  for (a= argument, c=arg; *a ; a++, c++)
  {
    switch(*a)
    {
      case '\\':
      case '\'':
      case '\"':
        (*c)='\\';
        c++;
        break;
      default:
        break;
    }
    *c=*a;
  }
  *c= '\0';
  args[0]= arg;
  
  UdmBuildParamStr(cmd, cmd_len, alias_prog, args, 1);
  aprog= popen(cmd,"r");
  
  UdmLog(Indexer, UDM_LOG_EXTRA, "Starting AliasProg: '%s'", cmd);
  
  if(aprog)
  {
    ares= fgets(res,(int)rsize,aprog);
    res[rsize-1]= '\0';
    pclose(aprog);
    if (!ares)
    {
      UdmLog(Indexer,UDM_LOG_ERROR,"AliasProg didn't return result: '%s'",cmd);
      UDM_FREE(cmd);
      UDM_FREE(arg);
      return(UDM_ERROR);
    }
  }
  else
  {
    UdmLog(Indexer,UDM_LOG_ERROR,"Can't start AliasProg: '%s'",cmd);
    UDM_FREE(cmd); UDM_FREE(arg);
    return(UDM_ERROR);
  }
  if(ares[0])
  {
    ares+= strlen(ares);
    ares--;
    while ( (ares>=res) && strchr(" \r\n\t",*ares))
    {
      *ares='\0';
      ares--;
    }
  }
  UDM_FREE(cmd);
  UDM_FREE(arg);
  return UDM_OK;
}
