/* $Source$ */
/* $Id$ */

/*
   +----------------------------------------------------------------------+
   | PHP Version 5                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2004 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license,       |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_0.txt.                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors:                                                             |
   |  Initial version     by  Alex Barkov <bar@izhcom.ru>                 |
   |                      and Ramil Kalimullin <ram@izhcom.ru>            |
   |  Further development by  Sergey Kartashoff <gluke@mail.ru>           |
   +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_mnogo.h"
#include "ext/standard/php_standard.h"
#include "ext/standard/info.h"
#include "php_globals.h"

#ifdef HAVE_MNOGOSEARCH

#define UDMSTRSIZ               1024*5

#define UDM_FIELD_URLID		1
#define UDM_FIELD_URL		2
#define UDM_FIELD_CONTENT	3	
#define UDM_FIELD_TITLE		4
#define UDM_FIELD_KEYWORDS	5
#define UDM_FIELD_DESC		6
#define UDM_FIELD_TEXT		7
#define UDM_FIELD_SIZE		8
#define UDM_FIELD_RATING	9
#define UDM_FIELD_MODIFIED	10
#define UDM_FIELD_ORDER		11
#define UDM_FIELD_CRC		12
#define UDM_FIELD_CATEGORY	13
#define UDM_FIELD_LANG		14
#define UDM_FIELD_CHARSET	15
#define UDM_FIELD_SITEID	16
#define UDM_FIELD_POP_RANK	17
#define UDM_FIELD_ORIGINID	18

/* udm_set_agent_param constants */
#define UDM_PARAM_PAGE_SIZE		1
#define UDM_PARAM_PAGE_NUM		2
#define UDM_PARAM_SEARCH_MODE		3
#define UDM_PARAM_CACHE_MODE		4
#define UDM_PARAM_TRACK_MODE		5
#define UDM_PARAM_CHARSET		6
#define UDM_PARAM_STOPTABLE		7
#define UDM_PARAM_STOPFILE		8
#define UDM_PARAM_WEIGHT_FACTOR		9
#define UDM_PARAM_WORD_MATCH		10
#define UDM_PARAM_PHRASE_MODE		11
#define UDM_PARAM_MIN_WORD_LEN		12
#define UDM_PARAM_MAX_WORD_LEN		13
#define UDM_PARAM_ISPELL_PREFIXES	14
#define UDM_PARAM_CROSS_WORDS		15
#define UDM_PARAM_VARDIR		16
#define UDM_PARAM_LOCAL_CHARSET		17
#define UDM_PARAM_BROWSER_CHARSET	18
#define UDM_PARAM_HLBEG			19
#define UDM_PARAM_HLEND			20
#define UDM_PARAM_SYNONYM		21
#define UDM_PARAM_SEARCHD		22	/* unused */
#define UDM_PARAM_QSTRING		23
#define UDM_PARAM_REMOTE_ADDR		24
#define UDM_PARAM_QUERY			25
#define UDM_PARAM_STORED		26
#define UDM_PARAM_GROUPBYSITE		27
#define UDM_PARAM_SITEID		28
#define UDM_PARAM_DETECT_CLONES		29
#define UDM_PARAM_SORT_ORDER		30
#define UDM_PARAM_RESULTS_LIMIT		31
#define UDM_PARAM_EXCERPT_SIZE		32
#define UDM_PARAM_EXCERPT_PADDING	33

/* udm_add_search_limit constants */
#define UDM_LIMIT_URL		1
#define UDM_LIMIT_TAG		2
#define UDM_LIMIT_LANG		3
#define UDM_LIMIT_CAT		4
#define UDM_LIMIT_DATE		5
#define UDM_LIMIT_TYPE		6

/* word match type */
#define UDM_MATCH_WORD		0
#define UDM_MATCH_BEGIN		1
#define UDM_MATCH_SUBSTR	2
#define UDM_MATCH_END		3

/* track modes */
#define UDM_TRACK_ENABLED	1
#define UDM_TRACK_DISABLED	0

/* cache modes */
#define UDM_CACHE_ENABLED	1
#define UDM_CACHE_DISABLED	0

/* phrase modes */
#define UDM_PHRASE_ENABLED	1
#define UDM_PHRASE_DISABLED	0

/* prefix modes */
#define UDM_PREFIXES_ENABLED	1
#define UDM_PREFIXES_DISABLED	0

/* crosswords modes */
#define UDM_CROSS_WORDS_ENABLED	 1
#define UDM_CROSS_WORDS_DISABLED 0

/* General modes */
#define UDM_ENABLED	 	1
#define UDM_DISABLED 		0

/* udm_get_res_param constants */
#define UDM_PARAM_NUM_ROWS	256
#define UDM_PARAM_FOUND		257
#define UDM_PARAM_WORDINFO	258
#define UDM_PARAM_SEARCHTIME	259
#define UDM_PARAM_FIRST_DOC	260
#define UDM_PARAM_LAST_DOC	261
#define UDM_PARAM_WORDINFO_ALL	262

/* udm_load_ispell_data constants */
#define UDM_ISPELL_TYPE_AFFIX	1
#define UDM_ISPELL_TYPE_SPELL	2
#define UDM_ISPELL_TYPE_DB	3
#define UDM_ISPELL_TYPE_SERVER	4

/* True globals, no need for thread safety */
static int le_link,le_res;

#include <udm_config.h>
#include <udmsearch.h>

/* {{{ mnogosearch_functions[]
 */
zend_function_entry mnogosearch_functions[]=
{
  PHP_FE(udm_api_version,         NULL)
  PHP_FE(udm_check_charset,       NULL)
  PHP_FE(udm_crc32,               NULL)
  PHP_FE(udm_parse_query_string,  NULL)
  PHP_FE(udm_make_excerpt,        NULL)
  PHP_FE(udm_set_agent_param_ex,  NULL)
  PHP_FE(udm_get_agent_param_ex,  NULL)
  PHP_FE(udm_get_res_field_ex,    NULL)
  PHP_FE(udm_hash32,              NULL)
  PHP_FE(udm_alloc_agent_array,   NULL)
  PHP_FE(udm_store_doc_cgi,       NULL)
  PHP_FE(udm_alloc_agent,         NULL)
  PHP_FE(udm_set_agent_param,     NULL)
  PHP_FE(udm_load_ispell_data,    NULL)
  PHP_FE(udm_free_ispell_data,    NULL)
  PHP_FE(udm_add_search_limit,    NULL)
  PHP_FE(udm_clear_search_limits, NULL)
  PHP_FE(udm_errno,               NULL)
  PHP_FE(udm_error,               NULL)
  PHP_FE(udm_find,                NULL)
  PHP_FE(udm_get_res_param,       NULL)
  PHP_FE(udm_get_res_field,       NULL)
  PHP_FE(udm_cat_list,            NULL)
  PHP_FE(udm_cat_path,            NULL)
  PHP_FE(udm_free_res,            NULL)
  PHP_FE(udm_free_agent,          NULL)
  PHP_FE(udm_get_doc_count,       NULL)
  {NULL, NULL, NULL}
};
/* }}} */

zend_module_entry mnogosearch_module_entry=
{
  STANDARD_MODULE_HEADER,
  "mnogosearch", 
  mnogosearch_functions, 
  PHP_MINIT(mnogosearch), 
  PHP_MSHUTDOWN(mnogosearch), 
  PHP_RINIT(mnogosearch), 
  NULL,
  PHP_MINFO(mnogosearch), 
  NO_VERSION_YET,
  STANDARD_MODULE_PROPERTIES
};


#ifdef COMPILE_DL_MNOGOSEARCH
ZEND_GET_MODULE(mnogosearch)
#endif

static void _free_udm_agent(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
  UDM_AGENT * Agent = (UDM_AGENT *)rsrc->ptr;
  UdmEnvFree(Agent->Conf);
  UdmAgentFree(Agent);
}

static void _free_udm_res(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
  UDM_RESULT * Res = (UDM_RESULT *)rsrc->ptr;
  UdmResultFree(Res);
}

/* {{{ PHP_MINIT_FUNCTION
 */
