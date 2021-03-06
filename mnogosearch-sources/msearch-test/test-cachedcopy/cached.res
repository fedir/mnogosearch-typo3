Content-type: text/html; charset=iso-8859-1

Size: 18042
Content-Type: text/xml
Document:
<sect1 id="charset">
	<title>Character sets
<indexterm><primary>Charsets</primary></indexterm>
</title>
	<sect2 id="supcharsets">
		<title>Supported character sets</title>
		<para><application>mnoGoSearch</application> supports almost all known 8 bit
character sets as well as some multi-byte charsets including Korean
euc-kr, Chinese big5 and gb2312, Japanese shift-jis, euc-jp and iso-2022-jp, as well as
utf8. Some multi-byte character sets are not
supported by default, because the conversion tables for them are
rather large that leads to increase of the executable files
size. See <filename>configure</filename> parameters to enable support
for these charsets.</para>

		<para>mnoGoSearch also supports the following
Macintosh chatacter sets: MacCE, MacCroatian, MacGreek, MacRoman,
MacTurkish, MacIceland, MacRomania, MacThai, MacArabic, MacHebrew,
MacCyrillic, MacGujarati.</para>

	</sect2>
	<sect2 id="charset-onedb">
		<title>Several languages in one database</title>
		<para>It is often necessary to deal with several
languages simultaneously. Number of supported languages depends on
choice of character set that <application>mnoGoSearch</application>
will use to store data. Character set is specified with
<command>LocalCharset</command> command.</para>

	</sect2>
	<sect2 id="charset-utf8">
		<title>UTF-8 mode</title>
		<para>When <literal>UTF-8</literal> is specified in
<command>LocalCharset</command> command, you may work with any
languages supported in <ulink
			      url="http://www.unicode.org/">Unicode</ulink>. That means you may use any number of over 650 languages. However, using UTF-8 may consume large amount of disk space (up to twice for some languages), leading to slower indexation and search.</para>

	</sect2>
	<sect2 id="charset-nonutf">
		<title>non-UTF-8 mode</title>
		<para>Since every character set includes latin
characters, any character set supports at least two languages -
English and one or more other languages. <literal>US-ASCII</literal>
is an exception - it supports only Latin characters.</para>

		<note>
			<para>When using
<application>mnoGoSearch</application> in standard (non-UTF-8) mode,
you may use as many languages as you like if they all belong to same
language group.</para>

		</note>
		<table>
			<title>Language groups</title>
			<tgroup cols="3">
				<tbody>
					<row>
						<entry>Language group</entry>
						<entry>Languages</entry>
						<entry>Character sets</entry>
					</row>
					<row>
						<entry>Group 1</entry>
						<entry>Western Europe:
Albanian, Catalan, Danish, Dutch, English, Faeroese, Finnish, French,
Galician, German, Icelandic, Italian, Norwegian, Portuguese, Spanish,
Swedish</entry>

						<entry>ASCII 8, CP437,
CP850, CP860, CP1252, ISO 8859-1, ISO 8859-15, MacRoman,
MacIceland</entry>

					</row>
					<row>
						<entry>Group 2</entry>
						<entry>Eastern Europe:
