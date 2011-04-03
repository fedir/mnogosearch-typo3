<?php
/***************************************************************
*  Copyright notice
*
*  (c) 2007 Dmitry Dulepov (dmitry@typo3.org)
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
 * Hooks to tt_news.
 *
 * $Id: class.tx_mnogosearch_ttnews.php 14803 2008-12-10 15:30:40Z dmitry $
 */
/**
 * [CLASS/FUNCTION INDEX of SCRIPT]
 *
 */

/**
 * This clas provides hook to tt_news to set page modification date correctly
 *
 * @author	Dmitry Dulepov <dmitry@typo3.org>
 * @package TYPO3
 * @subpackage comments
 */
class tx_mnogosearch_ttnews {
	/**
	 * Sets correct page modification date according to the current news item.
	 *
	 * @param	array		$markerArray	Array with merkers
	 * @param	array		$row	tt_news record
	 * @param	array		$lConf	Configuration array for current tt_news view
	 * @param	tx_ttnews		$pObj	Reference to parent object
	 * @return	array		Modified marker array
	 */
	function extraItemMarkerProcessor($markerArray, $row, $lConf, &$pObj) {
		/* @var $pObj tx_ttnews */
		$pObj->cObj->lastChanged(max($row['tstamp'], $row['datetime']));
		return $markerArray;
	}
}

if (defined('TYPO3_MODE') && $TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/hooks/class.tx_mnogosearch_ttnews.php']) {
	include_once($TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/hooks/class.tx_mnogosearch_ttnews.php']);
}

?>