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
 * $Id: class.tx_mnogosearch_baseconfig.php 25949 2009-10-28 15:17:14Z dmitry $
 */


/**
 * This class implements a base class for all indexing configurations
 *
 * @author	Dmitry Dulepov <dmitry.dulepov@gmail.com>
 * @package	TYPO3
 * @subpackage	tx_mnogosearch
 */
abstract class tx_mnogosearch_baseconfig {

	const DISALLOW = 1;
	const HREF_ONLY = 2;
	const CHECK_ONLY = 3;
	const SKIP = 4;

	const SITE = 1;
	const WORLD = 2;
	const PAGE = 3;

	/**
	 * Current user groups
	 *
	 * @var int
	 */
	protected $currentUserGroup = 0;

	/**
	 * Data row from the database
	 *
	 * @var	array
	 */
	protected $data;

	/**
	 * User groups for this configuration.
	 *
	 * @var	array
	 */
	protected $userGroups = array();

	/**
	 * "Server" directive methods. Keys correspond to values in the
	 * database record.
	 *
	 * @type	array
	 */
	static protected $serverMethods = array(
		self::DISALLOW => 'Disallow',
		self::HREF_ONLY => 'HrefOnly',
		self::CHECK_ONLY => 'CheckOnly ',
		self::SKIP => 'Skip'
	);

	/**
	 * Indexing subsections.
	 *
	 * @type	array
	 */
	static protected $subsections = array(
		self::SITE => 'site',
		self::WORLD => 'world',
		self::PAGE => 'page'
	);

	/**
	 * Sets a row from the database here.
	 *
	 * @param	array	$row
	 * @return	void
	 */
	public function setData(array $row) {
		$this->data = array();
		foreach ($row as $key => $value) {
			if (substr($key, 0, 15) == 'tx_mnogosearch_') {
				$key = substr($key, 15);
			}
			$this->data[$key] = $value;
		}
		$this->userGroups = $this->getUserGroups();
	}

	/**
	 * Obtains URls explicitely allowed by this configuration.
	 *
	 * @return	array
	 */
	abstract public function getAllowedServerURLs();

	/**
	 * Generates the configuration.
	 *
	 * @return	string
	 */
	public function generate() {
		if (!$this->isTagAllowed()) {
			$content = 'Tag ""' . chr(10);
			$this->currentUserGroup = 0;
			$content .= $this->doGenerate();
		}
		else {
			$tagLineFormat = 'Tag "T3UG_%d"';

			$content = '';
			foreach ($this->userGroups as $userGroup) {
				$this->currentUserGroup = $userGroup;
				$content .= sprintf($tagLineFormat, $userGroup) . chr(10);
				$content .= $this->doGenerate();
			}
		}

		return $content;
	}

	/**
	 * Obtains configuration lines ending with chr(10).
	 *
	 * @return string
	 */
	abstract public function getConfigurationLines();

	/**
	 * Obtains configuration id
	 *
	 * @return	int
	 */
	public function getId() {
		return $this->data['uid'];
	}

	/**
	 * Checks if group tags are allowed
	 *
	 * @return	void
	 */
	public function isTagAllowed() {
		return (count($this->userGroups) > 1);
	}

	/**
	 * Creates a set of configuration lines.
	 *
	 * @return	string
	 */
	protected function doGenerate() {
		$content .= $this->getConfigurationLines();
		$content .= $this->getRawConfig();

		return $content;
	}

	/**
	 * Obtains a string value of the current method
	 *
	 * @return	string
	 */
	protected function getMethodAsString() {
		$value = $this->data['method'];
		return isset(self::$serverMethods[$value]) ? self::$serverMethods[$value] . ' ' : '';
	}

	/**
	 * Obtains indexing period
	 *
	 * @return	string
	 */
	public function getPeriod() {
		return t3lib_div::testInt($this->data['period']) ?
						$this->data['period'] . 'h' :
						$this->data['period'];
	}

	/**
	 * Obtains raw indexer configuration. The result either empty or ends
	 * with a chr(10).
	 *
	 * @return	string
	 */
	public function getRawConfig() {
		$result = trim($this->data['additional_config']);
		if ($result != '') {
			$result .= chr(10);
		}
		return $result;
	}

	/**
	 * Checks if period is set for this configuration
	 *
	 * @return boolean
	 */
	public function hasPeriod() {
		$period = trim($this->data['period']);
		return $period != '' && $period != '0';
	}


	/**
	 * Obtains a subsection string
	 *
	 * @return	string
	 */
	protected function getSubSection() {
		$value = $this->data['subsection'];
		return isset(self::$subsections[$value]) ? self::$subsections[$value] . ' ' : '';
	}

	/**
	 * Obtains a list of user groups for indexing. The first element is always 0.
	 *
	 * @return	array
	 */
	protected function getUserGroups() {
		$list = t3lib_div::intExplode(',', $this->data['user_groups']);
		$list = array_unique($list);
		sort($list, SORT_NUMERIC);
		if (count($list) == 0 || $list[0] != 0) {
			array_unshift($list, 0);
		}

		return $list;
	}
}

if (defined('TYPO3_MODE') && $TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/cli/class.tx_mnogosearch_baseconfig.php'])	{
	include_once($TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/cli/class.tx_mnogosearch_baseconfig.php']);
}

?>