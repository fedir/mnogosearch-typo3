skip 0 substrenv UDM_TEST_DBADDR0 mimer://
skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer.conf 1>&2

fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl  $(UDM_TEST_DIR)/indexer.conf 1>&2

fail !0 exec $(SEARCH) "this&ul=site1&ul=site3" > $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "this&ue=site1&ue=site3" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "this&sl.meta.keywords=test1" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "this&type=text/plain" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "this&type=text/html" >> $(UDM_TEST_DIR)/search.rej

fail !0 mdiff $(UDM_TEST_DIR)/search.rej $(UDM_TEST_DIR)/search.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search.rej
pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer.conf 1>&2