DLEXPORT PHP_MINIT_FUNCTION(mnogosearch)
{
  UdmInit();
  le_link= zend_register_list_destructors_ex(_free_udm_agent,NULL,"mnogosearch agent",module_number);
  le_res= zend_register_list_destructors_ex(_free_udm_res,NULL,"mnogosearch result",module_number);

  REGISTER_LONG_CONSTANT("UDM_FIELD_URLID",        UDM_FIELD_URLID,    CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_FIELD_URL",          UDM_FIELD_URL,      CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_FIELD_CONTENT",      UDM_FIELD_CONTENT,  CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_FIELD_TITLE",        UDM_FIELD_TITLE,    CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_FIELD_KEYWORDS",     UDM_FIELD_KEYWORDS, CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_FIELD_DESC",         UDM_FIELD_DESC,     CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_FIELD_DESCRIPTION",  UDM_FIELD_DESC,     CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_FIELD_TEXT",         UDM_FIELD_TEXT,     CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_FIELD_SIZE",         UDM_FIELD_SIZE,     CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_FIELD_RATING",       UDM_FIELD_RATING,   CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_FIELD_SCORE",        UDM_FIELD_RATING,   CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_FIELD_MODIFIED",     UDM_FIELD_MODIFIED, CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_FIELD_ORDER",        UDM_FIELD_ORDER,    CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_FIELD_CRC",          UDM_FIELD_CRC,      CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_FIELD_CATEGORY",     UDM_FIELD_CATEGORY, CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_FIELD_LANG",         UDM_FIELD_LANG,     CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_FIELD_CHARSET",      UDM_FIELD_CHARSET,  CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_FIELD_SITEID",       UDM_FIELD_SITEID,   CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_FIELD_POP_RANK",     UDM_FIELD_POP_RANK, CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_FIELD_ORIGINID",     UDM_FIELD_ORIGINID, CONST_CS | CONST_PERSISTENT);

  /* udm_set_agent_param constants */
  REGISTER_LONG_CONSTANT("UDM_PARAM_PAGE_SIZE",       UDM_PARAM_PAGE_SIZE,       CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PARAM_PAGE_NUM",        UDM_PARAM_PAGE_NUM,        CONST_CS | CONST_PERSISTENT);

  REGISTER_LONG_CONSTANT("UDM_PARAM_SEARCH_MODE",     UDM_PARAM_SEARCH_MODE,     CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("UDM_PARAM_CACHE_MODE",      UDM_PARAM_CACHE_MODE,      CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("UDM_PARAM_TRACK_MODE",      UDM_PARAM_TRACK_MODE,      CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("UDM_PARAM_PHRASE_MODE",     UDM_PARAM_PHRASE_MODE,     CONST_CS | CONST_PERSISTENT);  
  
  REGISTER_LONG_CONSTANT("UDM_PARAM_CHARSET",         UDM_PARAM_CHARSET,         CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("UDM_PARAM_LOCAL_CHARSET",   UDM_PARAM_LOCAL_CHARSET,   CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("UDM_PARAM_BROWSER_CHARSET", UDM_PARAM_BROWSER_CHARSET, CONST_CS | CONST_PERSISTENT);  
  
  REGISTER_LONG_CONSTANT("UDM_PARAM_STOPTABLE",       UDM_PARAM_STOPTABLE,       CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("UDM_PARAM_STOP_TABLE",      UDM_PARAM_STOPTABLE,       CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("UDM_PARAM_STOPFILE",        UDM_PARAM_STOPFILE,        CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("UDM_PARAM_STOP_FILE",       UDM_PARAM_STOPFILE,        CONST_CS | CONST_PERSISTENT);  
  
  REGISTER_LONG_CONSTANT("UDM_PARAM_WEIGHT_FACTOR",   UDM_PARAM_WEIGHT_FACTOR,   CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PARAM_WORD_MATCH",      UDM_PARAM_WORD_MATCH,      CONST_CS | CONST_PERSISTENT);  
  
  REGISTER_LONG_CONSTANT("UDM_PARAM_MAX_WORD_LEN",    UDM_PARAM_MAX_WORD_LEN,    CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("UDM_PARAM_MAX_WORDLEN",     UDM_PARAM_MAX_WORD_LEN,    CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("UDM_PARAM_MIN_WORD_LEN",    UDM_PARAM_MIN_WORD_LEN,    CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("UDM_PARAM_MIN_WORDLEN",     UDM_PARAM_MIN_WORD_LEN,    CONST_CS | CONST_PERSISTENT);  
  
  REGISTER_LONG_CONSTANT("UDM_PARAM_ISPELL_PREFIXES", UDM_PARAM_ISPELL_PREFIXES,CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("UDM_PARAM_ISPELL_PREFIX",   UDM_PARAM_ISPELL_PREFIXES,CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("UDM_PARAM_PREFIXES",        UDM_PARAM_ISPELL_PREFIXES,CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("UDM_PARAM_PREFIX",          UDM_PARAM_ISPELL_PREFIXES,CONST_CS | CONST_PERSISTENT);
    
  REGISTER_LONG_CONSTANT("UDM_PARAM_CROSS_WORDS",  UDM_PARAM_CROSS_WORDS,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PARAM_CROSSWORDS",  UDM_PARAM_CROSS_WORDS,CONST_CS | CONST_PERSISTENT);  
  
  REGISTER_LONG_CONSTANT("UDM_PARAM_VARDIR",  UDM_PARAM_VARDIR,CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("UDM_PARAM_DATADIR",  UDM_PARAM_VARDIR,CONST_CS | CONST_PERSISTENT);  
  
  REGISTER_LONG_CONSTANT("UDM_PARAM_HLBEG",  UDM_PARAM_HLBEG,CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("UDM_PARAM_HLEND",  UDM_PARAM_HLEND,CONST_CS | CONST_PERSISTENT);  
  
  REGISTER_LONG_CONSTANT("UDM_PARAM_SYNONYM",  UDM_PARAM_SYNONYM,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PARAM_STORED",  UDM_PARAM_STORED,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PARAM_GROUPBYSITE",  UDM_PARAM_GROUPBYSITE,CONST_CS | CONST_PERSISTENT);
  
  REGISTER_LONG_CONSTANT("UDM_PARAM_QSTRING",  UDM_PARAM_QSTRING,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PARAM_REMOTE_ADDR",  UDM_PARAM_REMOTE_ADDR,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PARAM_QUERY",  UDM_PARAM_QUERY,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PARAM_SITEID",  UDM_PARAM_SITEID,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PARAM_DETECT_CLONES",UDM_PARAM_DETECT_CLONES,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PARAM_SORT_ORDER",UDM_PARAM_SORT_ORDER,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PARAM_RESULTS_LIMIT",UDM_PARAM_RESULTS_LIMIT,CONST_CS | CONST_PERSISTENT);
  
  REGISTER_LONG_CONSTANT("UDM_PARAM_EXCERPT_SIZE",UDM_PARAM_EXCERPT_SIZE,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PARAM_EXCERPT_PADDING",UDM_PARAM_EXCERPT_PADDING,CONST_CS | CONST_PERSISTENT);
  
  /* udm_add_search_limit constants */
  REGISTER_LONG_CONSTANT("UDM_LIMIT_CAT",    UDM_LIMIT_CAT,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_LIMIT_URL",    UDM_LIMIT_URL,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_LIMIT_TAG",    UDM_LIMIT_TAG,CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("UDM_LIMIT_LANG",  UDM_LIMIT_LANG,CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("UDM_LIMIT_DATE",  UDM_LIMIT_DATE,CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("UDM_LIMIT_TYPE",  UDM_LIMIT_TYPE,CONST_CS | CONST_PERSISTENT);  
  
  /* udm_get_res_param constants */
  REGISTER_LONG_CONSTANT("UDM_PARAM_FOUND",  UDM_PARAM_FOUND,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PARAM_NUM_ROWS",  UDM_PARAM_NUM_ROWS,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PARAM_WORDINFO",  UDM_PARAM_WORDINFO,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PARAM_WORDINFO_ALL",UDM_PARAM_WORDINFO_ALL,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PARAM_WORD_INFO",  UDM_PARAM_WORDINFO,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PARAM_SEARCHTIME",  UDM_PARAM_SEARCHTIME,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PARAM_SEARCH_TIME",  UDM_PARAM_SEARCHTIME,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PARAM_FIRST_DOC",  UDM_PARAM_FIRST_DOC,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PARAM_LAST_DOC",  UDM_PARAM_LAST_DOC,CONST_CS | CONST_PERSISTENT);

  /* search modes */
  REGISTER_LONG_CONSTANT("UDM_MODE_ALL",    UDM_MODE_ALL,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_MODE_ANY",    UDM_MODE_ANY,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_MODE_BOOL",    UDM_MODE_BOOL,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_MODE_PHRASE",  UDM_MODE_PHRASE,CONST_CS | CONST_PERSISTENT);

  /* search cache params */
  REGISTER_LONG_CONSTANT("UDM_CACHE_ENABLED",  UDM_ENABLED,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_CACHE_DISABLED",  UDM_DISABLED,CONST_CS | CONST_PERSISTENT);
  
  /* track mode params */
  REGISTER_LONG_CONSTANT("UDM_TRACK_ENABLED",  UDM_ENABLED,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_TRACK_DISABLED",  UDM_DISABLED,CONST_CS | CONST_PERSISTENT);
  
  /* phrase mode params */
  REGISTER_LONG_CONSTANT("UDM_PHRASE_ENABLED",  UDM_ENABLED,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PHRASE_DISABLED",  UDM_DISABLED,CONST_CS | CONST_PERSISTENT);

  /* general params */
  REGISTER_LONG_CONSTANT("UDM_ENABLED",    UDM_ENABLED,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_DISABLED",    UDM_DISABLED,CONST_CS | CONST_PERSISTENT);
  
  /* crosswords mode params */
  REGISTER_LONG_CONSTANT("UDM_CROSS_WORDS_ENABLED",UDM_ENABLED,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_CROSSWORDS_ENABLED",UDM_ENABLED,CONST_CS | CONST_PERSISTENT);  
  REGISTER_LONG_CONSTANT("UDM_CROSS_WORDS_DISABLED",UDM_DISABLED,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_CROSSWORDS_DISABLED",UDM_DISABLED,CONST_CS | CONST_PERSISTENT);
  
  /* prefixes mode params */
  REGISTER_LONG_CONSTANT("UDM_PREFIXES_ENABLED",  UDM_ENABLED,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PREFIX_ENABLED",  UDM_ENABLED,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_ISPELL_PREFIXES_ENABLED",UDM_ENABLED,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_ISPELL_PREFIX_ENABLED",UDM_ENABLED,CONST_CS | CONST_PERSISTENT);
  
  REGISTER_LONG_CONSTANT("UDM_PREFIXES_DISABLED",  UDM_DISABLED,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_PREFIX_DISABLED",  UDM_DISABLED,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_ISPELL_PREFIXES_DISABLED",UDM_DISABLED,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_ISPELL_PREFIX_DISABLED",UDM_DISABLED,CONST_CS | CONST_PERSISTENT);
  
  /* ispell type params */
  REGISTER_LONG_CONSTANT("UDM_ISPELL_TYPE_AFFIX",  UDM_ISPELL_TYPE_AFFIX,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_ISPELL_TYPE_SPELL",  UDM_ISPELL_TYPE_SPELL,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_ISPELL_TYPE_DB",  UDM_ISPELL_TYPE_DB,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_ISPELL_TYPE_SERVER",UDM_ISPELL_TYPE_SERVER,CONST_CS | CONST_PERSISTENT);

  /* word match mode params */
  REGISTER_LONG_CONSTANT("UDM_MATCH_WORD",  UDM_MATCH_WORD,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_MATCH_BEGIN",  UDM_MATCH_BEGIN,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_MATCH_SUBSTR",  UDM_MATCH_SUBSTR,CONST_CS | CONST_PERSISTENT);
  REGISTER_LONG_CONSTANT("UDM_MATCH_END",    UDM_MATCH_END,CONST_CS | CONST_PERSISTENT);
  
  return SUCCESS;
}
/* }}} */

DLEXPORT PHP_MSHUTDOWN_FUNCTION(mnogosearch)
{
  return SUCCESS;
}


DLEXPORT PHP_RINIT_FUNCTION(mnogosearch)
{
  return SUCCESS;
}


DLEXPORT PHP_MINFO_FUNCTION(mnogosearch)
{
  char buf[32];
  
  php_info_print_table_start();
  php_info_print_table_row(2, "mnoGoSearch Support", "enabled" );
  
  sprintf(buf,"%d", UDM_VERSION_ID);
  php_info_print_table_row(2, "mnoGoSearch library version", buf );
  php_info_print_table_end();
}


static char* MyRemoveHiLightDup(const char *s)
{
  size_t len= strlen(s)+1;
  char   *d, *res = (char*)emalloc(len);
  
  for(d=res; s[0]; s++)
  {
    switch(s[0])
    {
      case '\2':
      case '\3':
        break;
      case '&':
        if (s[1] == '#')
        {
          char *e;
          int code= 0;
          
          for (e= (char *)s+2; (*e >= '0') && (*e <= '9'); code= code*10 + e[0]-'0', e++);
          if (*e == ';')
          {
            *d++= (code < 128) ? code : '?';
            s= e;
            break;
          }
        }
        /* pass through */
        
      default:
        *d++=*s;
    }
  }
  *d='\0';
  return res;
}

/* {{{ proto int udm_alloc_agent(string dbaddr [, string dbmode])
   Allocate mnoGoSearch session */
DLEXPORT PHP_FUNCTION(udm_alloc_agent)
{
  switch(ZEND_NUM_ARGS())
  {

    case 1:
      {
        zval **yydbaddr;
        char *dbaddr;
        UDM_ENV   * Env;
        UDM_AGENT * Agent;
        
        if(zend_get_parameters_ex(1,&yydbaddr) == FAILURE)
          RETURN_FALSE;

        convert_to_string_ex(yydbaddr);
        dbaddr= Z_STRVAL_PP(yydbaddr);
        
        Env=UdmEnvInit(NULL);
        UdmVarListReplaceStr(&Env->Vars,"SyslogFacility","local7");
        UdmSetLogLevel(NULL,0);
        UdmOpenLog("mnoGoSearch-php",Env,0);
        UdmDBListAdd(&Env->dbl,dbaddr, UDM_OPEN_MODE_WRITE);
        Agent=UdmAgentInit(NULL,Env,0);
        ZEND_REGISTER_RESOURCE(return_value,Agent,le_link);
      }
      break;
      
    case 2:
      {
        zval **yydbaddr;
        zval **yydbmode;
        char *dbaddr;
        char *dbmode;
        UDM_ENV   * Env;
        UDM_AGENT * Agent;
        
        if (zend_get_parameters_ex(2,&yydbaddr,&yydbmode) == FAILURE)
          RETURN_FALSE;
        
        convert_to_string_ex(yydbaddr);
        convert_to_string_ex(yydbmode);
        dbaddr= Z_STRVAL_PP(yydbaddr);
        dbmode= Z_STRVAL_PP(yydbmode);
        
        Env= UdmEnvInit(NULL);
        UdmVarListReplaceStr(&Env->Vars,"SyslogFacility","local7");
        UdmSetLogLevel(NULL,0);
        UdmOpenLog("mnoGoSearch-php",Env,0);
        UdmDBListAdd(&Env->dbl,dbaddr, UDM_OPEN_MODE_WRITE);
        Agent=UdmAgentInit(NULL,Env,0);
        ZEND_REGISTER_RESOURCE(return_value,Agent,le_link);
      }
      break;
      
    default:
      WRONG_PARAM_COUNT;
      break;
  }
}
/* }}} */

/* {{{ proto int udm_set_agent_param(int agent, int var, string val)
   Set mnoGoSearch agent session parameters */
DLEXPORT PHP_FUNCTION(udm_set_agent_param)
{
  zval **yyagent, **yyvar, **yyval;
  char *val;
  int var;
  UDM_AGENT * Agent;

  switch(ZEND_NUM_ARGS())
  {
  
    case 3:     
      if(zend_get_parameters_ex(3,&yyagent,&yyvar,&yyval) == FAILURE)
        RETURN_FALSE;
      
      convert_to_long_ex(yyvar);
      convert_to_string_ex(yyval);
      ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, -1, "mnoGoSearch-agent", le_link);
      var= Z_LVAL_PP(yyvar);
      val= Z_STRVAL_PP(yyval);
      
      break;
      
    default:
      WRONG_PARAM_COUNT;
      break;
  }
  
  switch(var)
  {
    case UDM_PARAM_PAGE_SIZE: 
      UdmVarListReplaceStr(&Agent->Conf->Vars,"ps",val);
      break;
      
    case UDM_PARAM_PAGE_NUM: 
      UdmVarListReplaceStr(&Agent->Conf->Vars,"np",val);
      break;

    case UDM_PARAM_SEARCH_MODE:
      switch (atoi(val))
      {
        case UDM_MODE_ALL:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"m","all");
          break;
        
        case UDM_MODE_ANY:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"m","any");
          break;
        
        case UDM_MODE_BOOL: 
          UdmVarListReplaceStr(&Agent->Conf->Vars,"m","bool");
          break;

        case UDM_MODE_PHRASE: 
          php_error_docref(NULL TSRMLS_CC, E_WARNING,"Unknown search mode");
          RETURN_FALSE;
          break;
            
        default:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"m","all");
          php_error_docref(NULL TSRMLS_CC, E_WARNING,"Unknown search mode");
          RETURN_FALSE;
          break;
      }
      break;

    case UDM_PARAM_WORD_MATCH:
      switch (atoi(val))
      {
        case UDM_MATCH_WORD:          
          UdmVarListReplaceStr(&Agent->Conf->Vars,"wm","wrd");
          break;

        case UDM_MATCH_BEGIN:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"wm","beg");
          break;

        case UDM_MATCH_END:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"wm","end");
          break;

        case UDM_MATCH_SUBSTR:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"wm","sub");
          break;
            
        default:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"wm","wrd");
          php_error_docref(NULL TSRMLS_CC, E_WARNING,"Unknown word match mode");
          RETURN_FALSE;
          break;
      }
      break;
    case UDM_PARAM_CACHE_MODE: 
      switch (atoi(val))
      {
        case UDM_ENABLED:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"Cache","yes");
          break;
          
        case UDM_DISABLED:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"Cache","no");
          break;
          
        default:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"Cache","no");
          php_error_docref(NULL TSRMLS_CC, E_WARNING,"Unknown cache mode");
          RETURN_FALSE;
          break;
      }
      break;
      
    case UDM_PARAM_TRACK_MODE: 
      switch (atoi(val))
      {
        case UDM_ENABLED:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"TrackQuery","yes");
          break;
          
        case UDM_DISABLED:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"TrackQuery","no");
          break;
          
        default:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"TrackQuery","no");
          php_error_docref(NULL TSRMLS_CC, E_WARNING,"Unknown track mode");
          RETURN_FALSE;
          break;
      }
      break;
    
    case UDM_PARAM_PHRASE_MODE: 
      break;

    case UDM_PARAM_ISPELL_PREFIXES: 
      switch (atoi(val))
      {
        case UDM_ENABLED:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"IspellUsePrefixes","1");
          break;
          
        case UDM_DISABLED:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"IspellUsePrefixes","0");
          break;

        default:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"IspellUsePrefixes","0");
          php_error_docref(NULL TSRMLS_CC, E_WARNING,"Unknown ispell prefixes mode");
          RETURN_FALSE;
          break;
      }
      break;

    case UDM_PARAM_CHARSET:
    case UDM_PARAM_LOCAL_CHARSET:
      UdmVarListReplaceStr(&Agent->Conf->Vars,"LocalCharset",val);
      {
        const char * charset= UdmVarListFindStr(&Agent->Conf->Vars,"LocalCharset","iso-8859-1");
        Agent->Conf->lcs= UdmGetCharSet(charset);
      }
      break;
      
    case UDM_PARAM_BROWSER_CHARSET:
      UdmVarListReplaceStr(&Agent->Conf->Vars,"BrowserCharset",val);
      {
        const char * charset= UdmVarListFindStr(&Agent->Conf->Vars,"BrowserCharset","iso-8859-1");
        Agent->Conf->bcs= UdmGetCharSet(charset);
      }
      break;

    case UDM_PARAM_HLBEG:
      UdmVarListReplaceStr(&Agent->Conf->Vars,"HlBeg",val);
      break;

    case UDM_PARAM_HLEND:
      UdmVarListReplaceStr(&Agent->Conf->Vars,"HlEnd",val);
      break;
      
    case UDM_PARAM_SYNONYM:
      if (UdmSynonymListLoad(Agent->Conf,val))
      {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s",Agent->Conf->errstr);
        RETURN_FALSE;
      }
      else
      {
#if UDM_VERSION_ID >= 30307
        UdmSynonymListListSortItems(&Agent->Conf->Synonym);
#else
        UdmSynonymListSort(&(Agent->Conf->Synonyms));
#endif
      }
      break;
      
    case UDM_PARAM_QSTRING:
      UdmVarListReplaceStr(&Agent->Conf->Vars,"QUERY_STRING",val);
      break;

    case UDM_PARAM_REMOTE_ADDR:
      UdmVarListReplaceStr(&Agent->Conf->Vars,"IP",val);
      break;      
      
    case UDM_PARAM_STOPTABLE:
      break;

    case UDM_PARAM_STOPFILE: 
      if (UdmStopListLoad(Agent->Conf,val))
      {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", Agent->Conf->errstr);
        RETURN_FALSE;
      }
      break;
      
    case UDM_PARAM_WEIGHT_FACTOR: 
      UdmVarListReplaceStr(&Agent->Conf->Vars,"wf",val);
      break;
      
    case UDM_PARAM_MIN_WORD_LEN: 
      Agent->Conf->WordParam.min_word_len=atoi(val);
      break;
      
    case UDM_PARAM_MAX_WORD_LEN: 
      Agent->Conf->WordParam.max_word_len=atoi(val);
      break;
      
    case UDM_PARAM_CROSS_WORDS: 
      switch (atoi(val))
      {
        case UDM_ENABLED:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"CrossWords","yes");
          break;
          
        case UDM_DISABLED:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"CrossWords","no");
          break;
          
        default:
          php_error_docref(NULL TSRMLS_CC, E_WARNING,"Unknown crosswords mode");
          RETURN_FALSE;
          break;
      }
      break;

    case UDM_PARAM_VARDIR:
      UdmVarListReplaceStr(&Agent->Conf->Vars,"Vardir",val);
      break;

    case UDM_PARAM_QUERY:
      UdmVarListReplaceStr(&Agent->Conf->Vars,"q",val);
      break;

    case UDM_PARAM_STORED:
      UdmVarListReplaceStr(&Agent->Conf->Vars,"StoredAddr",val);
      break;
      
    case UDM_PARAM_GROUPBYSITE: 
      switch (atoi(val))
      {
        case UDM_ENABLED:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"GroupBySite","yes");
          break;
          
        case UDM_DISABLED:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"GroupBySite","no");
          break;
          
        default:
          php_error_docref(NULL TSRMLS_CC, E_WARNING,"Unknown groupbysite mode");
          RETURN_FALSE;
          break;
      }
      break;
      
    case UDM_PARAM_SITEID: 
      UdmVarListReplaceStr(&Agent->Conf->Vars,"site",val);
      break;

    case UDM_PARAM_DETECT_CLONES: 
      switch (atoi(val))
      {
        case UDM_ENABLED:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"DetectClones","yes");
          break;
          
        case UDM_DISABLED:
          UdmVarListReplaceStr(&Agent->Conf->Vars,"DetectClones","no");
          break;
          
        default:
          php_error_docref(NULL TSRMLS_CC, E_WARNING,"Unknown clones mode");
          RETURN_FALSE;
          break;
      }
      break;

    case UDM_PARAM_SORT_ORDER: 
      UdmVarListReplaceStr(&Agent->Conf->Vars,"s",val);
      break;

    case UDM_PARAM_RESULTS_LIMIT: 
      UdmVarListReplaceStr(&Agent->Conf->Vars,"ResultsLimit",val);
      break;

    case UDM_PARAM_EXCERPT_SIZE: 
      UdmVarListReplaceStr(&Agent->Conf->Vars,"ExcerptSize",val);
      break;
    case UDM_PARAM_EXCERPT_PADDING: 
      UdmVarListReplaceStr(&Agent->Conf->Vars,"ExcerptPadding",val);
      break;
    default:
      php_error_docref(NULL TSRMLS_CC, E_WARNING,"Unknown agent session parameter");
      RETURN_FALSE;
      break;
  }
  RETURN_TRUE;
}
/* }}} */

/* {{{ proto int udm_load_ispell_data(int agent, int var, string val1, [string charset], string val2, int flag)
   Load ispell data */
DLEXPORT PHP_FUNCTION(udm_load_ispell_data)
{
  zval **yyagent, **yyvar, **yyval1, **yyval2, **yyflag, **yycharset ;
  char *val1, *val2, *charset;
  int var, flag;
  UDM_AGENT * Agent;

  switch(ZEND_NUM_ARGS())
  {
    case 5:     
      if (zend_get_parameters_ex(5,&yyagent,&yyvar,&yyval1,&yyval2,&yyflag) == FAILURE)
        RETURN_FALSE;
      
      convert_to_long_ex(yyvar);
      convert_to_long_ex(yyflag);
      convert_to_string_ex(yyval1);
      convert_to_string_ex(yyval2);
      ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, -1, "mnoGoSearch-agent", le_link);
      var  = Z_LVAL_PP(yyvar);
      flag = Z_LVAL_PP(yyflag);
      val1 = Z_STRVAL_PP(yyval1);
      val2 = Z_STRVAL_PP(yyval2);
      charset = "us-ascii";
      break;

    case 6:     
      if (zend_get_parameters_ex(6,&yyagent,&yyvar,&yyval1,&yycharset,&yyval2,&yyflag) == FAILURE)
        RETURN_FALSE;
      
      convert_to_long_ex(yyvar);
      convert_to_long_ex(yyflag);
      convert_to_string_ex(yyval1);
      convert_to_string_ex(yyval2);
      convert_to_string_ex(yycharset);
      ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, -1, "mnoGoSearch-agent", le_link);
      var  = Z_LVAL_PP(yyvar);
      flag = Z_LVAL_PP(yyflag);
      val1 = Z_STRVAL_PP(yyval1);
      val2 = Z_STRVAL_PP(yyval2);
      charset = Z_STRVAL_PP(yycharset);
      break;
      
    default:
      WRONG_PARAM_COUNT;
      break;
  }


  switch(var)
  {

    case UDM_ISPELL_TYPE_AFFIX: 
      if (UdmAffixListListAdd(&Agent->Conf->Affixes,val1,charset,val2))
      {
        php_error_docref(NULL TSRMLS_CC, E_WARNING,"Cannot load affix file %s",val2);
        RETURN_FALSE;
      }
      break;
      
    case UDM_ISPELL_TYPE_SPELL: 
      if (UdmSpellListListAdd(&Agent->Conf->Spells,val1,charset,val2))
      {
        php_error_docref(NULL TSRMLS_CC, E_WARNING,"Cannot load spell file %s",val2);
        RETURN_FALSE;
      }
      break;

    default:
      php_error_docref(NULL TSRMLS_CC, E_WARNING,"Unknown ispell type parameter");
      RETURN_FALSE;
      break;
  }
  
  if (flag)
  {
    if(Agent->Conf->Spells.nitems && Agent->Conf->Affixes.nitems)
    {
      char *err= Agent->Conf->errstr;
      int flags= strcasecmp(UdmVarListFindStr(&Agent->Conf->Vars,
                "IspellUsePrefixes","no"),"no");
      flags= flags ? 0 : UDM_SPELL_NOPREFIX;
      if (UdmSpellListListLoad(&Agent->Conf->Spells, err, 128) ||
          UdmAffixListListLoad(&Agent->Conf->Affixes, flags, err, 128))
      {
        php_error_docref(NULL TSRMLS_CC, E_WARNING,"Error loading ispell data '%s'", err);
        RETURN_FALSE;
      }
    }
  }
  
  RETURN_TRUE;
}
/* }}} */

/* {{{ proto int udm_free_ispell_data(int agent)
   Free memory allocated for ispell data */
DLEXPORT PHP_FUNCTION(udm_free_ispell_data)
{
  zval ** yyagent;
  UDM_AGENT * Agent;
  switch(ZEND_NUM_ARGS())
  {
    case 1:
      if (zend_get_parameters_ex(1, &yyagent) == FAILURE)
        RETURN_FALSE;
      break;
    default:
      WRONG_PARAM_COUNT;
      break;
  }
  ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, -1, "mnoGoSearch-Agent", le_link);
  
  UdmSpellListListFree(&Agent->Conf->Spells); 
  UdmAffixListListFree(&Agent->Conf->Affixes);
  
  RETURN_TRUE;
}
/* }}} */

/* {{{ proto int udm_add_search_limit(int agent, int var, string val)
   Add mnoGoSearch search restrictions */
DLEXPORT PHP_FUNCTION(udm_add_search_limit)
{
  zval **yyagent, **yyvar, **yyval;
  char *val;
  int var;
  UDM_AGENT * Agent;

  switch(ZEND_NUM_ARGS()){
  
    case 3:     
      if(zend_get_parameters_ex(3,&yyagent,&yyvar,&yyval)==FAILURE){
        RETURN_FALSE;
      }
      convert_to_long_ex(yyvar);
      convert_to_string_ex(yyval);
      ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, -1, "mnoGoSearch-agent", le_link);
      var = Z_LVAL_PP(yyvar);
      val = Z_STRVAL_PP(yyval);
      
      break;
      
    default:
      WRONG_PARAM_COUNT;
      break;
  }
  
  switch(var){
    case UDM_LIMIT_URL: 
      UdmVarListAddStr(&Agent->Conf->Vars,"ul",val);
      break;
      
    case UDM_LIMIT_TAG: 
      UdmVarListAddStr(&Agent->Conf->Vars,"t",val);
      break;

    case UDM_LIMIT_LANG: 
      UdmVarListAddStr(&Agent->Conf->Vars,"lang",val);
      break;

    case UDM_LIMIT_CAT: 
      UdmVarListAddStr(&Agent->Conf->Vars,"cat",val);
      break;

    case UDM_LIMIT_TYPE: 
      UdmVarListAddStr(&Agent->Conf->Vars,"type",val);
      break;
      
    case UDM_LIMIT_DATE: 
                        {
                          struct tm       *d_tm;
                            time_t          d_t;
                            char            *d_val2;
                            char            d_db[20], d_de[20];
                            d_t = atol (val+1);
                            d_tm = localtime (&d_t);
                            if (val[0] == '>') {
                          UdmVarListReplaceStr(&Agent->Conf->Vars,"dt","er");
                                UdmVarListReplaceStr(&Agent->Conf->Vars,"dx","1");
                                sprintf (d_db, "%d", d_tm->tm_mday);
                                UdmVarListReplaceStr(&Agent->Conf->Vars,"dd",d_db);
                                sprintf (d_db, "%d", d_tm->tm_mon);
                                UdmVarListReplaceStr(&Agent->Conf->Vars,"dm",d_db);
                                sprintf (d_db, "%d", d_tm->tm_year+1900);
                                UdmVarListReplaceStr(&Agent->Conf->Vars,"dy",d_db);
                                RETURN_TRUE;
                            } else if (val[0] == '<') {
                                UdmVarListReplaceStr(&Agent->Conf->Vars,"dt","er");
                                UdmVarListReplaceStr(&Agent->Conf->Vars,"dx","-1");
                                sprintf (d_db, "%d", d_tm->tm_mday);
                                UdmVarListReplaceStr(&Agent->Conf->Vars,"dd",d_db);
                                sprintf (d_db, "%d", d_tm->tm_mon);
                                UdmVarListReplaceStr(&Agent->Conf->Vars,"dm",d_db);
                                sprintf (d_db, "%d", d_tm->tm_year+1900);
                                UdmVarListReplaceStr(&Agent->Conf->Vars,"dy",d_db);
                                RETURN_TRUE;
                            } else if ( (val[0]=='#') && (d_val2 = strchr(val,',')) ){
                                UdmVarListReplaceStr(&Agent->Conf->Vars,"dt","range");
                                sprintf (d_db, "%d/%d/%d", d_tm->tm_mday, d_tm->tm_mon+1, d_tm->tm_year+1900);
                                d_t = atol (d_val2+1);
                                d_tm = localtime (&d_t);
                                sprintf (d_de, "%d/%d/%d", d_tm->tm_mday, d_tm->tm_mon+1, d_tm->tm_year+1900);
                                UdmVarListReplaceStr(&Agent->Conf->Vars,"db",d_db);
                                UdmVarListReplaceStr(&Agent->Conf->Vars,"de",d_de);
                                RETURN_TRUE;
                            } else {
                                php_error_docref(NULL TSRMLS_CC, E_WARNING,"Incorrect date limit format");
                                RETURN_FALSE;
                            }
                       }
      break;
    default:
      php_error_docref(NULL TSRMLS_CC, E_WARNING,"Unknown search limit parameter");
      RETURN_FALSE;
      break;
  }
  RETURN_TRUE;
}
/* }}} */

/* {{{ proto int udm_clear_search_limits(int agent)
   Clear all mnoGoSearch search restrictions */
DLEXPORT PHP_FUNCTION(udm_clear_search_limits)
{
  zval ** yyagent;
  UDM_AGENT * Agent;
  int i;
  
  switch(ZEND_NUM_ARGS()){
    case 1: {
        if (zend_get_parameters_ex(1, &yyagent)==FAILURE) {
          RETURN_FALSE;
        }
      }
      break;
    default:
      WRONG_PARAM_COUNT;
      break;
  }
  ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, -1, "mnoGoSearch-Agent", le_link);
  for(i=0;i<Agent->Conf->Vars.nvars;i++){
    if ((!strcasecmp("ul",Agent->Conf->Vars.Var[i].name))||
        (!strcasecmp("cat",Agent->Conf->Vars.Var[i].name))||
        (!strcasecmp("t",Agent->Conf->Vars.Var[i].name))||
        (!strcasecmp("type",Agent->Conf->Vars.Var[i].name))||
        (!strcasecmp("lang",Agent->Conf->Vars.Var[i].name)))
    {
        UDM_FREE(Agent->Conf->Vars.Var[i].name);
        UDM_FREE(Agent->Conf->Vars.Var[i].val);
        Agent->Conf->Vars.nvars--;
    }
  }
  RETURN_TRUE;
}
/* }}} */


/* {{{ proto int udm_check_charset(int agent, string charset)
   Check if the given charset is known to mnogosearch */
DLEXPORT PHP_FUNCTION(udm_check_charset)
{
  zval ** yycharset, ** yyagent;
  UDM_AGENT * Agent;
  int id=-1;

  switch(ZEND_NUM_ARGS()){
    case 2: {
        if (zend_get_parameters_ex(2, &yyagent,&yycharset)==FAILURE) {
          RETURN_FALSE;
        }
      }
      break;
    default:        
      WRONG_PARAM_COUNT;
      break;
  }
  ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, id, "mnoGoSearch-Agent", le_link);
  convert_to_string_ex(yycharset);
  

  if (UdmGetCharSet(Z_STRVAL_PP(yycharset))) {
      RETURN_TRUE;
  } else {
      RETURN_FALSE;
  }  
}
/* }}} */



/* {{{ proto int udm_crc32(int agent, string str)
   Return CRC32 checksum of gived string */
DLEXPORT PHP_FUNCTION(udm_crc32)
{
  zval ** yystr, ** yyagent;
  char *str;
  int crc32;
  char buf[32];
  UDM_AGENT * Agent;
  int id=-1;

  switch(ZEND_NUM_ARGS()){
    case 2: {
        if (zend_get_parameters_ex(2, &yyagent,&yystr)==FAILURE) {
          RETURN_FALSE;
        }
      }
      break;
    default:        
      WRONG_PARAM_COUNT;
      break;
  }
  ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, id, "mnoGoSearch-Agent", le_link);
  convert_to_string_ex(yystr);
  str = Z_STRVAL_PP(yystr);

  crc32=UdmCRC32((str),strlen(str));
  snprintf(buf,sizeof(buf)-1,"%u",crc32);
  
  RETURN_STRING(buf,1);
}
/* }}} */



/* {{{ proto int udm_parse_query_string(int agent, string str)
   Parses query string, initialises variables and search limits taken from it */
DLEXPORT PHP_FUNCTION(udm_parse_query_string)
{
  zval ** yystr, ** yyagent;
  char *str;
  UDM_AGENT * Agent;
  int id=-1;

  switch(ZEND_NUM_ARGS()){
    case 2: {
        if (zend_get_parameters_ex(2, &yyagent,&yystr)==FAILURE) {
          RETURN_FALSE;
        }
      }
      break;
    default:        
      WRONG_PARAM_COUNT;
      break;
  }
  ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, id, "mnoGoSearch-Agent", le_link);
  convert_to_string_ex(yystr);
  str = Z_STRVAL_PP(yystr);

  UdmParseQueryString(Agent,&Agent->Conf->Vars,str);
  RETURN_TRUE;
}
/* }}} */

/* {{{ proto int udm_make_excerpt(int agent, int res, int row)
   Perform search */
DLEXPORT PHP_FUNCTION(udm_make_excerpt)
{
  zval ** yyagent, **yyres, **yyrow_num;
  UDM_RESULT * Res;
  UDM_AGENT * Agent;
  int id=-1, row;

  switch(ZEND_NUM_ARGS()){
    case 3: {
        if (zend_get_parameters_ex(3, &yyagent, &yyres, &yyrow_num)==FAILURE) {
          RETURN_FALSE;
        }
        convert_to_string_ex(yyrow_num);
        row=atoi(Z_STRVAL_PP(yyrow_num));
      }
      break;
    default:
      WRONG_PARAM_COUNT;
      break;
  }
  ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, id, "mnoGoSearch-Agent", le_link);
  ZEND_FETCH_RESOURCE(Res, UDM_RESULT *, yyres, -1, "mnoGoSearch-Result", le_res);
  
  if(row<Res->num_rows){
    char    *al;
    char    *Excerpt;
    size_t    ExcerptSize, ExcerptPadding;
    ExcerptSize = (size_t)UdmVarListFindInt(&Agent->Conf->Vars, "ExcerptSize", 256);
    ExcerptPadding = (size_t)UdmVarListFindInt(&Agent->Conf->Vars, "ExcerptPadding", 40);
  
    al = (char *)MyRemoveHiLightDup((const char *)(UdmVarListFindStr(&(Res->Doc[row].Sections), "URL", "")));
    UdmVarListReplaceInt(&(Res->Doc[row].Sections), "STORED_ID", UdmCRC32(al, strlen(al)));
    efree(al);
    
    Excerpt = UdmExcerptDoc(Agent, Res, &(Res->Doc[row]), ExcerptSize, ExcerptPadding);
    if (Excerpt) {
      UdmVarListReplaceStr(&(Res->Doc[row].Sections),"Body",Excerpt);
      UDM_FREE(Excerpt);
    }

    if (Excerpt != NULL && (UdmVarListFindStr(&(Res->Doc[row].Sections), "Z", NULL) == NULL)) {
      UdmVarListReplaceInt(&(Res->Doc[row].Sections),"ST",1);
      UDM_FREE(Excerpt);
    } else {
            UdmVarListReplaceInt(&(Res->Doc[row].Sections),"ST",0);
      RETURN_FALSE;
    }
  }else{
    php_error_docref(NULL TSRMLS_CC, E_WARNING,"row number too large");
    RETURN_FALSE;
  }
  
  RETURN_TRUE;
}
/* }}} */

/* {{{ proto int udm_set_agent_param_ex(int agent, string var, string val)
   Set mnoGoSearch agent session parameters extended */
DLEXPORT PHP_FUNCTION(udm_set_agent_param_ex)
{
  zval **yyagent, **yyvar, **yyval;
  char *val, *var;
  UDM_AGENT * Agent;

  switch(ZEND_NUM_ARGS()){

                case 2:
      if(zend_get_parameters_ex(2,&yyagent,&yyvar)==FAILURE){
        RETURN_FALSE;
      }
      convert_to_string_ex(yyvar);
      ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, -1, "mnoGoSearch-agent", le_link);
      var = Z_STRVAL_PP(yyvar);
      {
          UDM_CFG Cfg;
          UDM_SERVER Srv;
          UdmServerInit(&Srv);
          bzero((void*) &Cfg, sizeof(Cfg));
          Cfg.Indexer= Agent;
          Cfg.flags= UDM_FLAG_LOAD_LANGMAP + UDM_FLAG_SPELL;
          UdmEnvAddLine(&Cfg, var);
          UdmServerFree(&Srv);
          RETURN_TRUE;
          break;
      }

    case 3:     
      if(zend_get_parameters_ex(3,&yyagent,&yyvar,&yyval)==FAILURE){
        RETURN_FALSE;
      }
      convert_to_string_ex(yyvar);
      convert_to_string_ex(yyval);
      ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, -1, "mnoGoSearch-agent", le_link);
      var = Z_STRVAL_PP(yyvar);
      val = Z_STRVAL_PP(yyval);
      
      break;
      
    default:
      WRONG_PARAM_COUNT;
      break;
  }
  
  UdmVarListReplaceStr(&Agent->Conf->Vars,var,val);
  
  if (!strcasecmp(var,"LocalCharset")) {
    const char * charset=UdmVarListFindStr(&Agent->Conf->Vars,"LocalCharset","iso-8859-1");
    Agent->Conf->lcs=UdmGetCharSet(charset);
  } else if (!strcasecmp(var,"BrowserCharset")) {
    const char * charset=UdmVarListFindStr(&Agent->Conf->Vars,"BrowserCharset","iso-8859-1");
    Agent->Conf->bcs=UdmGetCharSet(charset);
  } else if (!strcasecmp(var,"Synonym")) {
    if (UdmSynonymListLoad(Agent->Conf,val)) {
      php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s",Agent->Conf->errstr);
      RETURN_FALSE;
    }
    else
    {
#if UDM_VERSION_ID >= 30307
      UdmSynonymListListSortItems(&Agent->Conf->Synonym);
#else
      UdmSynonymListSort(&(Agent->Conf->Synonyms));
#endif
    }
  } else if (!strcasecmp(var,"Stopwordfile")) {
    if (UdmStopListLoad(Agent->Conf,val)) {
      php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", Agent->Conf->errstr);
      RETURN_FALSE;
    }
  } else if (!strcasecmp(var,"MinWordLen")) {
    Agent->Conf->WordParam.min_word_len=atoi(val);
  } else if (!strcasecmp(var,"MaxWordLen")) {
    Agent->Conf->WordParam.max_word_len=atoi(val);
  }

  RETURN_TRUE;
}

/* {{{ proto string udm_get_agent_param_ex(int agent, string field)
   Fetch mnoGoSearch environment parameters */
DLEXPORT PHP_FUNCTION(udm_get_agent_param_ex)
{
  zval **yyagent, **yyfield_name;

  UDM_AGENT * Agent;
  char *field;
  
  switch(ZEND_NUM_ARGS()){
    case 2: {
        if (zend_get_parameters_ex(2, &yyagent,&yyfield_name)==FAILURE){
          RETURN_FALSE;
        }
        convert_to_string_ex(yyfield_name);
        field = Z_STRVAL_PP(yyfield_name);
      }
      break;
    default:
      WRONG_PARAM_COUNT;
      break;
  }
  
  ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, -1, "mnoGoSearch-agent", le_link);
  
  RETURN_STRING((char *)UdmVarListFindStr(&Agent->Conf->Vars,field,""),1);
}
/* }}} */

