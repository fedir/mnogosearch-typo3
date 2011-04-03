#skip !0 testenv UDM_TEST_ROOT
#skip !0 testenv UDM_TEST_DIR
#skip !0 testenv UDM_TEST_DBADDR0
#skip !0 testenv UDM_SHARE_DIR
#skip !0 testenv INDEXER
skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer.conf 1>&2

#
# Index into DBMode=single
#
fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl -v6  $(UDM_TEST_DIR)/indexer.conf 1>&2
# Check "Inverted word index not found" message
fail !0 exec $(SEARCH)   this > $(UDM_TEST_DIR)/searchni.rej
fail !0 mdiff $(UDM_TEST_DIR)/searchni.rej $(UDM_TEST_DIR)/searchni.res
fail !0 exec rm -f $(UDM_TEST_DIR)/searchni.rej
# Convert DBMode=signle to DBMode=blob
fail !0 exec $(INDEXER) -Eblob  -v6  $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ewordstat $(UDM_TEST_DIR)/indexer.conf 1>&2

fail !0 exec $(INDEXER) -Esqlmon $(UDM_TEST_DIR)/indexer.conf < $(UDM_TEST_DIR)/query.tst > $(UDM_TEST_DIR)/query.rej
fail !0 mdiff $(UDM_TEST_DIR)/query.rej $(UDM_TEST_DIR)/query.res
fail !0 exec rm -f $(UDM_TEST_DIR)/query.rej

# Test for searches DBMode=blob converted from DBMode=single
fail !0 exec $(SEARCH) this > $(UDM_TEST_DIR)/search.rej1
fail !0 exec $(SEARCH) "this+is" >> $(UDM_TEST_DIR)/search.rej1
fail !0 exec $(SEARCH) "this&m=any" >> $(UDM_TEST_DIR)/search.rej1
fail !0 exec $(SEARCH) "this&ul=test1.html" >> $(UDM_TEST_DIR)/search.rej1
fail !0 exec $(SEARCH) "this&fl=test1"  >> $(UDM_TEST_DIR)/search.rej1
fail !0 exec $(SEARCH) "this&fl=-test1" >> $(UDM_TEST_DIR)/search.rej1
# Search documents modified before 2003-12-01
fail !0 exec $(SEARCH) "this&dstmp=1070222400&dt=er&dx=-1" >> $(UDM_TEST_DIR)/search.rej1
fail !0 exec $(SEARCH) "this&DateFactor=100" >> $(UDM_TEST_DIR)/search.rej1
fail !0 exec $(SEARCH) "this&us=score1&UserScoreFactor=255" >> $(UDM_TEST_DIR)/search.rej1
fail !0 exec $(SEARCH) "this&us=score2&UserScoreFactor=255" >> $(UDM_TEST_DIR)/search.rej1
fail !0 mdiff $(UDM_TEST_DIR)/search.rej1 $(UDM_TEST_DIR)/search.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search.rej1
fail 20 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer.conf 1>&2

# Crawl with DBMode=blob
skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer-blob.conf 1>&2
fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer-blob.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer-blob.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl -v6  $(UDM_TEST_DIR)/indexer-blob.conf 1>&2

# Test for searches for DBMode=rawblob (without running "indexer -Eblob")
fail !0 exec $(SEARCH) "this&raw=raw" > $(UDM_TEST_DIR)/search.rej2
fail !0 exec $(SEARCH) "this+is&raw=raw" >> $(UDM_TEST_DIR)/search.rej2
fail !0 exec $(SEARCH) "this&raw=raw&m=any" >> $(UDM_TEST_DIR)/search.rej2
fail !0 exec $(SEARCH) "this&raw=raw&ul=test1.html" >> $(UDM_TEST_DIR)/search.rej2
fail !0 exec $(SEARCH) "this&raw=raw&ul=test1" >> $(UDM_TEST_DIR)/search.rej2
fail !0 exec $(SEARCH) "this&raw=raw&ue=test1" >> $(UDM_TEST_DIR)/search.rej2
fail !0 exec $(SEARCH) "this&raw=raw&dstmp=1070222400&dt=er&dx=-1" >> $(UDM_TEST_DIR)/search.rej2
fail !0 exec $(SEARCH) "this&raw=raw&DateFactor=100" >> $(UDM_TEST_DIR)/search.rej2
fail !0 exec $(SEARCH) "this&raw=raw&us=score1&UserScoreFactor=255" >> $(UDM_TEST_DIR)/search.rej2
fail !0 exec $(SEARCH) "this&raw=raw&us=score2&UserScoreFactor=255" >> $(UDM_TEST_DIR)/search.rej2
fail !0 mdiff $(UDM_TEST_DIR)/search.rej2 $(UDM_TEST_DIR)/search.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search.rej2

