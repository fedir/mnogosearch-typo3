skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer.conf 1>&2

fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl  -v6 $(UDM_TEST_DIR)/indexer.conf 1>&2

# Check that the document with the word only in title is worse
fail !0 exec $(SEARCH) "nwf1&wf=FF1&nwf=010" >  $(UDM_TEST_DIR)/test.rej
fail !0 exec $(SEARCH) "nwf1&wf=FF1&nwf=0F0" >> $(UDM_TEST_DIR)/test.rej

# Check that the document with the word only in meta.keywords is wors
fail !0 exec $(SEARCH) "nwf1&wf=FF1&nwf=100" >> $(UDM_TEST_DIR)/test.rej
fail !0 exec $(SEARCH) "nwf1&wf=FF1&nwf=F00" >> $(UDM_TEST_DIR)/test.rej

fail !0 mdiff $(UDM_TEST_DIR)/test.rej $(UDM_TEST_DIR)/test.res
fail !0 exec rm -f $(UDM_TEST_DIR)/test.rej
pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer.conf 1>&2
