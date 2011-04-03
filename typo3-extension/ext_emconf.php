<?php

########################################################################
# Extension Manager/Repository config file for ext: "mnogosearch"
#
# Auto generated 16-04-2009 18:36
#
# Manual updates:
# Only the data in the array - anything else is removed by next write.
# "version" and "dependencies" must not be touched!
########################################################################

$EM_CONF[$_EXTKEY] = array(
	'title' => 'mnoGoSearch',
	'description' => 'Web site search engine',
	'category' => 'plugin',
	'author' => 'Dmitry Dulepov',
	'author_email' => 'dmitry@typo3.org',
	'shy' => '',
	'dependencies' => 'pagebrowse,pagepath',
	'conflicts' => 'tstidy',
	'priority' => '',
	'module' => '',
	'state' => 'stable',
	'internal' => '',
	'uploadfolder' => 0,
	'createDirs' => 'typo3temp/mnogosearch/var',
	'modify_tables' => '',
	'clearCacheOnLoad' => 0,
	'lockType' => '',
	'author_company' => 'SIA "ACCIO"',
	'version' => '2.2.2',
	'constraints' => array(
		'depends' => array(
			'typo3' => '4.1.1-4.99.99',
			'php' => '5.1.0-100.0.0',
			'pagebrowse' => '1.0.1-100.0.0',
			'pagepath' => '0.1.3-100.0.0',
		),
		'conflicts' => array(
			'tstidy' => '',
		),
		'suggests' => array(
		),
	),
	'_md5_values_when_last_written' => 'a:40:{s:9:"ChangeLog";s:4:"5dfb";s:35:"class.tx_mnogosearch_cms_layout.php";s:4:"2e50";s:33:"class.tx_mnogosearch_postproc.php";s:4:"2a23";s:32:"class.tx_mnogosearch_tcemain.php";s:4:"7035";s:21:"ext_conf_template.txt";s:4:"5abf";s:12:"ext_icon.gif";s:4:"208b";s:17:"ext_localconf.php";s:4:"3fd8";s:14:"ext_tables.php";s:4:"a83c";s:14:"ext_tables.sql";s:4:"d1ff";s:35:"icon_tx_mnogosearch_indexconfig.gif";s:4:"475a";s:16:"locallang_db.xml";s:4:"81b1";s:7:"tca.php";s:4:"446d";s:13:"cli/catoo.php";s:4:"a480";s:23:"cli/cli_mnogosearch.php";s:4:"54ac";s:14:"doc/manual.sxw";s:4:"2b71";s:37:"hooks/class.tx_mnogosearch_ttnews.php";s:4:"1f77";s:44:"model/class.tx_mnogosearch_model_results.php";s:4:"df72";s:32:"pi1/class.tx_mnogosearch_pi1.php";s:4:"b694";s:19:"pi1/flexform_ds.xml";s:4:"dfa0";s:17:"pi1/locallang.xml";s:4:"b603";s:20:"resources/scripts.js";s:4:"1355";s:20:"resources/styles.css";s:4:"58c8";s:23:"resources/template.html";s:4:"133c";s:27:"resources/icons/acrobat.png";s:4:"5ee1";s:21:"resources/icons/c.png";s:4:"c770";s:29:"resources/icons/cplusplus.png";s:4:"3d73";s:28:"resources/icons/document.png";s:4:"a311";s:25:"resources/icons/excel.png";s:4:"7363";s:21:"resources/icons/h.png";s:4:"a899";s:28:"resources/icons/msoffice.png";s:4:"51e7";s:26:"resources/icons/oocalc.png";s:4:"b684";s:29:"resources/icons/ooimpress.png";s:4:"aacf";s:28:"resources/icons/oowriter.png";s:4:"f711";s:30:"resources/icons/powerpoint.png";s:4:"8c36";s:24:"resources/icons/text.png";s:4:"0da6";s:24:"resources/icons/word.png";s:4:"38af";s:30:"resources/images/relevance.gif";s:4:"5e01";s:32:"static/mnoGoSearch/constants.txt";s:4:"5927";s:28:"static/mnoGoSearch/setup.txt";s:4:"96bd";s:34:"view/class.tx_mnogosearch_view.php";s:4:"91cc";}',
	'suggests' => array(
	),
);

?>