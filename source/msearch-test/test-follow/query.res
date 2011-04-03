SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
ss3	16777217	-1348504385	http://site3/test1.html
b3	16777218	-1348504385	http://site3/test1.html
site3	16777219	-1348504385	http://site3/test1.html
site4	16777220	-1348504385	http://site3/test1.html
s3	33554433	-1348504385	http://site3/test1.html
t3	33554434	-1348504385	http://site3/test1.html
ss2	16777217	1279628532	http://site2/test.html
b2	16777218	1279628532	http://site2/test.html
site1	16777219	1279628532	http://site2/test.html
s2	33554433	1279628532	http://site2/test.html
t2	33554434	1279628532	http://site2/test.html
site4	16777217	1282642279	http://site1/test.html
ss1	16777218	1282642279	http://site1/test.html
bb1	16777219	1282642279	http://site1/test.html
site2	16777220	1282642279	http://site1/test.html
s1	33554433	1282642279	http://site1/test.html
t1	33554434	1282642279	http://site1/test.html
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32, hops, docsize'
200	154	1	-1348504385	http://site3/test1.html
200	77	0	0	http://site1/
200	78	0	0	http://site3/
200	77	2	0	http://site2/
200	109	3	1279628532	http://site2/test.html
200	159	1	1282642279	http://site1/test.html
404	0	2	0	http://site3/test/
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
200	-1348504385	http://site3/test1.html	body	Ss3 b3. Site3. Site4.
200	-1348504385	http://site3/test1.html	title	S3 t3
200	1279628532	http://site2/test.html	body	Ss2 b2. Site1.
200	1279628532	http://site2/test.html	title	S2 t2
200	1282642279	http://site1/test.html	body	Site4 Ss1 bb1. Site2.
200	1282642279	http://site1/test.html	title	S1 t1
SQL>'SELECT rec_id,url,ordre,parent FROM server ORDER BY rec_id'
439916271	http://site1/	3	0
568538817	http://site3/	3	1439877540
1439877540	http://site3/*	3	0
1563876320	http://site2/	3	439916271
SQL>'SELECT * FROM srvinfo ORDER BY srv_id, sname'
439916271	DetectClones	0
439916271	Follow	3
1439877540	DetectClones	0
1439877540	Follow	2
1439877540	Match_type	5
SQL>
