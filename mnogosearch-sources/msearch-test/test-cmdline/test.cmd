skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/arg-n.conf 1>&2
fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/arg-n.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/arg-n.conf 1>&2

# Check that -nX does not mark more than X documents as non-expired

fail !0 exec $(INDEXER) -Ecrawl -n1  $(UDM_TEST_DIR)/arg-n.conf 1>&2
fail !0 exec $(INDEXER) -Estat  -j3  $(UDM_TEST_DIR)/arg-n.conf | grep -v "Database" >  $(UDM_TEST_DIR)/query.rej 2>&1
fail !0 exec $(INDEXER) -Estat  -j3 -s400-500  $(UDM_TEST_DIR)/arg-n.conf | grep -v "Database" >> $(UDM_TEST_DIR)/query.rej 2>&1
fail !0 exec $(INDEXER) -Ecrawl -n2  $(UDM_TEST_DIR)/arg-n.conf 1>&2
fail !0 exec $(INDEXER) -Estat       $(UDM_TEST_DIR)/arg-n.conf | grep -v "Database" >> $(UDM_TEST_DIR)/query.rej 2>&1

# Bug#1716 Can't limit indexer to documents matching language
fail !0 exec $(INDEXER) -Ecrawl -am -L en $(UDM_TEST_DIR)/arg-n.conf

# Testing URLList flag -f
fail !0 exec echo "Testing -f urllist.txt" >> $(UDM_TEST_DIR)/query.rej
fail !0 exec $(INDEXER) -Ecrawl -f $(UDM_TEST_DIR)/urllist.txt -a -n0 $(UDM_TEST_DIR)/arg-n.conf 1>&2
fail !0 exec $(INDEXER) -Estat  -j3  $(UDM_TEST_DIR)/arg-n.conf | grep -v "Database" >> $(UDM_TEST_DIR)/query.rej 2>&1


fail !0 mdiff $(UDM_TEST_DIR)/query.rej $(UDM_TEST_DIR)/query.res
fail !0 exec rm -f $(UDM_TEST_DIR)/query.rej

pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/arg-n.conf 1>&2
