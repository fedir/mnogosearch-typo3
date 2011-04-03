#skip !0 testenv UDM_TEST_ROOT
#skip !0 testenv UDM_TEST_DIR
#skip !0 testenv UDM_TEST_DBADDR0
#skip !0 testenv UDM_SHARE_DIR
#skip !0 testenv INDEXER
skip 0 substrenv UDM_TEST_DBADDR0 oracle://
skip 0 substrenv UDM_TEST_DBADDR0 odbc-oracle8://
skip 0 substrenv UDM_TEST_DBADDR0 mssql://
skip 0 substrenv UDM_TEST_DBADDR0 monetdb://

# First part
skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer1.conf 1>&2
fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer1.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer1.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl  $(UDM_TEST_DIR)/indexer1.conf 1>&2
fail !0 exec cp -f $(UDM_TEST_DIR)/search1.htm $(UDM_TEST_DIR)/search.htm

fail !0 exec $(SEARCH) "test&bcs=iso-8859-1" > $(UDM_TEST_DIR)/expect1-1.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect1-1.rej $(UDM_TEST_DIR)/expect1-1.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect1-1.rej

fail !0 exec $(SEARCH) "test&bcs=koi8-r" > $(UDM_TEST_DIR)/expect1-2.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect1-2.rej $(UDM_TEST_DIR)/expect1-2.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect1-2.rej

fail !0 exec $(SEARCH) "test&bcs=utf-8" > $(UDM_TEST_DIR)/expect1-3.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect1-3.rej $(UDM_TEST_DIR)/expect1-3.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect1-3.rej

fail !0 exec $(SEARCH) "%26#1069%3B%26#1090%3B%26#1086%3B&bcs=iso-8859-1" > $(UDM_TEST_DIR)/expect1-4.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect1-4.rej $(UDM_TEST_DIR)/expect1-4.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect1-4.rej

fail !0 exec $(SEARCH) "%FC%D4%CF&bcs=koi8-r" > $(UDM_TEST_DIR)/expect1-5.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect1-5.rej $(UDM_TEST_DIR)/expect1-5.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect1-5.rej

fail !0 exec $(SEARCH) "%D1%8D%D1%82%D0%BE&bcs=utf-8" > $(UDM_TEST_DIR)/expect1-6.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect1-6.rej $(UDM_TEST_DIR)/expect1-6.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect1-6.rej

# First part, second section (wm=sub, beg, end)
fail !0 exec $(SEARCH) "es&wm=sub&bcs=iso-8859-1" > $(UDM_TEST_DIR)/expect-sub.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect-sub.rej $(UDM_TEST_DIR)/expect-sub.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect-sub.rej

fail !0 exec $(SEARCH) "te&wm=beg&bcs=iso-8859-1" > $(UDM_TEST_DIR)/expect-beg.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect-beg.rej $(UDM_TEST_DIR)/expect-beg.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect-beg.rej

fail !0 exec $(SEARCH) "st&wm=end&bcs=iso-8859-1" > $(UDM_TEST_DIR)/expect-end.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect-end.rej $(UDM_TEST_DIR)/expect-end.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect-end.rej

fail !0 exec $(SEARCH) "a-b&bcs=iso-8859-1" > $(UDM_TEST_DIR)/expect-phr.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect-phr.rej $(UDM_TEST_DIR)/expect-phr.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect-phr.rej

fail !0 exec rm -f $(UDM_TEST_DIR)/search.htm

# Second part
skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer2.conf 1>&2
fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer2.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer2.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl  $(UDM_TEST_DIR)/indexer2.conf 1>&2
fail !0 exec cp -f $(UDM_TEST_DIR)/search2.htm $(UDM_TEST_DIR)/search.htm

fail !0 exec $(SEARCH) "test&bcs=iso-8859-1" > $(UDM_TEST_DIR)/expect2-1.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect2-1.rej $(UDM_TEST_DIR)/expect2-1.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect2-1.rej

fail !0 exec $(SEARCH) "test&bcs=koi8-r" > $(UDM_TEST_DIR)/expect2-2.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect2-2.rej $(UDM_TEST_DIR)/expect2-2.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect2-2.rej

fail !0 exec $(SEARCH) "test&bcs=utf-8" > $(UDM_TEST_DIR)/expect2-3.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect2-3.rej $(UDM_TEST_DIR)/expect2-3.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect2-3.rej

