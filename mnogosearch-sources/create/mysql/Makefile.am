EXTRA_DIST= create.txt drop.txt url-raid.txt create.blob.txt drop.blob.txt

all: create drop

create: create.single.sql create.multi.sql create.blob.sql

drop: drop.single.sql drop.multi.sql drop.blob.sql

create.single.sql: create.txt
	cat $(srcdir)/create.txt > $@

create.multi.sql: create.txt
	cat  $(srcdir)/create.txt > $@
	for i in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	echo >> $@; \
	for j in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	echo "CREATE TABLE dict$$i$$j (url_id int(11) DEFAULT '0' NOT NULL, secno tinyint unsigned DEFAULT '0' NOT NULL, word varchar(255) DEFAULT '' NOT NULL, intag blob NOT NULL, KEY url_id (url_id), KEY word_url (word));" >> $@ \
	; done \
	; done

create.blob.sql: create.txt create.blob.txt
	cat $(srcdir)/create.txt $(srcdir)/create.blob.txt > $@

drop.single.sql: drop.txt
	cat  $(srcdir)/drop.txt > $@

drop.multi.sql: drop.txt
	cat  $(srcdir)/drop.txt > $@
	for i in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	echo >> $@; \
	for j in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	echo "DROP TABLE IF EXISTS dict$$i$$j;" >> $@ \
	; done \
	; done

drop.blob.sql: drop.txt drop.blob.txt
	cat $(srcdir)/drop.txt $(srcdir)/drop.blob.txt > $@

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
	$(mkinstalldirs) $(DESTDIR)$@CREATEDIR@/mysql
	list='${sql_FILES}'; \
	for p in $$list; do \
		${INSTALL_DATA} ./$$p $(DESTDIR)$@CREATEDIR@/mysql/ ; \
	done

uninstall-local:
	list='${sql_FILES}'; \
	for p in $$list; do \
		rm -f $(DESTDIR)$@CREATEDIR@/mysql/$$p; \
	done
	-rmdir $(DESTDIR)$@CREATEDIR@/mysql
