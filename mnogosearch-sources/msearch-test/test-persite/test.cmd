skip !0 exec $(INDEXER) -Echeck     $(UDM_TEST_DIR)/indexer.conf 1>&2

fail 20 exec $(INDEXER) -Edrop      $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate    $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl -v6 $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Esqlmon    $(UDM_TEST_DIR)/indexer.conf < $(UDM_TEST_DIR)/query.tst > $(UDM_TEST_DIR)/query.rej
fail !0 mdiff $(UDM_TEST_DIR)/query.rej $(UDM_TEST_DIR)/query.res
fail !0 exec rm -f $(UDM_TEST_DIR)/query.rej
fail !0 exec $(SEARCH) "this&GroupBySite=no&ps=10&np=0" >  $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "this&GroupBySite=yes&ps=1&np=0" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "this&GroupBySite=yes&ps=1&np=1" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "this&GroupBySite=rank&ps=10&np=0" >>  $(UDM_TEST_DIR)/search.rej
fail !0 mdiff $(UDM_TEST_DIR)/search.rej $(UDM_TEST_DIR)/search.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search.rej

pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer.conf 1>&2
