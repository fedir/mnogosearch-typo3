<?php
/***************************************************************
*  Copyright notice
*
*  (c) 2008 Dmitry Dulepov <dmitry@typo3.org>
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

/**
 * [CLASS/FUNCTION INDEX of SCRIPT]
 */


/**
 * Page and content manipulation watch.
 *
 * @author	Dmitry Dulepov <dmitry@typo3.org>
 * @package	TYPO3
 * @subpackage	tx_mnogosearch
 */
class tx_mnogosearch_tcemain {

	/**
	 * Extension configuration
	 *
	 * @var	array
	 */
	protected $sysconf = array();

	/**
	 * Creates an instance of this class
	 *
	 * @return	void
	 */
	function __construct() {
		$this->sysconf = unserialize($GLOBALS['TYPO3_CONF_VARS']['EXT']['extConf']['mnogosearch']);
	}

	/**
	 * Hooks to data change procedure to watch modified data. This hook is called
	 * after data is written to the database, so all paths are modified paths.
	 *
	 * @param	string	$status	Record status (new or update)
	 * @param	string	$table	Table name
	 * @param	integer	$id	Record ID
	 * @param	array	$fieldArray	Modified fields
	 * @param	object	$pObj	Reference to TCEmain
	 */
	public function processDatamap_afterDatabaseOperations($status, $table, $id, array $fieldArray, t3lib_TCEmain &$pObj) {
		// Only for LIVE records!
		if ($pObj->BE_USER->workspace == 0) {
			$this->storePageChanges($table, $id, $fieldArray, $pObj, false);
			if ($table != 'pages') {
				$this->storeRecordChanges($table, $id, $pObj);
			}
		}
	}

	/**
	 * Processes page changes
	 *
	 * @param	string	$table	Table name
	 * @param	mixed	$id	ID of the record (can be also 'NEW...')
	 * @param	array	$fieldArray	Field array
	 * @param	t3lib_TCEmain	$pObj	Parent object
	 * @param 	boolean	$processClearCachePages	If true, clearCache pages are processed for indexing as well
	 * @return	void
	 */
	protected function storePageChanges($table, $id, array $fieldArray, t3lib_TCEmain &$pObj, $processClearCachePages) {
		if ($table == 'pages') {
			if (!t3lib_div::testInt($id)) {
				// Page is just created. We do not index empty pages
				return;
			}
			$pid = $id;
		}
		else {
			$pid = $fieldArray['pid'];
			if ($pid) {
				if (!t3lib_div::testInt($pid)) {
					$negate = ($pid{0} == '-');
					$pid = $pObj->substNEWwithIDs[$negate ? substr($pid, 1) : $pid];
					if (!$pid) {
						return;
					}
					if ($negate) {
						$pid = -$pid;
					}
				}
				if ($pid < 0) {
					$rec = t3lib_BEfunc::getRecord($table, -$pid, 'pid');
					$pid = $rec['pid'];
				}
			}
			else {
				$rec = t3lib_BEfunc::getRecord($table, $id, 'pid');
				$pid = $rec['pid'];
			}
		}
		if ($pid) {
			$this->processPid($pid);
		}
		if ($processClearCachePages) {
			// Process any other page that may display results. This is important for
			// records that reside in sysfolder but shown on other pages. Normally
			// in this case cache for displaying page is cleared automatically.
			// So we just put such pages into list for reindexing
			list($tscPID) = t3lib_BEfunc::getTSCpid($table, $id, $pid);
			$TSConfig = $pObj->getTCEMAIN_TSconfig($tscPID);
			if ($TSConfig['clearCacheCmd'])	{
				$pidList = array_unique(t3lib_div::trimExplode(',', $TSConfig['clearCacheCmd'], true));
				foreach($pidList as $pid) {
					if (($pid = intval($pid))) {
						$this->processPid($pid);
					}
				}
			}
		}
	}

	/**
	 * Finds URL for page ID and inserts it to URL table.
	 *
	 * @param	int	$pid	Page ID
	 * @return	void
	 */
	protected function processPid($pid) {
		require_once(t3lib_extMgm::extPath('pagepath', 'class.tx_pagepath_api.php'));
		$pagePath = tx_pagepath_api::getPagePath($pid);
		if ($pagePath && !$this->recordAlreadyInLog($pagePath)) {
			// Check that page is not hidden and regular page
			if ($this->canIndexPage($pid, $pagePath)) {
				// Insert into log
				$this->addRecordToUrlLog($pagePath);
			}
		}
	}

	/**
	 * Checks if record is already in the URL log table.
	 *
	 * @param	string	$recordPath Record path
	 * @return	boolean	true if page is in the table
	 */
	protected function recordAlreadyInLog($recordPath) {
		list($rec) = $GLOBALS['TYPO3_DB']->exec_SELECTgetRows('COUNT(*) AS t',
			'tx_mnogosearch_urllog',
			'tx_mnogosearch_url=' .
				$GLOBALS['TYPO3_DB']->fullQuoteStr($recordPath, 'tx_mnogosearch_urllog'));
		return ($rec['t'] > 0);
	}

	/**
	 * Checks if page should be indexed
	 *
	 * @param	int	$pid	Page ID to check
	 * @param 	string	$path	Page path
	 * @return	boolean	true if page can be indexed
	 */
	function canIndexPage($pid, $path) {
		// Check page properties first
		$pageRec = t3lib_BEfunc::getRecord('pages', $pid, 'doktype,nav_hide,no_cache,no_search,fe_login_mode');
		$pageAllowsIndexing = ($pageRec['doktype'] <= 2 && !$pageRec['nav_hide'] && !$pageRec['no_cache']
				&& !$pageRec['no_search'] && !$pageRec['fe_login_mode']);
		if ($pageAllowsIndexing) {
			// Check indexing configurations
			$pageAllowsIndexing = $this->checkIndexingConfigurations($path);
		}
		return $pageAllowsIndexing;
	}

