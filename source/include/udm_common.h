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

#ifndef _UDM_COMMON_H
#define _UDM_COMMON_H

#include "udm_config.h"

#include <stdio.h> /* for FILE etc. */

#include <sys/types.h>
#include "udm_regex.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef MECAB
#include <mecab.h>
#endif

#ifdef HAVE_DEBUG
#include <assert.h>
#endif

#include "udm_unicode.h"
#include "udm_uniconv.h"
#include "udm_unidata.h"

extern int new_version;

/* Some constants */
#define UDM_LANGPERDOC				16		/* FIXME */
#define UDM_USER_AGENT				"MnoGoSearch/" VERSION
#define UDM_MAXWORDPERQUERY			64

/* Some sizes and others definitions */
#define UDM_MAXDOCSIZE				2*1024*1024	/**< 2 MB  */
#define UDM_DEFAULT_REINDEX_TIME		7*24*60*60	/**< 1week */
#define UDM_MAXWORDSIZE				32
#define UDM_MAXDISCWORDSIZE			64
#define UDM_DEFAULT_MAX_HOPS			256
#define UDM_READ_TIMEOUT			30
#define UDM_DOC_TIMEOUT				90
#define UDM_MAXNETERRORS			16
#define UDM_DEFAULT_NET_ERROR_DELAY_TIME	86400
#define UDM_DEFAULT_BAD_SINCE_TIME              15*24*60*60     /**< 15 days */
#define UDM_FINDURL_CACHE_SIZE                  128
#define UDM_SERVERID_CACHE_SIZE                 128


/* storage types */
#define UDM_DBMODE_SINGLE	0
#define UDM_DBMODE_MULTI	1
#define UDM_DBMODE_BLOB		6
#define UDM_DBMODE_RAWBLOB	7


/* database open modes */
#define UDM_OPEN_MODE_READ	0
#define UDM_OPEN_MODE_WRITE	1

/* TODO: remove deprecated constants*/
#define UDM_ORD_RATE		0
#define UDM_ORD_DATE		1

/* search modes */
#define UDM_MODE_ALL             0
#define UDM_MODE_ANY             1
#define UDM_MODE_BOOL            2
#define UDM_MODE_PHRASE          3
#define UDM_MODE_ALL_MINUS       4
#define UDM_MODE_ALL_MINUS_HALF  5

/* word match type */
#define UDM_MATCH_FULL		0
#define UDM_MATCH_BEGIN		1
#define UDM_MATCH_SUBSTR	2
#define UDM_MATCH_END		3
#define UDM_MATCH_REGEX		4
#define UDM_MATCH_WILD		5
#define UDM_MATCH_SUBNET	6
#define UDM_MATCH_NUMERIC_LT	7
#define UDM_MATCH_NUMERIC_GT	8

/* Case sensitivity */
#define UDM_CASE_SENSITIVE      0
#define UDM_CASE_INSENSITIVE    1

/* Indexer return codes */
#define UDM_OK			0
#define UDM_ERROR		1
#define UDM_NOTARGET		2
#define UDM_TERMINATED		3
#define UDM_UNSUPPORTED         4

/* Flags for indexing */
#define UDM_FLAG_REINDEX           1
#define UDM_FLAG_SORT_EXPIRED      2
#define UDM_FLAG_SORT_HOPS         4
#define UDM_FLAG_ADD_SERV          8
#define UDM_FLAG_SPELL	          16
#define UDM_FLAG_LOAD_LANGMAP	    32
#define UDM_FLAG_DONTSORT_SEED    64
#define UDM_FLAG_ADD_SERVURL	   128
#define UDM_FLAG_DONT_ADD_TO_DB  256


/* URLFile actions */
#define UDM_URL_FILE_REINDEX	1
#define UDM_URL_FILE_CLEAR	2
#define UDM_URL_FILE_INSERT	3
#define UDM_URL_FILE_PARSE	4

/* Ispell mode binary flags */
#define UDM_ISPELL_MODE_DB	1
#define UDM_ISPELL_USE_PREFIXES	2
#define UDM_ISPELL_MODE_SERVER  4

