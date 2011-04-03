SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
this	16777217	847135917	http://site/test1.html
is	16777218	847135917	http://site/test1.html
the	16777219	847135917	http://site/test1.html
first	16777220	847135917	http://site/test1.html
test	16777221	847135917	http://site/test1.html
page	16777222	847135917	http://site/test1.html
test	33554433	847135917	http://site/test1.html
1	33554434	847135917	http://site/test1.html
title	33554435	847135917	http://site/test1.html
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	103	1	-138857385	http://site/test2.html
200	121	0	587962988	http://site/
200	102	1	847135917	http://site/test1.html
200	41	1	1675675821	http://site/test1.txt
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
200	847135917	http://site/test1.html	body	This is the first test page.
200	847135917	http://site/test1.html	title	Test 1 title
SQL>
