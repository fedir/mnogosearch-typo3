SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
&#228;	16777217	-200217541	http://site/index.html
&#228;	16777218	-200217541	http://site/index.html
&#228;	67108865	-200217541	http://site/index.html
&#228;	67108866	-200217541	http://site/index.html
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	184	0	-200217541	http://site/index.html
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
200	-200217541	http://site/index.html	body	&#34; &#228;
200	-200217541	http://site/index.html	meta.description	&#34; &#228;
SQL>
