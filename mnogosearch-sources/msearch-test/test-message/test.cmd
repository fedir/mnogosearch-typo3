#skip !0 exec test -x /usr/local/bin/catdoc || test -x /usr/bin/catdoc

skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer.conf 1>&2
fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl  -v 6 $(UDM_TEST_DIR)/indexer.conf 1>&2
fail !0 exec $(INDEXER) -Esqlmon $(UDM_TEST_DIR)/indexer.conf < $(UDM_TEST_DIR)/query.tst > $(UDM_TEST_DIR)/query.rej
fail !0 exec $(SEARCH) "в" >> $(UDM_TEST_DIR)/query.rej
fail !0 exec $(SEARCH) "с+Sergey&cc=1&amp;URL=http:%2F%2Fsite%2Ftest01.eml" >> $(UDM_TEST_DIR)/query.rej

# Now test with no Content-Type in urlinfo
fail !0 exec $(INDEXER) --exec="DELETE FROM urlinfo WHERE sname='content-type'" >> $(UDM_TEST_DIR)/query.rej
fail !0 exec $(SEARCH) "с+Sergey&cc=1&amp;URL=http:%2F%2Fsite%2Ftest01.eml" >> $(UDM_TEST_DIR)/query.rej
fail !0 exec $(SEARCH) "scott&cc=1&amp;URL=http:%2F%2Fsite%2Ftest20.eml" >> $(UDM_TEST_DIR)/query.rej

fail !0 mdiff $(UDM_TEST_DIR)/query.rej $(UDM_TEST_DIR)/query.res
fail !0 exec rm -f $(UDM_TEST_DIR)/query.rej
pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer.conf 1>&2
