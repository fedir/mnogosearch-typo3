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
 * $Id: class.tx_mnogosearch_serverconfig.php 25950 2009-10-28 15:29:06Z dmitry $
 */

/**
 * This class implements a server config for the indexer.
 *
 * @author	Dmitry Dulepov <dmitry.dulepov@gmail.com>
 * @package	TYPO3
 * @subpackage	tx_mnogosearch
 */
class tx_mnogosearch_serverconfig extends tx_mnogosearch_baseconfig {

	/**
	 * Obtains configuration lines ending with chr(10).
	 *
	 * @return string
	 */
	public function getConfigurationLines() {
		$result = 'Server ';
		$result .= $this->getMethodAsString();
		$result .= $this->getSubSection();
		$result .= $this->injectURLParameter($this->data['url']);
		$result .= chr(10);

		return $result;
	}

	/**
	 * Obtains URls explicitely allowed by this configuration.
	 *
	 * @return	array
	 */
	public function getAllowedServerURLs() {
		$result = array();
		$method = $this->data['method'];
		if ($method != self::DISALLOW && $method != self::SKIP) {
			$result[] = $this->data['url'];
		}
		return $result;
	}

	/**
	 * Injects mnoGoSearch group parameter into the URL.
	 *
	 * @param	string	$url	URL
	 * @return	string	Modified URL
	 */
	protected function injectURLParameter($url) {
		if ($this->currentUserGroup != 0) {
			$groupParameter = 'tx_mnogosearch_gid=' . $this->currentUserGroup;

			if (($pos = strpos($url, '?')) === false) {
				$url .= '?' . $groupParameter;
			}
			elseif ($pos == strlen($url) - 1) {
				$url .= $groupParameter;
			}
			else {
				$url = substr($url, 0, $pos + 1) . $groupParameter .
					'&' . substr($url, $pos + 1);
			}
		}
		return $url;
	}
}

if (defined('TYPO3_MODE') && $TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/cli/class.tx_mnogosearch_serverconfig.php'])	{
	include_once($TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/cli/class.tx_mnogosearch_serverconfig.php']);
}

?>