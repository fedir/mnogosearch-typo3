## Process this file with automake to produce Makefile.in 
AUTOMAKE_OPTIONS = foreign no-installinfo
SUBDIRS = include src doc etc create misc scripts msearch-test
ACLOCAL_AMFLAGS= -I build/m4

# We need /var directory - external parsers store temporary files there
install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(localstatedir)

uninstall-local:
	-rmdir $(DESTDIR)$(datadir)
	-rmdir $(DESTDIR)$(sysconfdir)
	-rmdir $(DESTDIR)$(mandir)/man1
	-rmdir $(DESTDIR)$(mandir)/man5
	-rmdir $(DESTDIR)$(mandir)
	-rmdir $(DESTDIR)$(prefix)/doc
	-rmdir $(DESTDIR)$(localstatedir)
	-rmdir $(DESTDIR)$(bindir)
	-rmdir $(DESTDIR)$(sbindir)
	-rmdir $(DESTDIR)$(libdir)
	-rmdir $(DESTDIR)$(includedir)
	-rmdir $(DESTDIR)$(datadir)
	-rmdir $(DESTDIR)$(prefix)

bin-dist:
	$(MAKE) install DESTDIR=$(CDIR)/$(PACKAGE)-$(VERSION)
	cd $(CDIR)/$(PACKAGE)-$(VERSION)$(prefix) \
		&& tar -zcPf $(PACKAGE)-$(VERSION)-$(DBTYPE)-$(host_triplet)-$(LINKING).tar.gz ./
	mv $(CDIR)/$(PACKAGE)-$(VERSION)$(prefix)/*.tar.gz $(CDIR)
	rm -r $(CDIR)/$(PACKAGE)-$(VERSION)

PHP_EXTENSION_FILES= php/CREDITS php/Makefile.in php/README php/config.m4 \
                     php/index.php php/php_mnogo.c php/php_mnogo.h

EXTRA_DIST= install.pl bootstrap perl/Makefile.PL.in \
	FAQ README.XML README.CVS ${PHP_EXTENSION_FILES}

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

## libmnogocharset distribution

CHARSETDISTFILES =  src/uconv.c src/unidata.c src/sgml.c src/unicode.c src/mconv.c \
src/uconv-8bit.c src/uconv-alias.c src/uconv-big5.c src/uconv-eucjp.c src/uconv-euckr.c \
src/uconv-gb2312.c src/uconv-gbk.c src/uconv-sjis.c src/uconv-sys.c src/uconv-utf8.c src/uconv-gujarati.c \
src/uconv-tscii.c \
include/udm_config.h.in include/udm_uniconv.h include/udm_sgml.h include/udm_unicode.h \
include/udm_unidata.h src/uconv-8bit.h include/udm_utils.h src/utils.c

charsetdistdir = libmnogocharset-$(VERSION)

charsetdistdir: $(CHARSETDISTFILES)
	-chmod -R a+w $(charsetdistdir) >/dev/null 2>&1; rm -rf $(charsetdistdir)
	mkdir $(charsetdistdir)
	$(mkinstalldirs) $(charsetdistdir)/. $(charsetdistdir)/././include $(charsetdistdir)/./include $(charsetdistdir)/src
	@for file in $(CHARSETDISTFILES); do \
	  if test -f $$file; then d=.; else d=$(srcdir); fi; \
	  dir=`echo "$$file" | sed -e 's,/[^/]*$$,,'`; \
	  if test "$$dir" != "$$file" && test "$$dir" != "."; then \
	    $(mkinstalldirs) "$(charsetdistdir)/$$dir"; \
	  fi; \
	  if test -d $$d/$$file; then \
	    cp -pR $$d/$$file $(charsetdistdir) \
	    || exit 1; \
	  else \
	    test -f $(charsetdistdir)/$$file \
	    || cp -p $$d/$$file $(charsetdistdir)/$$file \
	    || exit 1; \
	  fi; \
	done
	cp bootstrap $(charsetdistdir)
	cp charset-configure.in $(charsetdistdir)/configure.in
	cp charset-Makefile.am $(charsetdistdir)/Makefile.am
	cp src/charset-Makefile.am $(charsetdistdir)/src/Makefile.am
	-find $(charsetdistdir) -type d ! -perm -777 -exec chmod a+rwx {} \; -o \
	  ! -type d ! -perm -444 -links 1 -exec chmod a+r {} \; -o \
	  ! -type d ! -perm -400 -exec chmod a+r {} \; -o \
	  ! -type d ! -perm -444 -exec $(SHELL) $(install_sh) -c -m a+r {} {} \; \
	|| chmod -R a+r $(charsetdistdir)


charset-dist: charsetdistdir
	$(AMTAR) chof - $(charsetdistdir) | GZIP=$(GZIP_ENV) gzip -c >$(charsetdistdir).tar.gz
	-chmod -R a+w $(charsetdistdir) >/dev/null 2>&1; rm -rf $(charsetdistdir)
