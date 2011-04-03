skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer.conf 1>&2

fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl  -v 6 $(UDM_TEST_DIR)/indexer.conf 1>&2

fail !0 exec $(SEARCH) "1&wdf=2"        > $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "1&wdf=25"      >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "1&wdf=250"     >> $(UDM_TEST_DIR)/search.rej

fail !0 exec $(SEARCH) "1+2+3&wdf=2"   >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "1+2+3&wdf=25"  >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "1+2+3&wdf=250" >> $(UDM_TEST_DIR)/search.rej

fail !0 mdiff $(UDM_TEST_DIR)/search.rej $(UDM_TEST_DIR)/search.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search.rej
pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer.conf 1>&2
