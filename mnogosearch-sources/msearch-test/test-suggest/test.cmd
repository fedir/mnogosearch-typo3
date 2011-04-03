skip 0 substrenv UDM_TEST_DBADDR0 sqlite://

skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer.conf 1>&2

fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -v6 -Ecrawl  $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Eblob   $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ewordstat $(UDM_TEST_DIR)/indexer.conf 1>&2

fail !0 exec $(SEARCH) "interesting"  > $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "intresting"   >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "rupert"       >> $(UDM_TEST_DIR)/search.rej

fail !0 mdiff $(UDM_TEST_DIR)/search.rej $(UDM_TEST_DIR)/search.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search.rej

pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer.conf 1>&2