/* {{{ proto string udm_get_res_field_ex(int res, int row, string field)
   Fetch mnoGoSearch result field */
DLEXPORT PHP_FUNCTION(udm_get_res_field_ex)
{
  zval **yyres, **yyrow_num, **yyfield_name;

  UDM_RESULT * Res;
  int row;
  char *field;
  
  switch(ZEND_NUM_ARGS()){
    case 3: {
        if (zend_get_parameters_ex(3, &yyres,&yyrow_num,&yyfield_name)==FAILURE){
          RETURN_FALSE;
        }
        convert_to_string_ex(yyrow_num);
        convert_to_string_ex(yyfield_name);
        field = Z_STRVAL_PP(yyfield_name);
        row = atoi(Z_STRVAL_PP(yyrow_num));
      }
      break;
    default:
      WRONG_PARAM_COUNT;
      break;
  }
  
  ZEND_FETCH_RESOURCE(Res, UDM_RESULT *, yyres, -1, "mnoGoSearch-Result", le_res);
  if(row<Res->num_rows){
    if (!strcasecmp(field,"URL")) {
        char  *al;
        al = (char *)MyRemoveHiLightDup((const char *)(UdmVarListFindStr(&(Res->Doc[row].Sections), field, "")));
        UdmVarListReplaceStr(&Res->Doc[row].Sections,field,al);
        efree(al);
    }
    RETURN_STRING((char *)UdmVarListFindStr(&Res->Doc[row].Sections,field,""),1);
  } else {
    php_error_docref(NULL TSRMLS_CC, E_WARNING,"row number too large");
    RETURN_FALSE;
  }
}
/* }}} */



