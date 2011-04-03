skip 0 substrenv UDM_TEST_DBADDR0 oracle://
skip 0 substrenv UDM_TEST_DBADDR0 db2://
skip 0 substrenv UDM_TEST_DBADDR0 mssql://
skip 0 substrenv UDM_TEST_DBADDR0 sybase://
skip 0 substrenv UDM_TEST_DBADDR0 ibase://
skip 0 substrenv UDM_TEST_DBADDR0 pgsql://
skip 0 substrenv UDM_TEST_DBADDR0 mimer://
skip 0 substrenv UDM_TEST_DBADDR0 sqlite://
skip 0 substrenv UDM_TEST_DBADDR0 sqlite3://
skip 0 substrenv UDM_TEST_DBADDR0 virtuoso://
skip 0 substrenv UDM_TEST_DBADDR0 monetdb://


skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer.conf 1>&2

fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -v6 -Ecrawl  $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Eblob   $(UDM_TEST_DIR)/indexer.conf 1>&2

fail !0 exec $(SEARCH) "citta&sss=yes&macf=255&ul=test1&ul=test2"    > $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "citt%E0&sss=yes&macf=255&ul=test1&ul=test2" >> $(UDM_TEST_DIR)/search.rej

fail !0 exec $(SEARCH) "citta&sss=yes&macf=255"   >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "citt%E0&sss=yes&macf=255" >> $(UDM_TEST_DIR)/search.rej

fail !0 exec $(SEARCH) "citta&sss=no&macf=10000"   >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "citt%E0&sss=no&macf=10000" >> $(UDM_TEST_DIR)/search.rej

fail !0 mdiff $(UDM_TEST_DIR)/search.rej $(UDM_TEST_DIR)/search.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search.rej

pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer.conf 1>&2