/* Action type: HTTP methods */
#define UDM_METHOD_UNKNOWN	0
#define UDM_METHOD_GET		1
#define UDM_METHOD_DISALLOW	2
#define UDM_METHOD_HEAD		3
#define UDM_METHOD_HREFONLY	4
#define UDM_METHOD_CHECKMP3	5
#define UDM_METHOD_CHECKMP3ONLY	6
#define UDM_METHOD_VISITLATER	7
#define UDM_METHOD_INDEX	8
#define UDM_METHOD_NOINDEX	9
#define UDM_METHOD_IMPORTONLY	10
#define UDM_METHOD_DEFAULT      UDM_METHOD_GET

/* Tags/categories defines */
#define UDM_CATSIZE	8
#define UDM_TAGSIZE	4

/* Words origins */
#define UDM_WORD_ORIGIN_QUERY           1
#define UDM_WORD_ORIGIN_SPELL           2
#define UDM_WORD_ORIGIN_SYNONYM         3
#define UDM_WORD_ORIGIN_SYNONYM_FINAL   4
#define UDM_WORD_ORIGIN_STOP            5
#define UDM_WORD_ORIGIN_SUGGEST         6
#define UDM_WORD_ORIGIN_COLLATION       7

/* URL data flags */
#define UDM_URLDATA_URL	        1
#define UDM_URLDATA_SITE        2
#define UDM_URLDATA_POP         4
#define UDM_URLDATA_LM          8 
#define UDM_URLDATA_SU         16
#define UDM_URLDATA_CACHE      32
#define UDM_URLDATA_SITE_RANK  64

/* Locking mutex numbers */
#define UDM_LOCK_DUMMY          0
#define UDM_LOCK_CONF		1
#define UDM_LOCK_WIN		2
#define UDM_LOCK_THREAD         3
#define UDM_LOCK_SQL            4
#define UDM_LOCK_SEGMENTER      5
#define UDM_LOCK_DB             6
#define UDM_LOCK_LOG		7
#define UDM_LOCK_ROBOT_FIRST	8
#define UDM_LOCK_ROBOT_LAST	31
#define UDM_LOCK_ROBOT_COUNT    (UDM_LOCK_ROBOT_LAST - UDM_LOCK_ROBOT_FIRST +1)
#define UDM_LOCK_MAX		(UDM_LOCK_ROBOT_LAST + 1)

/************************ Statistics **********************/
typedef struct stat_struct {
	int	status;
	int	expired;
	int	total;
} UDM_STAT;

typedef struct stat_list_struct{
	time_t		time;
	size_t		nstats;
	UDM_STAT	*Stat;
} UDM_STATLIST;
/************************ VARLISTs ************************/

/* Various variable flags */
#define UDM_VARFLAG_NOCLONE	1  /* Ignore for clone detection        */ 
#define UDM_VARFLAG_USERDEF	2  /* User defined section              */
#define UDM_VARFLAG_KEEPVAL	4  /* If value doesn't need to be freed */
#define UDM_VARFLAG_HTMLSOURCE	8  /* If apply HTML parser, for HTDB    */
#define UDM_VARFLAG_WIKI       16  /* If to remove text between [ and ] */
#define UDM_VARFLAG_HL         32  /* If variable has highlight markers */
#define UDM_VARFLAG_NOINDEX    64  /* If section should be in bdicti but not in bdict */

/* Variable types */
#define UDM_VAR_INT       1
#define UDM_VAR_STR       2
#define UDM_VAR_DOUBLE    3
#define UDM_VAR_ENV       16
#define UDM_VAR_AGENT     17
#define UDM_VAR_RESULT    18
#define UDM_VAR_DOCUMENT  19
#define UDM_VAR_SQLRESULT 20

#define UDM_VARSRC_QSTRING 1
#define UDM_VARSRC_ENV     2

struct udm_var_handler_st;

typedef struct udm_var_st {
	struct udm_var_handler_st *handler; /* Variable handler */
	int		section;	/**< Number 0..255   */
	size_t		maxlen;		/**< Max length      */
	size_t		curlen;		/**< Cur length      */
	char		*val;		/**< Field Value     */
	char		*name;		/**< Field Name      */
	int		flags;
} UDM_VAR;

typedef struct udm_varlist_st {
	int		freeme;
	size_t		nvars; /* Number of registered variables */
	size_t		mvars; /* Number of allocated variables  */
	size_t          svars; /* Number of sorted variables     */
	UDM_VAR		*Var;
} UDM_VARLIST;