	/**
	 * Checks indexing configuration (if path is allowed)
	 *
	 * @param	string	$path	Path
	 * @return	boolean	true if indexing configuration allows this page
	 */
	protected function checkIndexingConfigurations($path) {
		$where = 'tx_mnogosearch_type<>11';
		$rows = $GLOBALS['TYPO3_DB']->exec_SELECTgetRows('*',
					'tx_mnogosearch_indexconfig', $where);
		$result = false;
		foreach ($rows as $row) {
			if ($row['tx_mnogosearch_type'] == '0') {
				// Server
				$result = (strpos($path, $row['tx_mnogosearch_url']) === 0);
				break;
			}
			elseif ($row['tx_mnogosearch_type'] == '1') {
				// Realm
				$caseFlag = ($row['tx_mnogosearch_cmpoptions'] & 1 ? 'i' : '');
				if ($row['tx_mnogosearch_cmptype'] == 1) {
					// Regexp
					$regexp = '/' . str_replace('/', '\/', $row['tx_mnogosearch_url']) . '/' . $caseFlag;
					$result = @preg_match($regexp, $path);
					break;
				}
				else {
					// Wildcards
					$regexp = '/' . str_replace('/', '\/',
							str_replace('*', '.*', $row['tx_mnogosearch_url'])) . '/' . $caseFlag;
					$result = @preg_match($regexp, $path);
					break;
				}
			}
		}
		return $result;
	}

	/**
	 * Logs messages to dev log
	 *
	 * @param	string	$msg	Message to log
	 * @param	int	$severity	Severity
	 * @param	mixed	$rec	Additional data or <code>false</code> ifno data
	 */
	function log($msg, $rec = false) {
		if ($this->sysconf['debugLog'] == 'file') {
			$fd = fopen(PATH_site . 'fileadmin/mnogosearch.log', 'a');
			flock($fd, LOCK_EX);
			fprintf($fd, '[%s] "%s"' . ($rec ? ', data: ' . chr(10) . '%s' . chr(10) : '') . chr(10), date('d-m-Y H:i:s'), $msg, print_r($rec, true));
			flock($fd, LOCK_UN);
			fclose($fd);
		}
		elseif ($this->sysconf['debugLog'] == 'devlog') {
			if (TYPO3_DLOG) {
				t3lib_div::devLog($msg, 'mnogosearch', 0, $rec);
			}
		}
	}

	/**
	 * Checks if given table is indexable and if yes stores its information
	 * into the URL log
	 *
	 * @param	string	$table	Table name
	 * @param	int	$id	Record id
	 * @param	t3lib_TCEmain	Parent object
	 * @return	void
	 */
	protected function storeRecordChanges($table, $id, t3lib_TCEmain &$pObj) {
		$uid = (t3lib_div::testInt($id) ? $id : $pObj->substNEWwithIDs[$id]);
		$record = t3lib_BEfunc::getRecordRaw($table, 'uid=' . intval($uid) .
					$this->enableFields($table));
		$configUid = 0;
		if ($record && $this->isIndexable($table, $record, $configUid)) {
			$path = sprintf('htdb:/%s/%d/%d', $table, $configUid, $record['uid']);
			if (!$this->recordAlreadyInLog($path)) {
				$this->addRecordToUrlLog($path, $record['pid']);
			}
		}
	}

	/**
	 * Provides 'enableFields' clause and a workaround for bad SQL generated by
	 * some versions of TYPO3.
	 *
	 * @param	string	$table	Table name
	 * @return	string
	 */
	protected function enableFields($table) {
		$enableFields = trim(t3lib_BEfunc::BEenableFields($table));
		$deleteFields = trim(t3lib_BEfunc::deleteClause($table));
		return ' ' . ($enableFields != 'AND' ? $enableFields : '') . ' ' .
				($deleteFields != 'AND' ? $deleteFields : '');
	}

	/**
	 * Checks if record is indexed
	 *
	 * @param	string	$table	Table name
	 * @param	array	$record	Record data from the database
	 * @return	boolean	true if record is indexable
	 */
	protected function isIndexable($table, array $record, &$configUid) {
		$indexingConfigs = t3lib_BEfunc::getRecordsByField('tx_mnogosearch_indexconfig',
				'tx_mnogosearch_table', $table, 'AND tx_mnogosearch_type=11', '', 'sorting');
		$result = false;
		if (is_array($indexingConfigs)) {
			foreach ($indexingConfigs as $config) {
				$config['tx_mnogosearch_pid_only'] = trim($config['tx_mnogosearch_pid_only'], ', ');
				if ($config['tx_mnogosearch_pid_only'] == '' ||
						t3lib_div::inList($config['tx_mnogosearch_pid_only'], $record['pid'])) {
					$result = true;
					$configUid = $config['uid'];
					break;
				}
			}
		}
		return $result;
	}

	/**
	 * Adds record path to the urllog
	 *
	 * @param	string	$url	URl to add
	 * @return	void
	 */
	protected function addRecordToUrlLog($url) {
		$GLOBALS['TYPO3_DB']->exec_INSERTquery('tx_mnogosearch_urllog', array(
					'pid' => 0,
					'tstamp' => time(),
					'crdate' => time(),
					'tx_mnogosearch_url' => $url,
				)
			);
	}
}

if (defined('TYPO3_MODE') && $TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/hooks/class.tx_mnogosearch_tcemain.php']) {
	include_once($TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/hooks/class.tx_mnogosearch_tcemain.php']);
}

?>