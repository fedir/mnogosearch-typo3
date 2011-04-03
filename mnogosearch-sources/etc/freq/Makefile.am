EXTRA_DIST = ${freq_DATA}

freqdir = $@FREQDIR@

freq_DATA = \
mandarin.freq \
thai.freq \
TraditionalChinese.freq

uninstall-local:
	-rmdir $(DESTDIR)$@FREQDIR@

uninstall-am: uninstall-freqDATA uninstall-local
