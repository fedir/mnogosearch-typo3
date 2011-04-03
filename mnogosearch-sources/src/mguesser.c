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
#include <math.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include "udm_dirent.h"
/*#include "udm_common.h"*/
#include "udm_crc32.h"
#include "udm_guesser.h"
#include "udm_utils.h"
#include "udm_unicode.h"
#include "udm_log.h"

/*#define DEBUG_GUESSER*/

int UdmLoadLangMapList(UDM_LANGMAPLIST *L, const char * mapdir)
{
  DIR *dir;
  struct dirent *item;
  char fullname[1024]= "";
  char name[1024]= "";
  int res= UDM_OK;

  if(!(dir= opendir(mapdir)))
    return 0;

  while((item= readdir(dir)))
  {
    char *tail;
    strcpy(name, item->d_name);
    if((tail= strstr(name,".lm")))
    {
      *tail= '\0';
      sprintf(fullname,"%s/%s",mapdir,item->d_name);
      if (UDM_OK != (res= UdmLoadLangMapFile(L, fullname)))
        return res;
    }
  }
  closedir(dir);
  return UDM_OK;
}




/***************************************************************/

static void UdmPrintLangMap(UDM_LANGMAP *map, size_t model_length)
{
  size_t i;
  
  printf("#\n");
  printf("# Model data length: %d\n", model_length);
  printf("#\n");
  printf("\n");
  printf("Language: %s\n",map->lang);
  printf("Charset:  %s\n",map->charset);
  printf("\n");
  printf("\n");
  UdmSort(map->memb, UDM_LM_HASHMASK + 1, sizeof(UDM_LANGITEM), (udm_qsort_cmp)UdmLMcmpCount);
  for (i= 0; i < UDM_LM_TOPCNT; i++)
  {
    char *s;
    if(!map->memb[i].count)
      break;
    
    for(s= map->memb[i].str; *s; s++)
    {
      if(*s==' ')
        *s='_';
    }
    printf("%s\t%d\n", map->memb[i].str, map->memb[i].count);
  }
}


static void usage(void)
{
  printf("mguesser %s-%s\n\n", PACKAGE, VERSION);
  printf("To guess use:\n\n");
  printf("\tmguesser [-n maxhits]< FILENAME\n\n");
  printf("To create new language map use:\n\n");
  printf("\tmguesser -p -c charset -l language < FILENAME\n");
} 


int main(int argc, char ** argv)
{
  int ch, verbose= 0, print= 0;
  size_t n=1000, t, model_length= 0;
  char buf[1024]= "";
  UDM_LANGMAPLIST env;
  UDM_LANGMAP mchar;
  char *charset= NULL, *lang= NULL;

  while ((ch= getopt(argc, argv, "pv?c:l:n:")) != -1)
  {
    switch(ch)
    {
      case 'n':
        n= atoi(optarg);
        break;
      case 'c':
        charset= optarg;
        break;
      case 'l':
        lang= optarg;
        break;
      case 'p':
        print++;
        break;
      case 'v':
        verbose++;
        break;
      case '?':
      default:
        usage();
        exit(1);
    }
  }
  argc-= optind;
  argv+= optind;

  /* Init structures */
  memset(&env, 0, sizeof(env));
  memset(&mchar, 0, sizeof(mchar));
  for (t= 0; t <= UDM_LM_HASHMASK; t++)
    mchar.memb[t].index= t;

  if (!print)
  {
    /* Load all available lang ngram maps */
    if (verbose)
      fprintf(stderr, "Loading language maps from '%s'\n",
              UDM_CONF_DIR UDMSLASHSTR "langmap");
    if (UdmLoadLangMapList(&env, UDM_CONF_DIR UDMSLASHSTR "langmap") != UDM_OK)
      return 1;

    if(verbose)
      fprintf(stderr, "%d maps found\n", env.nmaps);
  }
  
  /* Add each STDIN line statistics */
  while (fgets(buf, sizeof(buf), stdin))
  {
    size_t buflen= strlen(buf);
    UdmBuildLangMap(&mchar, buf, buflen, 1);
    model_length+= buflen;
  }

#ifdef DEBUG_GUESSER  
  {
    float count0 = 0; int i;
    for (i= 0; i < UDM_LM_HASHMASK; i++)
    {
      if (mchar.memb[i].count == 0)
        count0+= 1.0;
    }
    fprintf(stderr, "Count 0: %f, %.2f\n", count0, count0 * 100 / UDM_LM_HASHMASK);
  }
#endif

  if(print)
  {
    /* Display built langmap */
    if(!charset)
    {
      fprintf(stderr,"You must specify charset using -c\n");
    }
    else if(!lang)
    {
      fprintf(stderr,"You must specify language using -l\n");
    }
    else
    {
      mchar.lang= (char*)UdmStrdup(lang);
      mchar.charset= (char*)UdmStrdup(charset);
      UdmPrintLangMap(&mchar, model_length);
    }
  }
  else
  {
    size_t i;
    UDM_MAPSTAT *mapstat;
    float InfMiss= UDM_LM_TOPCNT + 1;
    
    /* Prepare map to comparison */
    UdmPrepareLangMap(&mchar);

    /* Allocate memory for comparison statistics */
    mapstat= (UDM_MAPSTAT *)UdmMalloc(env.nmaps * sizeof(UDM_MAPSTAT));

    /* Calculate each lang map        */
    /* correlation with text          */
    /* and store in mapstat structure */

    for(i= 0; i < env.nmaps; i++)
    {
      UdmCheckLangMap(&env.Map[i], &mchar, &mapstat[i], InfMiss);
      mapstat[i].map= &env.Map[i];
    }

    /* Sort statistics in quality order */
    UdmSort(mapstat, env.nmaps, sizeof(UDM_MAPSTAT), &UdmLMstatcmp);


    /* Display results. Best language is shown first. */
    for(i= 0; (i < env.nmaps) && (i < n); i++)
      printf("%dh %dm\t%s\t%s\n",
             mapstat[i].hits, mapstat[i].miss,
             mapstat[i].map->lang, mapstat[i].map->charset);

    /* Free variables */
    UdmFree(mapstat);
  }
  
  UdmLangMapListFree(&env);

  return 0;
}
