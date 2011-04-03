#skip 0 substrenv UDM_TEST_DBADDR0 oracle://
#skip 0 substrenv UDM_TEST_DBADDR0 db2://
#skip 0 substrenv UDM_TEST_DBADDR0 mssql://
#skip 0 substrenv UDM_TEST_DBADDR0 sybase://
#skip 0 substrenv UDM_TEST_DBADDR0 ibase://
#skip 0 substrenv UDM_TEST_DBADDR0 pgsql://
#skip 0 substrenv UDM_TEST_DBADDR0 mimer://
skip 0 substrenv UDM_TEST_DBADDR0 sqlite://
skip 0 substrenv UDM_TEST_DBADDR0 sqlite3://

skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer.conf 1>&2

fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -v6 -Ecrawl  $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Eblob   $(UDM_TEST_DIR)/indexer.conf 1>&2

fail !0 exec $(SEARCH) "citta"    > $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "citt%E0" >> $(UDM_TEST_DIR)/search.rej

fail !0 exec $(SEARCH) "francois"   >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "fran%E7ois" >> $(UDM_TEST_DIR)/search.rej

fail !0 exec $(SEARCH) "definir"   >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "d%E9finir" >> $(UDM_TEST_DIR)/search.rej

fail !0 exec $(SEARCH) "%C0"   >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "%C1"   >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "%C2"   >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "%C3"   >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "%C4"   >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "%C5"   >> $(UDM_TEST_DIR)/search.rej
#fail !0 exec $(SEARCH) "%C6"   >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "%C7"   >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "%C8"   >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "%C9"   >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "%CA"   >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "%CB"   >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "%CC"   >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "%CD"   >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "%CE"   >> $(UDM_TEST_DIR)/search.rej
fail !0 exec $(SEARCH) "%CF"   >> $(UDM_TEST_DIR)/search.rej

# Bug#2022 "Dehyphenate yes" crashes with DBMode=blob with a single word
fail !0 exec $(SEARCH) "citta&dh=yes" >> $(UDM_TEST_DIR)/search.rej


fail !0 mdiff $(UDM_TEST_DIR)/search.rej $(UDM_TEST_DIR)/search.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search.rej

pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer.conf 1>&2
