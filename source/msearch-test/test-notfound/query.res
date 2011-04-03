SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
this	16777217	-1781601539	http://site/test1.txt
is	16777218	-1781601539	http://site/test1.txt
a	16777219	-1781601539	http://site/test1.txt
text	16777220	-1781601539	http://site/test1.txt
file	16777221	-1781601539	http://site/test1.txt
no	16777222	-1781601539	http://site/test1.txt
title	16777223	-1781601539	http://site/test1.txt
available	16777224	-1781601539	http://site/test1.txt
test1	100663297	-1781601539	http://site/test1.txt
txt	100663298	-1781601539	http://site/test1.txt
site	134217729	-1781601539	http://site/test1.txt
http	150994945	-1781601539	http://site/test1.txt
site	134217729	0	http://site/
http	150994945	0	http://site/
this	16777217	296718507	http://site/test1.html
is	16777218	296718507	http://site/test1.html
the	16777219	296718507	http://site/test1.html
first	16777220	296718507	http://site/test1.html
test	16777221	296718507	http://site/test1.html
page	16777222	296718507	http://site/test1.html
here	16777223	296718507	http://site/test1.html
is	16777224	296718507	http://site/test1.html
the	16777225	296718507	http://site/test1.html
second	16777226	296718507	http://site/test1.html
one	16777227	296718507	http://site/test1.html
test	33554433	296718507	http://site/test1.html
1	33554434	296718507	http://site/test1.html
title	33554435	296718507	http://site/test1.html
test1	100663297	296718507	http://site/test1.html
html	100663298	296718507	http://site/test1.html
site	134217729	296718507	http://site/test1.html
http	150994945	296718507	http://site/test1.html
this	16777217	1522560770	http://site/test2.html
is	16777218	1522560770	http://site/test2.html
the	16777219	1522560770	http://site/test2.html
second	16777220	1522560770	http://site/test2.html
test	16777221	1522560770	http://site/test2.html
page	16777222	1522560770	http://site/test2.html
here	16777223	1522560770	http://site/test2.html
is	16777224	1522560770	http://site/test2.html
the	16777225	1522560770	http://site/test2.html
third	16777226	1522560770	http://site/test2.html
one	16777227	1522560770	http://site/test2.html
test	33554433	1522560770	http://site/test2.html
2	33554434	1522560770	http://site/test2.html
title	33554435	1522560770	http://site/test2.html
test2	100663297	1522560770	http://site/test2.html
html	100663298	1522560770	http://site/test2.html
site	134217729	1522560770	http://site/test2.html
http	150994945	1522560770	http://site/test2.html
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	41	1	-1781601539	http://site/test1.txt
200	121	0	0	http://site/
200	151	1	296718507	http://site/test1.html
200	151	1	1522560770	http://site/test2.html
404	0	2	0	http://site/test3.html
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
200	-1781601539	http://site/test1.txt	body	This is a text file. No title available.
200	296718507	http://site/test1.html	body	This is the first test page. Here is the second one.
200	296718507	http://site/test1.html	title	Test 1 title
200	1522560770	http://site/test2.html	body	This is the second test page. Here is the third one.
200	1522560770	http://site/test2.html	title	Test 2 title
SQL>
SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
this	16777217	-1781601539	http://site/test1.txt
is	16777218	-1781601539	http://site/test1.txt
a	16777219	-1781601539	http://site/test1.txt
text	16777220	-1781601539	http://site/test1.txt
file	16777221	-1781601539	http://site/test1.txt
no	16777222	-1781601539	http://site/test1.txt
title	16777223	-1781601539	http://site/test1.txt
available	16777224	-1781601539	http://site/test1.txt
test1	100663297	-1781601539	http://site/test1.txt
txt	100663298	-1781601539	http://site/test1.txt
site	134217729	-1781601539	http://site/test1.txt
http	150994945	-1781601539	http://site/test1.txt
site	134217729	0	http://site/
http	150994945	0	http://site/
this	16777217	1522560770	http://site/test2.html
is	16777218	1522560770	http://site/test2.html
the	16777219	1522560770	http://site/test2.html
second	16777220	1522560770	http://site/test2.html
test	16777221	1522560770	http://site/test2.html
page	16777222	1522560770	http://site/test2.html
here	16777223	1522560770	http://site/test2.html
is	16777224	1522560770	http://site/test2.html
the	16777225	1522560770	http://site/test2.html
third	16777226	1522560770	http://site/test2.html
one	16777227	1522560770	http://site/test2.html
test	33554433	1522560770	http://site/test2.html
2	33554434	1522560770	http://site/test2.html
title	33554435	1522560770	http://site/test2.html
test2	100663297	1522560770	http://site/test2.html
html	100663298	1522560770	http://site/test2.html
site	134217729	1522560770	http://site/test2.html
http	150994945	1522560770	http://site/test2.html
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	41	1	-1781601539	http://site/test1.txt
200	121	0	0	http://site/
200	151	1	1522560770	http://site/test2.html
404	0	2	0	http://site/test3.html
404	151	1	296718507	http://site/test1.html
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
200	-1781601539	http://site/test1.txt	body	This is a text file. No title available.
200	1522560770	http://site/test2.html	body	This is the second test page. Here is the third one.
200	1522560770	http://site/test2.html	title	Test 2 title
404	296718507	http://site/test1.html	body	This is the first test page. Here is the second one.
404	296718507	http://site/test1.html	title	Test 1 title
SQL>