#define UDM_TEXTLIST_FLAG_SKIP_ADD_SECTION 1
typedef struct {
	char		*str;
	char		*href;
	char		*section_name;
	int		section;
	int		flags;
} UDM_TEXTITEM;

typedef struct {
	size_t		nitems;
	UDM_TEXTITEM	*Item;
} UDM_TEXTLIST;

/*****************************************************/

/** StopList unit */
typedef struct udm_stopword_struct {
	char		*word;
	char		*lang;
} UDM_STOPWORD;

#define UDM_STOPLIST_LANGLEN 32
#define UDM_STOPLIST_CSETLEN 32
#define UDM_STOPLIST_FILELEN 128

typedef struct
{
  size_t nstopwords;
  UDM_STOPWORD	*StopWord;
  char  lang[UDM_STOPLIST_LANGLEN];
  char  cset[UDM_STOPLIST_CSETLEN];
  char  fname[UDM_STOPLIST_FILELEN];
} UDM_STOPLIST;

typedef struct
{
  size_t nitems;
  UDM_STOPLIST *Item;
} UDM_STOPLISTLIST;

/*****************************************************/

/** Words parameters */
typedef struct {
	size_t		min_word_len;
	size_t		max_word_len;
} UDM_WORDPARAM;

typedef uint4 udm_pos_t;
typedef unsigned char udm_secno_t;
typedef unsigned char udm_wordnum_t;

/**************************/
typedef struct
{
  urlid_t       url_id;
  udm_pos_t     seclen;
  udm_pos_t     pos;
  udm_wordnum_t num;
  udm_secno_t   secno;
} UDM_URL_CRD;


typedef struct
{
  size_t       acoords;
  size_t       ncoords;
  size_t       order;
  char	       *word;
  UDM_URL_CRD  *Coords;
} UDM_URLCRDLIST;


/***************************/

typedef struct udm_coord2_st
{
  udm_pos_t pos:24;
  udm_wordnum_t order;
} UDM_COORD2;


typedef struct udm_section_st
{
  UDM_COORD2 *Coord;                 /* 4/8 bytes */
  const unsigned char *PackedCoord;  /* 4/8 bytes */
  urlid_t url_id;                    /* 4   bytes */
  udm_pos_t ncoords;                 /* 4   bytes */
  udm_pos_t seclen;                  /* 4   bytes */
  udm_pos_t minpos;                  /* 4   bytes */
  udm_pos_t maxpos;                  /* 4   bytes */
  udm_secno_t secno;                 /* 1   byte  */
  udm_wordnum_t wordnum;             /* 1   byte  */
  udm_wordnum_t order;               /* 1   byte  */
} UDM_SECTION;                       /* 32 bytes (i386), 40 bytes (64bit) */


typedef struct udm_sectionlist_st
{
  size_t mcoords;
  size_t ncoords;
  UDM_COORD2 *Coord;
  size_t msections;
  size_t nsections;
  UDM_SECTION *Section;
} UDM_SECTIONLIST;


typedef struct udm_sectionlistlist_st
{
  size_t nitems;
  size_t mitems;
  UDM_SECTIONLIST *Item;
} UDM_SECTIONLISTLIST;



/** Main search structure */
typedef struct {
  urlid_t url_id;
  uint4   score;
} UDM_URL_SCORE;

typedef struct {
  size_t nitems;
  UDM_URL_SCORE	*Item;
} UDM_URLSCORELIST;


/* UserScore and UserSiteScore structure */

typedef struct udm_url_int4_st
{
  urlid_t url_id;
  int4  param;
} UDM_URL_INT4;

typedef struct udm_url_int4_list_st
{
  size_t nitems;
  UDM_URL_INT4 *Item;
} UDM_URL_INT4_LIST;



/* Structure to handle limits */
typedef struct udm_urlid_list_st
{
  char empty;
  char exclude;
  urlid_t *urls;
  size_t nurls;
} UDM_URLID_LIST;


typedef struct {
        urlid_t         url_id;
        uint4           score;
        uint4           per_site;
        urlid_t         site_id;
        time_t          last_mod_time;
        double          pop_rank;
	char            *url;
        char            *section;
} UDM_URLDATA;

#define UDM_COORD2DBNUM(score) (255 - (int) ((score) & 0xFF))

