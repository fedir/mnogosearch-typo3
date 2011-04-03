<?php
if (!defined ('TYPO3_MODE')) 	die ('Access denied.');

t3lib_div::loadTCA('tt_content');
$TCA['tt_content']['types']['list']['subtypes_excludelist'][$_EXTKEY.'_pi1'] = 'layout,select_key,pages';
$TCA['tt_content']['types']['list']['subtypes_addlist'][$_EXTKEY.'_pi1'] = 'pi_flexform';
t3lib_extMgm::addPiFlexFormValue($_EXTKEY . '_pi1', 'FILE:EXT:mnogosearch/pi1/flexform_ds.xml');

t3lib_extMgm::addPlugin(array('LLL:EXT:mnogosearch/locallang_db.xml:tt_content.list_type_pi1', $_EXTKEY.'_pi1'),'list_type');

t3lib_extMgm::addStaticFile($_EXTKEY,'static/mnoGoSearch/', 'mnoGoSearch');

$tx_mnogosearch_sysconf = unserialize($GLOBALS['TYPO3_CONF_VARS']['EXT']['extConf'][$_EXTKEY]);

if (!function_exists('tx_mnogosearch_indexingConfig_labelFunc')) {
	/**
	 * Creates a record label from record data
	 *
	 * @param	array	$params	Parameters to the hook
	 * @return	void
	 */
	function tx_mnogosearch_indexingConfig_labelFunc(array &$params) {
		// Check user-entered title
		if ($params['row']['title']) {
			$params['title'] = $params['row']['title'];
			return;
		}

		// Generate title
		require_once(t3lib_extMgm::extPath('lang', 'lang.php'));
		if ($GLOBALS['LANG'] instanceof language) {
			$lang = &$GLOBALS['LANG'];
		}
		else {
			$lang = t3lib_div::makeInstance('language');
			/* @var $lang language */
			$lang->init($GLOBALS['BE_USER']->uc['lang']);
		}
		if ($params['row']['tx_mnogosearch_type'] == 11) {
			// Records
			$row = t3lib_BEfunc::getRecord($params['table'], $params['row']['uid'], 'tx_mnogosearch_table');
			if (isset($GLOBALS['TCA'][$row['tx_mnogosearch_table']])) {
				$params['title'] = sprintf(
						$lang->sL('LLL:EXT:mnogosearch/locallang_db.xml:table_records'),
						$lang->sL($GLOBALS['TCA'][$row['tx_mnogosearch_table']]['ctrl']['title'])
					);
			}
			else {
				$params['title'] = $params['table'];
			}
		}
		else {
			// Server or Realm
			$params['title'] = $params['row']['tx_mnogosearch_url'];
			$row = t3lib_BEfunc::getRecord($params['table'], $params['row']['uid'], 'tx_mnogosearch_method');
			t3lib_div::loadTCA($params['table']);
			foreach ($GLOBALS['TCA'][$params['table']]['columns']['tx_mnogosearch_method']['config']['items'] as $item) {
				if ($item[1] == $row['tx_mnogosearch_method']) {
					$method = $item[0];
					break;
				}
			}
			if ($method == '') {
				$method = 'LLL:EXT:mnogosearch/locallang_db.xml:tx_mnogosearch_indexconfig.tx_mnogosearch_method.allow';
			}
			$params['title'] = sprintf('%s: %s', $lang->sL($method), $params['row']['tx_mnogosearch_url']);
		}
	}
}

$TCA['tx_mnogosearch_indexconfig'] = array (
	'ctrl' => array (
		'title'     => 'LLL:EXT:mnogosearch/locallang_db.xml:tx_mnogosearch_indexconfig',
		'label'     => 'tx_mnogosearch_url',
		'label_userFunc' => 'tx_mnogosearch_indexingConfig_labelFunc',
		'type'		=> 'tx_mnogosearch_type',
		'tstamp'    => 'tstamp',
		'crdate'    => 'crdate',
		'cruser_id' => 'cruser_id',
		'sortby' 	=> 'sorting',
		'rootLevel'	=> -1,
		'dividers2tabs' => 2,
		'dynamicConfigFile' => t3lib_extMgm::extPath($_EXTKEY).'tca.php',
		'iconfile'	=> t3lib_extMgm::extRelPath($_EXTKEY).'icon_tx_mnogosearch_indexconfig.gif',
		'typeicon_column'	=> 'tx_mnogosearch_type',
		'requestUpdate' => 'tx_mnogosearch_table',
		'typeicons'	=> array(
			0 => 'pages_link.gif',
			1 => 'pages_catalog.gif',
			11 => 'tt_content.gif',
		),
	)
);
t3lib_extMgm::allowTableOnStandardPages('tx_mnogosearch_indexconfig');
//t3lib_extMgm::addLLrefForTCAdescr('tx_mnogosearch_indexconfig', 'EXT:mnogosearch/locallang_csh.xml');


$TCA['tx_mnogosearch_urllog'] = array (
	'ctrl' => array (
		'title'     => 'LLL:EXT:mnogosearch/locallang_db.xml:tx_mnogosearch_urllog',
		'label'     => 'tx_mnogosearch_url',
		'tstamp'    => 'tstamp',
		'crdate'    => 'crdate',
		'cruser_id' => 'cruser_id',
		'rootLevel'	=> 1,
		'readOnly'	=> 1,
		'hideTable' => $tx_mnogosearch_sysconf['hide_internal_tables'],
		'dynamicConfigFile' => t3lib_extMgm::extPath($_EXTKEY).'tca.php',
		'iconfile'	=> 'pages.gif',
	)
);

// Adding datastructure
$GLOBALS['TBE_MODULES_EXT']['xMOD_tx_templavoila_cm1']['staticDataStructures'][]=array(
	'title' => 'LLL:EXT:mnogosearch/locallang_db.xml:ds_short_search_form',
	'path' => 'EXT:'.$_EXTKEY.'/pi1/ds_short_search_form.xml',
	'icon' => '',
	'scope' => 0,
	'fileref' => 'EXT:'.$_EXTKEY.'/pi1/templates/tv.html',
);
$GLOBALS['TBE_MODULES_EXT']['xMOD_tx_templavoila_cm1']['staticDataStructures'][]=array(
	'title' => 'LLL:EXT:mnogosearch/locallang_db.xml:ds_long_search_form',
	'path' => 'EXT:'.$_EXTKEY.'/pi1/ds_long_search_form.xml',
	'icon' => '',
	'scope' => 0,
	'fileref' => 'EXT:'.$_EXTKEY.'/pi1/templates/tv.html',
);
$GLOBALS['TBE_MODULES_EXT']['xMOD_tx_templavoila_cm1']['staticDataStructures'][]=array(
	'title' => 'LLL:EXT:mnogosearch/locallang_db.xml:ds_search_results',
	'path' => 'EXT:'.$_EXTKEY.'/pi1/ds_search_results.xml',
	'icon' => '',
	'scope' => 0,
	'fileref' => 'EXT:'.$_EXTKEY.'/pi1/templates/tv.html',
);

unset($tx_mnogosearch_sysconf);

?>
