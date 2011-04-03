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
 * $Id: class.tx_mnogosearch_tsfeconfig.php 25978 2009-10-29 11:25:32Z dmitry $
 */


/**
 * This class inserts mnogosearch variables to linkVars if necessary
 *
 * @author	Dmitry Dulepov <dmitry.dulepov@gmail.com>
 * @package	TYPO3
 * @subpackage	tx_
 */
class tx_mnogosearch_tsfeconfig {

	/**
	 * Checks if mnogoseaqrch variables are set ptoperly. This is the best
	 * place between TS parsing and content generation.
	 *
	 * @return	void
	 */
	public function checkDataSubmission() {
		if (tx_mnogosearch_isIndexerRunning()) {
			if (!$this->hasMnogosearchLinkVar()) {
				$this->addMnogosearchLinkVar();
			}
			if (!$this->hasUniqueLinkVars()) {
				$this->addUniqueLinkVars();
			}
		}
	}

	/**
	 * Checks if variables are set up properly
	 *
	 * @return boolean
	 */
	protected function hasMnogosearchLinkVar() {
		$result = false;
		if (isset($GLOBALS['TSFE']->config['config']['linkVars']) &&
			isset($GLOBALS['TSFE']->config['config']['uniqueLinkVars']) &&
			intval($GLOBALS['TSFE']->config['config']['uniqueLinkVars'])) {
			$result = t3lib_div::inList($GLOBALS['TSFE']->config['config']['linkVars'], 'tx_mnogosearch_gid');
		}
		return $result;
	}

	/**
	 * Adds mnogosearch variable into linkVars.
	 *
	 * @return	void
	 */
	protected function addMnogosearchLinkVar() {
		$linkVars = '';
		if (isset($GLOBALS['TSFE']->config['config']['linkVars'])) {
			$linkVars = $GLOBALS['TSFE']->config['config']['linkVars'];
		}
		if ($linkVars != '') {
			$linkVars .= ',';
		}
		$linkVars .= 'tx_mnogosearch_gid';
		$GLOBALS['TSFE']->config['config']['linkVars'] = $linkVars;
	}

	/**
	 * Checks if variables are set up properly
	 *
	 * @return boolean
	 */
	protected function hasUniqueLinkVars() {
		return (isset($GLOBALS['TSFE']->config['config']['uniqueLinkVars']) &&
			intval($GLOBALS['TSFE']->config['config']['uniqueLinkVars']));
	}

	/**
	 * Sets uniqueLinkVars flag.
	 *
	 * @return	void
	 */
	protected function addUniqueLinkVars() {
		$GLOBALS['TSFE']->config['config']['uniqueLinkVars'] = '1';
	}
}

if (defined('TYPO3_MODE') && $TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/hooks/class.tx_mnogosearch_tsfeconfig.php'])	{
	include_once($TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/hooks/class.tx_mnogosearch_tsfeconfig.php']);
}

?>