EXTRA_DIST = ${stopwords_DATA}

stopwordsdir = $@STOPWORDSDIR@

stopwords_DATA = \
ct.sl \
cz.sl \
da.sl \
de.sl \
en.big.sl \
en.huge.sl \
en.sl \
es.sl \
fr.sl \
hu.sl \
it.sl \
ja.sl \
lt.sl \
nl.sl \
no.sl \
pl.sl \
pt.sl \
ru.sl \
se.sl \
sk.sl \
tr.sl \
ua.sl \
zh.sl

uninstall-local:
	-rmdir $(DESTDIR)$@STOPWORDSDIR@

uninstall-am:  uninstall-stopwordsDATA uninstall-local
