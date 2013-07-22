fail !0 exec UDMSEARCH_TEMPLATE=$(UDM_TEST_DIR)/search1.htm $(SEARCH) web > $(UDM_TEST_DIR)/search.rej
fail !0 exec UDMSEARCH_TEMPLATE=$(UDM_TEST_DIR)/search1.htm $(SEARCH) "web&ps=1&np=1" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec UDMSEARCH_TEMPLATE=$(UDM_TEST_DIR)/search1.htm $(SEARCH) "web&ps=1&np=2" >> $(UDM_TEST_DIR)/search.rej

fail !0 exec UDMSEARCH_TEMPLATE=$(UDM_TEST_DIR)/search2.htm $(SEARCH) web >> $(UDM_TEST_DIR)/search.rej

fail !0 exec UDMSEARCH_TEMPLATE=$(UDM_TEST_DIR)/search12.htm $(SEARCH) web >> $(UDM_TEST_DIR)/search.rej
fail !0 exec UDMSEARCH_TEMPLATE=$(UDM_TEST_DIR)/search12.htm $(SEARCH) "web&ps=3" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec UDMSEARCH_TEMPLATE=$(UDM_TEST_DIR)/search12.htm $(SEARCH) "web&ps=1&np=1" >> $(UDM_TEST_DIR)/search.rej
fail !0 exec UDMSEARCH_TEMPLATE=$(UDM_TEST_DIR)/search12.htm REQUEST_METHOD=GET QUERY_STRING="q=web&ps=1&np=2" $(SEARCH) >> $(UDM_TEST_DIR)/search.rej
fail !0 exec UDMSEARCH_TEMPLATE=$(UDM_TEST_DIR)/search3.htm $(SEARCH) web >> $(UDM_TEST_DIR)/search.rej
fail !0 exec UDMSEARCH_TEMPLATE=$(UDM_TEST_DIR)/search4g.htm $(SEARCH) test >> $(UDM_TEST_DIR)/search.rej
fail !0 exec UDMSEARCH_TEMPLATE=$(UDM_TEST_DIR)/search5.htm $(SEARCH) test >> $(UDM_TEST_DIR)/search.rej

fail !0 mdiff $(UDM_TEST_DIR)/search.rej $(UDM_TEST_DIR)/search.res
pass 0 exec rm -f $(UDM_TEST_DIR)/search.rej

