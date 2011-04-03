skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer.conf 1>&2

fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl  -v 6 $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Esqlmon $(UDM_TEST_DIR)/indexer.conf < $(UDM_TEST_DIR)/query.tst > $(UDM_TEST_DIR)/query.rej
fail !0 mdiff $(UDM_TEST_DIR)/query.rej $(UDM_TEST_DIR)/query.res
fail !0 exec rm -f $(UDM_TEST_DIR)/query.rej

fail !0 exec $(SEARCH) "%E9%82%A3%E6%97%B6"       > $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "%E9%82%A3-%E6%97%B6"      >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "%22%E9%82%A3%E6%97%B6%22" >> $(UDM_TEST_DIR)/search.rej

fail !0 exec $(SEARCH) "1-%E5%9B%A0-2" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "a-%E5%9B%A0-b" >> $(UDM_TEST_DIR)/search.rej

fail !0 exec $(SEARCH) "1%E5%9B%A02&m=all" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "a%E5%9B%A0b&m=all" >> $(UDM_TEST_DIR)/search.rej

fail !0 exec $(SEARCH) "1%E5%9B%A02&m=phrase" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "a%E5%9B%A0b&m=phrase" >> $(UDM_TEST_DIR)/search.rej

fail !0 mdiff $(UDM_TEST_DIR)/search.rej $(UDM_TEST_DIR)/search.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search.rej

pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer.conf 1>&2
