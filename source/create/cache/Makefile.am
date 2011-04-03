EXTRA_DIST= create.txt drop.txt

all: create drop

create: create.single.sql create.multi.sql

drop: drop.single.sql drop.multi.sql

create.single.sql: create.txt
	cat $(srcdir)/create.txt > $@

create.multi.sql: create.txt
	cat  $(srcdir)/create.txt > $@
	for i in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	for j in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	echo "CREATE TABLE dict$$i$$j (url_id INT NOT NULL, secno INT NOT NULL, word VARCHAR(255) NOT NULL, intag VARCHAR(15000) NOT NULL);" >> $@; \
	echo "CREATE INDEX url_id$$i$$j ON dict$num (url_id);" >> $@; \
	echo "CREATE INDEX word$$i$$j ON dict$num (word);" >> $@ \
	; done \
	; done

drop.single.sql: drop.txt
	cat  $(srcdir)/drop.txt > $@

drop.multi.sql: drop.txt
	cat  $(srcdir)/drop.txt > $@
	for i in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	echo >> $@; \
	for j in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	echo "DROP TABLE dict$$i$$j;" >> $@ \
	; done \
	; done

CLEANFILES= create.single.sql \
  create.multi.sql \
  drop.single.sql \
  drop.multi.sql

sql_FILES= create.single.sql \
  create.multi.sql \
  drop.single.sql \
  drop.multi.sql

dbdir=cache

install-data-local: $(sql_FILES)
	$(mkinstalldirs) $(DESTDIR)$@CREATEDIR@/$(dbdir)
	list='${sql_FILES}'; \
	for p in $$list; do \
		${INSTALL_DATA} ./$$p $(DESTDIR)$@CREATEDIR@/$(dbdir)/ ; \
	done

uninstall-local:
	list='${sql_FILES}'; \
	for p in $$list; do \
		rm -f $(DESTDIR)$@CREATEDIR@/$(dbdir)/$$p; \
	done
	-rmdir $(DESTDIR)$@CREATEDIR@/$(dbdir)

