SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
2	16777217	-1683210011	http://site/test2.html
test2	33554433	-1683210011	http://site/test2.html
test2	100663297	-1683210011	http://site/test2.html
html	100663298	-1683210011	http://site/test2.html
site	134217729	-1683210011	http://site/test2.html
http	150994945	-1683210011	http://site/test2.html
ttt	16777217	-1600865580	http://site/test3.html
ttt	16777218	-1600865580	http://site/test3.html
ttt	16777219	-1600865580	http://site/test3.html
ttt	16777220	-1600865580	http://site/test3.html
xxx	16777221	-1600865580	http://site/test3.html
xxx	16777222	-1600865580	http://site/test3.html
xxx	16777223	-1600865580	http://site/test3.html
site	16777224	-1600865580	http://site/test3.html
xxx	16777225	-1600865580	http://site/test3.html
xxx	16777226	-1600865580	http://site/test3.html
xxx	16777227	-1600865580	http://site/test3.html
ttt	16777217	-1299413799	http://site/userdef2-test1.html
ttt	16777218	-1299413799	http://site/userdef2-test1.html
ttt	16777219	-1299413799	http://site/userdef2-test1.html
ttt	16777220	-1299413799	http://site/userdef2-test1.html
this	184549377	-1299413799	http://site/userdef2-test1.html
is	184549378	-1299413799	http://site/userdef2-test1.html
html	184549379	-1299413799	http://site/userdef2-test1.html
meta10l	352321538	-1299413799	http://site/userdef2-test1.html
test2txttitle	16777217	-1117818212	http://site/test2.txt
test2txtbody	16777218	-1117818212	http://site/test2.txt
site	134217729	0	http://site/
http	150994945	0	http://site/
issuedata	16777217	66951968	http://site/test3.txt
engineer	16777218	66951968	http://site/test3.txt
mister	16777219	66951968	http://site/test3.txt
x	16777220	66951968	http://site/test3.txt
issuedata	16777221	66951968	http://site/test3.txt
severity	16777222	66951968	http://site/test3.txt
serious	16777223	66951968	http://site/test3.txt
serious	167772161	66951968	http://site/test3.txt
mister	184549377	66951968	http://site/test3.txt
x	184549378	66951968	http://site/test3.txt
bbb	16777217	246955969	http://site/test1.html
bbb	16777218	246955969	http://site/test1.html
bbb	16777219	246955969	http://site/test1.html
bbb	16777220	246955969	http://site/test1.html
ttt	33554433	246955969	http://site/test1.html
ttt	33554434	246955969	http://site/test1.html
ttt	33554435	246955969	http://site/test1.html
ttt	33554436	246955969	http://site/test1.html
kkk	50331649	246955969	http://site/test1.html
kkk	50331650	246955969	http://site/test1.html
kkk	50331651	246955969	http://site/test1.html
kkk	50331652	246955969	http://site/test1.html
ddd	67108865	246955969	http://site/test1.html
ddd	67108866	246955969	http://site/test1.html
ddd	67108867	246955969	http://site/test1.html
ddd	67108868	246955969	http://site/test1.html
test1	100663297	246955969	http://site/test1.html
html	100663298	246955969	http://site/test1.html
site	134217729	246955969	http://site/test1.html
http	150994945	246955969	http://site/test1.html
html	16777217	547394429	http://site/testa.txt
body	16777218	547394429	http://site/testa.txt
test	16777219	547394429	http://site/testa.txt
body	16777220	547394429	http://site/testa.txt
html	16777221	547394429	http://site/testa.txt
test	16777217	755544128	http://site/test1.txt
bbb	16777217	1133987098	http://site/test1nb.html
bbb	16777218	1133987098	http://site/test1nb.html
bbb	16777219	1133987098	http://site/test1nb.html
bbb	16777220	1133987098	http://site/test1nb.html
xxx	16777221	1133987098	http://site/test1nb.html
xxx	16777222	1133987098	http://site/test1nb.html
xxx	16777223	1133987098	http://site/test1nb.html
ttt	33554433	1133987098	http://site/test1nb.html
ttt	33554434	1133987098	http://site/test1nb.html
ttt	33554435	1133987098	http://site/test1nb.html
ttt	33554436	1133987098	http://site/test1nb.html
kkk	50331649	1133987098	http://site/test1nb.html
kkk	50331650	1133987098	http://site/test1nb.html
kkk	50331651	1133987098	http://site/test1nb.html
kkk	50331652	1133987098	http://site/test1nb.html
ddd	67108865	1133987098	http://site/test1nb.html
ddd	67108866	1133987098	http://site/test1nb.html
ddd	67108867	1133987098	http://site/test1nb.html
ddd	67108868	1133987098	http://site/test1nb.html
test1nb	100663297	1133987098	http://site/test1nb.html
html	100663298	1133987098	http://site/test1nb.html
site	134217729	1133987098	http://site/test1nb.html
http	150994945	1133987098	http://site/test1nb.html
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	238	1	-1683210011	http://site/test2.html
200	434	0	-1600865580	http://site/test3.html
200	434	0	-1299413799	http://site/userdef2-test1.html
200	87	0	-1117818212	http://site/test2.txt
200	163	0	0	http://site/
200	73	0	66951968	http://site/test3.txt
200	434	1	246955969	http://site/test1.html
200	35	0	547394429	http://site/testa.txt
200	35	0	755544128	http://site/test1.txt
200	434	0	1133987098	http://site/test1nb.html
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
200	-1683210011	http://site/test2.html	body	2
200	-1683210011	http://site/test2.html	title	Test2
200	-1600865580	http://site/test3.html	body	TTT TTT TTT TTT
200	-1600865580	http://site/test3.html	title	XXX XXX XXX
200	-1600865580	http://site/test3.html	udef	site XXX XXX XXX
200	-1299413799	http://site/userdef2-test1.html	Charset	iso-8859-1
200	-1299413799	http://site/userdef2-test1.html	lccag	this is html
200	-1299413799	http://site/userdef2-test1.html	meta_m10_long	M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789M123456789
200	-1299413799	http://site/userdef2-test1.html	title	TTT TTT TTT TTT
200	-1117818212	http://site/test2.txt	body	test2txtbody
200	-1117818212	http://site/test2.txt	content-type	text/html
200	-1117818212	http://site/test2.txt	lccag	this is html
200	-1117818212	http://site/test2.txt	title	test2txttitle
200	66951968	http://site/test3.txt	body	$ISSUEDATA['ENGINEER'] = 'Mister X'; $ISSUEDATA['SEVERITY'] = 'Serious';
200	66951968	http://site/test3.txt	engineer	Mister X
200	66951968	http://site/test3.txt	severity	Serious
200	246955969	http://site/test1.html	body	BBB BBB BBB BBB
200	246955969	http://site/test1.html	meta.description	DDD DDD DDD DDD
200	246955969	http://site/test1.html	meta.keywords	KKK KKK KKK KKK
200	246955969	http://site/test1.html	meta.m10	M123456789
200	246955969	http://site/test1.html	meta.m20	M123456789M123456789
200	246955969	http://site/test1.html	meta.m30	M123456789M123456789M123456789
200	246955969	http://site/test1.html	title	TTT TTT TTT TTT
200	547394429	http://site/testa.txt	body	&#60;html&#62; &#60;body&#62; test &#60;/body&#62; &#60;/html&#62;
200	547394429	http://site/testa.txt	content-type	text/plain
200	755544128	http://site/test1.txt	body	test
200	755544128	http://site/test1.txt	content-type	text/html
200	1133987098	http://site/test1nb.html	body	BBB BBB BBB BBB XXX XXX XXX
200	1133987098	http://site/test1nb.html	meta.description	DDD DDD DDD DDD
200	1133987098	http://site/test1nb.html	meta.keywords	KKK KKK KKK KKK
200	1133987098	http://site/test1nb.html	meta.m10	M123456789
200	1133987098	http://site/test1nb.html	meta.m20	M123456789M123456789
200	1133987098	http://site/test1nb.html	meta.m30	M123456789M123456789M123456789
200	1133987098	http://site/test1nb.html	title	TTT TTT TTT TTT
SQL>SQL>'SELECT url FROM url WHERE url='http://site/''
http://site/
SQL>