typedef struct {
	size_t		nitems;
	UDM_URLDATA	*Item;
} UDM_URLDATALIST;


/** Word list unit */
typedef struct
{
  char		*word;
  udm_pos_t     pos;
  udm_secno_t   secno;
  unsigned char hash;
  unsigned char seclen_marker;
} UDM_WORD;

typedef struct {
	size_t		mwords;	/**< Number of memory allocated for words     */
	size_t		nwords;	/**< Real number of words in list             */
	size_t		swords;	/**< Number of words in sorted list           */
	size_t		wordpos;/**< For phrases, number of current word      */
#ifdef TRIAL_VER
	UDM_WORD	Word[256];
#else
	UDM_WORD	*Word;	/**< Word list  itself                        */
#endif
} UDM_WORDLIST;


typedef struct
{
  size_t nitems;
  UDM_WORDLIST Item[256];
} UDM_WORDLISTLIST;


/***************************************************************/

/** Cross-word list unit */
typedef struct {
	short	pos;
	short	weight;
	char	*word;
	char	*url;
	urlid_t	referree_id;
} UDM_CROSSWORD;

typedef struct {
	size_t		ncrosswords;
	size_t		mcrosswords;
	size_t		wordpos[256];
	UDM_CROSSWORD	*CrossWord;
} UDM_CROSSLIST;

/*****************************************************************/

typedef struct {
	int	max_net_errors;
	int	net_error_delay_time;
	int	read_timeout;
	int	doc_timeout;
	int	period;		/**< Reindex period           */
	int	maxhops;	/**< Max way in mouse clicks  */
	int	index;		/**< Whether to index words   */
	int	follow;		/**< Whether to follow links  */
	int	use_robots;	/**< Whether to use robots.txt*/
	int	use_clones;	/**< Whether to detect clones */
        int	doc_per_site;
        int	collect_links;
        int	crawl_delay;
} UDM_SPIDERPARAM;

/*****************************************************************/

#define UDM_MATCH_FLAG_SKIP_OPTIMIZATION 1

typedef struct {
	int		match_type;
	int		nomatch;
	int		case_sense;
	int		flags; /* e.g. skip normalizaton */
	char		*section;
	char		*pattern;
	size_t          pattern_length;
	regex_t		*reg;
	char		*arg;
	char		*arg1;
} UDM_MATCH;

typedef struct {
	size_t		nmatches;
	UDM_MATCH	*Match;
} UDM_MATCHLIST;

typedef struct {
	int beg;
	int end;
} UDM_MATCH_PART;
/*****************************************************************/

/** Structure to store server parameters */
typedef struct {
	UDM_MATCH	Match;
	urlid_t         site_id;        /**< server.rec_id            */
	char            command;        /**< 'S' - server,realm, 'F' - disallow,allow */
	int             ordre;          /**< order in list to find    */
	urlid_t         parent;         /**< parent rec_id for grouping by site */
	float           weight;         /**< server weight for popularity rank calculation */
	UDM_VARLIST	Vars;		/**< Default lang, charset,etc*/
        uint4           MaxHops;
        int             follow;         /* Page, Path, Site, World, etc*/
        int             method;         /* Allow, Disallow, etc */
        int             enabled;
} UDM_SERVER;

typedef struct {
	size_t		nservers;
	size_t		mservers;
	int		have_subnets;
	UDM_SERVER	*Server;
} UDM_SERVERLIST;


/*******************************************************/
/* All links are stored in the cache of this structure */
/* before actual INSERT into database                  */

typedef struct {
	char	*url;
        urlid_t referrer;
	uint4	hops;
	int	stored;	
	int	method;
	int	collect_links;
        urlid_t site_id;
        urlid_t server_id;
        urlid_t rec_id;
        size_t  max_doc_per_site;
        UDM_VARLIST Vars;
} UDM_HREF;

typedef struct {
	size_t		mhrefs;
	size_t		nhrefs;
	size_t		shrefs;
	size_t		dhrefs;
	UDM_HREF	*Href;
} UDM_HREFLIST;

/*******************************************************/

/** Resolve stuff */
typedef struct udm_host_addr_struct {
	char		*hostname;
	struct in_addr	addr;
	int		net_errors;
	time_t		last_used;
}UDM_HOST_ADDR;

