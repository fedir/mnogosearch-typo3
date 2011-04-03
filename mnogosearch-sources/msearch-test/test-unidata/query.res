SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
முருகன்	16777217	-1966067369	http://site/tamil.html
அல்லது	16777218	-1966067369	http://site/tamil.html
அழகு	16777219	-1966067369	http://site/tamil.html
திரு	16777220	-1966067369	http://site/tamil.html
வி	16777221	-1966067369	http://site/tamil.html
க	16777222	-1966067369	http://site/tamil.html
உலகு	16777224	-1966067369	http://site/tamil.html
குளிர	16777225	-1966067369	http://site/tamil.html
எமது	16777226	-1966067369	http://site/tamil.html
மதியில்	16777227	-1966067369	http://site/tamil.html
ஒழுகு	16777228	-1966067369	http://site/tamil.html
மமுத	16777229	-1966067369	http://site/tamil.html
கிரணமே	16777230	-1966067369	http://site/tamil.html
உருகு	16777231	-1966067369	http://site/tamil.html
மடிய	16777232	-1966067369	http://site/tamil.html
ரிதய	16777233	-1966067369	http://site/tamil.html
நெகிழ	16777234	-1966067369	http://site/tamil.html
உணர்வி	16777235	-1966067369	http://site/tamil.html
லெழுந	16777236	-1966067369	http://site/tamil.html
லுதயமே	16777237	-1966067369	http://site/tamil.html
கலையு	16777238	-1966067369	http://site/tamil.html
நிறைவு	16777239	-1966067369	http://site/tamil.html
மறிவு	16777240	-1966067369	http://site/tamil.html
முதிர	16777241	-1966067369	http://site/tamil.html
முதிரு	16777242	-1966067369	http://site/tamil.html
மதுர	16777243	-1966067369	http://site/tamil.html
நறவமே	16777244	-1966067369	http://site/tamil.html
கழுவு	16777245	-1966067369	http://site/tamil.html
துகளர்	16777246	-1966067369	http://site/tamil.html
முழுக	16777247	-1966067369	http://site/tamil.html
நெடிய	16777248	-1966067369	http://site/tamil.html
கருணை	16777249	-1966067369	http://site/tamil.html
பெருகு	16777250	-1966067369	http://site/tamil.html
சலதியே	16777251	-1966067369	http://site/tamil.html
அலகில்	16777252	-1966067369	http://site/tamil.html
புவன	16777253	-1966067369	http://site/tamil.html
முடியும்	16777254	-1966067369	http://site/tamil.html
வெளியில்	16777255	-1966067369	http://site/tamil.html
அளியு	16777256	-1966067369	http://site/tamil.html
மொளியி	16777257	-1966067369	http://site/tamil.html
னிலயமே	16777258	-1966067369	http://site/tamil.html
அறிவு	16777259	-1966067369	http://site/tamil.html
ளறிவை	16777260	-1966067369	http://site/tamil.html
யறிவு	16777261	-1966067369	http://site/tamil.html
மவரும்	16777262	-1966067369	http://site/tamil.html
அறிய	16777263	-1966067369	http://site/tamil.html
வரிய	16777264	-1966067369	http://site/tamil.html
பிரமமே	16777265	-1966067369	http://site/tamil.html
மலையின்	16777266	-1966067369	http://site/tamil.html
மகள்கண்	16777267	-1966067369	http://site/tamil.html
மணியை	16777268	-1966067369	http://site/tamil.html
யனைய	16777269	-1966067369	http://site/tamil.html
மதலை	16777270	-1966067369	http://site/tamil.html
வருக	16777271	-1966067369	http://site/tamil.html
வருகவே	16777272	-1966067369	http://site/tamil.html
வளமை	16777273	-1966067369	http://site/tamil.html
தழுவு	16777274	-1966067369	http://site/tamil.html
பரிதி	16777275	-1966067369	http://site/tamil.html
புரியின்	16777276	-1966067369	http://site/tamil.html
மருவு	16777277	-1966067369	http://site/tamil.html
குமரன்	16777278	-1966067369	http://site/tamil.html
வருகவே	16777279	-1966067369	http://site/tamil.html
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	1174	1	-1966067369	http://site/tamil.html
200	78	0	0	http://site/
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
200	-1966067369	http://site/tamil.html	body	முருகன் அல்லது அழகு திரு வி.க. குமரகுருபரர் உலகு குளிர எமது மதியில் ஒழுகு மமுத கிரணமே உருகு மடிய
SQL>SQL>'SELECT url FROM url WHERE url='http://site/''
http://site/
SQL>
