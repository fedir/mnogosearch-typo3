skip !0 exec $(INDEXER) --checkconf  $(UDM_TEST_DIR)/indexer.conf 1>&2

fail 20 exec $(INDEXER) --drop   -d $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) --create -d $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) --crawl  -d $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) --index  -d $(UDM_TEST_DIR)/indexer.conf 1>&2

fail !0 exec $(SEARCH) "((jana herdt)|(michael mead))" > $(UDM_TEST_DIR)/search.rej

# Stopword is found in all documents found by non-stopwords
fail !0 exec $(SEARCH) "((jana herdt)|(michael mead)|(x))" >> $(UDM_TEST_DIR)/search.rej

# When the numner of search terms gets more than 64,
# the result is not predictable. In this particulare case
# it finds all documents.
fail !0 exec $(SEARCH) "((jana herdt)|(michael mead)|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx|xxx)"  >> $(UDM_TEST_DIR)/search.rej

fail !0 mdiff $(UDM_TEST_DIR)/search.rej $(UDM_TEST_DIR)/search.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search.rej

pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer.conf 1>&2
