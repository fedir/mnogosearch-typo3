SQL>'FIELDS=OFF'
SQL>SQL>'SELECT status, seed, docsize, hops, url FROM url ORDER BY status, seed, docsize'
200	154	14860	0	http://site/sample.docx
200	194	6884	0	http://site/sample2.docx
200	215	6352	0	http://site/sample4-lists.docx
SQL>'SELECT url.status,url.docsize,url.url,urlinfo.sname,urlinfo.sval FROM url,urlinfo WHERE url.rec_id=urlinfo.url_id ORDER BY url.status,url.docsize,urlinfo.sname'
200	6352	http://site/sample4-lists.docx	body	BoldHeader Paragraph 1. Paragraph 2. Paragraph 3. Italic text. OLItem1 OLItem2 OLItem3 Bold test-no-number1 ULIttem1 Bold Italic ULItem2 ULItem3 UItem31 test-no-number2 ULItem4
200	6352	http://site/sample4-lists.docx	cachedcopy	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
200	6884	http://site/sample2.docx	body	&#1056;&#1045;&#1064;&#1045;&#1053;&#1048;&#1045; &#1089;&#1086;&#1073;&#1089;&#1090;&#1074;&#1077;&#1085;&#1085;&#1080;&#1082;&#1072; &#1087;&#1086;&#1084;&#1077;&#1097;&#1077;&#1085;&#1080;&#1103; &#1087;&#1086; &#1074;&#1086;&#1087;&#1088;&#1086; (   /
200	6884	http://site/sample2.docx	cachedcopy	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
200	6884	http://site/sample2.docx	title	&#1056;&#1045;&#1064;&#1045;&#1053;&#1048;&#1045;
200	14860	http://site/sample.docx	body	Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Nunc at risus vel erat tempus posuere. Aenean non ante. Suspendisse vehicula dolor sit amet odio. Sed at sem. Nunc fringilla. Etiam ut diam. Nunc diam neque, adipiscing sed, ultrices a, pulvinar vit
200	14860	http://site/sample.docx	cachedcopy	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
SQL>
----------------
Search for "Nunc at risus".
Search results: <b>nunc : 4 / 4, at : 7 / 7, risus : 3 / 3.
Results 1-1 of 1.
1.
Title=
Boby=... amet, consectetuer adipiscing elit. <b>Nunc</b> <b>at</b> <b>risus</b> vel erat tempus posuere. ... vehicula dolor sit amet odio. Sed <b>at</b> sem. <b>Nunc</b> fringilla. Etiam ut diam.  <b>Nunc</b> diam neque, adipiscing sed, ultrices ...</small>
Content-Length: 14860 bytes
Content-Type: 
Last-Modified: Wed, 15 Aug 2007 06:28:00 GMT
----------------
----------------
Search for "vestibulum".
Search results: <b>vestibulum : 2 / 2.
Results 1-1 of 1.
1.
Title=
Boby=... In sit amet lorem at velit faucibus <b>vestibulum</b>....</small>
Content-Length: 14860 bytes
Content-Type: 
Last-Modified: Wed, 15 Aug 2007 06:28:00 GMT
----------------
----------------
Search for "8".
Search results: <b>8 : 2 / 2.
Results 1-1 of 1.
1.
Title=РЕШЕНИЕ
Boby=... в подвал), другие расходы. В районе <b>8</b> подъезда поднятие асфальтового ...</small>
Content-Length: 6884 bytes
Content-Type: 
Last-Modified: Sat, 24 Nov 2012 07:11:00 GMT
----------------
----------------
Search for "вода".
Search results: <b>вода : 3 / 3.
Results 1-1 of 1.
1.
Title=РЕШЕНИЕ
Boby=... ограждающими панелями. (потери тепла, <b>вода</b> стекает в подвал), другие расходы. В ... канализации. т. к. в подвал стекает <b>вода</b>. Установка светильников с датчиками на ...</small>
Content-Length: 6884 bytes
Content-Type: 
Last-Modified: Sat, 24 Nov 2012 07:11:00 GMT
----------------
Content-type: text/html; charset=utf-8

###################### Cached Copy
Size: 14860
Content-Type: 
Document:

<p>
<span style="white-space:pre-wrap">Lorem ipsum dolor sit amet, consectetuer adipiscing elit. <b>Nunc</b> <b>at</b> <b>risus</b> vel erat tempus posuere. Aenean non ante. Suspendisse vehicula dolor sit amet odio. Sed <b>at</b> sem. <b>Nunc</b> fringilla. Etiam ut diam. <b>Nunc</b> diam neque, adipiscing sed, ultrices a, pulvinar vitae, mauris. Suspendisse <b>at</b> elit vitae quam volutpat dapibus. Phasellus consequat magna in tellus. Mauris mauris dolor, dapibus sed, commodo et, pharetra eget, diam. </span><span style="white-space:pre-wrap">Nullam consequat lacus vitae mi. Sed tortor <b>risus</b>, posuere sed, condimentum pellentesque, pharetra eu, nisl. </span>
<p>
<span style="white-space:pre-wrap">Nullam sapien. Quisque ligula tellus, consectetuer <b>at</b>, elementum eget, interdum vitae, arcu. Aenean tempus. Nulla venenatis pellentesque urna. Quisque tincidunt, augue eget scelerisque sodales, sem elit convallis mi, ut sollicitudin metus turpis a nisi. Vivamus iaculis eleifend quam. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas. Integer tortor lacus, congue in, vehicula eget, suscipit vel, mi. Donec et purus. Sed elementum dapibus neque. </span>
<p>
<span style="white-space:pre-wrap">Fusce blandit, augue eu luctus posuere, mi arcu consequat lorem, non volutpat neque enim a mi. Aliquam nonummy justo in purus. Quisque dolor purus, auctor in, varius non, auctor quis, dolor. Mauris vitae elit. Suspendisse dolor ante, imperdiet id, sodales auctor, tempus mollis, nisl. Donec vehicula tincidunt lectus. Donec cursus. Nam tempor pede eu urna. Praesent nec magna <b>at</b> enim ornare posuere. Fusce pharetra dapibus ligula. </span>Phasellus ultricies mi nec leo. Sed tempus. In sit amet lorem <b>at</b> velit faucibus vestibulum.
###################### End Of Cached Copy
----------------
Search for "bold italic".
Search results: <b>bold : 3 / 3, italic : 3 / 3.
Results 1-1 of 1.
1.
Title=
Boby=... Paragraph 1. Paragraph 2. Paragraph 3. <b>Italic</b> text. OLItem1 OLItem2 OLItem3 <b>Bold</b>  test-no-number1 ULIttem1 <b>Bold</b> <b>Italic</b> ULItem2 ULItem3 UItem31 ...</small>
Content-Length: 6352 bytes
Content-Type: 
Last-Modified: Wed, 15 Aug 2007 06:28:00 GMT
----------------
Content-type: text/html; charset=utf-8

###################### Cached Copy
Size: 6884
Content-Type: 
Document:

<p>
<b><span style="white-space:pre-wrap">                                                            </span></b><b>РЕШЕНИЕ</b>
<p>

<p>
собственника помещения по вопросам повестки общего собрания всех собственников многоквартирного дома № 26 по ул. К. Либкнехта в форме заочного голосования (опроса)
<p>
<span style="white-space:pre-wrap">                                                 </span>( отметьте свое решение любым знаком)
<table border="1">
<tr>
<td>
№<span style="white-space:pre-wrap">/п    </span></td>
<td>
Содержание вопроса повестки собрания</td>
<td>
За</td>
<td>
<span style="white-space:pre-wrap">Против </span></td>
<td>
Воздержался</td>
</tr>
<tr>
<td>
<b>1.</b></td>
<td>
<b><span style="white-space:pre-wrap">  </span></b><b>Утвердить плату за капитальный ремонт общего имущества жилого дома № 26 по ул. К. Либкнехта в размере 2,5 руб. с 1 кв.м. общей площади помещения в месяц. Плату взимать  с 01.01.2013 г.</b></td>
<td>
 </td>
<td>
 </td>
<td>
 </td>
</tr>

</table>

<p>

<p>
_____________________________________________________________________________
<p>

<p>
_____________________________________________________________________________
<p>
<span style="font-size:10">(Ф.И.О члена товарищества, № квартиры, доля собственности)</span>
<p>

<p>
<span style="font-size:10"><span style="white-space:pre-wrap">                                                                                                    </span></span><span style="font-size:10">________________________________________</span>
<p>
<span style="font-size:10">(подпись, дата)</span>
<p>
<b><u><span style="font-size:16"><span style="white-space:pre-wrap">Просьба, после заполнения лист голосования положить в почтовый ящик кв.  </span></span></u></b><b><u><span style="font-size:10"><span style="white-space:pre-wrap">                                            </span></span></u></b>
<p>
<b><u><span style="font-size:14">Время голосования по повестке собрания – две недели.  Последний день учета листков голосования -  20  декабря  2012 года.</span></u></b>
<p>
<span style="font-size:14"><span style="white-space:pre-wrap">                                                 </span></span>Приложение к листу голосования.
<p>
<b><span style="white-space:pre-wrap">                                                      </span></b><b><span style="font-size:16">План капитального ремонта</span></b><b><span style="white-space:pre-wrap">.       </span></b>
<p>
<ul>
<li>
<span style="font-size:14">В связи с аварийным состоянием системы горячего и холодного водоснабжения, средства в первую очередь будут направлены на замену стояков ХВС и ГВС в квартирах, циркуляционных трубопроводах ГВС на 10-х этажах. По причине засоров трубопроводов система ГВС функционирует неудовлетворительно , и жильцам приходиться пропускать воду до нужной температуры , что ведет к перерасходу и дополнительным платежам. Частичная замена аварийных участков также ведет к дополнительным нерациональным расходам.</span>
</li>
</ul>