typedef struct {
	size_t		nhost_addr;
	size_t		mhost_addr;
	UDM_HOST_ADDR	*host_addr;
} UDM_HOSTLIST;

/** Used in FTP sessions */
typedef struct udm_conn_struct {
        int	status;
        int	connected;
        int	err;
        time_t	host_last_used;
        int	conn_fd;
#ifdef WIN32
        unsigned short port;
#else
        int	port;
#endif
        int	timeout;
        char	*hostname;
        char    *user;
        char    *pass;
        struct	sockaddr_in sin;
        int	buf_len;
        size_t	buf_len_total;
        int	len;
        char	*buf;
        int     net_errors;
        struct	udm_conn_struct *connp;
} UDM_CONN;

/** Parsed URL string */
typedef struct udm_url {
	char	*schema;
	char	*specific;
	char	*hostinfo;
	char	*auth;
	char	*hostname;
	char	*path;
	char	*filename;
	char	*anchor;
	int	port;
	int	default_port;
} UDM_URL;


/***************************************************/

typedef struct {
	char	*buf;		/**< Buffer to download document to          */
	char	*content;	/**< Pointer to content, after headers       */
	size_t	size;		/**< Number of bytes loaded                  */
	size_t	maxsize;	/**< Maximum bytes to load into buf          */
	size_t  content_length; /**  For HTDB **/
} UDM_HTTPBUF;

typedef struct {
	int	freeme;		/**< Whether  memory was allocated for doc   */
	int	stored;		/**< If it is already stored, forAddHref()   */
	int	method;		/**< How to download document: GET, HEAD etc */
	
	UDM_HTTPBUF		Buf;		/**< Buffer       */
	
	UDM_HREFLIST		Hrefs;		/**< Link list    */
	UDM_WORDLIST		Words;		/**< Word list    */
	UDM_CROSSLIST		CrossWords;	/**< Crosswords   */
	
	UDM_VARLIST		RequestHeaders;	/**< Extra headers*/
	UDM_VARLIST		Sections;	/**< User sections*/
	
	UDM_TEXTLIST		TextList;	/**< Text list    */
	UDM_URL			CurURL;		/**< Parsed URL   */
	UDM_CHARSET		*lcs;		/**< LocalCharser */
	UDM_SPIDERPARAM		Spider;		/**< Spider prms  */
	UDM_CONN		connp;		/**< For FTP      */
	UDM_CONN		connp2;		/**< For FTP      */
} UDM_DOCUMENT;

/********************************************************/

/** External Parsers */
typedef struct udm_parser_struct{
        char		*from_mime;
	char		*to_mime;
	char		*cmd;
	char		*src;
} UDM_PARSER;

typedef struct {
	size_t		nparsers;
	UDM_PARSER	*Parser;
} UDM_PARSERLIST;


/******* Ispell BEGIN ********/

#define UDM_SPELL_NOPREFIX 1

typedef struct udm_spell_st
{
  char *word;
  char *flags;
} UDM_SPELL;

#define UDM_SPELL_LANGLEN 32
#define UDM_SPELL_CSETLEN 32
#define UDM_SPELL_FILELEN 128
#define UDM_SPELL_FMT_TEXT 0
#define UDM_SPELL_FMT_HASH 1

typedef struct udm_dict_st
{
  char  lang[UDM_SPELL_LANGLEN];
  char  cset[UDM_SPELL_CSETLEN];
  char  fname[UDM_SPELL_FILELEN];
  int   fmt;
  int   fd;
  size_t wordlen;
  UDM_CHARSET *cs;
  char   *fbody;
  size_t nitems;
  size_t mitems;
  UDM_SPELL *Item;
} UDM_SPELLLIST;


typedef struct udm_spelllistlist_st
{
  size_t nitems;
  size_t mitems;
  size_t nspell;              /* Backward capability with php-4.3.x */
  UDM_SPELLLIST *Item;
} UDM_SPELLLISTLIST;


typedef struct udm_aff_st
{
  char flag;
  char type;
  regex_t regex;
  char *find;
  char *repl;
  char *mask;
  size_t findlen;
  size_t replen;
} UDM_AFFIX;


typedef struct udm_afflist_st
{
  size_t mitems;
  size_t nitems;
  char  lang[UDM_SPELL_LANGLEN];
  char  cset[UDM_SPELL_CSETLEN];
  char  fname[UDM_SPELL_FILELEN];
  UDM_CHARSET *cs;
  UDM_AFFIX *Item;
} UDM_AFFIXLIST;


