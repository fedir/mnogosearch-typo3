EXTRA_DIST= create.single.txt drop.multi.txt drop.single.txt create.multi.txt create.blob.txt drop.blob.txt

all: create drop

create: create.single.sql create.multi.sql create.blob.sql

drop: drop.single.sql drop.multi.sql drop.blob.sql

create.single.sql: create.single.txt
	cat $(srcdir)/create.single.txt > $@

create.multi.sql: create.single.txt create.multi.txt
	cat  $(srcdir)/create.single.txt  $(srcdir)/create.multi.txt > $@

create.blob.sql: create.single.txt create.blob.txt
	cat  $(srcdir)/create.single.txt  $(srcdir)/create.blob.txt > $@

drop.single.sql: drop.single.txt
	cat  $(srcdir)/drop.single.txt > $@

drop.multi.sql: drop.single.txt drop.multi.txt
	cat  $(srcdir)/drop.single.txt  $(srcdir)/drop.multi.txt > $@

drop.blob.sql: drop.single.txt drop.blob.txt
	cat  $(srcdir)/drop.single.txt  $(srcdir)/drop.blob.txt > $@

CLEANFILES= create.single.sql \
  create.multi.sql \
  create.blob.sql \
  drop.single.sql \
  drop.multi.sql \
  drop.blob.sql

sql_FILES= create.single.sql \
  create.multi.sql \
  create.blob.sql \
  drop.single.sql \
  drop.multi.sql \
  drop.blob.sql

install-data-local: $(sql_FILES)
	$(mkinstalldirs) $(DESTDIR)$@CREATEDIR@/sybase
	list='${sql_FILES}'; \
	for p in $$list; do \
		${INSTALL_DATA} ./$$p $(DESTDIR)$@CREATEDIR@/sybase/ ; \
	done

uninstall-local:
	list='${sql_FILES}'; \
	for p in $$list; do \
		rm -f $(DESTDIR)$@CREATEDIR@/sybase/$$p; \
	done
	-rmdir $(DESTDIR)$@CREATEDIR@/sybase
