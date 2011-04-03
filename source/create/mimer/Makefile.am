EXTRA_DIST= create.txt drop.txt create.blob.txt drop.blob.txt

all: create drop

create: create.single.sql create.multi.sql create.blob.sql

drop: drop.single.sql drop.multi.sql drop.blob.sql

create.single.sql: create.txt
	cat $(srcdir)/create.txt > $@

create.multi.sql: create.txt
	cat $(srcdir)/create.txt > $@
	for i in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	echo >> $@; \
	for j in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	echo "CREATE TABLE dict$$i$$j (url_id INT NOT NULL, secno INT NOT NULL, word VARCHAR(255) NOT NULL, intag VARCHAR(15000) NOT NULL);" >> $@ \
	; done \
	; done
	for i in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	for j in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	echo >> $@ \
	; echo "CREATE INDEX url_id$$i$$j ON dict$$i$$j (url_id);" >> $@ \
	; echo "CREATE INDEX word$$i$$j ON dict$$i$$j (word);" >> $@ \
	; done \
	; done

create.blob.sql: create.txt create.blob.txt
	cat $(srcdir)/create.txt $(srcdir)/create.blob.txt > $@

drop.single.sql: drop.txt
	cat $(srcdir)/drop.txt > $@

drop.multi.sql: drop.txt
	cat $(srcdir)/drop.txt > $@
	for i in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	echo >> $@; \
	for j in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	echo "DROP TABLE dict$$i$$j;" >> $@ \
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
	$(mkinstalldirs) $(DESTDIR)$@CREATEDIR@/mimer
	list='${sql_FILES}'; \
	for p in $$list; do \
		${INSTALL_DATA} ./$$p $(DESTDIR)$@CREATEDIR@/mimer/ ; \
	done

uninstall-local:
	list='${sql_FILES}'; \
	for p in $$list; do \
		rm -f $(DESTDIR)$@CREATEDIR@/mimer/$$p; \
	done
	-rmdir $(DESTDIR)$@CREATEDIR@/mimer

