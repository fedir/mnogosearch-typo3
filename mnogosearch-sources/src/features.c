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

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_vars.h"

void
UdmFeatures(UDM_VARLIST *V)
{
#ifdef HAVE_PTHREAD
  UdmVarListReplaceStr(V,"HAVE_PTHREAD","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_PTHREAD","no");
#endif
#ifdef HAVE_FHS_LAYOUT
  UdmVarListReplaceStr(V, "HAVE_FHS_LAYOUT", "yes");
#else
  UdmVarListReplaceStr(V, "HAVE_FHS_LAYOUT", "no");
#endif
#ifdef USE_HTTPS
  UdmVarListReplaceStr(V,"USE_HTTPS","yes");
#else
  UdmVarListReplaceStr(V,"USE_HTTPS","no");
#endif
#ifdef DMALLOC
  UdmVarListReplaceStr(V,"DMALLOC","yes");
#else
  UdmVarListReplaceStr(V,"DMALLOC","no");
#endif
#ifdef EFENCE
  UdmVarListReplaceStr(V,"EFENCE","yes");
#else
  UdmVarListReplaceStr(V,"EFENCE","no");
#endif
#ifdef CHASEN
  UdmVarListReplaceStr(V,"CHASEN","yes");
#else
  UdmVarListReplaceStr(V,"CHASEN","no");
#endif
#ifdef MECAB
  UdmVarListReplaceStr(V,"MECAB","yes");
#else
  UdmVarListReplaceStr(V,"MECAB","no");
#endif
#ifdef HAVE_ZLIB
  UdmVarListReplaceStr(V,"HAVE_ZLIB","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_ZLIB","no");
#endif
#ifdef USE_SYSLOG
  UdmVarListReplaceStr(V,"USE_SYSLOG","yes");
#else
  UdmVarListReplaceStr(V,"USE_SYSLOG","no");
#endif
#ifdef USE_PARSER
  UdmVarListReplaceStr(V,"USE_PARSER","yes");
#else
  UdmVarListReplaceStr(V,"USE_PARSER","no");
#endif
#ifdef USE_MP3
  UdmVarListReplaceStr(V,"USE_MP3","yes");
#else
  UdmVarListReplaceStr(V,"USE_MP3","no");
#endif
#ifdef USE_FILE
  UdmVarListReplaceStr(V,"USE_FILE","yes");
#else
  UdmVarListReplaceStr(V,"USE_FILE","no");
#endif
#ifdef USE_HTTP
  UdmVarListReplaceStr(V,"USE_HTTP","yes");
#else
  UdmVarListReplaceStr(V,"USE_HTTP","no");
#endif
#ifdef USE_FTP
  UdmVarListReplaceStr(V,"USE_FTP","yes");
#else
  UdmVarListReplaceStr(V,"USE_FTP","no");
#endif
#ifdef USE_NEWS
  UdmVarListReplaceStr(V,"USE_NEWS","yes");
#else
  UdmVarListReplaceStr(V,"USE_NEWS","no");
#endif
#ifdef HAVE_MYSQL
  UdmVarListReplaceStr(V,"HAVE_MYSQL","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_MYSQL","no");
#endif
#ifdef HAVE_PGSQL
  UdmVarListReplaceStr(V,"HAVE_PGSQL","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_PGSQL","no");
#endif
#ifdef HAVE_IODBC
  UdmVarListReplaceStr(V,"HAVE_IODBC","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_IODBC","no");
#endif
#ifdef HAVE_UNIXODBC
  UdmVarListReplaceStr(V,"HAVE_UNIXODBC","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_UNIXODBC","no");
#endif
#ifdef HAVE_DB2
  UdmVarListReplaceStr(V,"HAVE_DB2","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_DB2","no");
#endif
#ifdef HAVE_SOLID
  UdmVarListReplaceStr(V,"HAVE_SOLID","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_SOLID","no");
#endif
#ifdef HAVE_VIRT
  UdmVarListReplaceStr(V,"HAVE_VIRT","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_VIRT","no");
#endif
#ifdef HAVE_EASYSOFT
  UdmVarListReplaceStr(V,"HAVE_EASYSOFT","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_EASYSOFT","no");
#endif
#ifdef HAVE_SAPDB
  UdmVarListReplaceStr(V,"HAVE_SAPDB","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_SAPDB","no");
#endif
#ifdef HAVE_IBASE
  UdmVarListReplaceStr(V,"HAVE_IBASE","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_IBASE","no");
#endif
#ifdef HAVE_CTLIB
  UdmVarListReplaceStr(V,"HAVE_CTLIB","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_CTLIB","no");
#endif
#ifdef HAVE_ORACLE8
  UdmVarListReplaceStr(V,"HAVE_ORACLE8","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_ORACLE8","no");
#endif
#ifdef HAVE_CHARSET_big5
  UdmVarListReplaceStr(V,"HAVE_CHARSET_big5","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_CHARSET_big5","no");
#endif
#ifdef HAVE_CHARSET_euc_kr
  UdmVarListReplaceStr(V,"HAVE_CHARSET_euc_kr","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_CHARSET_euc_kr","no");
#endif
#ifdef HAVE_CHARSET_gb2312
  UdmVarListReplaceStr(V,"HAVE_CHARSET_gb2312","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_CHARSET_gb2312","no");
#endif
#ifdef HAVE_CHARSET_japanese
  UdmVarListReplaceStr(V,"HAVE_CHARSET_japanese","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_CHARSET_japanese","no");
#endif
#ifdef HAVE_CHARSET_gbk
  UdmVarListReplaceStr(V,"HAVE_CHARSET_gbk","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_CHARSET_gbk","no");
#endif
#ifdef HAVE_CHARSET_gujarati
  UdmVarListReplaceStr(V,"HAVE_CHARSET_gujarati","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_CHARSET_gujarati","no");
#endif
#ifdef HAVE_CHARSET_tscii
  UdmVarListReplaceStr(V,"HAVE_CHARSET_tscii","yes");
#else
  UdmVarListReplaceStr(V,"HAVE_CHARSET_tscii","no");
#endif
}
