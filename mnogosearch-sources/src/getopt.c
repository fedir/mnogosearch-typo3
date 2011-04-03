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
#ifndef TEST_GETOPT
#include "udm_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "udm_common.h"
#include "udm_utils.h"
#include "udm_getopt.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define UDM_ASSERT(x) assert(x)
#define HAVE_SQL
#define udm_snprintf snprintf
#endif

#ifdef TEST_GETOPT
typedef enum udm_opttype_en
{
  UDM_OPT_BOOL,
  UDM_OPT_INT,
  UDM_OPT_STR,
  UDM_OPT_TITLE
} udm_opttype_t;


typedef struct udm_cmdline_opt_st
{
  int id;
  const char *name;
  int type;
  void *value;
  const char *comment;
} UDM_CMDLINE_OPT;


void UdmCmdLineOptionsPrint(UDM_CMDLINE_OPT *options, FILE *file);
int UdmCmdLineOptionsGet(int argc, char **argv,
                         UDM_CMDLINE_OPT *options,
                         int(*handle)(UDM_CMDLINE_OPT *opt, const char *value),
                         size_t *n);
#endif


static UDM_CMDLINE_OPT *
UdmCmdLineFindShortOption(UDM_CMDLINE_OPT *options, const char *name)
{
  UDM_CMDLINE_OPT *opt;
  for (opt= options ; opt->name; opt++)
  {
    if (opt->id && *name == opt->id)
      return opt;
  }
  return NULL;
}


static int
optend(const char *a)
{
  return *a == '=' || *a == '\0';
}

static int
optcmp(const char *a, const char *b)
{
  for ( ; !optend(a); a++, b++)
  {
    if (*a != *b)
      break;
  }
  return !(optend(a) && optend(b));
}


static UDM_CMDLINE_OPT *
UdmCmdLineFindLongOption(UDM_CMDLINE_OPT *options, const char *name)
{
  UDM_CMDLINE_OPT *opt;
  if (!*name)
    return NULL;
  for (opt= options ; opt->name; opt++)
  {
    if (!optcmp(opt->name, name))
      return opt;
  }
  return NULL;
}


static const char *
udm_opt_value_str(UDM_CMDLINE_OPT *opt, int is_long)
{
  if (is_long)
  return
    opt->type == UDM_OPT_INT ? "=#" :
    opt->type == UDM_OPT_STR ? "=name" :
    "";

  return
    opt->type == UDM_OPT_INT ? " #" :
    opt->type == UDM_OPT_STR ? " name" :
    "";
}


 __C_LINK void __UDMCALL
UdmCmdLineOptionsPrint(UDM_CMDLINE_OPT *options, FILE *file)
{
  UDM_CMDLINE_OPT *opt;
  for (opt= options; opt->name; opt++)
  {
    char command[40]= "";
    const char *comment;
    size_t clen= 0;
    if (!opt->comment)
      continue;
    if (opt->type == UDM_OPT_TITLE)
    {
      fprintf(file, "%s\n", opt->comment);
      continue;
    }
    if ((opt->id >= 0x20) && (opt->id <= 0x7E))
      clen= udm_snprintf(command, 20, "-%c%s%s", 
                         opt->id,
                         udm_opt_value_str(opt, 0),
                         opt->name[0] ? ", " : "");
    if (opt->name[0])
      udm_snprintf(command + clen, sizeof(command) - clen, "--%s%s",
                   opt->name, udm_opt_value_str(opt, 1));
    fprintf(file, "  %-15s ", command);
    for (comment= opt->comment; *comment; comment++)
    {
      if (*comment == '\r')
        ;
      else if (*comment == '\n')
        fprintf(file, "\n                  ");
      else
        fprintf(file, "%c", *comment);
    }
    printf("\n");
  }
}


