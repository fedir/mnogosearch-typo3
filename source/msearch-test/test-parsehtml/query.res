SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
one	16777217	-1503629308	http://site/index.html
two	16777218	-1503629308	http://site/index.html
five	16777219	-1503629308	http://site/index.html
six	16777220	-1503629308	http://site/index.html
nine	16777221	-1503629308	http://site/index.html
ten	16777222	-1503629308	http://site/index.html
this	33554433	-1503629308	http://site/index.html
is	33554434	-1503629308	http://site/index.html
a	33554435	-1503629308	http://site/index.html
title	33554436	-1503629308	http://site/index.html
in	33554437	-1503629308	http://site/index.html
the	33554438	-1503629308	http://site/index.html
middle	33554439	-1503629308	http://site/index.html
i	33554440	-1503629308	http://site/index.html
m	33554441	-1503629308	http://site/index.html
wondering	33554442	-1503629308	http://site/index.html
if	33554443	-1503629308	http://site/index.html
this	33554444	-1503629308	http://site/index.html
is	33554445	-1503629308	http://site/index.html
working	33554446	-1503629308	http://site/index.html
as	33554447	-1503629308	http://site/index.html
expected	33554448	-1503629308	http://site/index.html
index	100663297	-1503629308	http://site/index.html
html	100663298	-1503629308	http://site/index.html
site	134217729	-1503629308	http://site/index.html
http	150994945	-1503629308	http://site/index.html
yyy1	167772161	-1503629308	http://site/index.html
yyy2	167772162	-1503629308	http://site/index.html
site	134217729	0	http://site/
http	150994945	0	http://site/
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	636	1	-1503629308	http://site/index.html
200	78	0	0	http://site/
404	0	2	0	http://site/iframe.html
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
200	-1503629308	http://site/index.html	attribute.yyy	yyy1 yyy2
200	-1503629308	http://site/index.html	body	One two Five Six Nine Ten
200	-1503629308	http://site/index.html	charset	koi8-r
200	-1503629308	http://site/index.html	title	This is a title in the middle . I'm wondering if this is working as expected.
200	0	http://site/	charset	iso-8859-1
SQL>SQL>'SELECT url FROM url WHERE url='http://site/''
http://site/
SQL>
