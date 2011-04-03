SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag,dict.word'
a	16777217	1750758558	http://site/index.htm.gz
b	16777218	1750758558	http://site/index.htm.gz
c	16777219	1750758558	http://site/index.htm.gz
d1&#1072;	16777220	1750758558	http://site/index.htm.gz
d2&#1073;	16777221	1750758558	http://site/index.htm.gz
d3&#1074;	16777222	1750758558	http://site/index.htm.gz
a	16777223	1750758558	http://site/index.htm.gz
b	16777223	1750758558	http://site/index.htm.gz
c	16777223	1750758558	http://site/index.htm.gz
d1&#1072;	16777223	1750758558	http://site/index.htm.gz
d2&#1073;	16777223	1750758558	http://site/index.htm.gz
d3&#1074;	16777223	1750758558	http://site/index.htm.gz
d4&#228;	67108865	1750758558	http://site/index.htm.gz
d4&#228;	67108866	1750758558	http://site/index.htm.gz
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	103	0	1750758558	http://site/index.htm.gz
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
200	1750758558	http://site/index.htm.gz	body	a b c d1&#1072; d2&#1073; d3&#1074;
200	1750758558	http://site/index.htm.gz	meta.description	d4&#228;
SQL>
