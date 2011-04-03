skip !0 exec $(INDEXER) -Echeck      $(UDM_TEST_DIR)/indexer.conf 1>&2
fail 20 exec $(INDEXER) -Edrop       $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate     $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl  -v6 $(UDM_TEST_DIR)/indexer.conf 1>&2

fail !0 exec $(SEARCH) "aaaa+bbbb" > $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "importance100:aaaa+importance256:bbbb" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "importance256:aaaa+importance100:bbbb" >> $(UDM_TEST_DIR)/search.rej
fail !0 mdiff $(UDM_TEST_DIR)/search.rej $(UDM_TEST_DIR)/search.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search.rej

pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer.conf 1>&2
