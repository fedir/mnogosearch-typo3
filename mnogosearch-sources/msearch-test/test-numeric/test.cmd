skip 0 substrenv UDM_TEST_DBADDR0 oracle://
skip 0 substrenv UDM_TEST_DBADDR0 odbc-oracle8://
skip 0 substrenv UDM_TEST_DBADDR0 mssql://
skip 0 substrenv UDM_TEST_DBADDR0 ibase://
skip 0 substrenv UDM_TEST_DBADDR0 mimer://

skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer.conf 1>&2

fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl  -v 6 $(UDM_TEST_DIR)/indexer.conf 1>&2

fail !0 exec $(SEARCH) ">10"        > $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) ">100"      >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) ">200"      >> $(UDM_TEST_DIR)/search.rej


fail !0 mdiff $(UDM_TEST_DIR)/search.rej $(UDM_TEST_DIR)/search.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search.rej
pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer.conf 1>&2
