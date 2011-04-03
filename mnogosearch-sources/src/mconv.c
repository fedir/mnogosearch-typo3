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

#include "udm_uniconv.h"
#include "udm_utils.h"
#include "udm_common.h"
#include "udm_getopt.h"


static const char*
csgroup(const UDM_CHARSET *cs)
{
  switch(cs->family){
    case UDM_CHARSET_ARABIC    :  return "Arabic";
    case UDM_CHARSET_ARMENIAN  :  return "Armenian";
    case UDM_CHARSET_BALTIC    :  return "Baltic";
    case UDM_CHARSET_CELTIC    :  return "Celtic";
    case UDM_CHARSET_CENTRAL  :  return "Central Eur";
    case UDM_CHARSET_CHINESE_SIMPLIFIED:  return "Chinese Simplified";
    case UDM_CHARSET_CHINESE_TRADITIONAL:  return "Chinese Traditional";
    case UDM_CHARSET_CYRILLIC  :  return "Cyrillic";
    case UDM_CHARSET_GREEK    :  return "Greek";
    case UDM_CHARSET_HEBREW    :  return "Hebrew";
    case UDM_CHARSET_ICELANDIC  :  return "Icelandic";
    case UDM_CHARSET_JAPANESE  :  return "Japanese";
    case UDM_CHARSET_KOREAN    :  return "Korean";
    case UDM_CHARSET_NORDIC    :  return "Nordic";
    case UDM_CHARSET_SOUTHERN  :  return "South Eur";
    case UDM_CHARSET_THAI    :  return "Thai";
    case UDM_CHARSET_TURKISH  :  return "Turkish";
    case UDM_CHARSET_UNICODE  :  return "Unicode";
    case UDM_CHARSET_VIETNAMESE  :  return "Vietnamese";
    case UDM_CHARSET_WESTERN  :  return "Western";
    case UDM_CHARSET_GEORGIAN       :  return "Georgian";
    case UDM_CHARSET_INDIAN   :  return "Indian";
    default        :  return "Unknown";
  }
}


static int
cmpgrp(const void *v1, const void *v2)
{
  int res;
  const UDM_CHARSET *c1=v1;
  const UDM_CHARSET *c2=v2;
  if ((res=strcasecmp(csgroup(c1),csgroup(c2))))return res;
  return strcasecmp(c1->name,c2->name);
}


static void
display_charsets(void)
{
  UDM_CHARSET *cs=NULL;
  UDM_CHARSET c[100];
  size_t i=0;
  size_t n=0;
  int family=-1;
  
  for(cs=UdmGetCharSetByID(0) ; cs && cs->name ; cs++)
  {
    /* Skip not compiled charsets */
    if(cs->family != UDM_CHARSET_UNKNOWN)
      c[n++]=*cs;
  }
  fprintf(stderr,"\n%d charsets available:\n",n);

  UdmSort(c,n,sizeof(UDM_CHARSET),&cmpgrp);
  for(i=0;i<n;i++){
    if(family!=c[i].family){
      fprintf(stderr,"\n%19s : ",csgroup(&c[i]));
      family=c[i].family;
    }
    fprintf(stderr,"%s ",c[i].name);
  }
  fprintf(stderr,"\n");
}



static int
usage(UDM_CMDLINE_OPT *options, int level)
{
  printf(
"\n"
"mconv from %s-%s\n"
"http://www.mnogosearch.org/ (C) 1998-2010, LavTech Corp.\n"
"\n"
"Usage: mconv [OPTIONS] -f charset_from -t charset_to  < infile > outfile\n"
"\n",
  PACKAGE,VERSION);

  UdmCmdLineOptionsPrint(options, stdout);

  printf(
"\n"
"\n"
"Please post bug reports and suggestions at http://www.mnogosearch.org/bugs/"
"\n");
  
  if (level > 1) display_charsets();
  return(0);
}


static int html_from= 0, html_to= 0;
static int help= 0, verbose= 0;
static const char *charset_from= NULL;
static const char *charset_to= NULL;


static int
UdmCmdLineHandleOption(UDM_CMDLINE_OPT *opt, const char *value)
{
  switch (opt->id)
  {
    case 'v': verbose= 1; break;
    case 'e': html_from= UDM_RECODE_HTML_SPECIAL; break;
    case 'E': html_to= UDM_RECODE_HTML_NONASCII; break;
    case 'x': html_to= UDM_RECODE_HTML_NONASCII_HEX; break;
    case 't': charset_to=  value; break;
    case 'f': charset_from= value; break;
    case '?':
    case 'h':
    default: help++;
  }
  return 0;
}


static UDM_CMDLINE_OPT udm_mconv_options[]=
{
  {-1,  "", UDM_OPT_TITLE,NULL, "Conversion options:"},
  {'v', "", UDM_OPT_BOOL, NULL, "verbose output"},
  {'e', "", UDM_OPT_BOOL, NULL, "convert HTML special characters to entities (text to html)"},
  {'E', "", UDM_OPT_BOOL, NULL, "use HTML dec escape entities for output, e.g. &#1040;"},
  {'x', "", UDM_OPT_BOOL, NULL, "use HTML hex escape entities for output, e.g. &#x410;"},
  {'t', "", UDM_OPT_STR,  NULL, NULL},
  {'f', "", UDM_OPT_STR,  NULL, NULL},
  {-1,  "", UDM_OPT_TITLE,NULL, ""},
  {'h', "", UDM_OPT_BOOL, NULL, "Print help page and exit; -hh print more help"},
  {'?', "", UDM_OPT_BOOL, NULL, "Print help page and exit; -?? print more help"},
};


/*
  Buffers should be long enough to make
  fgets() scan input string in a single shot.
  
  In case of too long input line, fgets will break the line
  into multiple fgets calls, so a multibyte character can
  be split apart, which will display question marks.
  
  1Mb long input buffer should be enough for the most cases.
*/
#define MCONV_BUF_SIZE 1024*1024

static char from_buf[MCONV_BUF_SIZE];
static char to_buf[8*MCONV_BUF_SIZE];



int main(int argc, char **argv)
{
  UDM_CHARSET *CH_F, *CH_T;
  UDM_CONV conv;
  size_t noptions;

  if (UdmCmdLineOptionsGet(argc, argv, udm_mconv_options,
                           UdmCmdLineHandleOption, &noptions))
    return 0;
  argc-= noptions;
  argv+= noptions;

  if((argc>1) || (help) || (!charset_from) || (!charset_to))
  {
    usage(udm_mconv_options, help);
    return(1);
  }

  if (!(CH_F = UdmGetCharSet(charset_from)))
  {
    if (verbose)
    {
      fprintf(stderr, "Charset: %s not found or not supported", charset_from);
      display_charsets();
    }
    exit(1);
  }
  
  
  if (!(CH_T = UdmGetCharSet(charset_to)))
  {
    if (verbose)
    {
      fprintf(stderr, "Charset: %s not found or not supported", charset_to);
      display_charsets();
    }
    exit(1);
  }

  UdmConvInit(&conv, CH_F, CH_T, html_from | html_to);

  while (fgets(from_buf, sizeof(from_buf), stdin))
  {
    size_t from_len= strlen(from_buf);
    size_t to_len= UdmConv(&conv, to_buf, sizeof(to_buf), from_buf, from_len);
    fwrite(to_buf, 1, to_len, stdout);
  }

  return 0;
}