/* {{{ proto int udm_store_doc_cgi(int agent)
   Get CachedCopy of document and return TRUE if cached copy found */
DLEXPORT PHP_FUNCTION(udm_store_doc_cgi)
{
  zval ** yyagent;
  UDM_AGENT * Agent;
  int id=-1;
  
  const char  *htok;
  char            *last = NULL;
  UDM_DOCUMENT  *Doc;
  UDM_RESULT  *Res;
  UDM_HTMLTOK  tag;
  char    *HDoc=NULL;
  char    *HEnd=NULL;
  char            ch;
  const char      *content_type, *charset;
  UDM_CHARSET  *cs;

  switch(ZEND_NUM_ARGS()){
    case 1: {
        if (zend_get_parameters_ex(1, &yyagent)==FAILURE) {
          RETURN_FALSE;
        }
      }
      break;
    default:        
      WRONG_PARAM_COUNT;
      break;
  }
  ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, id, "mnoGoSearch-Agent", le_link);
  
  Doc=UdmDocInit(NULL);
  Res=UdmResultInit(NULL);
  
  UdmPrepare(Agent,Res);
  UdmVarListReplaceStr(&Doc->Sections, "URL", UdmVarListFindStr(&Agent->Conf->Vars, "URL", "0"));
  UdmVarListReplaceStr(&Doc->Sections, "dbnum", UdmVarListFindStr(&Agent->Conf->Vars, "dbnum", "0"));
  UdmURLAction(Agent, Doc, UDM_URL_ACTION_GET_CACHED_COPY);
  UdmVarListReplaceLst(&Agent->Conf->Vars, &Doc->Sections, NULL, "*");

  charset = UdmVarListFindStr(&Agent->Conf->Vars,"Charset","iso-8859-1");
  cs = UdmGetCharSet(charset);
  
  /* UnStore Doc, Highlight and Display */
  
  content_type = UdmVarListFindStr(&Agent->Conf->Vars, "Content-Type", "text/html");

  if(!Doc->Buf.content) {
      UdmResultFree(Res);
      UdmDocFree(Doc);
  
      RETURN_FALSE;
  }
  
  HEnd=HDoc = (char*)emalloc(UDM_MAXDOCSIZE + 32);
  *HEnd='\0';
  
  if (strncasecmp(content_type, "text/plain", 10) == 0) {
    sprintf(HEnd, "<pre>\n");
    HEnd += strlen(HEnd);
  }

  UdmHTMLTOKInit(&tag);
  for(htok = UdmHTMLToken(Doc->Buf.content, (const char **)&last, &tag) ; htok ;) {
    switch(tag.type) {
    case UDM_HTML_COM:
    case UDM_HTML_TAG:
      memcpy(HEnd,htok,(size_t)(last-htok));
      HEnd+=last-htok;
      HEnd[0]='\0';
        UdmHTMLParseTag(&tag,Doc);
      break;
    case UDM_HTML_TXT:
            ch = *last; *last = '\0';
      if (tag.title || tag.script) {
        sprintf(HEnd, "%s", UdmHlConvert(NULL, htok, cs, cs));
      } else {
        sprintf(HEnd, "%s", UdmHlConvert(&Res->WWList, htok, cs, cs));
      }
      HEnd=UDM_STREND(HEnd);
      *last = ch;
      break;
    }
    htok = UdmHTMLToken(NULL, (const char **)&last, &tag);
  }

  if (strncasecmp(content_type, "text/plain", 10) == 0) {
    sprintf(HEnd, "</pre>\n");
    HEnd += strlen(HEnd);
  }

  UdmVarListAddStr(&Agent->Conf->Vars, "document", HDoc);
  
  UdmResultFree(Res);
  UdmDocFree(Doc);
  efree(HDoc);
  
  RETURN_TRUE;
}
/* }}} */