Croatian, Czech, Estonian, Hungarian, Latvian, Lithuanian, Polish,
Romanian, Slovak, Slovene</entry>

						<entry>CP852, CP1250, ISO 8859-2, MacCentralEurope, MacRomania, MacCroatian</entry>
					</row>
					<row>
						<entry>Group 4</entry>
						<entry>Baltic</entry>
						<entry>CP1257, iso-8859-4, iso-8859-13</entry>
					</row>
					<row>
						<entry>Group 5</entry>
						<entry>Cyrillic: Bulgarian, Byelorussian, Macedonian, Russian, Serbian, Ukrainian</entry>
						<entry>CP855, CP866, CP1251, ISO 8859-5, Koi8-r, Koi8-u, MacCyrillic</entry>
					</row>
					<row>
						<entry>Group 6</entry>
						<entry>Arabic</entry>
						<entry>CP864, CP1256, ISO 8859-6, MacArabic</entry>
					</row>
					<row>
						<entry>Group 7</entry>
						<entry>Greek</entry>
						<entry>CP869, CP1253, ISO 8859-7, MacGreek</entry>
					</row>
					<row>
						<entry>Group 8</entry>
						<entry>Hebrew</entry>
						<entry>CP1255, ISO 8859-8, MacHebrew</entry>
					</row>
					<row>
						<entry>Group 9</entry>
						<entry>Turkish</entry>
						<entry>CP857, CP1254, ISO 8859-9, MacTurkish</entry>
					</row>
					<row>
						<entry>Group 101</entry>
						<entry>Japanese</entry>
						<entry>Shift-JIS, EUC-JP, ISO-2022-JP</entry>
					</row>
					<row>
						<entry>Group 102</entry>
						<entry>Simplified Chinese (PRC)</entry>
						<entry>EUC-GB</entry>
					</row>
					<row>
						<entry>Group 103</entry>
						<entry>Traditional Chinese (ROC)</entry>
						<entry>Big 5</entry>
					</row>
					<row>
						<entry>Group 104</entry>
						<entry>Korean</entry>
						<entry>EUC-KR</entry>
					</row>
					<row>
						<entry>Group 105</entry>
						<entry>Thai</entry>
						<entry>CP874, TIS 620, MacThai</entry>
					</row>
					<row>
						<entry>Group 106</entry>
						<entry>Vietnamese</entry>
						<entry>CP1258</entry>
					</row>
					<row>
						<entry>Group 107</entry>
						<entry>Indian</entry>
						<entry>MacGujarati</entry>
					</row>
					<row>
						<entry>Group 108</entry>
						<entry>Georgian</entry>
						<entry>geostd8</entry>
					</row>
					<row>
						<entry>Unicode</entry>
						<entry>Over 650 languages</entry>
						<entry>UTF-8 (Unicode)</entry>
					</row>
				</tbody>
			</tgroup>
		</table>
		<para>E.g. in case you search engine is configured to
use <command>LocalCharset</command> from the 5th group (Cyrillic), you
may index servers containing documents in Bulgarian, Byelorussian,
Macedonian, Russian, Serbian and Ukrainian. Indexing a multi-language
document in UTF-8 is possible as well; however
<literal>indexer</literal> will extract and save only cyrillic content
from the page. To provide support for over 650 languages, please use
<command>LocalCharset utf-8</command> command.</para>

	</sect2>
	<sect2 id="recoding">
		<title>Recoding</title>
		<para>
			<literal>indexer</literal> recodes all
documents to the character set specified in the
<command>LocalCharset</command>
			<filename>indexer.conf</filename>
command. Internally recoding is implemented using Unicode. Please note
that some recoding may loose some data. For example, recoding between
any Greek and Russian charsets looses all national characters. This
does not matter for a single language sites. If you want to build
multi-lingual search engine use UTF8 character set as a
<command>LocalCharset</command>.</para>

	</sect2>
	<sect2 id="charset-searchdec">
		<title>Recoding at search time</title>
		<para>You may use <command>BrowserCharset</command>
command to choose a charset which will be used to display search
results. <command>BrowserCharset</command> may differ from
<command>LocalCharset</command>.</para>

	</sect2>
	<sect2 id="charsetsalias">
		<title>Character sets aliases</title>
		<para>Each charset is recognized by a number of its
