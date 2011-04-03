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
 * $Id: class.tx_mnogosearch_dboperations.php 25735 2009-10-22 09:24:34Z dmitry $
 */


/**
 * This class contains database operations for the mnoGoSearch CLI
 *
 * @author	Dmitry Dulepov <dmitry.dulepov@gmail.com>
 * @package	TYPO3
 * @subpackage	tx_mnogosearch
 */
class tx_mnogosearch_dboperations {

	/**
	 * MySQL connection to the mnoGoSearch database
	 *
	 * @var	resource
	 */
	protected $connection;

	/**
	 * Database name
	 *
	 * @var string
	 */
	protected $databaseName;

	/**
	 * Indexer instance
	 *
	 * @var tx_mnogosearch_indexer
	 */
	protected $indexer;

	/**
	 * Checks and creates mnoGoSearch database
	 */
	public function checkAndCreate() {
		$this->connect();
		if (!$this->hasTables()) {
			$this->createDatabase();
		}
		mysql_close($this->connection);
	}

	/**
	 * Sets indexer instance
	 *
	 * @param	tx_mnogosearch_indexer	$indexer
	 * @return	void
	 */
	public function setIndexer(tx_mnogosearch_indexer &$indexer) {
		$this->indexer = $indexer;
	}

	/**
	 * Obtains MySQL connection to the database
	 *
	 * @return	void
	 */
	protected function connect() {
		$sysconf = unserialize($GLOBALS['TYPO3_CONF_VARS']['EXT']['extConf']['mnogosearch']);
		$parts = parse_url($sysconf['dbaddr']);
		if ($parts['scheme'] == 'mysql') {
			$this->connection = mysql_connect($parts['host'], $parts['user'], $parts['pass']);
			if (!$this->connection) {
				throw new Exception('Unable to connect to the mnoGoSearch database');
			}
			$this->databaseName = trim($parts['path'], '/');
		}
		else {
			throw new Exception(sprintf('Unsupported mnoGoSearch connection scheme: "%s"', $parts['scheme']));
		}
	}

	/**
	 * Creates mnoGoSearch database by ruuning indexer with appropriate
	 * commands
	 *
	 * @return	void
	 */
	protected function createDatabase() {
		$this->indexer->createDatabase();
	}

	/**
	 * Checks if tables are present in the mnoGoSearch database
	 *
	 * @return	boolean
	 */
	protected function hasTables() {
		$query = sprintf('SHOW TABLES FROM `%s` LIKE \'qcache\'',
				mysql_real_escape_string($this->databaseName, $this->connection));
		$recordSet = mysql_query($query, $this->connection);
		if (mysql_errno($this->connection) == 0) {
			$hasTables = (mysql_num_rows($recordSet) > 0);
			mysql_free_result($recordSet);
		}
		else {
			throw new Exception(sprintf('Unable to query mnoGoSearch database. Error %1$d (%1$s)',
				mysql_errno($this->connection), mysql_error($this->connection)));
		}
		return $hasTables;
	}
}

if (defined('TYPO3_MODE') && $TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/cli/class.tx_mnogosearch_dboperations.php'])	{
	include_once($TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/cli/class.tx_mnogosearch_dboperations.php']);
}

?>