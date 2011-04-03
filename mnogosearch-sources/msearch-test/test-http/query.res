SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag,dict.word'
html1	16777217	-1316669187	http://site1/test2.html
html2	16777218	-1316669187	http://site1/test2.html
html3	16777219	-1316669187	http://site1/test2.html
html1	16777220	-1316669187	http://site1/test2.html
html2	16777220	-1316669187	http://site1/test2.html
html3	16777220	-1316669187	http://site1/test2.html
text1	16777217	-258172453	http://site1/test1.txt
text2	16777218	-258172453	http://site1/test1.txt
text3	16777219	-258172453	http://site1/test1.txt
text1	16777220	-258172453	http://site1/test1.txt
text2	16777220	-258172453	http://site1/test1.txt
text3	16777220	-258172453	http://site1/test1.txt
test3b1	16777217	1189801760	http://site1/test3a.txt
test3b2	16777218	1189801760	http://site1/test3a.txt
test3b3	16777219	1189801760	http://site1/test3a.txt
test3b1	16777220	1189801760	http://site1/test3a.txt
test3b2	16777220	1189801760	http://site1/test3a.txt
test3b3	16777220	1189801760	http://site1/test3a.txt
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	46	0	-1316669187	http://site1/test2.html
200	18	0	-258172453	http://site1/test1.txt
200	52	0	1189801760	http://site1/test3a.txt
SQL>'SELECT url.status,url.crc32,url.url,url.last_mod_time,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
200	-1316669187	http://site1/test2.html	1157982282	body	html1 html2 html3
200	-258172453	http://site1/test1.txt	1157982282	body	text1 text2 text3
200	1189801760	http://site1/test3a.txt	1157982283	body	test3b1 test3b2 test3b3
SQL>
