<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
   
  <!-- INCLUDE was broken in 3.2.32 -->
fail !0 exec $(SEARCH) > $(UDM_TEST_DIR)/search.rej
fail !0 mdiff $(UDM_TEST_DIR)/search.rej $(UDM_TEST_DIR)/search.res
pass 0 exec rm -f $(UDM_TEST_DIR)/search.rej

  
  <!--  bug610: search.cgi crashes displaying  when var is empty -->
  <a href="/search?q=">test</a>
  <!--  /bug610 -->

  <!-- Check that HTML and URL encoding works in comments -->
  
  <!-- " &#34; %22 -->
  
  
  
  a=abcd b=abcd
  <OPTION NAME="a" value="abcd" selected="selected">a==abcd</OPTION>
  <OPTION NAME="a" value="abce">a!=abcd</OPTION>
  <OPTION NAME="a" value="abcd" selected="selected">a==abcd</OPTION>
  <OPTION NAME="a" value="abce">a!=abcd</OPTION>
    
  
  test11
  
  
  
  test21
  
  
  <!-- The second condition is true -->
  
  test222
  
  
  
   
   test34
   
  

  
  test41
  
  
  
  
  
  test51
  
  
  <!-- Don't process SELECTED written without a variable -->
  <OPTION value="a" SELECTED>
  
  <OPTION value="a" SELECTED>
  
  <!-- Check HTML encoding for dangerous characters -->
  
  &#38; &#38; &#34; &#60; &#62;
  
  <!-- Check functions -->
  
  bcd
  Abcd
  a
  abcd
  
  <!-- Case sensitive comparison -->
  
  
  a
  
  <!-- URLDECODE -->
  
  
  http://Alpha?url=http://Beta/
  
  <!-- Bug#718 incorrect parsing of XHTML isolated tags in search templates -->
  <!-- Check that there is no space between slash and closing lt -->
  <INPUT>
  <INPUT />
  <INPUT TYPE="TEXT" NAME="q" value="" />
  
  <!-- HTMLENCODE -->
  
  a='test &<>" sgml &<>" encode &<>"'
  a='test &amp;&lt;&gt;&quot; sgml &amp;&lt;&gt;&quot; encode &amp;&lt;&gt;&quot;'
  
  <!-- Numeric operations -->
  
  
  200
  
  30
  
  -10
  
  
  1 le 1
  1 lt 2
  1 not le 0
  1 not lt 0
  1 ge 1
  1 gt 0
  1 not ge 2
  1 not gt 2

  <!-- Regular expressions -->
  a=http www.host.com path/file.ext

<!-- FOR loop -->
10
11
12
13
14
15
16
17
18
19
20


<!-- Nested FOR loop -->
10:
 50 51 52 53 54 55 56 57 58 59 60
11:
 50 51 52 53 54 55 56 57 58 59 60
12:
 50 51 52 53 54 55 56 57 58 59 60
13:
 50 51 52 53 54 55 56 57 58 59 60
14:
 50 51 52 53 54 55 56 57 58 59 60
15:
 50 51 52 53 54 55 56 57 58 59 60
16:
 50 51 52 53 54 55 56 57 58 59 60
17:
 50 51 52 53 54 55 56 57 58 59 60
18:
 50 51 52 53 54 55 56 57 58 59 60
19:
 50 51 52 53 54 55 56 57 58 59 60
20:
 50 51 52 53 54 55 56 57 58 59 60


<!-- FOR loop with variables -->


a=80
a=81
a=82
a=83
a=84
a=85
a=86
a=87
a=88
a=89
a=90

  
<!-- bug#1010: check that "<" and ">" work insuide RESULT -->

a<br>b
<tag/>
<!-- /bug#1010 -->

<!-- bug#708,part2 -->

<!-- check that <!XXX> commands are ignored inside a comment -->
<!--
<!SET NAME="a" CONTENT="yyy">
<!SET NAME="a" CONTENT="zzz">
-->
xxx
<!-- /bug#708,part2 -->

<!-- base64 format -->

aHR0cDovL3NvbWUudXJsLmNvbQ==

<!-- left and right format tests -->

'1 &#34; &#1023; &#43707; 1234'
'1 &#34; &#1023; &#43707; 1234'
'1...'
'1 ...'
'1 ...'
'1 ...'
'1 ...'
'1 ...'
'1 ...'
'1 &#34;...'
'1 &#34; ...'
'1 &#34; ...'
'1 &#34; ...'
'1 &#34; ...'
'1 &#34; ...'
'1 &#34; ...'
'1 &#34; ...'
'1 &#34; &#1023;...'
'1 &#34; &#1023; ...'
'1 &#34; &#1023; ...'
'1 &#34; &#1023; ...'
'1 &#34; &#1023; ...'
'1 &#34; &#1023; ...'
'1 &#34; &#1023; ...'
'1 &#34; &#1023; ...'
'1 &#34; &#1023; ...'
'1 &#34; &#1023; &#43707;...'
'1 &#34; &#1023; &#43707; ...'
'1 &#34; &#1023; &#43707; 1...'
'1 &#34; &#1023; &#43707; 12...'
'1 &#34; &#1023; &#43707; 123...'
'1 &#34; &#1023; &#43707; 1234'
'1 &#34; &#1023; &#43707; 1234'
'1 &#34; &#1023; &#43707; 1234'

'1 &#34; &#1023; &#43707; 1234'
'4'
'34'
'234'
'1234'
' 1234'
'&#43707; 1234'
'&#43707; 1234'
'&#43707; 1234'
'&#43707; 1234'
'&#43707; 1234'
'&#43707; 1234'
'&#43707; 1234'
'&#43707; 1234'
' &#43707; 1234'
'&#1023; &#43707; 1234'
'&#1023; &#43707; 1234'
'&#1023; &#43707; 1234'
'&#1023; &#43707; 1234'
'&#1023; &#43707; 1234'
'&#1023; &#43707; 1234'
'&#1023; &#43707; 1234'
' &#1023; &#43707; 1234'
'&#34; &#1023; &#43707; 1234'
'&#34; &#1023; &#43707; 1234'
'&#34; &#1023; &#43707; 1234'
'&#34; &#1023; &#43707; 1234'
'&#34; &#1023; &#43707; 1234'
'&#34; &#1023; &#43707; 1234'
' &#34; &#1023; &#43707; 1234'
'1 &#34; &#1023; &#43707; 1234'
'1 &#34; &#1023; &#43707; 1234'
'1 &#34; &#1023; &#43707; 1234'
<!-- /left and right formats tests -->

<!-- no trailing ')' character in a variable name -->
$(aaaa
$(aaaa