__C_LINK int __UDMCALL
UdmCmdLineOptionsGet(int argc, char **argv,
                     UDM_CMDLINE_OPT *options,
                     int(*handle)(UDM_CMDLINE_OPT *opt, const char *value),
                     size_t *n)
{
  int error= 0;
  size_t i;
  const char *cmd= argv[0];
  UDM_ASSERT(argc >= 1);
  UDM_ASSERT(argv);
  argc--;
  argv++;
  
  for (i= 0; i < (size_t) argc && !error; i++)
  {
    const char *av= argv[i];
    UDM_CMDLINE_OPT *opt;
    if (av[0] != '-' || av[1] == '\0') /* av[1]=0 example: "cmd -"  */
      break;
    /*printf("%s\n", av);*/
    if (av[1] == '-')
    {
      const char *eq;
      av+= 2;
      if (!(opt= UdmCmdLineFindLongOption(options, av)))
      {
        printf("%s: unknown option '--%s'\n", cmd, av);
        error++;
        break;
      }
      if (!(eq= strchr(av, '=')) && opt->type != UDM_OPT_BOOL)
      {
        if (i < argc)
        {
          i++;
          handle(opt, argv[i]);
          continue;
        }
        else
        {
          printf("%s: option '--%s' requires an argument\n", cmd, av);
          error++;
          break;
        }
      }
      if (eq && opt->type == UDM_OPT_BOOL)
      {
        printf("%s: option '--%s' cannot have an argument\n", cmd, av);
        error++;
        break;
      }
      handle(opt, eq ? eq + 1 : opt->type == UDM_OPT_BOOL ? "TRUE" : NULL);
    }
    else
    {
      av+= 1;
      while ((opt= UdmCmdLineFindShortOption(options, av)))
      {
        switch (opt->type)
        {
          case UDM_OPT_BOOL:
            error= handle(opt, "TRUE");
            av++;
            continue; /* while */

          case UDM_OPT_INT:
          case UDM_OPT_STR:
            if (av[1])
              error= handle(opt, av + 1);
            else if (i + 1 < argc)
            {
              i++;
              error= handle(opt, (av= argv[i]));
            }
            else
            {
              printf("%s: option '-%c' requires an argument\n", cmd, *av);
              error++;
            }
            av= "";
            break;
        }
        break;
      }
      
      if (*av)
      {
        printf("%s: unknown option '-%c'\n", cmd, *av);
        error++;
        break;
      }
    }
  }
  *n= i + 1;
  return error;
}


#ifdef TEST_GETOPT

enum udm_indcmd
{
  UDM_IND_AMBIGUOUS=     0,
  UDM_IND_UNKNOWN=       1,
  UDM_IND_INDEX=         300,
  UDM_IND_CRAWL=         301,
  UDM_IND_STAT=          'S',
  UDM_IND_CREATE=        303,
  UDM_IND_DROP=          304,
  UDM_IND_DELETE=        'C',
  UDM_IND_REFERERS=      'I',
  UDM_IND_SQLMON=        'Q',
  UDM_IND_CHECKCONF=     308,
  UDM_IND_CONVERT=       309,
  UDM_IND_MULTI2BLOB=    310,
  UDM_IND_EXPORT=        311,
  UDM_IND_WRDSTAT=       312,
  UDM_IND_REWRITEURL=    313,
  UDM_IND_HASHSPELL=     314,
  UDM_IND_DUMPSPELL=     315,
  UDM_IND_REWRITELIMITS= 316,
  UDM_IND_DUMPCONF=      317,
  UDM_IND_DUMPDATA=      318,
  UDM_IND_RESTOREDATA=   319,
  UDM_IND_EXECSQL=       320
};


