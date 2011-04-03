SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
test2	16777217	-1722273439	http://site/test2.html
test2	16777218	-1722273439	http://site/test2.html
test2	16777219	-1722273439	http://site/test2.html
test2	16777220	-1722273439	http://site/test2.html
test2	100663297	-1722273439	http://site/test2.html
html	100663298	-1722273439	http://site/test2.html
site	134217729	-1722273439	http://site/test2.html
http	150994945	-1722273439	http://site/test2.html
site	134217729	0	http://site/
http	150994945	0	http://site/
test3	16777217	981562435	http://site/test3.html
test3	16777218	981562435	http://site/test3.html
test3	16777219	981562435	http://site/test3.html
test3	16777220	981562435	http://site/test3.html
test3	16777221	981562435	http://site/test3.html
test3	100663297	981562435	http://site/test3.html
html	100663298	981562435	http://site/test3.html
site	134217729	981562435	http://site/test3.html
http	150994945	981562435	http://site/test3.html
test1	16777217	1792417788	http://site/test1.html
test1	16777218	1792417788	http://site/test1.html
test1	16777219	1792417788	http://site/test1.html
test1	100663297	1792417788	http://site/test1.html
html	100663298	1792417788	http://site/test1.html
site	134217729	1792417788	http://site/test1.html
http	150994945	1792417788	http://site/test1.html
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32, hops'
200	122	0	0	http://site/
304	56	1	-1722273439	http://site/test2.html
304	62	1	981562435	http://site/test3.html
304	50	1	1792417788	http://site/test1.html
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
304	-1722273439	http://site/test2.html	body	test2 test2 test2 test2
304	981562435	http://site/test3.html	body	test3 test3 test3 test3 test3
304	1792417788	http://site/test1.html	body	test1 test1 test1
SQL>
