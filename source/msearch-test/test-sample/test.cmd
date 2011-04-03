skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer.conf 1>&2

fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl -u "http://site/%"  $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl -u "http://site2/%" $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -R       $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Esqlmon $(UDM_TEST_DIR)/indexer.conf < $(UDM_TEST_DIR)/query.tst > $(UDM_TEST_DIR)/query.rej
fail !0 mdiff $(UDM_TEST_DIR)/query.rej $(UDM_TEST_DIR)/query.res
fail !0 exec rm -f $(UDM_TEST_DIR)/query.rej

fail !0 exec $(SEARCH) "this&s=RP" > $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "this&GroupBySite=yes" >> $(UDM_TEST_DIR)/search.rej

# Check cross site scripting in a variable value:
fail !0 exec $(SEARCH) "this&ps=1&qqq=<tag>&s=RP" >> $(UDM_TEST_DIR)/search.rej

# Check cross site scripting in a variable name:
fail !0 exec $(SEARCH) 'this&ps=1&>"><script>alert(123)</script><"=Search' >> $(UDM_TEST_DIR)/search.rej

# Check SQL injections protection
fail !0 exec $(SEARCH) "1&ul=%')+and+1=1+--+" >> $(UDM_TEST_DIR)/search.rej

fail !0 exec $(SEARCH) "'01-01-1970'" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "this+is+the+first+page+test&StrictModeThresHold=2" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "the-third-one" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "the-third-one&cw=yes" >> $(UDM_TEST_DIR)/search.rej
# Test ResultsLimit
fail !0 exec $(SEARCH) "the&ps=1000" >> $(UDM_TEST_DIR)/search.rej
# Make sure query parameter does not overwrite config value.
fail !0 exec $(SEARCH) "the&ps=1100&ResultsLimit=2000" >> $(UDM_TEST_DIR)/search.rej
# TODO: add aslo "the&ps=300&ResultsLimit=300". Should return "not found".

fail !0 mdiff $(UDM_TEST_DIR)/search.rej $(UDM_TEST_DIR)/search.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search.rej

fail !0 exec $(INDEXER) -Esqlmon $(UDM_TEST_DIR)/indexer.conf < $(UDM_TEST_DIR)/qtrack.tst > $(UDM_TEST_DIR)/qtrack.rej
fail !0 mdiff $(UDM_TEST_DIR)/qtrack.rej $(UDM_TEST_DIR)/qtrack.res
fail !0 exec rm -f $(UDM_TEST_DIR)/qtrack.rej

pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer.conf 1>&2
