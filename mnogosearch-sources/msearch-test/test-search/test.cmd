skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer.conf 1>&2

fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl  $(UDM_TEST_DIR)/indexer.conf 1>&2

fail !0 exec $(SEARCH) "t1w1 %26 t1w2" > $(UDM_TEST_DIR)/search1.rej
fail !0 mdiff $(UDM_TEST_DIR)/search1.rej $(UDM_TEST_DIR)/search1.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search1.rej

fail !0 exec $(SEARCH) "t1w2 %26 t1w1" > $(UDM_TEST_DIR)/search2.rej
fail !0 mdiff $(UDM_TEST_DIR)/search2.rej $(UDM_TEST_DIR)/search2.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search2.rej

fail !0 exec $(SEARCH) "t1w1 %26 ~t1w3" > $(UDM_TEST_DIR)/search3.rej
fail !0 mdiff $(UDM_TEST_DIR)/search3.rej $(UDM_TEST_DIR)/search3.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search3.rej

fail !0 exec $(SEARCH) "~t1w3 %26 t1w2" > $(UDM_TEST_DIR)/search4.rej
fail !0 mdiff $(UDM_TEST_DIR)/search4.rej $(UDM_TEST_DIR)/search4.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search4.rej

fail !0 exec $(SEARCH) "t1w1 | t1w3" > $(UDM_TEST_DIR)/search5.rej
fail !0 mdiff $(UDM_TEST_DIR)/search5.rej $(UDM_TEST_DIR)/search5.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search5.rej

fail !0 exec $(SEARCH) "\"t1w1 t1w2\"" > $(UDM_TEST_DIR)/search6.rej
fail !0 mdiff $(UDM_TEST_DIR)/search6.rej $(UDM_TEST_DIR)/search6.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search6.rej

fail !0 exec $(SEARCH) "\"t2w1 t2w2 t2w1\"" > $(UDM_TEST_DIR)/search7.rej
fail !0 mdiff $(UDM_TEST_DIR)/search7.rej $(UDM_TEST_DIR)/search7.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search7.rej

fail !0 exec $(SEARCH) "\"t3w1 t3w1 t3w2\"" > $(UDM_TEST_DIR)/search8.rej
fail !0 mdiff $(UDM_TEST_DIR)/search8.rej $(UDM_TEST_DIR)/search8.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search8.rej

fail !0 exec $(SEARCH) "\"t4w1 t4w2 t4w2\"" > $(UDM_TEST_DIR)/search9.rej
fail !0 mdiff $(UDM_TEST_DIR)/search9.rej $(UDM_TEST_DIR)/search9.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search9.rej

fail !0 exec $(SEARCH) "'t1w1 t2w2'" > $(UDM_TEST_DIR)/search10.rej
fail !0 mdiff $(UDM_TEST_DIR)/search10.rej $(UDM_TEST_DIR)/search10.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search10.rej

fail !0 exec $(SEARCH) "t1w1 t2w3&m=any" > $(UDM_TEST_DIR)/search11.rej
fail !0 mdiff $(UDM_TEST_DIR)/search11.rej $(UDM_TEST_DIR)/search11.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search11.rej

fail !0 exec $(SEARCH) "t1w1 t2w3&m=all" > $(UDM_TEST_DIR)/search12.rej
fail !0 mdiff $(UDM_TEST_DIR)/search12.rej $(UDM_TEST_DIR)/search12.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search12.rej

fail !0 exec $(SEARCH) "\"t3w1 %26 t3w1 | t3w2\"" > $(UDM_TEST_DIR)/search13.rej
fail !0 mdiff $(UDM_TEST_DIR)/search13.rej $(UDM_TEST_DIR)/search13.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search13.rej

fail !0 exec $(SEARCH) "\"t5w1 t5w2 t5w3\"" > $(UDM_TEST_DIR)/search14.rej
fail !0 mdiff $(UDM_TEST_DIR)/search14.rej $(UDM_TEST_DIR)/search14.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search14.rej

fail !0 exec $(SEARCH) "\"t5w2 t5w3\"" > $(UDM_TEST_DIR)/search15.rej
fail !0 mdiff $(UDM_TEST_DIR)/search15.rej $(UDM_TEST_DIR)/search15.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search15.rej

# Test two phrases, make sure both are highlighted
fail !0 exec $(SEARCH) "t1w1_t1w2 t2w1_t2w2" > $(UDM_TEST_DIR)/search16.rej
fail !0 mdiff $(UDM_TEST_DIR)/search16.rej $(UDM_TEST_DIR)/search16.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search16.rej

# Test "Unsupported DBAddr" error message
fail !0 exec $(SEARCH) -d $(UDM_TEST_DIR)/bad-dbaddr.htm > $(UDM_TEST_DIR)/bad-dbaddr.rej
fail !0 mdiff $(UDM_TEST_DIR)/bad-dbaddr.rej $(UDM_TEST_DIR)/bad-dbaddr.res
fail !0 exec rm -f $(UDM_TEST_DIR)/bad-dbaddr.rej


pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer.conf 1>&2
