SQL>'FIELDS=OFF'
SQL>'select seed, status, crc32, url from url order by crc32, status, seed'
46	200	0	http://site/test.html
52	200	0	http://site/
12	404	0	http://site/?a=b&c=d
29	404	0	http://site/a%20b.txt
47	404	0	http://site/test/
60	404	0	http://site/?a=/c/
105	404	0	http://site/test.html?c=d
122	404	0	http://site/$-_.+!*(),
130	404	0	http://site/?a=b
134	404	0	http://site/dir/?a=/b/
139	404	0	http://site/port.txt
179	404	0	http://site/%3C%3E%23%25%7B%7D%7C%5C%5E~%5B%5D%60
191	404	0	http://site/%FE.txt
192	404	0	http://site/test?a=/b/
199	404	0	http://site/index.html?PARAMFILTER%3AfunctionName%3Dsearch%7CpageIndex%3D2%7C=2
200	404	0	http://site/?a=%F1
224	404	0	http://site/%FF.txt
240	404	0	http://site/test
251	404	0	http://site/dir/test?a=/b/
140	200	256198999	http://site/b.txt
221	200	919106962	http://site/a.txt
120	200	2117050366	http://site/~.txt
SQL>