typedef struct udm_afflistlist_st
{
  size_t mitems;
  size_t nitems;
  UDM_AFFIXLIST *Item;
} UDM_AFFIXLISTLIST;


/******* Ispell END **********/


typedef struct{
	int		cmd; /**< 'allow' or 'disallow' */
	char		*path;
} UDM_ROBOT_RULE;

typedef struct{
	char		*hostinfo;
	size_t		nrules;
	UDM_ROBOT_RULE	*Rule;
} UDM_ROBOT;

typedef struct{
	size_t		nrobots;
	UDM_ROBOT	*Robot;
} UDM_ROBOTS;


typedef struct {
	size_t		order;
        size_t          order_extra_width; /* For multi-word synonyms, see below */
	size_t		count;
	size_t          doccount; /* Number of documents this word appears in */
	char		*word;
	size_t		len;
        int     	origin;
        int             weight; /* origin-dependent weight   */
        int             user_weight; /* User-supplied weight */
        int		match;
        size_t		secno;  /* Which section to search in */
        size_t		phrpos; /* 0 means "not in phrase"    */
        size_t		phrlen; /* phase length               */
        size_t          phrwidth; /* How many additional parts in a multi-word */
} UDM_WIDEWORD;

/*
  order_extra_width - use in case of many-to-one and many-to-many synonyms.
  It represents the number of query words this synonym covers.
  For example, if we have synonym:
  "aaaa bbbb" -> "cccc"
  then origin_extra_width for the words "cccc" will be 2, because
  it is a replacement for two words.
  0 means "not a multi-word replacement".
*/

typedef struct {
	int		wm;              /* Search mode: wrd, sub, beg, end */
	int             strip_noaccents; /* If accent insensitive comparison*/
	size_t		nuniq;
	size_t		nwords;
	UDM_WIDEWORD	*Word;
} UDM_WIDEWORDLIST;


typedef struct {
	char	*p;
	char	*s;
	int     origin; /* SYNONYM or SYNONYM_FINAL */
} UDM_SYNONYM;

#define UDM_SYNONYM_LANGLEN 32
#define UDM_SYNONYM_CSETLEN 32
#define UDM_SYNONYM_FILELEN 128
#define UDM_SYNONYM_FMT_TEXT 0
#define UDM_SYNONYM_FMT_HASH 1

typedef struct {
	size_t		nsynonyms;
	size_t		msynonyms;
	UDM_SYNONYM	*Synonym;
        char  lang[UDM_SYNONYM_LANGLEN];
        char  cset[UDM_SYNONYM_CSETLEN];
        char  fname[UDM_SYNONYM_FILELEN];
        size_t max_phrase_length; /* Many-to-many, many-to-one, one-to-many */
} UDM_SYNONYMLIST;


typedef struct
{
  size_t nitems;
  UDM_SYNONYMLIST *Item;
} UDM_SYNONYMLISTLIST;


typedef struct udm_chinaword_struct {
  int *word;
  int  freq;
} UDM_CHINAWORD;

typedef struct {
  size_t        nwords, mwords;
  size_t        total;
  UDM_CHINAWORD *ChiWord;
  size_t        *hash;
} UDM_CHINALIST;


typedef struct udm_category_struct {
	int		rec_id;
	char		path[128];
	char		link[128];
	char		name[128];
} UDM_CATITEM;

typedef struct {
	char		addr[128];
	size_t		ncategories;
	UDM_CATITEM	*Category;
} UDM_CATEGORY;

/* Boolean search constants and types */
#define UDM_MAXSTACK	 128
#define UDM_STACK_LEFT	 0
#define UDM_STACK_RIGHT	 1
#define UDM_STACK_BOT	 2
#define UDM_STACK_OR	 3
#define UDM_STACK_AND	 4
#define UDM_STACK_NOT	 5
#define UDM_STACK_PHRASE 6
#define UDM_STACK_WORD	 200
#define UDM_STACK_STOP   201

typedef struct {
	size_t		ncstack, mcstack;
	int		*cstack;
	size_t		nastack, mastack;
	unsigned long	*astack;
} UDM_BOOLSTACK;

typedef struct {
	int		cmd;
	unsigned long	arg;
} UDM_STACK_ITEM;