/*
  Available new options:
  Capital letters: AB    GH JK M OP   TUVXWXYZ
  Small   letters:           k             x z
  Digits:          123456789
*/
static UDM_CMDLINE_OPT udm_options[]=
{
  {-1, "",  UDM_OPT_TITLE,NULL, "\nCrawler options:"},
  {'a', "", UDM_OPT_BOOL, NULL, "Revisit all documents even if not expired (can be\n"
                                "limited using -t, -u, -s, -c, -y and -f options)"},
  {'m', "", UDM_OPT_BOOL, NULL, "Update expired documents even if not modified (can be\n"
                                "limited using -t, -u, -c, -s, -y and -f options)"},
  {'e', "", UDM_OPT_BOOL, NULL, "Visit 'most expired' (oldest) documents first"},
  {'o', "", UDM_OPT_BOOL, NULL, "Visit documents with less depth (hops value) first"},
  {'r', "", UDM_OPT_BOOL, NULL, "Do not try to reduce remote servers load by randomising\n"
                                "crawler queue order (faster, but less polite)"},
  {'n', "", UDM_OPT_INT,  NULL, "Visit only # documents and exit"},
  {'c', "", UDM_OPT_INT,  NULL, "Visit only # seconds and exit"},
  {'q', "", UDM_OPT_BOOL, NULL, "Quick startup (do not add Server URLs);  -qq even quicker"},
  {'b', "", UDM_OPT_BOOL, NULL, "Block starting more than one indexer instances"},
  {'i', "", UDM_OPT_BOOL, NULL, "Insert new URLs (URLs to insert must be given using -u or -f)"},
  {'p', "", UDM_OPT_INT,  NULL, "Sleep # seconds after downloading every URL"},
  {'w', "", UDM_OPT_BOOL, NULL, "Do not ask for confirmation when clearing documents\n"
                                "from the database (e.g.: indexer -Cw)"},
  {'N', "", UDM_OPT_INT,  NULL, "Run # crawler threads"},


  {-1, "",  UDM_OPT_TITLE,NULL, "\nSubsection control options (can be combined):"},
  {'s', "", UDM_OPT_STR,  NULL, "Limit indexer to documents matching status (HTTP Status code)"},
  {'t', "", UDM_OPT_STR,  NULL, "Limit indexer to documents matching tag"},
  {'g', "", UDM_OPT_STR,  NULL, "Limit indexer to documents matching category"},
  {'y', "", UDM_OPT_STR,  NULL, "Limit indexer to documents matching content-type"},
  {'L', "", UDM_OPT_STR,  NULL, "Limit indexer to documents matching language"},
  {'u', "", UDM_OPT_STR,  NULL, "Limit indexer to documents with URLs matching pattern\n"
                                "(supports SQL LIKE wildcards '%' and '_')"},
  {0, "seed",UDM_OPT_STR, NULL, "Limit indexer to documents with the given seed (0-255)"},
  {'D', "", UDM_OPT_STR,  NULL, "Work with the n-th database only (i.e. with the n-th DBAddr)"},
  {'f', "", UDM_OPT_STR,  NULL, "Read URLs to be visited/inserted/deleted from file (with -a\n"
                                "or -C option, supports SQL LIKE wildcard '%%'; has no effect\n"
                                "when combined with -m option)"},
  {-1,  "", UDM_OPT_TITLE,NULL,
  "  -f -            Use stdin instead of a file as an URL list"},


  {-1,  "", UDM_OPT_TITLE,NULL, "\nLogging options:"},
  {'l', "", UDM_OPT_BOOL, NULL, "Do not log to stdout/stderr"},
  {'v', "", UDM_OPT_INT,  NULL, "Verbose level (0-5)"},


  {-1, "",  UDM_OPT_TITLE,NULL, "\nMisc. options:"},
  {'F', "", UDM_OPT_STR,  NULL, "Print compile configuration and exit (e.g.: indexer -F '*')"},
  {'h', "", UDM_OPT_BOOL, NULL, "Print help page and exit; -hh print more help"},
  {'?', "", UDM_OPT_BOOL, NULL, "Print help page and exit; -?? print more help"},
  {'d', "", UDM_OPT_STR,  NULL, "Use the given configuration file instead of indexer.conf"
#ifndef WIN32
                                "\nThis option is usefull when running indexer as an\n"
                                "interpreter, e.g.: #!/usr/local/sbin/indexer -d"},
#endif
  {'j', "", UDM_OPT_STR,  NULL, "Set current time for statistic (use with -S),\n"
                                "format: YYYY-MM[-DD[ HH[:MM[:SS]]]]\n"
                                "or time offset, e.g. 1d12h (see Period in indexer.conf)"},

  {-1, "",  UDM_OPT_TITLE,NULL, "\nCommands (can be used with subsection control options):"},
  {UDM_IND_CRAWL,        "crawl",        UDM_OPT_BOOL,NULL, "Crawl (default command)"},
  {UDM_IND_INDEX,        "index",        UDM_OPT_BOOL,NULL, "Create search index"},
  {UDM_IND_MULTI2BLOB,   "blob",         UDM_OPT_BOOL,NULL, "Create search index"},
  {UDM_IND_WRDSTAT,      "wordstat",     UDM_OPT_BOOL,NULL, "Create statistics for misspelled word suggestions"},
  {UDM_IND_REWRITEURL,   "rewriteurl",   UDM_OPT_BOOL,NULL, "Rewrite URL data into the current search index"},
  {UDM_IND_REWRITELIMITS,"rewritelimits",UDM_OPT_BOOL,NULL, "Recreate all Limit, UserScore, UserOrder data"},
  {UDM_IND_DELETE, /*C*/  "delete",      UDM_OPT_BOOL,NULL, "Delete documents from the database"},
  {UDM_IND_STAT,   /*S*/ "statistics",   UDM_OPT_BOOL,NULL, "Print statistics and exit"},
  {UDM_IND_REFERERS,/*I*/ "referers",    UDM_OPT_BOOL,NULL, "Print referers and exit "},
  {'R', "", UDM_OPT_BOOL, NULL, "Crawl then calculate popularity rank"},

  {-1, "",  UDM_OPT_TITLE,NULL, "\nOther commands:"},
#ifdef HAVE_SQL
  {UDM_IND_CREATE,       "create",       UDM_OPT_BOOL,NULL, "Create SQL table structure and exit"},
  {UDM_IND_DROP,         "drop",         UDM_OPT_BOOL,NULL, "Drop SQL table structure and exit"},
  {UDM_IND_SQLMON, /*Q*/ "sqlmon",       UDM_OPT_BOOL,NULL, "Run interactive SQL monitor"},
  {UDM_IND_EXECSQL,      "exec",         UDM_OPT_STR, NULL, "Execute SQL query"},
#endif
  {UDM_IND_CHECKCONF,    "checkconf",    UDM_OPT_BOOL,NULL, "Check configuration file for good syntax"},
  {UDM_IND_EXPORT,       "export",       UDM_OPT_BOOL,NULL, NULL}, /* TODO */
  {UDM_IND_HASHSPELL,    "hashspell",    UDM_OPT_BOOL,NULL, "Create hash files for the active Ispell dictionaries"},
  {UDM_IND_DUMPSPELL,    "dumpspell",    UDM_OPT_BOOL,NULL, "Dump Ispell data for use with SQLWordForms"},
  {UDM_IND_DUMPCONF,     "dumpconf",     UDM_OPT_BOOL,NULL, NULL},
  {UDM_IND_DUMPDATA,     "dumpdata",     UDM_OPT_BOOL,NULL, "Dump collected data using SQL statements"},
  {UDM_IND_RESTOREDATA,  "restoredata",  UDM_OPT_BOOL,NULL, "Load prevously dumped data (give a filename using -f)"},

  {0,NULL,0,NULL,NULL}
};



static int
UdmCmdLineHandleOption(UDM_CMDLINE_OPT *opt, const char *value)
{
  const char *type= 
    opt->type == UDM_OPT_BOOL ? "BOOL" :
                 UDM_OPT_INT  ? "INT"  :
                 UDM_OPT_STR  ? "STR"  : "UNKNOWN";
  printf("  %s: '%c' %s '%s'\n",
         type, (opt->id > 0x20 && opt->id <= 0x7E) ? opt->id : ' ',
         opt->name, value);
  switch (opt->id)
  {
    case 'h':
    case '?':
      UdmCmdLineOptionsPrint(udm_options, stdout);
      exit(0);
  }
  return 0;
}


int
main(int argc, char **argv)
{
  int rc;
  size_t noptions, i;
  if (!(rc= UdmCmdLineOptionsGet(argc, argv, udm_options,
                                 UdmCmdLineHandleOption, &noptions)))
  {
    argc-= noptions;
    argv+= noptions;
    printf("%d arguments left:\n", argc);
    for (i= 0; i < argc; i++)
    {
      printf("  arg[%d]=%s\n", i, argv[i]);
    }
  }
  return 0;
}
#endif
