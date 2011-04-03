<?php
/***************************************************************
*  Copyright notice
*
*  (c) 2007 Dmitry Dulepov <dmitry@typo3.org>
*  All rights reserved
*
*  This script is part of the TYPO3 project. The TYPO3 project is
*  free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  The GNU General Public License can be found at
*  http://www.gnu.org/copyleft/gpl.html.
*
*  This script is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  This copyright notice MUST APPEAR in all copies of the script!
***************************************************************/

require_once(PATH_tslib . 'class.tslib_pibase.php');
require_once(t3lib_extMgm::extPath('mnogosearch') . 'view/class.tx_mnogosearch_view.php');
require_once(t3lib_extMgm::extPath('mnogosearch') . 'model/class.tx_mnogosearch_model_results.php');

define('UDM_ENABLED', 1);
define('UDM_DISABLED', 0);

/**
 * Plugin '[mnoGoSearch] Search form' for the 'mnogosearch' extension.
 *
 * @author	Dmitry Dulepov <dmitry@typo3.org>
 * @package	TYPO3
 * @subpackage	tx_mnogosearch
 */
class tx_mnogosearch_pi1 extends tslib_pibase {
	public $prefixId      = 'tx_mnogosearch_pi1';		// Same as class name
	public $scriptRelPath = 'pi1/class.tx_mnogosearch_pi1.php';	// Path to this script relative to the extension dir.
	public $extKey        = 'mnogosearch';	// The extension key.
	protected $view = false;
	protected $udmApiVersion;
	public $highlightParts = array('', '');
	public $sysconf;
	protected $templateTestMode = false;

	/**
	 * Initializes the plugin. Checks mnoGoSearch version and plugin configuration.
	 *
	 * @return	string	Error message (empty if successful)
	 */
	protected function init() {
		$this->pi_setPiVarDefaults();
		$this->pi_loadLL();
		$this->pi_USER_INT_obj = 1;	// Configuring so caching is not expected. This value means that no cHash params are ever set. We do this, because it's a USER_INT object!
		$this->pi_initPIflexForm();
		$this->pi_checkCHash = false;

		// Check if we have TS setup
		if (!isset($this->conf['templateFile'])) {
			return $this->pi_getLL('no_ts_setup');
		}

		// Tempate test mode (manual activation through the class attribute)
		if (!$this->templateTestMode) {
			// Check mnoGoSearch plugin
			if (!extension_loaded('mnogosearch')) {
				return $this->pi_getLL('no_mnogosearch');
			}
			if (($this->udmApiVersion = Udm_Api_Version()) < 30306) {
				return sprintf($this->pi_getLL('mnogosearch_too_old'), '3.3.6');
			}
		}

		// Get extension configuration
		$this->sysconf = unserialize($GLOBALS['TYPO3_CONF_VARS']['EXT']['extConf']['mnogosearch']);

		// Mode
		$modes = $this->pi_getFFvalue($this->cObj->data['pi_flexform'], 'field_mode');
		if ($modes != '') {
			$this->conf['mode'] = $modes;
		}

		// Init view class
		$this->view = t3lib_div::makeInstance('tx_mnogosearch_view');
		if (!$this->view->init($this)) {
			return $this->pi_getLL('cannot_create_renderer');
		}

		// Add domain restrictions
		$domainLimitList = $GLOBALS['TYPO3_DB']->cleanIntList($this->pi_getFFvalue($this->cObj->data['pi_flexform'], 'field_siteList'));
		if (!$domainLimitList) {
			$domainLimitList = $GLOBALS['TYPO3_DB']->cleanIntList($this->conf['siteList']);
		}
		// Combine with values from the query
		if (is_array($this->piVars['siteList'])) {
			$configuredLimit = t3lib_div::trimExplode(',', $domainLimitList);
			$receivedLimit = $GLOBALS['TYPO3_DB']->cleanIntArray($this->piVars['siteList']);
			$domainLimitList = implode(array_intersect($configuredLimit, $receivedLimit));
		}
		$this->conf['siteList'] = $domainLimitList;

		return '';
	}

