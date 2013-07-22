SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
&#259;&#257;	16777219	-1115386959	http://site/test.ppt
&#267;&#269;	16777220	-1115386959	http://site/test.ppt
&#273;&#273;	16777221	-1115386959	http://site/test.ppt
&#365;&#367;	16777222	-1115386959	http://site/test.ppt
&#382;&#382;	16777223	-1115386959	http://site/test.ppt
&#1377;&#1378;	16777226	-1115386959	http://site/test.ppt
&#1379;&#1380;	16777227	-1115386959	http://site/test.ppt
&#1381;&#1382;	16777228	-1115386959	http://site/test.ppt
&#945;&#946;	16777229	-1115386959	http://site/test.ppt
&#947;&#948;	16777230	-1115386959	http://site/test.ppt
&#945;&#946;	16777231	-1115386959	http://site/test.ppt
&#947;&#948;	16777232	-1115386959	http://site/test.ppt
&#12528;&#12529;	16777233	-1115386959	http://site/test.ppt
&#12530;&#12531;	16777234	-1115386959	http://site/test.ppt
&#12532;&#12527;	16777235	-1115386959	http://site/test.ppt
&#20717;&#20685;	16777236	-1115386959	http://site/test.ppt
&#20653;&#20621;	16777237	-1115386959	http://site/test.ppt
&#20525;&#20493;	16777238	-1115386959	http://site/test.ppt
created	16777239	-1115386959	http://site/test.ppt
with	16777240	-1115386959	http://site/test.ppt
ppthtml	16777241	-1115386959	http://site/test.ppt
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	74752	0	-1115386959	http://site/test.ppt
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
200	-1115386959	http://site/test.ppt	body	&#1058;&#1077;&#1089;&#1090;&#1086;&#1074;&#1099;&#1081; &#1090;&#1080;&#1090;&#1091;&#1083; &#258;&#256; &#266;&#268; &#272;&#273; &#364;&#366; &#381;&#382; &#1058;&#1077;&#1089;&#1090;&#1086;&#1074;&#1099;&#1081; &#1058;&#1077;&#1082;&#1089;&#1090; &#1329;&#1330; &#1331;&#1332; &#1333;&#1334; &#913;&#914; &#915;&#916; &#945;&#946; &#947;&#948; &#12528;&#12529; &#12530;&#12531; &#12532;&#12527; &#20717;&#20685; &#20653;&#20621; &#20525;&#20493; Created with pptHtml
200	-1115386959	http://site/test.ppt	charset	utf-8
SQL>
