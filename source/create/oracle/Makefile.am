EXTRA_DIST= create.txt drop.multi.txt drop.txt multi.txt create.blob.txt drop.blob.txt

create_sql_FILES= create.single.sql create.multi.sql create.blob.sql
drop_sql_FILES= drop.single.sql drop.multi.sql drop.blob.sql
sql_FILES= $(create_sql_FILES) $(drop_sql_FILES)
CLEANFILES= $(sql_FILES)
DBDIR= oracle

all: create drop

create: create.single.sql create.multi.sql create.blob.sql

drop: drop.single.sql drop.multi.sql drop.blob.sql

create.single.sql: create.txt
	cat $(srcdir)/create.txt > $@

create.multi.sql: create.txt multi.txt
	cat  $(srcdir)/create.txt  $(srcdir)/multi.txt > $@

create.blob.sql: create.txt create.blob.txt
	cat  $(srcdir)/create.txt  $(srcdir)/create.blob.txt > $@

drop.single.sql: drop.txt
	cat  $(srcdir)/drop.txt > $@

drop.multi.sql: drop.txt drop.multi.txt
	cat  $(srcdir)/drop.txt  $(srcdir)/drop.multi.txt > $@

drop.blob.sql: drop.txt drop.blob.txt
	cat  $(srcdir)/drop.txt  $(srcdir)/drop.blob.txt > $@


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