aliases. Web servers can return the same charset in different
notation. For example, iso-8859-2, iso8859-2, latin2 are the same
charsets. There is support for charsets names aliases which search
engine can understand:</para>

		<table>
			<title>Charsets aliases</title>
			<tgroup cols="2">
				<tbody>
					<row>
						<entry>iso-8859-1:</entry>
						<entry>
              CP819, CSISOLATIN, IBM819, ISO-8859-1, ISO-IR-100, ISO_8859-1, ISO_8859-1:1987, L1, LATIN1
            </entry>
					</row>
					<row>
						<entry>iso-8859-10:</entry>
						<entry>
              CSISOLATIN6, ISO-8859-10, ISO-IR-157, ISO_8859-10, ISO_8859-10:1992, L6, LATIN6
            </entry>
					</row>
					<row>
						<entry>iso-8859-11:</entry>
						<entry>
              ISO-8859-11, TIS-620, TIS620, TACTIS
            </entry>
					</row>
					<row>
						<entry>iso-8869-13:</entry>
						<entry>
              ISO-8859-13, ISO-IR-179, ISO_8859-13, L7, LATIN7
            </entry>
					</row>
					<row>
						<entry>iso-8859-14:</entry>
						<entry>
              ISO-8859-14, ISO-IR-199, ISO_8859-14, ISO_8859-14:1998, L8, LATIN8
            </entry>
					</row>
					<row>
						<entry>iso-8859-15:</entry>
						<entry>
              ISO-8859-15, ISO-IR-203, ISO_8859-15, ISO_8859-15:1998
            </entry>
					</row>
					<row>
						<entry>iso-8859-16:</entry>
						<entry>
              ISO-8859-16, ISO-IR-226, ISO_8859-16, ISO_8859-16:2000
            </entry>
					</row>
					<row>
						<entry>iso-8859-2:</entry>
						<entry>
              CSISOLATIN2, ISO-8859-2, ISO-IR-101, ISO_8859-2, ISO_8859-2:1987, L2, LATIN2
            </entry>
					</row>
					<row>
						<entry>iso-8859-3:</entry>
						<entry>
              CSISOLATIN3, ISO-8859-3, ISO-IR-109, ISO_8859-3, ISO_8859-3:1988, L3, LATIN3
            </entry>
					</row>
					<row>
						<entry>iso-8859-4:</entry>
						<entry>
              CSISOLATIN4, ISO-8859-4, ISO-IR-110, ISO_8859-4, ISO_8859-4:1988, L4, LATIN4
            </entry>
					</row>
					<row>
						<entry>iso-8859-5:</entry>
						<entry>CSISOLATINCYRILLIC, CYRILLIC, ISO-8859-5, ISO-IR-144, ISO_8859-5, ISO_8859-5:1988</entry>
					</row>
					<row>
						<entry>iso-8859-6:</entry>
						<entry>
              ARABIC, ASMO-708, CSISOLATINARABIC, ECMA-114, ISO-8859-6, ISO-IR-127, ISO_8859-6, ISO_8859-6:1987
            </entry>
					</row>
					<row>
						<entry>iso-8859-7:</entry>
						<entry>
              CSISOLATINGREEK, ECMA-118, ELOT_928, GREEK, GREEK8, ISO-8859-7, ISO-IR-126, ISO_8859-7, ISO_8859-7:1987
            </entry>
					</row>
					<row>
						<entry>iso-8859-8:</entry>
						<entry>
              CSISOLATINHEBREW, HEBREW, ISO-8859-8, ISO-IR-138, ISO_8859-8, ISO_8859-8:1988
            </entry>
					</row>
					<row>
						<entry>iso-8859-9:</entry>
						<entry>
              CSISOLATIN5, ISO-8859-9, ISO-IR-148, ISO_8859-9, ISO_8859-9:1989, L5, LATIN5
            </entry>
					</row>
					<row>
						<entry>armscii-8:</entry>
						<entry>
              ARMSCII-8
            </entry>
					</row>
					<row>
						<entry>big5:</entry>
						<entry>
              BIG-5, BIG-FIVE, BIG5, BIGFIVE, CN-BIG5, CSBIG5
            </entry>
					</row>
					<row>
						<entry>cp1250:</entry>
						<entry>
              CP1250, MS-EE, WINDOWS-1250
            </entry>
					</row>
					<row>
						<entry>cp1251:</entry>
						<entry>
              CP1251, MS-CYRL, WINDOWS-1251
            </entry>
					</row>
					<row>
						<entry>cp1252:</entry>
						<entry>
              CP1252, MS-ANSI, WINDOWS-1252
            </entry>
					</row>
					<row>
						<entry>cp1253:</entry>
						<entry>
              CP1253, MS-GREEK, WINDOWS-1253
            </entry>
					</row>
					<row>
						<entry>cp1254:</entry>
						<entry>
              CP1254, MS-TURK, WINDOWS-1254
            </entry>
					</row>
					<row>
						<entry>cp1255:</entry>
						<entry>
              CP1255, MS-HEBR, WINDOWS-1255
            </entry>
					</row>
					<row>
						<entry>cp1256:</entry>
						<entry>
              CP1256, MS-ARAB, WINDOWS-1256
            </entry>
					</row>
					<row>
						<entry>cp1257:</entry>
						<entry>
              CP1257, WINBALTRIM, WINDOWS-1257
            </entry>
					</row>
					<row>
						<entry>cp1258:</entry>
						<entry>
              CP1258, WINDOWS-1258
            </entry>
					</row>
					<row>
						<entry>cp437:</entry>
						<entry>
              437, CP437, IBM437
            </entry>
					</row>
					<row>
						<entry>cp850:</entry>
						<entry>
              850, CP850, CSPC850MULTILINGUAL, IBM850
            </entry>
					</row>
					<row>
						<entry>cp852:</entry>
						<entry>
              852, CP852, IBM852
            </entry>
					</row>
					<row>
						<entry>cp855:</entry>
						<entry>
              855, CP855, IBM855
            </entry>
					</row>
					<row>
						<entry>cp857:</entry>
						<entry>
              857, CP857, IBM857
            </entry>
					</row>
					<row>
						<entry>cp860:</entry>
						<entry>
              860, CP860, IBM860
            </entry>
					</row>
					<row>
						<entry>cp861:</entry>
						<entry>
              861, CP861, IBM861
            </entry>
					</row>
					<row>
						<entry>cp862:</entry>
						<entry>
              862, CP862, IBM862
            </entry>
					</row>
					<row>
						<entry>cp863:</entry>
						<entry>
              863, CP863, IBM863
            </entry>
					</row>
					<row>
						<entry>cp864:</entry>
						<entry>
              864, CP864, IBM864
            </entry>
					</row>
					<row>
						<entry>cp865:</entry>
						<entry>
              865, CP865, IBM865
            </entry>
					</row>
					<row>
						<entry>cp866:</entry>
						<entry>
              866, CP866, CSIBM866, IBM866
            </entry>
					</row>
					<row>
						<entry>cp869:</entry>
						<entry>
              869, CP869, IBM869, CP874, WINDOWS-874
            </entry>
					</row>
					<row>
						<entry>euc-kr:</entry>
						<entry>
              CSEUCKR, EUC-KR, EUCKR
            </entry>
					</row>
					<row>
						<entry>gb2312:</entry>
						<entry>
              CHINESE, CSGB2312, CSISO58GB231280, GB2312, GB_2312-80, ISO-IR-58
            </entry>
					</row>
					<row>
						<entry>koi8-r:</entry>
						<entry>
              CSKOI8R, KOI8-R
            </entry>
					</row>
					<row>
						<entry>koi8-u</entry>
						<entry>
              KOI8-U
            </entry>
					</row>
					<row>
						<entry>shift-jis:</entry>
						<entry>
              CSSHIFTJIS, MS_KANJI, S-JIS, SHIFT-JIS, SHIFT_JIS, SJIS
            </entry>
					</row>
					<row>
						<entry>cp367:</entry>
						<entry>
              ANSI_X3.4-1968, ASCII, CP367, CSASCII, IBM367, ISO-IR-6, ISO646-US, ISO_646.IRV:1991, US, US-ASCII
            </entry>
					</row>
					<row>
						<entry>utf8:</entry>
						<entry>
              UTF-8, UTF8
            </entry>
					</row>
					<row>
						<entry>viscii:</entry>
						<entry>
              CSVISCII, VISCII, VISCII1.1-1
            </entry>
					</row>
					<row>
						<entry>maccyrillic:</entry>
						<entry>
              MACCYRILLIC, X-MAC-CYRILLIC
            </entry>
					</row>
					<row>
						<entry>macroman:</entry>
						<entry>
             MACROMAN, MACINTOSH, CSMACINTOSH,  MAC
            </entry>
					</row>
					<row>
						<entry>MacCentralEurope:</entry>
						<entry>
             MACCENTRALEUROPE, MACCE 
            </entry>
					</row>
				</tbody>
			</tgroup>
		</table>
	</sect2>
	<sect2 id="charsetdetect">
		<title>Document charset detection</title>
		<para>indexer detects document character set in this order:</para>
		<orderedlist>
			<listitem>
				<para>&#34;Content-type: <b>text</b>/html; charset=xxx&#34;</para>
			</listitem>
			<listitem>
				<para>&#60;META NAME=&#34;Content-Type&#34; CONTENT=&#34;<b>text</b>/html; charset=xxx&#34;&#62;</para>
