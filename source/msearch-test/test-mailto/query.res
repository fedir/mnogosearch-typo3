SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
report	16777217	-1475143956	http://site/test.html
bugs	16777218	-1475143956	http://site/test.html
here	16777219	-1475143956	http://site/test.html
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	92	1	-1475143956	http://site/test.html
200	77	0	0	http://site/
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
200	-1475143956	http://site/test.html	body	Report bugs here
SQL>
