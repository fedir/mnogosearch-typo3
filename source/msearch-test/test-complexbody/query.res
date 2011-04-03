SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
this	16777217	-1357020172	http://site/test1.html
is	16777218	-1357020172	http://site/test1.html
the	16777219	-1357020172	http://site/test1.html
complex	16777220	-1357020172	http://site/test1.html
body	16777221	-1357020172	http://site/test1.html
test	16777222	-1357020172	http://site/test1.html
page	16777223	-1357020172	http://site/test1.html
test	33554433	-1357020172	http://site/test1.html
title	33554434	-1357020172	http://site/test1.html
test1	100663297	-1357020172	http://site/test1.html
html	100663298	-1357020172	http://site/test1.html
site	134217729	-1357020172	http://site/test1.html
http	150994945	-1357020172	http://site/test1.html
site	134217729	0	http://site/
http	150994945	0	http://site/
blabla	16777217	614229196	http://site/test2.html
blabla	16777218	614229196	http://site/test2.html
test2	100663297	614229196	http://site/test2.html
html	100663298	614229196	http://site/test2.html
site	134217729	614229196	http://site/test2.html
http	150994945	614229196	http://site/test2.html
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	314	1	-1357020172	http://site/test1.html
200	100	0	0	http://site/
200	48	1	614229196	http://site/test2.html
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
200	-1357020172	http://site/test1.html	body	This is the complex body test page.
200	-1357020172	http://site/test1.html	title	Test title
200	614229196	http://site/test2.html	body	blabla blabla ...
SQL>SQL>'SELECT url FROM url WHERE url='http://site/''
http://site/
SQL>