/* {{{ proto int udm_alloc_agent_array(array dbaddr)
   Allocate mnoGoSearch session */
DLEXPORT PHP_FUNCTION(udm_alloc_agent_array)
{
  switch(ZEND_NUM_ARGS()){

    case 1: {
        zval **yydbaddr;
        zval **tmp;
        char *dbaddr;
        UDM_ENV   * Env;
        UDM_AGENT * Agent;
        HashPosition   pos;
        
        if(zend_get_parameters_ex(1,&yydbaddr)==FAILURE){
          RETURN_FALSE;
        }
        
        if (Z_TYPE_PP(yydbaddr) != IS_ARRAY) {
              php_error_docref(NULL TSRMLS_CC, E_WARNING, "Argument DBAddr must be an array.");
          RETURN_FALSE;
        }
        convert_to_array_ex(yydbaddr);
        
        Env=UdmEnvInit(NULL);
        UdmVarListReplaceStr(&Env->Vars,"SyslogFacility","local7");
        UdmSetLogLevel(NULL,0);
        UdmOpenLog("mnoGoSearch-php",Env,0);
        
        zend_hash_internal_pointer_reset_ex(HASH_OF(*yydbaddr), &pos);
        
        while (zend_hash_get_current_data_ex(HASH_OF(*yydbaddr), (void **)&tmp, &pos) == SUCCESS) {
            convert_to_string_ex(tmp);
            dbaddr = Z_STRVAL_PP(tmp);
            UdmDBListAdd(&Env->dbl,dbaddr, UDM_OPEN_MODE_WRITE);
            
            zend_hash_move_forward_ex(HASH_OF(*yydbaddr), &pos);
        }
        
        Agent=UdmAgentInit(NULL,Env,0);
        ZEND_REGISTER_RESOURCE(return_value,Agent,le_link);
      }
      break;
      
    default:
      WRONG_PARAM_COUNT;
      break;
  }
}
/* }}} */

