SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
this	16777217	738759967	http://site/test1.txt
is	16777218	738759967	http://site/test1.txt
a	16777219	738759967	http://site/test1.txt
text	16777220	738759967	http://site/test1.txt
file	16777221	738759967	http://site/test1.txt
no	16777222	738759967	http://site/test1.txt
title	16777223	738759967	http://site/test1.txt
available	16777224	738759967	http://site/test1.txt
some	16777225	738759967	http://site/test1.txt
special	16777226	738759967	http://site/test1.txt
characters	16777227	738759967	http://site/test1.txt
let	16777228	738759967	http://site/test1.txt
s	16777229	738759967	http://site/test1.txt
check	16777230	738759967	http://site/test1.txt
how	16777231	738759967	http://site/test1.txt
they	16777232	738759967	http://site/test1.txt
get	16777233	738759967	http://site/test1.txt
escaped	16777234	738759967	http://site/test1.txt
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	77	0	0	http://site/
200	149	1	738759967	http://site/test1.txt
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
200	738759967	http://site/test1.txt	body	This is a text file. No title available. Some special characters: &#34; '. ' . ' . ' . ' &#34; . &#34; . &#34; . &#34; \ \ \ \ \ Let's check how they get escaped.
SQL>