	/**
	 * The main method of the PlugIn
	 *
	 * @param	string		$content: The PlugIn content
	 * @param	array		$conf: The PlugIn configuration
	 * @return	The content that is displayed on the website
	 */
	public function main($content, $conf)	{
		$this->conf = $conf;

		if ('' != ($error = $this->init())) {
			return $this->pi_wrapInBaseClass($error);
		}

		$modes = t3lib_div::trimExplode(',', $this->conf['mode'], true);
		foreach ($modes as $mode) {
			switch ($mode) {
				case 'short_form':
					// Simple form
					$content .= $this->view->render_simpleSearchForm();
					break;
				case 'long_form':
					// Full form
					$content .= $this->view->render_searchForm();
					break;
				case 'results':
					// Search results
					if ($this->templateTestMode) {
						$content .= $this->getTestResults();
					}
					elseif ($this->piVars['q']) {
						$result = $this->search();
						$content .= $this->view->render_searchResults($result);
					}
					break;
			}
		}

		return $this->pi_wrapInBaseClass($content);
	}

	/**
	 * Searches for a query phrase
	 *
	 * @return	mixed	Returns found records or string in case of error
	 */
	protected function search() {

		// Allocate and setup agent
		$udmAgent = Udm_Alloc_Agent_Array(array($this->sysconf['dbaddr']));
		if (!$udmAgent) {
			return $this->pi_getLL('agent_alloc_failure');
		}
		if ('' != ($error = $this->setUpAgent($udmAgent))) {
			Udm_Free_Agent($udmAgent);
			return $error;
		}

		// Search
		$res = Udm_Find($udmAgent, $this->piVars['q']);
		if (!$res) {
			$error = sprintf($this->pi_getLL('mnogosearch_error'), Udm_ErrNo($udmAgent), Udm_Error($udmAgent));
			Udm_Free_Agent($udmAgent);
			return $error;
		}
		// Process search results
		$results = t3lib_div::makeInstance('tx_mnogosearch_model_results');
		/* @var $results tx_mnogosearch_model_results */
		$results->init($udmAgent, $res, $this);

		// Do not call Udm_Free_Ispell_data(), otherwise Udm_Free_Agent() will die!
		@Udm_Free_Agent($udmAgent);

		return $results;
	}

	/**
	 * Sets up mnoGoSearch agent
	 *
	 * @param	resource	$udmAgent	Agent configuration
	 * @return	string	Empty if no error
	 */
	protected function setUpAgent(&$udmAgent) {
		$page = intval($this->conf['search.']['resultsPerPage']);
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_PAGE_SIZE, $page ? $page : 20);
		$this->piVars['page'] = max(0, intval($this->piVars['page']));

