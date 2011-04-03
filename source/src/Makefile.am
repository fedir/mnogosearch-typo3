SUBDIRS = .

noinst_HEADERS = uconv-8bit.h
bin_PROGRAMS = search.cgi mguesser mconv @db_bin_PROGS@
sbin_PROGRAMS = @db_sbin_PROGS@

EXTRA_PROGRAMS= indexer

indexer_SOURCES = main.c mutex.c
indexer_LDADD = libmnogosearch.la libmnogocharset.la @PTHREAD_LDADD@ @READLINE_LDADD@
indexer_LDFLAGS = @PTHREAD_LFLAGS@

search_cgi_SOURCES = search.c mutex.c
search_cgi_LDADD = libmnogosearch.la libmnogocharset.la @PTHREAD_LDADD@
search_cgi_LDFLAGS = @PTHREAD_LFLAGS@

mguesser_SOURCES = mguesser.c
mguesser_LDADD = libmnogosearch.la libmnogocharset.la @PTHREAD_LDADD@
mguesser_LDFLAGS = @PTHREAD_LFLAGS@  

mconv_SOURCES = mconv.c
mconv_LDADD = libmnogosearch.la libmnogocharset.la @PTHREAD_LDADD@
mconv_LDFLAGS = @PTHREAD_LFLAGS@  

EXTRA_DIST= regex.c regex.ic

DEFS= -DUDM_CONF_DIR=\"@sysconfdir@\"   \
      -DUDM_VAR_DIR=\"@localstatedir@\" \
      -DUDM_SHARE_DIR=\"@datadir@\"    \
      @DEFS@

lib_LTLIBRARIES= libmnogocharset.la libmnogosearch.la
libmnogosearch_la_SOURCES = indexer.c url.c parser.c sql.c htdb.c spell.c \
log.c proto.c utils.c robots.c hrefs.c highlight.c conf.c search_tl.c \
crc32.c parsedate.c ftp.c socket.c host.c udmxmalloc.c http.c id3.c signals.c \
boolean.c searchtool.c searchcache.c server.c alias.c env.c httpd.c \
agent.c word.c parsehtml.c stopwords.c execget.c crossword.c dstr.c \
doc.c fuzzy.c contentencoding.c vars.c db.c dbmode-blob.c dbmode-multi.c \
sdp.c textlist.c result.c score.c synonym.c match.c store.c template.c \
guesser.c mirror.c sqldbms.c chinese.c parsexml.c wild.c hash.c segment.c \
sql-mysql.c sql-oracle.c sql-ibase.c sql-pgsql.c sql-sqlite.c sql-ctlib.c \
base64.c section.c qcache.c urldata.c urlidlist.c wordinfo.c coords.c \
wordcache.c dbmode-single.c dbmode-rawblob.c sql-odbc.c sqlmon.c \
dbug.c string.c date.c suggest.c groupby.c distance.c sitemap.c getopt.c \
features.c

EXTRA_libmnogosearch_la_SOURCES = efence.c ef_page.c ef_print.c
libmnogosearch_la_LIBADD = @efOBJ@ -L. -L.libs -lmnogocharset
libmnogosearch_la_LDFLAGS = -release 3.3
libmnogosearch_la_DEPENDENCIES = libmnogocharset.la @efOBJ@

libmnogocharset_la_SOURCES = uconv.c unidata.c sgml.c unicode.c \
uconv-8bit.c uconv-alias.c uconv-big5.c uconv-eucjp.c uconv-euckr.c \
uconv-gb2312.c uconv-gbk.c uconv-sjis.c uconv-sys.c uconv-utf8.c uconv-gujarati.c \
uconv-tscii.c 
libmnogocharset_la_LDFLAGS = -release 3.3

gcov_FILES = ${libmnogosearch_la_SOURCES} ${indexer_SOURCES}

gcov-gen:
	list='${gcov_FILES}'; for p in $$list; do gcov $$p; done

gcov-clean:
	list='${gcov_FILES}'; for p in $$list; do rm -f $$p.gcov; done

INCLUDES = -I$(srcdir)/../include \
@VIRT_INCLUDE@ \
@SOLID_INCLUDE@ \
@UNIXODBC_INCLUDE@ \
@DB2_INCLUDE@ \
@IODBC_INCLUDE@ \
@EASYSOFT_INCLUDE@ \
@MYSQL_INCLUDE@ \
@PGSQL_INCLUDE@ \
@IBASE_INCLUDE@ \
@CTLIB_INCLUDE@ \
@SQLITE_INCLUDE@ \
@SQLITE3_INCLUDE@ \
@ORACLE8_INCLUDE@ \
@SAPDB_INCLUDE@ \
@SSL_INCLUDE@ \
@CHASEN_INCLUDE@

LIBS = @STATIC_LFLAGS@ \
@LIBS@ \
@VIRT_LFLAGS@ \
@SOLID_LFLAGS@ \
@UNIXODBC_LFLAGS@ \
@DB2_LFLAGS@ \
@IODBC_LFLAGS@ \
@EASYSOFT_LFLAGS@ \
@MYSQL_LFLAGS@ \
@PGSQL_LFLAGS@ \
@IBASE_LFLAGS@ \
@CTLIB_LFLAGS@ \
@SQLITE_LFLAGS@ \
@SQLITE3_LFLAGS@ \
@ORACLE8_LFLAGS@ \
@SAPDB_LFLAGS@ \
@SSL_LFLAGS@ \
@CHASEN_LFLAGS@