<para><indexterm><primary>Command</primary><secondary>GuesserUseMeta</secondary></indexterm>
Selection of this variant may be switch off by command: <command>GuesserUseMeta no</command> in your
<filename>indexer.conf</filename>.</para>
			</listitem>
			<listitem>
				<para>Defaults from &#34;Charset&#34; field in Common Parameters</para>
			</listitem>
		</orderedlist>
	</sect2>
	<sect2 id="charset-guesser">
		<title>Automatic charset guesser</title>
		<para>Since 3.2.0 <application>mnoGoSearch</application> has an automatic charset
and language guesser. It currently recognizes more than 100 various
charsets and languages. Charset and language detection  is implemented
using &#34;N-Gram-Based <b>Text</b> Categorization&#34; technique. There is a number
of so called &#34;language map&#34; files, one for each language-charset
pair. They are installed under
<filename>/usr/local/mnogosearch/etc/langmap/</filename> directory by
default. Take a look there to check the list of currently provided
charset-language pairs. Guesser works fine for texts bigger than 500
characters. Shorter texts may not be guessed well.</para>

<sect3 id="mguesser">
<title>Build your own language maps</title>
<para>To build your own language map use <indexterm><primary>mguesser</primary></indexterm><literal>mguesser</literal>
utility. In addition, your need to collect file with language samples in charset desired. For new language map creattion,
use the following command:
<programlisting>
        mguesser -p -c charset -l language &#60; FILENAME &#62; language.charset.lm
