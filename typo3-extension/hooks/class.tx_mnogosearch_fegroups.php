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
 * $Id: class.tx_mnogosearch_fegroups.php 25977 2009-10-29 11:06:35Z dmitry $
 */


/**
 * This class sets FE user groups to TSFE as necessary.
 *
 * @author	Dmitry Dulepov <dmitry.dulepov@gmail.com>
 * @package	TYPO3
 * @subpackage	tx_mnogosearch
 */
class tx_mnogosearch_fegroups {

	/**
	 * Sets user groups to TSFE if necessary and if mnogosearch indexer is running.
	 *
	 * @return	void
	 */
	public function setFEGroups() {
		if (tx_mnogosearch_isIndexerRunning()) {
			$group = t3lib_div::_GET('tx_mnogosearch_gid');
			if (!empty($group)) {
				if (!is_array($GLOBALS['TSFE']->fe_user->user))	{
					$GLOBALS['TSFE']->fe_user->user = array();
				}
				$GLOBALS['TSFE']->fe_user->user['usergroup'] = $group;
			}
		}
	}
}

if (defined('TYPO3_MODE') && $TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/hooks/class.tx_mnogosearch_fegroups.php'])	{
	include_once($TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/hooks/class.tx_mnogosearch_fegroups.php']);
}

?>