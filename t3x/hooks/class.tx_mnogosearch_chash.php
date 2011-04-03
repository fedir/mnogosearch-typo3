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
 * $Id: class.tx_mnogosearch_chash.php 25712 2009-10-21 12:13:35Z dmitry $
 */


/**
 * This class implements a cHash post processing hook. It will exclude our
 * parameters from cHash calculation.
 *
 * @author	Dmitry Dulepov <dmitry.dulepov@gmail.com>
 * @package	TYPO3
 * @subpackage	tx_mnogosearch
 */
class tx_mnogosearch_chash {

	/**
	 * Updates cHash parameter list removing mnoGoSearch own parameter if
	 * necessary.
	 *
	 * @param	array	$params	Hook parameters
	 * @return	void
	 */
	public function updateCHashParameters(array &$params) {
		if (isset($params['pA']['tx_mnogosearch_gid'])) {
			unset($params['pA']['tx_mnogosearch_gid']);
		}
	}

}

if (defined('TYPO3_MODE') && $TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/hooks/class.tx_mnogosearch_chash.php'])	{
	include_once($TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/hooks/class.tx_mnogosearch_chash.php']);
}

?>