# Test for searches for DBMode=blob&LiveUpdates=yes (before running "indexer -Eblob")
fail !0 exec $(SEARCH) "this&LiveUpdates=yes" > $(UDM_TEST_DIR)/search.rej3
fail !0 exec $(SEARCH) "this+is&LiveUpdates=yes" >> $(UDM_TEST_DIR)/search.rej3
fail !0 exec $(SEARCH) "this&LiveUpdates=yes&m=any" >> $(UDM_TEST_DIR)/search.rej3
fail !0 exec $(SEARCH) "this&LiveUpdates=yes&ul=test1.html" >> $(UDM_TEST_DIR)/search.rej3
fail !0 exec $(SEARCH) "this&LiveUpdates=yes&ul=test1" >> $(UDM_TEST_DIR)/search.rej3
fail !0 exec $(SEARCH) "this&LiveUpdates=yes&ue=test1" >> $(UDM_TEST_DIR)/search.rej3
fail !0 exec $(SEARCH) "this&LiveUpdates=yes&dstmp=1070222400&dt=er&dx=-1" >> $(UDM_TEST_DIR)/search.rej3
fail !0 exec $(SEARCH) "this&LiveUpdates=yes&DateFactor=100" >> $(UDM_TEST_DIR)/search.rej3
fail !0 exec $(SEARCH) "this&LiveUpdates=yes&us=score1&UserScoreFactor=255" >> $(UDM_TEST_DIR)/search.rej3
fail !0 exec $(SEARCH) "this&&LiveUpdates=yes&us=score2&UserScoreFactor=255" >> $(UDM_TEST_DIR)/search.rej3
fail !0 mdiff $(UDM_TEST_DIR)/search.rej3 $(UDM_TEST_DIR)/search.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search.rej3

# Test for searches for DBMode=blob converted from DBMode=blob
fail !0 exec $(INDEXER) -Eblob   $(UDM_TEST_DIR)/indexer-blob.conf 1>&2
fail !0 exec $(SEARCH) this > $(UDM_TEST_DIR)/search.rej4
fail !0 exec $(SEARCH) "this+is" >> $(UDM_TEST_DIR)/search.rej4
fail !0 exec $(SEARCH) "this&m=any" >> $(UDM_TEST_DIR)/search.rej4
fail !0 exec $(SEARCH) "this&ul=test1.html" >> $(UDM_TEST_DIR)/search.rej4
fail !0 exec $(SEARCH) "this&fl=test1" >> $(UDM_TEST_DIR)/search.rej4
fail !0 exec $(SEARCH) "this&fl=-test1" >> $(UDM_TEST_DIR)/search.rej4
fail !0 exec $(SEARCH) "this&dstmp=1070222400&dt=er&dx=-1" >> $(UDM_TEST_DIR)/search.rej4
fail !0 exec $(SEARCH) "this&DateFactor=100" >> $(UDM_TEST_DIR)/search.rej4
fail !0 exec $(SEARCH) "this&us=score1cached&UserScoreFactor=255" >> $(UDM_TEST_DIR)/search.rej4
fail !0 exec $(SEARCH) "this&us=score2cached&UserScoreFactor=255" >> $(UDM_TEST_DIR)/search.rej4
fail !0 mdiff $(UDM_TEST_DIR)/search.rej4 $(UDM_TEST_DIR)/search.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search.rej4

# Reindex everything
fail !0 exec $(INDEXER) -am   $(UDM_TEST_DIR)/indexer-blob.conf 1>&2

# Test for searches for DBMode=blob&LiveUpdates=yes,
# after running "indexer -Eblob" and then "indexer -am"
# Now we have two copies of each document:
# "bdict"  - converted copy
# "bdicti" - updated copy
fail !0 exec $(SEARCH) "this&LiveUpdates=yes" > $(UDM_TEST_DIR)/search.rej5
fail !0 exec $(SEARCH) "this+is&LiveUpdates=yes" >> $(UDM_TEST_DIR)/search.rej5
fail !0 exec $(SEARCH) "this&LiveUpdates=yes&m=any" >> $(UDM_TEST_DIR)/search.rej5
fail !0 exec $(SEARCH) "this&LiveUpdates=yes&ul=test1.html" >> $(UDM_TEST_DIR)/search.rej5
fail !0 exec $(SEARCH) "this&LiveUpdates=yes&ul=test1" >> $(UDM_TEST_DIR)/search.rej5
fail !0 exec $(SEARCH) "this&LiveUpdates=yes&ue=test1" >> $(UDM_TEST_DIR)/search.rej5
fail !0 exec $(SEARCH) "this&LiveUpdates=yes&dstmp=1070222400&dt=er&dx=-1" >> $(UDM_TEST_DIR)/search.rej5
fail !0 exec $(SEARCH) "this&LiveUpdates=yes&DateFactor=100" >> $(UDM_TEST_DIR)/search.rej5
fail !0 exec $(SEARCH) "this&LiveUpdates=yes&us=score1&UserScoreFactor=255" >> $(UDM_TEST_DIR)/search.rej5
fail !0 exec $(SEARCH) "this&&LiveUpdates=yes&us=score2&UserScoreFactor=255" >> $(UDM_TEST_DIR)/search.rej5
fail !0 mdiff $(UDM_TEST_DIR)/search.rej5 $(UDM_TEST_DIR)/search.res
fail !0 exec rm -f $(UDM_TEST_DIR)/search.rej5

# Bug#1741 "indexer.conf -Eblob -t tag" fails with error Unknown table 's'...
fail !0 exec $(INDEXER) -Eblob -t tag1  $(UDM_TEST_DIR)/indexer-blob.conf 1>&2

# Test with limit
fail !0 exec $(INDEXER) -Eblob -u "%test1%" $(UDM_TEST_DIR)/indexer-blob.conf 1>&2
fail !0 exec $(INDEXER) -Esqlmon $(UDM_TEST_DIR)/indexer.conf < $(UDM_TEST_DIR)/query.tst > $(UDM_TEST_DIR)/query-u.rej
fail !0 mdiff $(UDM_TEST_DIR)/query-u.rej $(UDM_TEST_DIR)/query-u.res
fail !0 exec rm -f $(UDM_TEST_DIR)/query-u.rej

pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer-blob.conf 1>&2
