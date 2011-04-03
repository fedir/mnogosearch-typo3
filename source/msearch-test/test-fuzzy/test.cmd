# "found" is a keyword in MonetDB
skip 0 substrenv UDM_TEST_DBADDR0 monetdb://

skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer.conf 1>&2

fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl  $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -R       $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Esqlmon $(UDM_TEST_DIR)/indexer.conf < $(UDM_TEST_DIR)/query.tst > $(UDM_TEST_DIR)/query.rej
fail !0 mdiff $(UDM_TEST_DIR)/query.rej $(UDM_TEST_DIR)/query.res
fail !0 exec rm -f $(UDM_TEST_DIR)/query.rej

# Check ispell in hashed format
fail !0 exec $(INDEXER) -Ehashspell
fail !0 exec $(SEARCH) "color&sy=0&sp=1&s=u&msp=.msp&we_param=yes"  >  $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "colour&sy=0&sp=1&s=u&msp=.msp&we_param=yes" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec rm  $(UDM_TEST_DIR)/american.xlg.msp
fail !0 exec rm  $(UDM_TEST_DIR)/british.xlg.msp

# Check ispell in text format
fail !0 exec $(SEARCH) "color&sy=0&sp=0&s=u"  >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "colour&sy=0&sp=0&s=u" >> $(UDM_TEST_DIR)/search.rej

fail !0 exec $(SEARCH) "color&sy=1&sp=0&s=u"  >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "colour&sy=1&sp=0&s=u" >> $(UDM_TEST_DIR)/search.rej

fail !0 exec $(SEARCH) "color&sy=0&sp=1&s=u"  >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "colour&sy=0&sp=1&s=u" >> $(UDM_TEST_DIR)/search.rej

fail !0 exec $(SEARCH) "color&sy=1&sp=1&s=u"  >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "colour&sy=1&sp=1&s=u" >> $(UDM_TEST_DIR)/search.rej

fail !0 exec $(SEARCH) "color&sy=1&sp=1&s=u&wfw=255"  >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "colour&sy=1&sp=1&s=u&wfw=255" >> $(UDM_TEST_DIR)/search.rej

fail !0 exec $(SEARCH) "rgb"  >>  $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "rg"   >>  $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "gb"   >>  $(UDM_TEST_DIR)/search.rej

#
# Test WordFormFactor with a single unique word
#
fail !0 exec $(SEARCH) "test&sy=0&sp=1&s=u&wfw=255"    >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "testing&sy=0&sp=1&s=u&wfw=255" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "test&sy=0&sp=1&s=u&wfw=100"    >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "testing&sy=0&sp=1&s=u&wfw=100" >> $(UDM_TEST_DIR)/search.rej

#
# With multiple unique words
#
fail !0 exec $(SEARCH) "is+test&sy=0&sp=1&s=u&wfw=255"    >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "is+testing&sy=0&sp=1&s=u&wfw=255" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "is+test&sy=0&sp=1&s=u&wfw=100"    >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "is+testing&sy=0&sp=1&s=u&wfw=100" >> $(UDM_TEST_DIR)/search.rej


#
# Prases with multi-words
#
fail !0 exec $(SEARCH) '"red gb"'  >>  $(UDM_TEST_DIR)/search.rej

# FIXME: this doesn't work: "multiword word"
#fail !0 exec $(SEARCH) '"rg blue"'  >>  $(UDM_TEST_DIR)/search.rej


#
# this one should not return results
#
fail !0 exec $(SEARCH) "rb"   >>  $(UDM_TEST_DIR)/search.rej

#
# these ones should not return results because of "oneway" synonym.
#
fail !0 exec $(SEARCH) "azure"   >>  $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "purple"  >>  $(UDM_TEST_DIR)/search.rej

#
# Synonyms of "return" type
#
fail !0 exec $(SEARCH) "scarlet"  >>  $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "crimson"  >>  $(UDM_TEST_DIR)/search.rej

#
# Mode "final"
#
fail !0 exec $(SEARCH) "June">>  $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "06"  >>  $(UDM_TEST_DIR)/search.rej

# test translit
fail !0 exec $(SEARCH) "%D2%C5%C4&tl=yes" >> $(UDM_TEST_DIR)/search.rej

# test dehyphenation
fail !0 exec $(SEARCH) "peace-making&dh=no" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "peace-making&dh=yes" >> $(UDM_TEST_DIR)/search.rej

# test complex synonyms
fail !0 exec $(SEARCH) "peace+making&csyn=no"  >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "peace+making&csyn=yes" >> $(UDM_TEST_DIR)/search.rej


# test that dash character is not considered as a word by its own
fail !0 exec $(SEARCH) "red+-+green&dh=no" >> $(UDM_TEST_DIR)/search.rej

#
# Bug#746 Stopwords in a long boolean query
#
fail !0 exec $(SEARCH) '"is writing a log file can"' >>  $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "is%26writing%26a%26log%26file%26can" >>  $(UDM_TEST_DIR)/search.rej

#
# Check that AND/NOT operators are automatically inserted before NOT
#
fail !0 exec $(SEARCH) "log %7Efile" >>  $(UDM_TEST_DIR)/search.rej

#
# Check that NOT operator is ignored in a query with a single word
#
fail !0 exec $(SEARCH) "%7Efile" >>  $(UDM_TEST_DIR)/search.rej

#
# Test many words and phrases with ispell
#
fail !0 exec $(SEARCH) 'test+my+phrase&we_param=yes'    >>  $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) 'testing+my+phrase&we_param=yes' >>  $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) 'my+test+phrase&we_param=yes'    >>  $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) 'my+testing+phrase&we_param=yes' >>  $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) 'my+phrase+test&we_param=yes'    >>  $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) 'my+phrase+testing&we_param=yes' >>  $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) 'test_my_phrase&we_param=yes'    >>  $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) 'testing_my_phrase&we_param=yes' >>  $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) 'my_test_phrase&we_param=yes'    >>  $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) 'my_testing_phrase&we_param=yes' >>  $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) 'my_phrase_test&we_param=yes'    >>  $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) 'my_phrase_testing&we_param=yes' >>  $(UDM_TEST_DIR)/search.rej

#
# Test SubstringMatchMinWordLength
#
fail !0 exec $(SEARCH) "colour&sy=0&sp=0&wm=sub&smmwl=3&s=u&we_param=yes" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "colour&sy=0&sp=0&wm=sub&smmwl=7&s=u&we_param=yes" >> $(UDM_TEST_DIR)/search.rej

#
# Bug#3790 Internal 500 error on spelling words with DGJRSZ flags
#
fail !0 exec $(SEARCH) "travel&sy=0&sp=1" >> $(UDM_TEST_DIR)/search.rej


fail !0 mdiff $(UDM_TEST_DIR)/search.rej $(UDM_TEST_DIR)/search.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search.rej

pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer.conf 1>&2