		// Set search options
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_PAGE_NUM, $this->piVars['page']);
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_TRACK_MODE, ($this->conf['search.']['options.']['track_queries'] ? UDM_ENABLED : UDM_DISABLED));
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_CACHE_MODE, ($this->conf['search.']['options.']['cache_queries'] ? UDM_ENABLED : UDM_DISABLED));
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_ISPELL_PREFIXES, ($this->conf['search.']['options.']['use_spell_data'] ? UDM_ENABLED : UDM_DISABLED));
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_CROSS_WORDS, ($this->conf['search.']['options.']['crosswords'] ? UDM_ENABLED : UDM_DISABLED));
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_DETECT_CLONES, ($this->conf['search.']['options.']['detect_clones'] ? UDM_ENABLED : UDM_DISABLED));
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_PHRASE_MODE, ($this->conf['search.']['options.']['phrase_search'] ? UDM_ENABLED : UDM_DISABLED));

		// Date format (like for strftime, see sql.c#5592
		Udm_Set_Agent_Param_Ex($udmAgent, 'DateFormat', '%d.%m.%Y.%H.%M.%S');

		// Popularity rank
		Udm_Set_Agent_Param_Ex($udmAgent, 'PopRankUseShowCnt', 'yes');

		// LocalCharset
		if ($this->sysconf['LocalCharset'] != '') {
			if (!Udm_Check_Charset($udmAgent, $this->sysconf['LocalCharset'])) {
				return sprintf($this->pi_getLL('bad_local_charset'), $this->sysconf['LocalCharset']);
			}
			Udm_Set_Agent_Param($udmAgent, UDM_PARAM_CHARSET, $this->sysconf['LocalCharset']);
		}
		else {
			Udm_Set_Agent_Param($udmAgent, UDM_PARAM_CHARSET, 'utf-8');
		}

		// BrowserCharset
		$browserCharset = ($GLOBALS['TSFE']->config['config']['renderCharset'] ?
				$GLOBALS['TSFE']->config['config']['renderCharset'] :
					($GLOBALS['TYPO3_CONF_VARS']['BE']['forceCharset'] ?
						$GLOBALS['TYPO3_CONF_VARS']['BE']['forceCharset'] :
							($this->sysconf['BrowserCharset'] ? $this->sysconf['BrowserCharset'] : 'utf-8'
							)
					)
				);
		if (!Udm_Check_Charset($udmAgent, $browserCharset)) {
			return sprintf($this->pi_getLL('bad_browser_charset'), $browserCharset);
		}
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_BROWSER_CHARSET, $browserCharset);

		// Excerpt & highlight
		$val = intval($this->conf['search.']['excerptSize']);
		if ($val && t3lib_div::testInt($val)) {
			Udm_Set_Agent_Param_Ex($udmAgent, 'ExcerptSize', $val);
		}
		$val = intval($this->conf['search.']['excerptPadding']);
		if ($val && t3lib_div::testInt($val)) {
			Udm_Set_Agent_Param_Ex($udmAgent, 'ExcerptPadding', $val);
		}
		if ($this->conf['search.']['excerptHighlight']) {
			$this->highlightParts = t3lib_div::trimExplode('|', $this->conf['search.']['excerptHighlight']);
		}
		if (count($this->highlightParts) != 2) {
			$this->highlightParts = array('', '');
		}
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_HLBEG, $this->highlightParts[0]);
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_HLEND, $this->highlightParts[1]);

		// Create query string -- not really needed?
		/*
		$q_string = '';
		foreach ($this->piVars as $key => $val) {
			$q_string .= '&' . $key . '=' . urlencode($val);
		}
		$q_string = substr($q_string, 1);
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_QSTRING, $q_string);
		Udm_Parse_Query_String($udmAgent, $q_string);
		*/
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_REMOTE_ADDR, t3lib_div::getIndpEnv('REMOTE_ADDR'));
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_QUERY, $this->piVars['q']);
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_GROUPBYSITE, UDM_DISABLED);
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_SEARCH_MODE, UDM_MODE_ALL);
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_WORD_MATCH, UDM_MATCH_WORD);
		$val = intval($this->conf['search.']['minimumWordLength']);
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_MIN_WORD_LEN, $val ? $val : 3);
		$val = intval($this->conf['search.']['maximumWordLength']);
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_MAX_WORD_LEN, $val ? $val : 32);
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_VARDIR, PATH_site . 'typo3temp/mnogosearch/var'); //$this->sysconf['mnoGoSearchPath'] . '/var');

		// Number of sections (must be inline with weightFactor)
		$numSections = intval($this->conf['search.']['numberOfSections']);
		Udm_Set_Agent_Param_Ex($udmAgent, 'NumSections', $numSections ? $numSections : 4);	// Affects PageRank!!!
		
		// Weight factors (0-15, which is 0-F in hex, see CLI script for sections):
		//	body: 8 (more than 8 will lower rank for title, less than 8 will lower rank for body)
		//	title: F (title needs highest!)
		//	keywords: 0 (minimal)
		//	description: 0 (minimal)
		//	fe group id: 0 (may not set to 0 but must not have any real weight!)
		$weightFactor = 0;
		if (0 == sscanf(strtolower($this->conf['search.']['weightFactor']), '%x', $weightFactor) || $weightFactor == 0) {
			$weightFactor = 0x00F8;
		}
		$weightFactor = sprintf('%0' . $numSections . 'X', $weightFactor);
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_WEIGHT_FACTOR, $weightFactor);
		Udm_Set_Agent_Param_Ex($udmAgent, 'nwf', sprintf('%0' . $numSections . 'X', 0));

		// Increase score for documents with higher word density (use small values!)
		Udm_Set_Agent_Param_Ex($udmAgent, 'WordDensityFactor', 10);
		// Increase position of docs where searched words are close to the beginning
		Udm_Set_Agent_Param_Ex($udmAgent, 'MinCoordFactor', 0);
		// Count number of found words (low effect)
		Udm_Set_Agent_Param_Ex($udmAgent, 'NumWordFactor', 10);
		// Prevent smaller documents from appearing higher in results
		Udm_Set_Agent_Param_Ex($udmAgent, 'DocSizeWeight', 0);
		// Improve position for words found with fuzzy search
		Udm_Set_Agent_Param_Ex($udmAgent, 'WordFormFactor', 50);

		$val = $this->conf['search.']['sortMode'];
		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_SORT_ORDER, $val ? $val : 'RPD'); // or DRP

		if ($this->conf['search.']['options.']['use_spell_data']) {
			$this->loadIspellData($udmAgent);
			Udm_Set_Agent_Param_Ex($udmAgent, 'suggest', ($this->conf['search.']['options.']['suggest_mode'] ? 'yes' : 'no'));
			// "sp=1" will search for all word forms
			Udm_Set_Agent_Param_Ex($udmAgent, 'sp', ($this->conf['search.']['options.']['search_word_forms'] ? '1' : '0'));
		}

		$this->addSearchRestrictions($udmAgent);

		// Process extended configuration
		foreach ($this->conf['search.']['extendedConfiguration.'] as $param => $value) {
			Udm_Set_Agent_Param_Ex($udmAgent, $param, $value);
		}

		$error = Udm_Error($udmAgent);
		return $error ? sprintf($this->pi_getLL('mnogosearch.error'), Udm_ErrNo($udmAgent), $error) : '';
	}

	/**
	 * Loads Ispell data from external files
	 *
	 * @param	resource	$udmAgent	Agent
	 */
	protected function loadIspellData(&$udmAgent) {
		$extraConfig = $this->loadExtraConfig();

		if (!($affix_files = $extraConfig['affix'])) {
			return;
		}

		Udm_Set_Agent_Param($udmAgent, UDM_PARAM_ISPELL_PREFIXES, UDM_ENABLED);

		if (!is_array($affix_files)) {
			$this->loadSpellFile($udmAgent, UDM_ISPELL_TYPE_AFFIX, $affix_files, 0);
		}
		else {
			foreach ($affix_files as $affix_file) {
				$this->loadSpellFile($udmAgent, UDM_ISPELL_TYPE_AFFIX, $affix_file, 0);
			}
		}
		if (!($spell_files = $extraConfig['spell'])) {
			return;
		}
		if (!is_array($spell_files)) {
			$this->loadSpellFile($udmAgent, UDM_ISPELL_TYPE_SPELL, $spell_files, 1);
		}
		else {
			$count = count($spell_files);
			foreach ($spell_files as $spell_file) {
				$this->loadSpellFile($udmAgent, UDM_ISPELL_TYPE_SPELL, $spell_file, (--$count) == 0 ? 1 : 0);
			}
		}
	}

	/**
	 * Loads a single Spell file
	 *
	 * @param	resource	$udmAgent	Agent
	 * @param	int	$fileType	File type
	 * @param	string	$data	Spell file information (see mnoGoSearch docs). Example: <code>en us-ascii /etc/ispell/english.aff</code>
	 * @param	int	$sort	1, if sorting should happen
	 */
	protected function loadSpellFile(&$udmAgent, $fileType, $data, $sort) {
		list($file, $charset, $lang) = array_reverse(t3lib_div::trimExplode(' ', $data, 1));
		Udm_Load_Ispell_Data($udmAgent, $fileType, $lang, $charset, $file, $sort);
	}

	/**
	 * Loads extra configuration
	 *
	 * @return	array	Key/value pair for configuration
	 */
	protected function loadExtraConfig() {
		$result = array();
		if ($this->sysconf['IncludeFile']) {
			$lines = @file($this->sysconf['IncludeFile']);
			if (is_array($lines)) {
				foreach ($lines as $line) {
					$line = trim($line);
					if (substr($line, 0, 1) != '#') {
						$parts = explode(' ', $line, 2);
						if (count($parts) == 2) {
							$key = strtolower(trim($parts[0]));
							if (isset($result[$key]) && !is_array($result[$key])) {
								$result[$key] = array($result[$key]);
								$result[$key][] = $parts[1];
							}
							else {
								$result[$key] = $parts[1];
							}
						}
					}
				}
			}
		}
		return $result;
	}

	/**
	 * Creates fake test results.
	 *
	 * @return	tx_mnogosearch_model_results	Generated results
	 */
	protected function getTestResults() {
		$result = t3lib_div::makeInstance('tx_mnogosearch_model_results');
		/* @var $result tx_mnogosearch_model_results */
		$result->initTest($this);
		return $result;
	}

	/**
	 * Adds domain limitations from configuration
	 *
	 * @param	resource	$udmAgent	UDM agent
	 * @return	void
	 * @see	tx_mnogosearch_view::getSearchSections()
	 */
	protected function addSearchRestrictions(&$udmAgent) {
		// We process two lists:
		//	- configuration ($this->conf['siteList'] + tx_mnogosearch_indexconfig)
		//	- custom (added by hooks)
		$configLimitList = array();
		$customLimitList = array();

		// Check if user sent something with the request
		if (!isset($this->piVars['l']) || !is_array($this->piVars['l']) || in_array('', $this->piVars['l'])) {
			// Nothing sent, use configuration
			$configLimitList = explode(',', $this->conf['siteList']);
		}
		else {
			// User has specified custom limits in the search form. We need
			// to check that they are in the allowed site list. If it is not,
			// call a hook to very custom limit
			foreach ($this->piVars['l'] as $key => $limit) {
				if ($limit != '') {
					if (!t3lib_div::inList($this->conf['siteList'], $limit)) {
						unset($this->piVars['l'][$key]);
						$customLimitList[] = $limit;
					}
					else {
						// $limit is an int (because it is in the siteList)
						$configLimitList[] = $limit;
					}
				}
			}
		}

		// Add all configuration limits
		if (count($configLimitList) > 0) {
			$res = $GLOBALS['TYPO3_DB']->exec_SELECTquery(
				'uid,tx_mnogosearch_type,tx_mnogosearch_url,tx_mnogosearch_table',
				'tx_mnogosearch_indexconfig',
				'uid IN (' . implode(',', $configLimitList) . ') AND (' .
				// Server
				'(tx_mnogosearch_type=0 AND tx_mnogosearch_method<=0) OR ' .
				// Tables
				'(tx_mnogosearch_type=11)' .
				')' . $this->cObj->enableFields('tx_mnogosearch_indexconfig'));
			while (false !== ($row = $GLOBALS['TYPO3_DB']->sql_fetch_assoc($res))) {
				if ($row['tx_mnogosearch_type'] != 11) {
					// Server
					Udm_Add_Search_Limit($udmAgent, UDM_LIMIT_URL, $row['tx_mnogosearch_url']);
				}
				else {
					// Table
					$limit = sprintf('htdb:/%s/%d/', $row['tx_mnogosearch_table'], $row['uid']);
					Udm_Add_Search_Limit($udmAgent, UDM_LIMIT_URL, $limit);
				}
			}
			$GLOBALS['TYPO3_DB']->sql_free_result($res);
		}

		// Call hooks to add custom limits
		if (is_array($GLOBALS['TYPO3_CONF_VARS']['EXTCONF']['mnogosearch']['addCustomSearchLimit'])) {
			foreach($GLOBALS['TYPO3_CONF_VARS']['EXTCONF']['mnogosearch']['addCustomSearchLimit'] as $userFunc) {
				$params = array(
					'pObj' => &$this,
					'customLimits' => $customLimitList,
					'configLimits' => $configLimitList,
					'udmAgent' => &$udmAgent,
				);
				t3lib_div::callUserFunction($userFunc, $params, $this);
			}
		}
	}

	/**
	 * Returns udmApiVersion. This function is temporary and it is here until
	 * agent is moved to a separate class.
	 *
	 * @return	int	API version
	 */
	public function getUdmApiVersion() {
		return $this->udmApiVersion;
	}
}

if (defined('TYPO3_MODE') && $TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/pi1/class.tx_mnogosearch_pi1.php'])	{
	include_once($TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/pi1/class.tx_mnogosearch_pi1.php']);
}

?>