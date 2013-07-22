#skip !0 testenv UDM_TEST_ROOT
#skip !0 testenv UDM_TEST_DIR
#skip !0 testenv UDM_TEST_DBADDR0
#skip !0 testenv UDM_SHARE_DIR
#skip !0 testenv INDEXER
skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer.conf 1>&2

skip !0 exec $(INDEXER) --check  $(UDM_TEST_DIR)/indexer.conf 1>&2
fail 20 exec $(INDEXER) --drop   $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) --create $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) --crawl -v6  $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) --index  $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -S  $(UDM_TEST_DIR)/indexer.conf 1>&2

fail !0 exec $(SEARCH) "[AAA TO AAA]" > $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "[AAA TO BBB]" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "[AAA TO BBB}" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "{AAA TO BBB]" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "{AAA TO BBZ}" >> $(UDM_TEST_DIR)/search.rej

fail !0 exec $(SEARCH) "\"[AAA TO AAA][BBB TO BBB]\"" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "\"[AAA TO AAA][CCC TO CCC]\"" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "\"AAA [BBB TO BBB]\"" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "\"AAA [CCC TO CCC]\"" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "\"[BBB TO BBB] CCC\"" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "\"[CCC TO CCC] EEE\"" >> $(UDM_TEST_DIR)/search.rej

fail !0 exec $(SEARCH) "title:[1 TO 2]" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "body:[1 TO 2]" >> $(UDM_TEST_DIR)/search.rej

fail !0 exec $(SEARCH) "title:\"test [1 TO 2]\"" >> $(UDM_TEST_DIR)/search.rej

fail !0 exec $(SEARCH) "price:\"[99 TO 200]\"" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "price:\"[99 TO 201]\"" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "price:\"[99 TO 201.001]\"" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "price:\"[99 TO 202]\"" >> $(UDM_TEST_DIR)/search.rej


fail !0 mdiff $(UDM_TEST_DIR)/search.rej $(UDM_TEST_DIR)/search.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search.rej

pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer.conf 1>&2
