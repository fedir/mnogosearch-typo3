skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer.conf 1>&2

fail 20 exec $(INDEXER)  -Edrop    $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER)  -Ecreate  $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER)  -Ecrawl   $(UDM_TEST_DIR)/indexer.conf 1>&2
#fail !0 exec $(INDEXER) -Esqlmon  $(UDM_TEST_DIR)/indexer.conf < $(UDM_TEST_DIR)/test.sql > $(UDM_TEST_DIR)/search.rej

fail !0 exec $(SEARCH) "a+b&s=RU&wdw=255"  >  $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "a+b&s=RU&wdw=0"   >>  $(UDM_TEST_DIR)/search.rej

fail !0 mdiff $(UDM_TEST_DIR)/search.rej $(UDM_TEST_DIR)/search.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search.rej

pass 0 exec  $(INDEXER) -Edrop --CaseFolding=turkish $(UDM_TEST_DIR)/indexer.conf 1>&2
