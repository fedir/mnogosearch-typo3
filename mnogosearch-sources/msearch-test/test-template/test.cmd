fail !0 exec QUERY_STRING="test&xxx=%3CXXX%3E&ResultContentType=IgnoreThis&charset=IgnoreThis" $(SEARCH)  > $(UDM_TEST_DIR)/search.rej
fail !0 mdiff $(UDM_TEST_DIR)/search.rej $(UDM_TEST_DIR)/search.res
pass 0 exec rm -f $(UDM_TEST_DIR)/search.rej