fail !0 exec $(SEARCH) "%26#1069%3B%26#1090%3B%26#1086%3B&bcs=iso-8859-1" > $(UDM_TEST_DIR)/expect2-4.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect2-4.rej $(UDM_TEST_DIR)/expect2-4.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect2-4.rej

fail !0 exec $(SEARCH) "%FC%D4%CF&bcs=koi8-r" > $(UDM_TEST_DIR)/expect2-5.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect2-5.rej $(UDM_TEST_DIR)/expect2-5.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect2-5.rej

fail !0 exec $(SEARCH) "%D1%8D%D1%82%D0%BE&bcs=utf-8" > $(UDM_TEST_DIR)/expect2-6.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect2-6.rej $(UDM_TEST_DIR)/expect2-6.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect2-6.rej

fail !0 exec rm -f $(UDM_TEST_DIR)/search.htm

# Third part
skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer3.conf 1>&2
fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer3.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer3.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl  $(UDM_TEST_DIR)/indexer3.conf 1>&2
fail !0 exec cp -f $(UDM_TEST_DIR)/search3.htm $(UDM_TEST_DIR)/search.htm

fail !0 exec $(SEARCH) "test&bcs=iso-8859-1" > $(UDM_TEST_DIR)/expect3-1.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect3-1.rej $(UDM_TEST_DIR)/expect3-1.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect3-1.rej

fail !0 exec $(SEARCH) "test&bcs=koi8-r" > $(UDM_TEST_DIR)/expect3-2.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect3-2.rej $(UDM_TEST_DIR)/expect3-2.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect3-2.rej

fail !0 exec $(SEARCH) "test&bcs=utf-8" > $(UDM_TEST_DIR)/expect3-3.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect3-3.rej $(UDM_TEST_DIR)/expect3-3.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect3-3.rej

fail !0 exec $(SEARCH) "%26#1069%3B%26#1090%3B%26#1086%3B&bcs=iso-8859-1" > $(UDM_TEST_DIR)/expect3-4.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect3-4.rej $(UDM_TEST_DIR)/expect3-4.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect3-4.rej

fail !0 exec $(SEARCH) "%FC%D4%CF&bcs=koi8-r" > $(UDM_TEST_DIR)/expect3-5.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect3-5.rej $(UDM_TEST_DIR)/expect3-5.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect3-5.rej

fail !0 exec $(SEARCH) "%D1%8D%D1%82%D0%BE&bcs=utf-8" > $(UDM_TEST_DIR)/expect3-6.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect3-6.rej $(UDM_TEST_DIR)/expect3-6.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect3-6.rej

fail !0 exec rm -f $(UDM_TEST_DIR)/search.htm

# Fourth part
skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer4.conf 1>&2
fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer4.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer4.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl  $(UDM_TEST_DIR)/indexer4.conf 1>&2
fail !0 exec cp -f $(UDM_TEST_DIR)/search1.htm $(UDM_TEST_DIR)/search.htm

fail !0 exec $(SEARCH) "test&bcs=iso-8859-1" > $(UDM_TEST_DIR)/expect4-1.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect4-1.rej $(UDM_TEST_DIR)/expect4-1.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect4-1.rej

fail !0 exec $(SEARCH) "test&bcs=koi8-r" > $(UDM_TEST_DIR)/expect4-2.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect4-2.rej $(UDM_TEST_DIR)/expect4-2.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect4-2.rej

fail !0 exec $(SEARCH) "test&bcs=utf-8" > $(UDM_TEST_DIR)/expect4-3.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect4-3.rej $(UDM_TEST_DIR)/expect4-3.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect4-3.rej

fail !0 exec $(SEARCH) "%26#1069%3B%26#1090%3B%26#1086%3B&bcs=iso-8859-1" > $(UDM_TEST_DIR)/expect4-4.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect4-4.rej $(UDM_TEST_DIR)/expect4-4.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect4-4.rej

fail !0 exec $(SEARCH) "%FC%D4%CF&bcs=koi8-r" > $(UDM_TEST_DIR)/expect4-5.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect4-5.rej $(UDM_TEST_DIR)/expect4-5.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect4-5.rej

fail !0 exec $(SEARCH) "%D1%8D%D1%82%D0%BE&bcs=utf-8" > $(UDM_TEST_DIR)/expect4-6.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect4-6.rej $(UDM_TEST_DIR)/expect4-6.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect4-6.rej

