<?php
/***************************************************************
*  Copyright notice
*
*  (c) 2009 Dmitry Dulepov <dmitry.dulepov@gmail.com>
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
 *
 * $Id: class.tx_mnogosearch_recordconfig.php 25754 2009-10-22 14:36:03Z dmitry $
 */


/**
 * This class implements a record indexing configuration
 *
 * @author	Dmitry Dulepov <dmitry.dulepov@gmail.com>
 * @package	TYPO3
 * @subpackage	tx_mnogosearch
 */
class tx_mnogosearch_recordconfig extends tx_mnogosearch_baseconfig {

	/**
	 * Returns allowed URLs for this configuration
	 *
	 * @return 	array
	 * @see tx_mnogosearch_baseconfig::getAllowedServerURLs()
	 */
	public function getAllowedServerURLs() {
		// Notice: no slash in the end!
		return array(
			sprintf('htdb:/%s/%d', $this->data['table'], $this->data['uid'])
		);
	}

	/**
	 * Obtains configuration lines for this configuration.
	 *
	 * @return  string
	 * @see tx_mnogosearch_baseconfig::getConfigurationLines()
	 */
	public function getConfigurationLines() {
		$result = $this->getHtdbAddr();
		$result .= $this->getHtdbList();
		$result .= 'HTDBLimit 4096' . chr(10);
		$result .= $this->getHTDBDoc();
		return $result;
	}

	/**
	 * Creates 'HTDBAddr' directive
	 *
	 * @return	string
	 */
	protected function getHtdbAddr() {
		$setnames = $this->getSetNames();
		return sprintf('HTDBAddr mysql://%s:%s@%s/%s/%s',
			TYPO3_db_username, TYPO3_db_password, TYPO3_db_host, TYPO3_db,
				$setnames ? '?' . $setnames : '') .
			chr(10);
	}

	/**
	 * Obtains HTDBDoc statement
	 *
	 * @return	string
	 */
	protected function getHTDBDoc() {
		$table = $this->getTable();

		$result .= 'HTDBDoc "SELECT `';

		// Add title
		$result .= ($this->data['title_field'] ? $this->data['title_field'] : '\'\'');
		$result .= '` AS title,';

		// Add last modified
		if ($this->data['lastmod_field']) {
			$result .= '`' . $this->data['lastmod_field'] . '` AS last_mod_time,';
		}

		// TODO Add status and get rid of enableFields above.
		// See http://www.mnogosearch.org/doc33/msearch-extended-indexing.html#htdb
		// See http://dev.mysql.com/doc/refman/5.0/en/case-statement.html
		// $result .= 'CASE WHEN (delete=1 OR hidden=1 OR startime<... ....) THEN 404 ELSE 200';

		// Add body
		$bodyFields = $this->getQuotedFieldArray($this->data['body_field']);
		$result .= (count($bodyFields) > 1 ? 'CONCAT(' . implode(',\' \',', $bodyFields) . ')' : $bodyFields[0]);
		$result .= ' AS body FROM `' . $table . '` WHERE uid=$3"' . chr(10);
		$result .= 'Server htdb:/' . $table . '/' . chr(10);

		return $result;
	}

	/**
	 * Creates 'HTDBList' directive
	 *
	 * @return	string
	 */
	protected function getHtdbList() {
		$table = $this->getTable();
		$result = sprintf('HTDBList "SELECT CONCAT(\'htdb:/%s/%d/\', uid) FROM `%s`',
			$table, $this->getId(), $table);

		$where = $this->getTableWhere();
		if ($where != '') {
			$result .= ' WHERE ' . $where;
		}

		return $result . '"' . chr(10);
	}

	/**
	 * Fetches setnames configuration from the TYPO3 configuration
	 *
	 * @return	string
	 */
	protected function getSetNames() {
		$setnames = '';
		if (preg_match('/set\s+names\s+([a-z\-0-9]+)/i', $GLOBALS['TYPO3_CONF_VARS']['SYS']['setDBinit'], $matches)) {
			$setnames = 'setnames=' . $matches[1];
		}
		return $setnames;
	}

	/**
	 * Obtains a table
	 *
	 * @return	string
	 */
	protected function getTable() {
		return $this->data['table'];
	}

	/**
	 * Obtains table "where" statement.
	 *
	 * @return	string
	 */
	protected function getTableWhere() {
		$table = $this->getTable();
		$pidOnly = $this->data['pid_only'];
		if ($pidOnly) {
			if ($pidOnly) {
				$where = 'pid=' . $pidOnly;
			}
			else {
				$where = 'pid IN (' . $GLOBALS['TYPO3_DB']->cleanIntList($pidOnly) . ')';
			}
		}
		else {
			$where = '1=1';
		}
		$where .= t3lib_BEfunc::BEenableFields($table) . t3lib_BEfunc::deleteClause($table);
		return $where;
	}

	/**
	 * Quotes database fields for the query
	 *
	 * @param	array	$fields
	 * @return	array
	 */
	protected function getQuotedFieldArray($fields) {
		$fields = t3lib_div::trimExplode(',', $this->data['body_field'], true);
		foreach ($fields as &$field) {
			$field = '`' . $field . '`';
		}

		return $fields;
	}
}

if (defined('TYPO3_MODE') && $TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/cli/class.tx_mnogosearch_recordconfig.php'])	{
	include_once($TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/cli/class.tx_mnogosearch_recordconfig.php']);
}

?>