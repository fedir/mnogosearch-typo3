EXTRA_DIST = ${synonym_DATA}

synonymdir = $@SYNONYMDIR@

synonym_DATA = \
english.syn \
italian.syn \
french.syn \
russian.syn \
russian.big.syn

uninstall-local:
	-rmdir $(DESTDIR)$@SYNONYMDIR@

uninstall-am: uninstall-synonymDATA uninstall-local
