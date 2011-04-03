SQL>'FIELDS=OFF'
SQL>'SELECT dict.word,dict.intag,url.crc32,url.url FROM dict, url WHERE url.rec_id=dict.url_id ORDER BY url.crc32,dict.intag'
colourer	16777217	-2110098218	http://site/colourer.txt
colour	16777217	-1971583867	http://site/colour.txt
colourings	16777217	-1313331084	http://site/colourings.txt
red	16777217	-1256976899	http://site/rgb.txt
green	16777218	-1256976899	http://site/rgb.txt
blue	16777219	-1256976899	http://site/rgb.txt
peacemaking	16777220	-1256976899	http://site/rgb.txt
2006	16777221	-1256976899	http://site/rgb.txt
colouring	16777217	-1027013481	http://site/colouring.txt
colours	16777217	-917762030	http://site/colours.txt
colored	16777217	-506004432	http://site/colored.txt
is	16777217	-171252770	http://site/logfiles.txt
writing	16777218	-171252770	http://site/logfiles.txt
a	16777219	-171252770	http://site/logfiles.txt
log	16777220	-171252770	http://site/logfiles.txt
file	16777221	-171252770	http://site/logfiles.txt
can	16777222	-171252770	http://site/logfiles.txt
almost	16777223	-171252770	http://site/logfiles.txt
certainly	16777224	-171252770	http://site/logfiles.txt
gain	16777225	-171252770	http://site/logfiles.txt
testing	16777226	-171252770	http://site/logfiles.txt
my	16777227	-171252770	http://site/logfiles.txt
phrase	16777228	-171252770	http://site/logfiles.txt
x	16777229	-171252770	http://site/logfiles.txt
x	16777230	-171252770	http://site/logfiles.txt
x	16777231	-171252770	http://site/logfiles.txt
x	16777232	-171252770	http://site/logfiles.txt
x	16777233	-171252770	http://site/logfiles.txt
my	16777234	-171252770	http://site/logfiles.txt
test	16777235	-171252770	http://site/logfiles.txt
phrase	16777236	-171252770	http://site/logfiles.txt
x	16777237	-171252770	http://site/logfiles.txt
x	16777238	-171252770	http://site/logfiles.txt
x	16777239	-171252770	http://site/logfiles.txt
x	16777240	-171252770	http://site/logfiles.txt
x	16777241	-171252770	http://site/logfiles.txt
my	16777242	-171252770	http://site/logfiles.txt
phrase	16777243	-171252770	http://site/logfiles.txt
test	16777244	-171252770	http://site/logfiles.txt
x	16777245	-171252770	http://site/logfiles.txt
x	16777246	-171252770	http://site/logfiles.txt
x	16777247	-171252770	http://site/logfiles.txt
x	16777248	-171252770	http://site/logfiles.txt
x	16777249	-171252770	http://site/logfiles.txt
colors	16777217	-130801002	http://site/colors.txt
colorers	16777217	-80509783	http://site/colorers.txt
colourers	16777217	148858879	http://site/colourers.txt
colorer	16777217	352471393	http://site/colorer.txt
coloring	16777217	824567745	http://site/coloring.txt
color	16777217	1524166490	http://site/color.txt
colorings	16777217	1683657352	http://site/colorings.txt
coloured	16777217	1995358599	http://site/coloured.txt
SQL>'SELECT status, docsize, hops, crc32, url FROM url ORDER BY status, crc32'
200	9	1	-2110098218	http://site/colourer.txt
200	7	1	-1971583867	http://site/colour.txt
200	11	1	-1313331084	http://site/colourings.txt
200	32	1	-1256976899	http://site/rgb.txt
200	10	1	-1027013481	http://site/colouring.txt
200	8	1	-917762030	http://site/colours.txt
200	8	1	-506004432	http://site/colored.txt
200	132	1	-171252770	http://site/logfiles.txt
200	7	1	-130801002	http://site/colors.txt
200	9	1	-80509783	http://site/colorers.txt
200	430	0	0	http://site/
200	10	1	148858879	http://site/colourers.txt
200	8	1	352471393	http://site/colorer.txt
200	9	1	824567745	http://site/coloring.txt
200	6	1	1524166490	http://site/color.txt
200	10	1	1683657352	http://site/colorings.txt
200	9	1	1995358599	http://site/coloured.txt
SQL>'SELECT url.status,url.crc32,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.crc32,urlinfo.sname'
200	-2110098218	http://site/colourer.txt	body	colourer
200	-1971583867	http://site/colour.txt	body	colour
200	-1313331084	http://site/colourings.txt	body	colourings
200	-1256976899	http://site/rgb.txt	body	red green blue peacemaking 2006
200	-1027013481	http://site/colouring.txt	body	colouring
200	-917762030	http://site/colours.txt	body	colours
200	-506004432	http://site/colored.txt	body	colored
200	-171252770	http://site/logfiles.txt	body	is writing a log file can almost certainly gain testing my phrase x x x x x my test phrase x x x x x my phrase test x x x x x
200	-130801002	http://site/colors.txt	body	colors
200	-80509783	http://site/colorers.txt	body	colorers
200	148858879	http://site/colourers.txt	body	colourers
200	352471393	http://site/colorer.txt	body	colorer
200	824567745	http://site/coloring.txt	body	coloring
200	1524166490	http://site/color.txt	body	color
200	1683657352	http://site/colorings.txt	body	colorings
200	1995358599	http://site/coloured.txt	body	coloured
SQL>'SELECT url FROM url WHERE url='http://site/''
http://site/
SQL>