</programlisting>
</para>
<para>You can also use <literal>mguesser</literal> utility for guessing document's language and charset by exsisting 
language maps. To do this, use following command:
<programlisting>
        mguesser [-n maxhits] &#60; FILENAME
</programlisting>
</para>

<para>For some languages, it may be used few different charset. To convert from one charset supported by 
<application>mnoGoSearch</application> to another, use <indexterm><primary>mconv</primary></indexterm><literal>mconv</literal>
utility.
<programlisting>
        mconv [OPTIONS] -f charset_from -t charset_to [configfile] &#60; infile &#62; outfile
</programlisting>
</para>

<para>By default, both <literal>mguesser</literal> and <literal>mconv</literal> utilities is installed into
<filename>/usr/local/mnoogosearch/sbin/</filename> directory.
</para>
</sect3>

<para><indexterm><primary>Command</primary><secondary>LangMapUpdate</secondary></indexterm>
Since version 3.2.14, <application>mnoGoSearch</application> has an ability to update language and charset maps
automaticaly while indexing, if remote server supply with pages exactly specified language and charset.
To enable this function, specify command
<programlisting>
LangMapUpdate yes
</programlisting>
in your <filename>indexer.conf</filename> file.
</para>

	</sect2>


	<sect2 id="defcharset">
		<title>Default charset
<indexterm><primary>Command</primary><secondary>Charset</secondary></indexterm>
</title>
		<para>Use Charset <filename>indexer.conf</filename> command to choose the default charset of indexed servers.</para>
	</sect2>
	<sect2 id="deflang">
		<title>Default Language
<indexterm><primary>Command</primary><secondary>DefaultLang</secondary></indexterm>
</title>
		<para>You can set default language for Servers by using <varname>DefaultLang</varname>
			<filename>indexer.conf</filename> variable. This is useful while restricting search by URL language.</para>
	</sect2>
</sect1>

######################
Content-type: text/html; charset=iso-8859-1

Size: 211
Content-Type: text/plain
Document:
<pre>
This is a <b>text</b> file!

A typical HTML file looks like this:

&#60;html&#62;
&#60;head&#62;
&#60;title&#62;test1 title&#60;/title&#62;
&#60;meta name=&#34;Content-Type&#34; content=&#34;<b>text</b>/plain; charset=latin1&#34;&#62;
&#60;/head&#62;
&#60;body&#62;
This is a body
&#60;/body&#62;
&#60;/html&#62;
</pre>

######################
Content-type: text/html; charset=iso-8859-1

Size: 
Content-Type: 
Document:

######################
