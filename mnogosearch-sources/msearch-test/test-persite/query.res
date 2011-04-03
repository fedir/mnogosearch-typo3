SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
this	16777217	-1574177844	http://site0/site0.page2.html
is	16777218	-1574177844	http://site0/site0.page2.html
the	16777219	-1574177844	http://site0/site0.page2.html
second	16777220	-1574177844	http://site0/site0.page2.html
test	16777221	-1574177844	http://site0/site0.page2.html
page	16777222	-1574177844	http://site0/site0.page2.html
222	16777223	-1574177844	http://site0/site0.page2.html
test	33554433	-1574177844	http://site0/site0.page2.html
2	33554434	-1574177844	http://site0/site0.page2.html
title	33554435	-1574177844	http://site0/site0.page2.html
this	16777217	-622938870	http://site1/site1.page3.html
is	16777218	-622938870	http://site1/site1.page3.html
the	16777219	-622938870	http://site1/site1.page3.html
third	16777220	-622938870	http://site1/site1.page3.html
test	16777221	-622938870	http://site1/site1.page3.html
page	16777222	-622938870	http://site1/site1.page3.html
333	16777223	-622938870	http://site1/site1.page3.html
333	16777224	-622938870	http://site1/site1.page3.html
test	33554433	-622938870	http://site1/site1.page3.html
3	33554434	-622938870	http://site1/site1.page3.html
title	33554435	-622938870	http://site1/site1.page3.html
this	16777217	435923780	http://site1/site1.page4.html
the	16777218	435923780	http://site1/site1.page4.html
fourth	16777219	435923780	http://site1/site1.page4.html
test	16777220	435923780	http://site1/site1.page4.html
page	16777221	435923780	http://site1/site1.page4.html
444	16777222	435923780	http://site1/site1.page4.html
test	33554433	435923780	http://site1/site1.page4.html
4	33554434	435923780	http://site1/site1.page4.html
title	33554435	435923780	http://site1/site1.page4.html
this	16777217	1404023774	http://site0/site0.page1.html
is	16777218	1404023774	http://site0/site0.page1.html
the	16777219	1404023774	http://site0/site0.page1.html
first	16777220	1404023774	http://site0/site0.page1.html
test	16777221	1404023774	http://site0/site0.page1.html
page	16777222	1404023774	http://site0/site0.page1.html
111	16777223	1404023774	http://site0/site0.page1.html
test	33554433	1404023774	http://site0/site0.page1.html
1	33554434	1404023774	http://site0/site0.page1.html
title	33554435	1404023774	http://site0/site0.page1.html
this	16777217	2034095681	http://site1/site1.page5.html
the	16777218	2034095681	http://site1/site1.page5.html
fifth	16777219	2034095681	http://site1/site1.page5.html
test	16777220	2034095681	http://site1/site1.page5.html
page	16777221	2034095681	http://site1/site1.page5.html
555	16777222	2034095681	http://site1/site1.page5.html
test	33554433	2034095681	http://site1/site1.page5.html
5	33554434	2034095681	http://site1/site1.page5.html
title	33554435	2034095681	http://site1/site1.page5.html
SQL>'SELECT status, docsize, hops, crc32, url FROM url WHERE crc32<>0 ORDER BY status, crc32'
200	110	1	-1574177844	http://site0/site0.page2.html
200	118	1	-622938870	http://site1/site1.page3.html
200	114	1	435923780	http://site1/site1.page4.html
200	108	1	1404023774	http://site0/site0.page1.html
200	119	1	2034095681	http://site1/site1.page5.html
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
200	-1574177844	http://site0/site0.page2.html	body	This is the second test page. 222.
200	-1574177844	http://site0/site0.page2.html	title	Test 2 title
200	-622938870	http://site1/site1.page3.html	body	This is the third test page. 333. 333.
200	-622938870	http://site1/site1.page3.html	title	Test 3 title
200	435923780	http://site1/site1.page4.html	body	This the fourth test page. 444.
200	435923780	http://site1/site1.page4.html	title	Test 4 title
200	1404023774	http://site0/site0.page1.html	body	This is the first test page. 111.
200	1404023774	http://site0/site0.page1.html	title	Test 1 title
200	2034095681	http://site1/site1.page5.html	body	This the fifth test page. 555.
200	2034095681	http://site1/site1.page5.html	title	Test 5 title
SQL>'SELECT u1.docsize,u2.crc32,u2.docsize,u1.url,u2.url FROM url u1,url u2, links l WHERE u1.rec_id=l.ot AND u2.rec_id=l.k AND u2.crc32<>0 ORDER BY u1.docsize,u2.crc32,u2.docsize'
196	-1574177844	110	http://site0/	http://site0/site0.page2.html
196	-622938870	118	http://site1/	http://site1/site1.page3.html
196	435923780	114	http://site1/	http://site1/site1.page4.html
196	1404023774	108	http://site0/	http://site0/site0.page1.html
196	2034095681	119	http://site1/	http://site1/site1.page5.html
SQL>'SELECT url FROM url WHERE url='http://site/''
SQL>