/* {{{ proto int udm_hash32(int agent, string str)
   Return Hash32 checksum of gived string */
DLEXPORT PHP_FUNCTION(udm_hash32)
{
  zval ** yystr, ** yyagent;
  char *str;
  udmhash32_t hash32;
  char buf[32];
  UDM_AGENT * Agent;
  int id=-1;

  switch(ZEND_NUM_ARGS()){
    case 2: {
        if (zend_get_parameters_ex(2, &yyagent,&yystr)==FAILURE) {
          RETURN_FALSE;
        }
      }
      break;
    default:        
      WRONG_PARAM_COUNT;
      break;
  }
  ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, id, "mnoGoSearch-Agent", le_link);
  convert_to_string_ex(yystr);
  str = Z_STRVAL_PP(yystr);

  hash32=UdmHash32((str),strlen(str));
  snprintf(buf,sizeof(buf)-1,"%i",hash32);
  RETURN_STRING(buf,1);
}
/* }}} */


/* {{{ proto int udm_find(int agent, string query)
   Perform search */
DLEXPORT PHP_FUNCTION(udm_find)
{
  zval ** yyquery=NULL, ** yyagent;
  UDM_RESULT * Res;
  UDM_AGENT * Agent;
  int id=-1;

  switch(ZEND_NUM_ARGS()){
    case 1: {
        if (zend_get_parameters_ex(1, &yyagent)==FAILURE) {
          RETURN_FALSE;
        }
      }
      break;
    case 2: {
        if (zend_get_parameters_ex(2, &yyagent,&yyquery)==FAILURE) {
          RETURN_FALSE;
        }
        convert_to_string_ex(yyquery);
      }
      break;
    default:
      WRONG_PARAM_COUNT;
      break;
  }
  ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, id, "mnoGoSearch-Agent", le_link);
  
  if ((yyquery) && (strlen(Z_STRVAL_PP(yyquery)))) 
      UdmVarListReplaceStr(&Agent->Conf->Vars, "q",
          Z_STRVAL_PP(yyquery));
  if ((UDM_OK == UdmEnvPrepare(Agent->Conf)) &&
      (Res= UdmFind(Agent)))
  {
      ZEND_REGISTER_RESOURCE(return_value,Res,le_res);
  } else {
      RETURN_FALSE;
  }  
}
/* }}} */