<p>
<span style="font-size:14"><span style="white-space:pre-wrap">      </span></span><b><span style="font-size:14"><span style="white-space:pre-wrap">Внимание:  </span></span></b><b><u><span style="font-size:14"><span style="white-space:pre-wrap">Собственникам, уже заменившим стояки ГВС, ХВС самостоятельно   </span></span></u></b>
<p>
<b><u><span style="font-size:14"><span style="white-space:pre-wrap">       </span></span></u></b><b><u><span style="font-size:14">за свой счет,  будет произведён перерасчёт .</span></u></b>
<p>
<ul>
<li>
<span style="font-size:14"><span style="white-space:pre-wrap">Ремонт коммуникаций ГВС , ХВС, отопления и канализации подвала. в т. ч. теплоизоляция. </span></span>
</li>
</ul>

<p>
<ul>
<li>
<span style="font-size:14">Ремонт и реконструкция системы ливневой канализации на кровле и в чердачной части т. к. часто случаются затопления во время сильного дождя.</span>
</li>
</ul>

<p>
<ul>
<li>
<span style="font-size:14">Благоустройство и асфальтирование дворовой территории.</span>
</li>
</ul>

<p>
<ul>
<li>
<span style="font-size:14">Устройство пандусов для детских и  инвалидных колясок.</span>
</li>
</ul>

<p>
<ul>
<li>
<span style="font-size:14">Прочистка канализационных стояков с использованием гидродинамических аппаратов.</span>
</li>
</ul>

<p>
<ul>
<li>
<span style="font-size:14">Ремонт входных групп.</span>
</li>
</ul>

<p>
<ul>
<li>
<span style="font-size:14">Общестроительные работы в подвальной части – заделка отверстий под крыльцами и под ограждающими панелями. (потери тепла, <b>вода</b> стекает в подвал), другие расходы.</span>
</li>
</ul>

<p>
<ul>
<li>
<span style="font-size:14">В районе 8 подъезда поднятие асфальтового покрытия и реконструкция ливневой канализации. т. к. в подвал стекает <b>вода</b>.</span>
</li>
</ul>

<p>
<ul>
<li>
<span style="font-size:14">Установка</span><span style="font-size:14"><span style="white-space:pre-wrap"> светильников с датчиками на включение.</span></span>
</li>
</ul>

<p>
<ul>
<li>
<span style="font-size:14">Перенос контейнерной площадки для ТБО на расстояние 20   м. от здания.</span>
</li>
</ul>

<p>
<ul>
<li>
<span style="font-size:14">Строительство автостоянки (будет решаться отдельным собранием).</span>
</li>
</ul>

###################### End Of Cached Copy
Content-type: text/html; charset=utf-8

###################### Cached Copy
Size: 6352
Content-Type: 
Document:

<p>
<b>BoldHeader</b>
<p>
Paragraph 1.
<p>
Paragraph 2.
<p>
<span style="white-space:pre-wrap">Paragraph 3. </span><i><b>Italic</b> text.</i>
<p>
<ul>
<li>
OLItem1
</li>
</ul>

<p>
<ul>
<li>
OLItem2
</li>
</ul>

<p>
<ul><ul>
<li>
<b><span style="white-space:pre-wrap">OLItem3 </span></b><b><b>Bold</b></b>
</li>
</ul></ul>

<p>
test-no-number1
<p>
<ul>
<li>
<b><i><span style="white-space:pre-wrap">ULIttem1 </span></i></b><b><i><b>Bold</b> <b>Italic</b></i></b>
</li>
</ul>

<p>
<ul>
<li>
ULItem2
</li>
</ul>

<p>
<ul><ul>
<li>
ULItem3
</li>
</ul></ul>

<p>
<ul><ul><ul>
<li>
UItem31
</li>
</ul></ul></ul>

<p>
test-no-number2
<p>
<ul><ul>
<li>
ULItem4
</li>
</ul></ul>

<p>

###################### End Of Cached Copy
