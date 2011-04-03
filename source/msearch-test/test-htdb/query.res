SQL>'FIELDS=OFF'
SQL>'SELECT id, body FROM htdb ORDER BY id'
1	a1 a2 a3
2	b1 b2 b3
3	c1 c2 c3
4	<p><br>d1 d2 d3
5	text01 [type1:id1:text11 text12 [type2:id2:text21 text22]] test02 [u:betsy:betsy], [u:pachadan:dan]
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
text01	16777217	-2062301997	htdb:/1/5
type1	16777218	-2062301997	htdb:/1/5
id1	16777219	-2062301997	htdb:/1/5
text11	16777220	-2062301997	htdb:/1/5
text12	16777221	-2062301997	htdb:/1/5
type2	16777222	-2062301997	htdb:/1/5
id2	16777223	-2062301997	htdb:/1/5
text21	16777224	-2062301997	htdb:/1/5
text22	16777225	-2062301997	htdb:/1/5
test02	16777226	-2062301997	htdb:/1/5
u	16777227	-2062301997	htdb:/1/5
betsy	16777228	-2062301997	htdb:/1/5
betsy	16777229	-2062301997	htdb:/1/5
u	16777230	-2062301997	htdb:/1/5
pachadan	16777231	-2062301997	htdb:/1/5
dan	16777232	-2062301997	htdb:/1/5
c1	16777217	-1927680573	htdb:/1/3
c2	16777218	-1927680573	htdb:/1/3
c3	16777219	-1927680573	htdb:/1/3
text01	16777217	-679679406	htdb:/5/5
text11	16777218	-679679406	htdb:/5/5
text12	16777219	-679679406	htdb:/5/5
text21	16777220	-679679406	htdb:/5/5
text22	16777221	-679679406	htdb:/5/5
test02	16777222	-679679406	htdb:/5/5
betsy	16777223	-679679406	htdb:/5/5
dan	16777224	-679679406	htdb:/5/5
a1	16777217	149790906	htdb:/1/1
a2	16777218	149790906	htdb:/1/1
a3	16777219	149790906	htdb:/1/1
b1	16777217	576407647	htdb:/1/2
b2	16777218	576407647	htdb:/1/2
b3	16777219	576407647	htdb:/1/2
p	16777217	771223522	htdb:/1/4
br	16777218	771223522	htdb:/1/4
d1	16777219	771223522	htdb:/1/4
d2	16777220	771223522	htdb:/1/4
d3	16777221	771223522	htdb:/1/4
body43	16777217	1836849640	htdb:/4/1
a1	16777218	1836849640	htdb:/4/1
a2	16777219	1836849640	htdb:/4/1
a3	16777220	1836849640	htdb:/4/1
d1	16777217	2000066965	htdb:/3/4
d2	16777218	2000066965	htdb:/3/4
d3	16777219	2000066965	htdb:/3/4
SQL>'SELECT status, docsize, hops, crc32, last_mod_time, url FROM url WHERE hops>0 ORDER BY status, crc32'
200	99	1	-2062301997	0	htdb:/1/5
200	8	1	-1927680573	0	htdb:/1/3
200	99	1	-679679406	0	htdb:/5/5
200	8	1	149790906	0	htdb:/1/1
200	8	1	576407647	0	htdb:/1/2
200	15	1	771223522	0	htdb:/1/4
200	14	1	1836849640	1173549913	htdb:/4/1
200	15	1	2000066965	0	htdb:/3/4
301	0	1	0	0	htdb:/2/1
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
200	-2062301997	htdb:/1/5	body	text01 [type1:id1:text11 text12 [type2:id2:text21 text22]] test02 [u:betsy:betsy], [u:pachadan:dan]
200	-1927680573	htdb:/1/3	body	c1 c2 c3
200	-679679406	htdb:/5/5	body5	text01 text11 text12 text21 text22 test02 betsy , dan
200	149790906	htdb:/1/1	body	a1 a2 a3
200	576407647	htdb:/1/2	body	b1 b2 b3
200	771223522	htdb:/1/4	body	&#60;p&#62;&#60;br&#62;d1 d2 d3
200	1836849640	htdb:/4/1	body	a1 a2 a3
200	1836849640	htdb:/4/1	body3	body43
200	2000066965	htdb:/3/4	body2	d1 d2 d3
SQL>'SELECT u1.docsize,u2.docsize,u1.url,u2.url FROM url u1,url u2, links l WHERE u1.rec_id=l.ot AND u2.rec_id=l.k ORDER BY u1.docsize,u2.docsize'
SQL>'SELECT url FROM url WHERE url='http://site/''
SQL>
