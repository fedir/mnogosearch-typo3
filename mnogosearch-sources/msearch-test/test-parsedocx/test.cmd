skip !0 exec $(INDEXER) --check      $(UDM_TEST_DIR)/indexer.conf 1>&2
fail 20 exec $(INDEXER) --drop       $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) --create     $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) --crawl  -v6 $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) --index      $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Esqlmon $(UDM_TEST_DIR)/indexer.conf < $(UDM_TEST_DIR)/query.tst > $(UDM_TEST_DIR)/query.rej
fail !0 exec $(SEARCH) "Nunc at risus" >> $(UDM_TEST_DIR)/query.rej
fail !0 exec $(SEARCH) "vestibulum"    >> $(UDM_TEST_DIR)/query.rej
fail !0 exec $(SEARCH) "8"  >> $(UDM_TEST_DIR)/query.rej
fail !0 exec $(SEARCH) "%D0%B2%D0%BE%D0%B4%D0%B0"  >> $(UDM_TEST_DIR)/query.rej
fail !0 exec $(SEARCH) "Nunc+at+risus&cc=1&URL=http:%2F%2Fsite%2Fsample.docx" >> $(UDM_TEST_DIR)/query.rej
fail !0 exec $(SEARCH) "bold+italic"  >> $(UDM_TEST_DIR)/query.rej
fail !0 exec $(SEARCH) "%D0%B2%D0%BE%D0%B4%D0%B0&cc=1&URL=http:%2F%2Fsite%2Fsample2.docx" >> $(UDM_TEST_DIR)/query.rej
fail !0 exec $(SEARCH) "bold+italic&cc=1&URL=http:%2F%2Fsite%2Fsample4-lists.docx" >> $(UDM_TEST_DIR)/query.rej



fail !0 mdiff $(UDM_TEST_DIR)/query.rej $(UDM_TEST_DIR)/query.res
fail !0 exec rm -f $(UDM_TEST_DIR)/query.rej
pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer.conf 1>&2
