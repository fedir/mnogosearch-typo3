#skip !0 testenv UDM_TEST_ROOT
#skip !0 testenv UDM_TEST_DIR
skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer0.conf 1>&2

# Prepare table structure
fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer0.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer0.conf 1>&2

# Index with ReversAlias
fail !0 exec $(INDEXER) -Ecrawl  $(UDM_TEST_DIR)/indexer0.conf 1>&2

# Check url contents
fail !0 exec $(INDEXER) -Esqlmon $(UDM_TEST_DIR)/indexer0.conf < $(UDM_TEST_DIR)/url.tst > $(UDM_TEST_DIR)/url0.rej
fail !0 mdiff $(UDM_TEST_DIR)/url0.rej $(UDM_TEST_DIR)/url0.res
fail !0 exec rm -f $(UDM_TEST_DIR)/url0.rej

# Check dict contents
fail !0 exec $(INDEXER) -Esqlmon $(UDM_TEST_DIR)/indexer0.conf < $(UDM_TEST_DIR)/dict.tst > $(UDM_TEST_DIR)/dict.rej
fail !0 mdiff $(UDM_TEST_DIR)/dict.rej $(UDM_TEST_DIR)/dict.res
fail !0 exec rm -f $(UDM_TEST_DIR)/dict.rej

# Clear database
fail !0 exec $(INDEXER) -Cw $(UDM_TEST_DIR)/indexer0.conf 1>&2

# Index without ReverseAlias
fail !0 exec $(INDEXER) -Ecrawl  $(UDM_TEST_DIR)/indexer1.conf 1>&2

# Check dict contents: must be the same
fail !0 exec $(INDEXER) -Esqlmon $(UDM_TEST_DIR)/indexer1.conf < $(UDM_TEST_DIR)/dict.tst > $(UDM_TEST_DIR)/dict.rej
fail !0 mdiff $(UDM_TEST_DIR)/dict.rej $(UDM_TEST_DIR)/dict.res
fail !0 exec rm -f $(UDM_TEST_DIR)/dict.rej

# Check url contens
fail !0 exec $(INDEXER) -Esqlmon $(UDM_TEST_DIR)/indexer1.conf < $(UDM_TEST_DIR)/url.tst > $(UDM_TEST_DIR)/url1.rej
fail !0 mdiff $(UDM_TEST_DIR)/url1.rej $(UDM_TEST_DIR)/url1.res
fail !0 exec rm -f $(UDM_TEST_DIR)/url1.rej

# Drop tables
pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer0.conf 1>&2
