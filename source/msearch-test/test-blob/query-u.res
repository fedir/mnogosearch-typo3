SQL>'FIELDS=OFF'
SQL>'SELECT word, intag FROM dict WHERE word='longbody1' ORDER BY intag'
SQL>'SELECT snd, cnt, word FROM wrdstat ORDER BY snd, cnt DESC, word'
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	41	1	-1781601539	http://site/test1.txt
200	147546	1	-1447753567	http://site/test5.html
200	226	1	-1397441923	http://site/test4.html
200	187	0	0	http://site/
200	162	1	87087022	http://site/test3.html
200	224	1	1777461348	http://site/test1.html
200	198	1	1931407332	http://site/test2.html
404	0	2	0	http://site/test1.html?a=b
SQL>'SELECT sname FROM urlinfo ORDER BY sname'
body
body
body
body
body
body
title
title
title
title
title
user.date
user.date
user.date
user.date
SQL>'colflags2=1'
SQL>'SELECT word, secno, intag FROM bdict WHERE word LIKE '#%' AND word <> '#last_mod_time' AND word<>'#rec_id' AND word<>'#ts' AND word<>'#version' AND word<>'#limit#test1' AND word<>'#score#score1cached' AND word<>'#score#score2cached' ORDER BY word'
#site_id	0	0x34FF291734FF291734FF2917
SQL>'SELECT word, secno FROM bdict WHERE word='#last_mod_time''
#last_mod_time	0
SQL>'SELECT word, secno FROM bdict WHERE word='#rec_id''
#rec_id	0
SQL>'SELECT word, secno FROM bdict WHERE word='#ts''
#ts	0
SQL>'SELECT word, secno FROM bdict WHERE word='#version''
#version	0
SQL>'SELECT word, secno FROM bdict WHERE word='#limit#test1''
#limit#test1	0
SQL>'SELECT word, secno FROM bdict WHERE word='#score#score1cached''
#score#score1cached	0
SQL>'SELECT word, secno FROM bdict WHERE word='#score#score2cached''
#score#score2cached	0
SQL>'SELECT word, secno, intag FROM bdict WHERE word >= '0' AND word <='9999' AND word NOT LIKE '#%' ORDER BY word, secno'
1	2	0x02000000020202
650	1	0x02000000020D02
SQL>'SELECT word, secno, intag FROM bdict WHERE word >= 'a' AND word <='zzzz' AND word NOT LIKE '#%' ORDER BY word, secno'
a	1	0x03000000020306
available	1	0x03000000020801
bug	1	0x02000000020C03
file	1	0x03000000020504
first	1	0x0200000002040B
here	1	0x02000000020708
html	6	0x02000000020201
http	9	0x0200000002010103000000020101
is	1	0x020000000302060703000000020207
no	1	0x03000000020603
one	1	0x02000000020B04
page	1	0x02000000020609
second	1	0x02000000020A05
site	8	0x0200000002010103000000020101
test	1	0x0200000003050901
test	2	0x02000000020103
test1	6	0x0200000002010203000000020102
text	1	0x03000000020405
the	1	0x0200000003030606
this	1	0x0200000002010E03000000020108
title	1	0x03000000020702
title	2	0x02000000020301
txt	6	0x03000000020201
SQL>
