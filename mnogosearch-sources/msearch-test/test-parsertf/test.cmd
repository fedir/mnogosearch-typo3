skip !0 exec $(INDEXER) -Echeck      $(UDM_TEST_DIR)/indexer.conf 1>&2
fail 20 exec $(INDEXER) -Edrop       $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate     $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl  -v6 $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Esqlmon $(UDM_TEST_DIR)/indexer.conf < $(UDM_TEST_DIR)/query.tst > $(UDM_TEST_DIR)/query.rej

fail !0 exec $(SEARCH) "some" >> $(UDM_TEST_DIR)/query.rej
fail !0 exec $(SEARCH) "%26%231073%3B"  >> $(UDM_TEST_DIR)/query.rej
fail !0 exec $(SEARCH) "%26%2313514%3B" >> $(UDM_TEST_DIR)/query.rej
fail !0 exec $(SEARCH) "domain"         >> $(UDM_TEST_DIR)/query.rej

fail !0 exec $(SEARCH) "some&cc=1&URL=http:%2F%2Fsite%2Ftest.rtf" >> $(UDM_TEST_DIR)/query.rej
fail !0 exec $(SEARCH) "some&cc=1&URL=http:%2F%2Fsite%2Ftest2.rtf" >> $(UDM_TEST_DIR)/query.rej
fail !0 exec $(SEARCH) "domain&cc=1&URL=http:%2F%2Fsite%2Ftest3.rtf" >> $(UDM_TEST_DIR)/query.rej


fail !0 mdiff $(UDM_TEST_DIR)/query.rej $(UDM_TEST_DIR)/query.res
fail !0 exec rm -f $(UDM_TEST_DIR)/query.rej
pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer.conf 1>&2