typedef struct {
        size_t nitems;
        size_t mitems;
        size_t ncmds;
        UDM_STACK_ITEM *items;
} UDM_STACKITEMLIST;

typedef struct {
	size_t			work_time;
	size_t			first;
	size_t			last;
	size_t			total_found;
	size_t			num_rows;
	size_t			cur_row;
/*	size_t			offset;*/
	size_t			memused;
	int			freeme;
	UDM_DOCUMENT		*Doc;
	
	UDM_WIDEWORDLIST	WWList;
	UDM_URLDATALIST		URLData;
	UDM_STACKITEMLIST	ItemList;
	
} UDM_RESULT;


#include "udm_db_int.h"

typedef struct {
	size_t		nitems;
        size_t          currdbnum;
	UDM_DB		*db;
} UDM_DBLIST;


typedef struct
{
  int is_log_open; /* Flag indicating if openlog() has been called      */
  FILE *logFD;     /* File descriptor, when logging to stderr or file   */
  int facility;    /* Which facility to use, or negative number if none */
} UDM_LOG;


/** Forward declaration of UDM_AGENT */
struct udm_indexer_struct;

/** Config file */
typedef struct udm_config_struct {
	int		freeme;
	char		errstr[2048];
	UDM_CHARSET	*bcs;
	UDM_CHARSET	*lcs;
	
	int		url_number;	/**< For indexer -nXXX          */
	
	UDM_SERVERLIST	Servers;	/**< List of servers and realms */
        UDM_SERVER      *Cfg_Srv;
	
	UDM_MATCHLIST	Aliases;	/**< Straight aliases           */
	UDM_MATCHLIST	ReverseAliases;	/**< Reverse aliases            */
	UDM_MATCHLIST	MimeTypes;	/**< For AddType commands       */
	UDM_MATCHLIST	Filters;	/**< Allow, Disallow,etc        */
	UDM_MATCHLIST	SectionFilters; /**< IndexIf, NoIndexIf         */
        UDM_MATCHLIST   SectionHdrMatch;/**< User sections after headers*/
        UDM_MATCHLIST   SectionGsrMatch;/**< User sections after quesser*/
        UDM_MATCHLIST   SectionMatch;   /**< User sections after parser */
        UDM_MATCHLIST	Encodings;	/**< For AddType commands       */
	
	UDM_HREFLIST	Hrefs;		/**< Links cache                */
	UDM_RESULT	Targets;	/**< Targets cache              */
	
	UDM_VARLIST	Sections;	/**< document section to parse  */
	UDM_VARLIST	Vars;		/**< Config parameters          */
	UDM_VARLIST	Cookies;	/**< Cookie list                */
	UDM_VARLIST	XMLEnterHooks;
	UDM_VARLIST	XMLLeaveHooks;
	UDM_VARLIST	XMLDataHooks;
	
	UDM_LANGMAPLIST	LangMaps;	/**< For lang+charset quesser   */
	UDM_ROBOTS	Robots;		/**< robots.txt information     */
	UDM_SYNONYMLISTLIST Synonym;    /**< Synonims list              */
	UDM_STOPLISTLIST StopWord;      /**< Stopwords list             */
	UDM_PARSERLIST	Parsers;	/**< External  parsers          */
	UDM_DBLIST	dbl;		/**< Searchd addresses	      */
	UDM_HOSTLIST	Hosts;		/**< Resolve cache              */
	UDM_SPELLLISTLIST Spells;	/**< For ispell dictionaries    */
	UDM_AFFIXLISTLIST Affixes;	/**< For ispell affixes         */
	UDM_WORDPARAM	WordParam;	/**< Word limits                */
        UDM_CHINALIST   Chi;            /**< Chinese words list         */
        UDM_CHINALIST   Thai;           /**< Thai words list            */
	
	UDM_LOG Log;
	
	int		CVS_ignore;	/**< Skip CVS directgories - for tests */
	
	/* Various virtual functions */
	void (*ThreadInfo)(struct udm_indexer_struct *,const char * state,const char * str);
	void (*LockProc)(struct udm_indexer_struct *,int command,int type,const char *fname,int lineno);
	void (*RefInfo)(int code,const char *url, const char *ref);
	int  (*DumpDoc)(struct udm_indexer_struct *, UDM_DOCUMENT *Doc);
	int (*ThreadCreate)(void **thd, void *(*start_routine)(void *), void *arg);
	int (*ThreadJoin)(void *thd);
#ifdef MECAB
        mecab_t         *mecab;
#endif
        UDM_UNIDATA *unidata;

} UDM_ENV;


