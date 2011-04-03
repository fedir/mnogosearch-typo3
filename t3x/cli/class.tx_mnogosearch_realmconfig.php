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
 * $Id: class.tx_mnogosearch_realmconfig.php 25716 2009-10-21 15:14:23Z dmitry $
 */


/**
 * This class implements a REALM config
 *
 * @author	Dmitry Dulepov <dmitry.dulepov@gmail.com>
 * @package	TYPO3
 * @subpackage	tx_mnogosearch
 */
class tx_mnogosearch_realmconfig extends tx_mnogosearch_baseconfig {

	/**
	 * Obtains a list of URLs explicitly allowed by this configuration.
	 *
	 * @return 	array
	 * @see tx_mnogosearch_baseconfig::getAllowedServerURLs()
	 */
	public function getAllowedServerURLs() {
		$result = array();
		if ($this->data['method'] != DISALLOW && $this->data['method'] != SKIP) {
			$regExp = ($this->data['cmptype'] == 1);
			if (!$regExp) {
				$result[] = $this->data['url'];
			}
			else {
				$result[] = '/' . str_replace('/', '\/', $this->data['url']) . '/';
			}
		}
		return $result;
	}

	/**
	 * Obtains configuration lines.
	 *
	 * @return  string
	 * @see tx_mnogosearch_baseconfig::getConfigurationLines()
	 */
	public function getConfigurationLines() {
		$result = 'Realm ';
		$result .= $this->getMethodAsString();
		$result .= $this->getCmpOptions();
		$result .= $this->data['url'] . chr(10);

		return $result;
	}

	/**
	 * Obtains comparison options
	 *
	 * @return	string
	 */
	protected function getCmpOptions() {
		$result = ($this->data['cmpoptions'] & 2 ? 'NoMatch ' : '');
		if ($this->data['cmptype'] > 0 || ($this->data['cmpoptions'] & 1)) {
			$result = ($this->data['cmptype'] == 1 ? 'Regex ' : 'String ');
		}
		return $result;
	}
}

if (defined('TYPO3_MODE') && $TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/cli/class.tx_mnogosearch_realmconfig.php'])	{
	include_once($TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/cli/class.tx_mnogosearch_realmconfig.php']);
}

?>