/* {{{ proto string udm_get_res_field(int res, int row, int field)
   Fetch mnoGoSearch result field */
DLEXPORT PHP_FUNCTION(udm_get_res_field)
{
  zval **yyres, **yyrow_num, **yyfield_name;

  UDM_RESULT * Res;
  int row,field;
  
  switch(ZEND_NUM_ARGS()){
    case 3: {
        if (zend_get_parameters_ex(3, &yyres,&yyrow_num,&yyfield_name)==FAILURE){
          RETURN_FALSE;
        }
        convert_to_string_ex(yyrow_num);
        convert_to_string_ex(yyfield_name);
        field=atoi(Z_STRVAL_PP(yyfield_name));
        row=atoi(Z_STRVAL_PP(yyrow_num));
      }
      break;
    default:
      WRONG_PARAM_COUNT;
      break;
  }
  ZEND_FETCH_RESOURCE(Res, UDM_RESULT *, yyres, -1, "mnoGoSearch-Result", le_res);
  if(row<Res->num_rows){
    switch(field){
      case UDM_FIELD_URL:     
          {
        char  *al;
        al = (char *)MyRemoveHiLightDup((const char *)(UdmVarListFindStr(&(Res->Doc[row].Sections), "URL", "")));
        UdmVarListReplaceStr(&Res->Doc[row].Sections,"URL",al);
        efree(al);
        
        RETURN_STRING((char *)UdmVarListFindStr(&(Res->Doc[row].Sections),"URL",""),1);
          }
        break;
        
      case UDM_FIELD_CONTENT:   
        RETURN_STRING((char *)UdmVarListFindStr(&(Res->Doc[row].Sections),"Content-Type",""),1);
        break;
        
      case UDM_FIELD_TITLE:    
        RETURN_STRING((char *)UdmVarListFindStr(&(Res->Doc[row].Sections),"Title",""),1);
        break;
        
      case UDM_FIELD_KEYWORDS:  
        RETURN_STRING((char *)UdmVarListFindStr(&(Res->Doc[row].Sections),"Meta.Keywords",""),1);
        break;
        
      case UDM_FIELD_DESC:    
        RETURN_STRING((char *)UdmVarListFindStr(&(Res->Doc[row].Sections),"Meta.Description",""),1);
        break;
        
      case UDM_FIELD_TEXT:    
        RETURN_STRING((char *)UdmVarListFindStr(&(Res->Doc[row].Sections),"Body",""),1);
        break;
        
      case UDM_FIELD_SIZE:    
        RETURN_LONG(UdmVarListFindInt(&(Res->Doc[row].Sections),"Content-Length",0));
        break;
        
      case UDM_FIELD_URLID:
        RETURN_LONG(UdmVarListFindInt(&(Res->Doc[row].Sections),"ID",0));
        break;
        
      case UDM_FIELD_RATING:    
        RETURN_STRING((char *)UdmVarListFindStr(&(Res->Doc[row].Sections),"Score",""),1);
        break;
        
      case UDM_FIELD_MODIFIED:  
        RETURN_STRING((char *)UdmVarListFindStr(&(Res->Doc[row].Sections),"Last-Modified",""),1);
        break;

      case UDM_FIELD_ORDER:  
        RETURN_LONG(UdmVarListFindInt(&(Res->Doc[row].Sections),"Order",0));
        break;
        
      case UDM_FIELD_CRC:  
        RETURN_LONG(UdmVarListFindInt(&(Res->Doc[row].Sections),"crc32",0));
        break;
        
      case UDM_FIELD_CATEGORY:
        RETURN_STRING((char *)UdmVarListFindStr(&(Res->Doc[row].Sections),"Category",""),1);
        break;

      case UDM_FIELD_LANG:
        RETURN_STRING((char *)UdmVarListFindStr(&(Res->Doc[row].Sections),"Content-Language",""),1);
        break;

      case UDM_FIELD_CHARSET:    
        RETURN_STRING((char *)UdmVarListFindStr(&(Res->Doc[row].Sections),"Charset",""),1);
        break;

      case UDM_FIELD_SITEID:
        RETURN_LONG(UdmVarListFindInt(&(Res->Doc[row].Sections),"Site_id",0));
        
        break;

      case UDM_FIELD_POP_RANK:
        RETURN_STRING((char *)UdmVarListFindStr(&(Res->Doc[row].Sections),"Pop_Rank",""),1);
        
        break;

      case UDM_FIELD_ORIGINID:
        RETURN_LONG(UdmVarListFindInt(&(Res->Doc[row].Sections),"Origin-Id",0));

        break;
        
      default: 
        php_error_docref(NULL TSRMLS_CC, E_WARNING,"Unknown mnoGoSearch field name");
        RETURN_FALSE;
        break;
    }
  }else{
    php_error_docref(NULL TSRMLS_CC, E_WARNING,"row number too large");
    RETURN_FALSE;
  }
}
/* }}} */

/* {{{ proto string udm_get_res_param(int res, int param)
   Get mnoGoSearch result parameters */
