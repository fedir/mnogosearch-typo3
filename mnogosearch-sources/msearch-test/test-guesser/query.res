SQL>'FIELDS=OFF'
SQL>SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32, docsize'
200	1369	0	-1500644130	http://site/de.latin1.txt
200	1311	0	197289867	http://site/nl.latin1.txt
200	952	0	417228382	http://site/ru.koi8r.txt
200	1615	0	417228382	http://site/ru.utf8.txt
200	1192	0	942507265	http://site/pl.latin2.txt
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
200	-1500644130	http://site/de.latin1.txt	Charset	iso-8859-1
200	-1500644130	http://site/de.latin1.txt	Content-Language	de
200	197289867	http://site/nl.latin1.txt	Charset	iso-8859-1
200	197289867	http://site/nl.latin1.txt	Content-Language	nl
200	417228382	http://site/ru.koi8r.txt	Charset	koi8-r
200	417228382	http://site/ru.koi8r.txt	Content-Language	ru
200	942507265	http://site/pl.latin2.txt	Charset	iso-8859-2
200	942507265	http://site/pl.latin2.txt	Content-Language	pl
SQL>SQL>'SELECT url FROM url WHERE url='http://site/''
SQL>
