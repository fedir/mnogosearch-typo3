SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
this	16777217	-660273043	http://site/test2.html
is	16777218	-660273043	http://site/test2.html
the	16777219	-660273043	http://site/test2.html
second	16777220	-660273043	http://site/test2.html
test	16777221	-660273043	http://site/test2.html
page	16777222	-660273043	http://site/test2.html
test	16777223	-660273043	http://site/test2.html
a	16777224	-660273043	http://site/test2.html
b	16777225	-660273043	http://site/test2.html
test	33554433	-660273043	http://site/test2.html
2	33554434	-660273043	http://site/test2.html
title	33554435	-660273043	http://site/test2.html
test2	100663297	-660273043	http://site/test2.html
html	100663298	-660273043	http://site/test2.html
site	134217729	-660273043	http://site/test2.html
http	150994945	-660273043	http://site/test2.html
site	134217729	0	http://site/
http	150994945	0	http://site/
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
200	-660273043	http://site/test2.html	body	This is the second test page. test a-b
200	-660273043	http://site/test2.html	title	Test 2 title
200	0	http://site/test1.html	title2	Test 1 title
SQL>'SELECT crc32, status, seed, url FROM url ORDER BY seed, crc32, status'
0	404	8	http://site/catalog?item=12&desc=vacation_hawaii_sitemap1
-660273043	200	29	http://site/test2.html
0	404	44	http://site/catalog?item=73&desc=vacation_new_zealand_sitemap1
0	200	52	http://site/
0	200	133	http://site/test1.html
0	404	212	http://site/catalog?item=74&desc=vacation_newfoundland_sitemap2
0	404	244	http://site/catalog?item=83&desc=vacation_usa_sitemap2
SQL>
