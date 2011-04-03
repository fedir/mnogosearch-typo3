SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
blabla	16777217	614229196	http://site/
blabla	16777218	614229196	http://site/
site	134217729	614229196	http://site/
http	150994945	614229196	http://site/
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	48	0	614229196	http://site/
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
200	614229196	http://site/	body	blabla blabla ...
SQL>SQL>'SELECT url FROM url WHERE url='http://site/''
http://site/
SQL>