#define UDM_AGENT_STATE_MUTEXES 5

typedef struct udm_agent_state_t
{
  const char *task;
  const char *param;
  const char *extra;
  time_t start_time;
  size_t mutex_owned[UDM_AGENT_STATE_MUTEXES];
  size_t nmutexes;
} UDM_AGENT_STATE;

/** Indexer */
typedef struct udm_indexer_struct{
	int		freeme;		/**< whenever it was allocated    */
	int		handle;		/**< Handler for threaded version */
	time_t		start_time;	/**< Time of allocation, for stat */
	udm_uint8   nbytes;     /**< Number of bytes downloaded   */
	size_t		ndocs;		/**< Number of documents          */
        size_t          nsleepsecs;     /**> Number of sleep seconds      */
	int		flags;		/**< Callback function to request action*/
        int             action;
	int		doccount;	/**< for UdmGetDocCount()         */
	UDM_ENV		*Conf;		/**< Configuration                */
	UDM_LANGMAP	*LangMap;	/**< LangMap for current document */
	UDM_RESULT	Indexed;	/**< Indexed cache              */

	UDM_AGENT_STATE State;
	
        char    *UdmFindURLCache[UDM_FINDURL_CACHE_SIZE];
        urlid_t UdmFindURLCacheId[UDM_FINDURL_CACHE_SIZE];
        size_t  pURLCache;
        char    *ServerIdCache[UDM_SERVERID_CACHE_SIZE];
        char    ServerIdCacheCommand[UDM_SERVERID_CACHE_SIZE];
        urlid_t ServerIdCacheId[UDM_SERVERID_CACHE_SIZE];
        size_t  pServerIdCache;

#ifdef USE_TRACE
  FILE *TR;
#endif
	
} UDM_AGENT;



typedef struct {
	char	*url;
	int	status;
} UDM_URLSTATE;

typedef int (*udm_qsort_cmp)(const void*, const void*);

typedef struct {
	uint4	hi,lo;
	off_t	pos;
	size_t	len;
} UDM_UINT8_POS_LEN;

typedef struct {
	uint4	val;
	off_t	pos;
	size_t	len;
} UDM_UINT4_POS_LEN;

typedef struct {
	uint4	val;
	urlid_t	url_id;
} UDM_UINT4URLID;

typedef struct {
	size_t		nitems;
	UDM_UINT4URLID	*Item;
} UDM_UINT4URLIDLIST;

typedef struct {
	uint4	hi,lo;
	urlid_t	url_id;
} UDM_UINT8URLID;

typedef struct {
	size_t		nitems;
	UDM_UINT8URLID	*Item;
} UDM_UINT8URLIDLIST;


#define UDM_LOGD_CMD_WORD  0
#define UDM_LOGD_CMD_DATA  1
#define UDM_LOGD_CMD_CHECK 2

#define UDM_MAXTAGVAL	64

typedef struct {
	int	type;
	int	script;
	int	style;
	int	title;
	int	body;
	int	follow;
	int	index;
	int	comment;
	int	nonbreaking;
	int     skip_attribute_sections;
	char	*lasthref;
	size_t	ntoks;
	struct {
		const char *name;
		const char *val;
		size_t     nlen;
		size_t     vlen;
	} toks[UDM_MAXTAGVAL+1];
} UDM_HTMLTOK;

typedef struct udm_cfg_st {
        UDM_AGENT       *Indexer;
	UDM_SERVER	*Srv;
	int		flags;
	int		level;
	int		ordre;
} UDM_CFG;

#ifndef udm_max
#define udm_max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef udm_min
#define udm_min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define UDM_DT_BACK    1
#define UDM_DT_ER      2
#define UDM_DT_RANGE   3
#define UDM_DT_UNKNOWN 4

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
  UDM_IND_EXECSQL=       320,
  UDM_IND_SET=           321, /* indexer --set=a=b -> a=b    */
  UDM_IND_SET0=          322  /* indexer --fl=xxx  -> fl=xxx */
};

#endif
