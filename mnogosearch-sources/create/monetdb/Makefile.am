EXTRA_DIST= create.single.txt create.multi.txt create.blob.txt drop.single.txt drop.multi.txt drop.blob.txt

create_sql_FILES= create.single.sql create.multi.sql create.blob.sql
drop_sql_FILES= drop.single.sql drop.multi.sql drop.blob.sql
sql_FILES= $(create_sql_FILES) $(drop_sql_FILES)
CLEANFILES= $(sql_FILES)
DBDIR= monetdb

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


install-data-local: $(sql_FILES)
	$(mkinstalldirs) $(DESTDIR)$@CREATEDIR@/$(DBDIR)
	list='${sql_FILES}'; \
	for p in $$list; do \
		${INSTALL_DATA} ./$$p $(DESTDIR)$@CREATEDIR@/$(DBDIR)/ ; \
	done

uninstall-local:
	list='${sql_FILES}'; \
	for p in $$list; do \
		rm -f $(DESTDIR)$@CREATEDIR@/$(DBDIR)/$$p; \
	done
	-rmdir $(DESTDIR)$@CREATEDIR@/$(DBDIR)

