skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer.conf 1>&2

fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl  $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Esqlmon $(UDM_TEST_DIR)/indexer.conf < $(UDM_TEST_DIR)/query.tst > $(UDM_TEST_DIR)/query.rej
fail !0 exec $(INDEXER) -am -u http://site/test1.html -Ecrawl  $(UDM_TEST_DIR)/indexer1.conf 1>&2
fail !0 exec $(INDEXER) -Esqlmon $(UDM_TEST_DIR)/indexer.conf < $(UDM_TEST_DIR)/query.tst >> $(UDM_TEST_DIR)/query.rej
fail !0 mdiff $(UDM_TEST_DIR)/query.rej $(UDM_TEST_DIR)/query.res
fail !0 exec rm -f $(UDM_TEST_DIR)/query.rej
pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer.conf 1>&2