DLEXPORT PHP_FUNCTION(udm_get_res_param)
{
  zval ** yyres, ** yyparam;
  int param;
  UDM_RESULT * Res;
  switch(ZEND_NUM_ARGS()){
    case 2: {
        if (zend_get_parameters_ex(2, &yyres, &yyparam)==FAILURE) {
          RETURN_FALSE;
        }
        convert_to_long_ex(yyparam);
        param=(Z_LVAL_PP(yyparam));
      }
      break;
    default:
      WRONG_PARAM_COUNT;
      break;
  }
  ZEND_FETCH_RESOURCE(Res, UDM_RESULT *, yyres, -1, "mnoGoSearch-Result", le_res);
  switch(param){
    case UDM_PARAM_NUM_ROWS:   
      RETURN_LONG(Res->num_rows);
      break;
    
    case UDM_PARAM_FOUND:     
      RETURN_LONG(Res->total_found);
      break;
    
    case UDM_PARAM_WORDINFO: 
#if UDM_VERSION_ID >= 30204
      {
        size_t len, i;
        for(len= i = 0; i < Res->WWList.nwords; i++) 
          len+= Res->WWList.Word[i].len + 64;
        
        {  
          size_t wsize=(1+len)*sizeof(char);
          char *wordinfo = (char*) emalloc(wsize);
          *wordinfo = '\0';
          
          for(i = 0; i < Res->WWList.nwords; i++)
          {
            if ((Res->WWList.Word[i].count > 0) || 
                (Res->WWList.Word[i].origin == UDM_WORD_ORIGIN_QUERY))
            {
              if(wordinfo[0]) strcat(wordinfo,", ");
              sprintf(UDM_STREND(wordinfo)," %s : %d",
                      Res->WWList.Word[i].word, Res->WWList.Word[i].count);
            }
            else if (Res->WWList.Word[i].origin == UDM_WORD_ORIGIN_STOP)
            {
              if(wordinfo[0]) strcat(wordinfo,", ");
              sprintf(UDM_STREND(wordinfo)," %s : stopword",
                      Res->WWList.Word[i].word);
            }
          }
          RETURN_STRING(wordinfo,0);
        }
      }
#else
      RETURN_STRING((Res->wordinfo)?(Res->wordinfo):"",1);
#endif
      break;

#if UDM_VERSION_ID >= 30204
    case UDM_PARAM_WORDINFO_ALL: 
      {
        size_t len, i;
        for(len= i= 0; i < Res->WWList.nwords; i++) 
          len+= Res->WWList.Word[i].len + 64;
        
        {
          size_t wsize= (1 + len) * sizeof(char);
          char *wordinfo= (char*) emalloc(wsize);
          *wordinfo = '\0';
          
          for(i= 0; i < Res->WWList.nwords; i++)
          {
            size_t j;
            size_t corder= Res->WWList.Word[i].order;
            size_t ccount= 0;
            for(j = 0; j < Res->WWList.nwords; j++)
            {
              if (Res->WWList.Word[j].order == corder)
              {
                ccount+= Res->WWList.Word[j].count;
              }
            }
            if (Res->WWList.Word[i].origin == UDM_WORD_ORIGIN_STOP)
            {
              sprintf(UDM_STREND(wordinfo),"%s%s : stopword", (*wordinfo) ? ", " : "",  Res->WWList.Word[i].word);
            }
            else if (Res->WWList.Word[i].origin == UDM_WORD_ORIGIN_QUERY)
            {
              sprintf(UDM_STREND(wordinfo),"%s%s : %d / %d",
                      (*wordinfo) ? ", " : "",
                      Res->WWList.Word[i].word,
                      Res->WWList.Word[i].count, ccount);
            }
            else
              continue;
          }
          RETURN_STRING(wordinfo,0);
        }
      }
      break;
#endif
      
    case UDM_PARAM_SEARCHTIME:   
      RETURN_DOUBLE(((double)Res->work_time)/1000);
      break;      
      
    case UDM_PARAM_FIRST_DOC:     
      RETURN_LONG(Res->first);
      break;

    case UDM_PARAM_LAST_DOC:     
      RETURN_LONG(Res->last);
      break;

    default:
      php_error_docref(NULL TSRMLS_CC, E_WARNING,"Unknown mnoGoSearch param name");
      RETURN_FALSE;
      break;
  }
}
/* }}} */

/* {{{ proto int udm_free_res(int res)
   mnoGoSearch free result */
DLEXPORT PHP_FUNCTION(udm_free_res)
{
  zval ** yyres;
  UDM_RESULT * Res;
  switch(ZEND_NUM_ARGS()){
    case 1: {
        if (zend_get_parameters_ex(1, &yyres)==FAILURE) {
          RETURN_FALSE;
        }
      }
      break;
    default:
      WRONG_PARAM_COUNT;
      break;
  }
  ZEND_FETCH_RESOURCE(Res, UDM_RESULT *, yyres, -1, "mnoGoSearch-Result", le_res);
  zend_list_delete(Z_LVAL_PP(yyres));
  
  RETURN_TRUE;
}
/* }}} */

/* {{{ proto int udm_free_agent(int agent)
   Free mnoGoSearch session */
DLEXPORT PHP_FUNCTION(udm_free_agent)
{
  zval ** yyagent;
  UDM_RESULT * Agent;
  switch(ZEND_NUM_ARGS()){
    case 1: {
        if (zend_get_parameters_ex(1, &yyagent)==FAILURE) {
          RETURN_FALSE;
        }
      }
      break;
    default:
      WRONG_PARAM_COUNT;
      break;
  }
  ZEND_FETCH_RESOURCE(Agent, UDM_RESULT *, yyagent, -1, "mnoGoSearch-agent", le_link);
  zend_list_delete(Z_LVAL_PP(yyagent));
  
  RETURN_TRUE;
}
/* }}} */

/* {{{ proto int udm_errno(int agent)
   Get mnoGoSearch error number */
DLEXPORT PHP_FUNCTION(udm_errno)
{
  zval ** yyagent;
  UDM_AGENT * Agent;
  switch(ZEND_NUM_ARGS()){
    case 1: {
        if (zend_get_parameters_ex(1, &yyagent)==FAILURE) {
          RETURN_FALSE;
        }
      }
      break;
    default:
      WRONG_PARAM_COUNT;
      break;
  }
  ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, -1, "mnoGoSearch-Agent", le_link);
  if (UdmEnvErrMsg(Agent->Conf) && strlen(UdmEnvErrMsg(Agent->Conf))) {
      RETURN_LONG(1);
  } else {
      RETURN_LONG(0);
  }
}
/* }}} */

/* {{{ proto string udm_error(int agent)
   Get mnoGoSearch error message */
DLEXPORT PHP_FUNCTION(udm_error)
{
  zval ** yyagent;
  UDM_AGENT * Agent;
  
  switch(ZEND_NUM_ARGS()){
    case 1: {
        if (zend_get_parameters_ex(1, &yyagent)==FAILURE) {
          RETURN_FALSE;
        }
      }
      break;
    default:
      WRONG_PARAM_COUNT;
      break;
  }
  ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, -1, "mnoGoSearch-Agent", le_link);
  RETURN_STRING((UdmEnvErrMsg(Agent->Conf))?(UdmEnvErrMsg(Agent->Conf)):"",1);
}
/* }}} */

/* {{{ proto int udm_api_version()
   Get mnoGoSearch API version */
DLEXPORT PHP_FUNCTION(udm_api_version)
{
  RETURN_LONG(UDM_VERSION_ID);
}
/* }}} */

/* {{{ proto array udm_cat_list(int agent, string category)
   Get mnoGoSearch categories list with the same root */
DLEXPORT PHP_FUNCTION(udm_cat_list)
{
  zval ** yycat, ** yyagent;
  UDM_AGENT * Agent;
  char *cat;
  UDM_CATEGORY C;
  char *buf=NULL;
  int id=-1;

  switch(ZEND_NUM_ARGS()){
    case 2: {
        if (zend_get_parameters_ex(2, &yyagent,&yycat)==FAILURE) {
          RETURN_FALSE;
        }
      }
      break;
    default:
      WRONG_PARAM_COUNT;
      break;
  }
  ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, id, "mnoGoSearch-Agent", le_link);
  convert_to_string_ex(yycat);
  cat = Z_STRVAL_PP(yycat);

  bzero(&C,sizeof(C));
  strncpy(C.addr,cat,sizeof(C.addr)-1);
  if(UDM_OK == UdmCatAction(Agent,&C,UDM_CAT_ACTION_LIST)){
    array_init(return_value);
    
    if (!(buf=ecalloc(1,UDMSTRSIZ+1))) {
      RETURN_FALSE;
    }
    
    {
        int i;
        if (C.ncategories) {
      for(i=0;i<C.ncategories;i++){
          add_next_index_stringl(return_value, C.Category[i].path,strlen(C.Category[i].path),1);
          add_next_index_stringl(return_value, C.Category[i].name,strlen(C.Category[i].name),1);
      }
        } else {
      RETURN_FALSE;
        }
    }
    efree(buf);
  } else {
    RETURN_FALSE;
  }
}
/* }}} */

/* {{{ proto array udm_cat_path(int agent, string category)
   Get mnoGoSearch categories path from the root to the given catgory */
DLEXPORT PHP_FUNCTION(udm_cat_path)
{
  zval ** yycat, ** yyagent;
  UDM_AGENT * Agent;
  char *cat;
  UDM_CATEGORY C;
  char *buf=NULL;
  int id=-1;

  switch(ZEND_NUM_ARGS()){
    case 2: {
        if (zend_get_parameters_ex(2, &yyagent,&yycat)==FAILURE) {
          RETURN_FALSE;
        }
      }
      break;
    default:
      WRONG_PARAM_COUNT;
      break;
  }
  ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, id, "mnoGoSearch-Agent", le_link);
  convert_to_string_ex(yycat);
  cat = Z_STRVAL_PP(yycat);

  bzero(&C,sizeof(C));
  strncpy(C.addr,cat,sizeof(C.addr)-1);
  if(UDM_OK == UdmCatAction(Agent,&C,UDM_CAT_ACTION_PATH)){
    array_init(return_value);
    
    if (!(buf=ecalloc(1,UDMSTRSIZ+1))) {
      RETURN_FALSE;
    }
    
    {
        int i;
        if (C.ncategories) {
      for(i=0;i<C.ncategories;i++){      
          add_next_index_stringl(return_value, C.Category[i].path,strlen(C.Category[i].path),1);
          add_next_index_stringl(return_value, C.Category[i].name,strlen(C.Category[i].name),1);
      }
        } else {
      RETURN_FALSE;
        }
    }
    efree(buf);
  } else {
    RETURN_FALSE;
  }
}
/* }}} */



/* {{{ proto int udm_get_doc_count(int agent)
   Get total number of documents in database */
DLEXPORT PHP_FUNCTION(udm_get_doc_count)
{
  zval ** yyagent;
  UDM_AGENT * Agent;
  int id=-1;

  switch(ZEND_NUM_ARGS())
  {
    case 1:
      if (zend_get_parameters_ex(1, &yyagent) == FAILURE)
        RETURN_FALSE;
      break;
    default:
      WRONG_PARAM_COUNT;
      break;
  }
  ZEND_FETCH_RESOURCE(Agent, UDM_AGENT *, yyagent, id, "mnoGoSearch-Agent", le_link);
  if (!Agent->doccount) UdmURLAction(Agent,NULL,UDM_URL_ACTION_DOCCOUNT);
  RETURN_LONG(Agent->doccount);
}
/* }}} */


#endif /* HAVE_MNOGOSEARCH */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