fail !0 exec rm -f $(UDM_TEST_DIR)/search.htm

# Fifth part
skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer5.conf 1>&2
fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer5.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer5.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl  $(UDM_TEST_DIR)/indexer5.conf 1>&2
fail !0 exec cp -f $(UDM_TEST_DIR)/search2.htm $(UDM_TEST_DIR)/search.htm

fail !0 exec $(SEARCH) "test&bcs=iso-8859-1" > $(UDM_TEST_DIR)/expect5-1.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect5-1.rej $(UDM_TEST_DIR)/expect5-1.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect5-1.rej

fail !0 exec $(SEARCH) "test&bcs=koi8-r" > $(UDM_TEST_DIR)/expect5-2.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect5-2.rej $(UDM_TEST_DIR)/expect5-2.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect5-2.rej

fail !0 exec $(SEARCH) "test&bcs=utf-8" > $(UDM_TEST_DIR)/expect5-3.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect5-3.rej $(UDM_TEST_DIR)/expect5-3.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect5-3.rej

fail !0 exec $(SEARCH) "%26#1069%3B%26#1090%3B%26#1086%3B&bcs=iso-8859-1" > $(UDM_TEST_DIR)/expect5-4.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect5-4.rej $(UDM_TEST_DIR)/expect5-4.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect5-4.rej

fail !0 exec $(SEARCH) "%FC%D4%CF&bcs=koi8-r" > $(UDM_TEST_DIR)/expect5-5.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect5-5.rej $(UDM_TEST_DIR)/expect5-5.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect5-5.rej

fail !0 exec $(SEARCH) "%D1%8D%D1%82%D0%BE&bcs=utf-8" > $(UDM_TEST_DIR)/expect5-6.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect5-6.rej $(UDM_TEST_DIR)/expect5-6.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect5-6.rej

fail !0 exec rm -f $(UDM_TEST_DIR)/search.htm

# Sixth part
skip !0 exec $(INDEXER) -Echeck  $(UDM_TEST_DIR)/indexer6.conf 1>&2
fail 20 exec $(INDEXER) -Edrop   $(UDM_TEST_DIR)/indexer6.conf 1>&2
fail !0 exec $(INDEXER) -Ecreate $(UDM_TEST_DIR)/indexer6.conf 1>&2
fail !0 exec $(INDEXER) -Ecrawl  $(UDM_TEST_DIR)/indexer6.conf 1>&2
fail !0 exec cp -f $(UDM_TEST_DIR)/search3.htm $(UDM_TEST_DIR)/search.htm

fail !0 exec $(SEARCH) "test&bcs=iso-8859-1" > $(UDM_TEST_DIR)/expect6-1.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect6-1.rej $(UDM_TEST_DIR)/expect6-1.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect6-1.rej

fail !0 exec $(SEARCH) "test&bcs=koi8-r" > $(UDM_TEST_DIR)/expect6-2.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect6-2.rej $(UDM_TEST_DIR)/expect6-2.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect6-2.rej

fail !0 exec $(SEARCH) "test&bcs=utf-8" > $(UDM_TEST_DIR)/expect6-3.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect6-3.rej $(UDM_TEST_DIR)/expect6-3.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect6-3.rej

fail !0 exec $(SEARCH) "%26#1069%3B%26#1090%3B%26#1086%3B&bcs=iso-8859-1" > $(UDM_TEST_DIR)/expect6-4.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect6-4.rej $(UDM_TEST_DIR)/expect6-4.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect6-4.rej

fail !0 exec $(SEARCH) "%FC%D4%CF&bcs=koi8-r" > $(UDM_TEST_DIR)/expect6-5.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect6-5.rej $(UDM_TEST_DIR)/expect6-5.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect6-5.rej

fail !0 exec $(SEARCH) "%D1%8D%D1%82%D0%BE&bcs=utf-8" > $(UDM_TEST_DIR)/expect6-6.rej
fail !0 mdiff $(UDM_TEST_DIR)/expect6-6.rej $(UDM_TEST_DIR)/expect6-6.res
fail !0 exec rm -f $(UDM_TEST_DIR)/expect6-6.rej

fail !0 exec rm -f $(UDM_TEST_DIR)/search.htm

pass 0 exec  $(INDEXER) -Edrop $(UDM_TEST_DIR)/indexer1.conf 1>&2
