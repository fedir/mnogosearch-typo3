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
 * $Id: class.tx_mnogosearch_clioptions.php 25754 2009-10-22 14:36:03Z dmitry $
 */


/**
 * This class implements a command line option parser
 *
 * @author	Dmitry Dulepov <dmitry.dulepov@gmail.com>
 * @package	TYPO3
 * @subpackage	tx_mnogosearch
 */
class tx_mnogosearch_clioptions {

	const CLEARDB = '-z';
	const CREATEDB = '-c';
	const DISPLAYCONFIG = '-d';
	const DRYRUN = '--dry-run';
	const EXTFLAGS = '-x';
	const HELP = '-?';
	const REINDEX = '-n';
	const PID = '-p';
	const SPELLSTATS = '-w';
	const VERBOSE = '-v';

	/**
	 * Parsed option list
	 *
	 * @var array
	 */
	protected $options = array();

	/**
	 * Parses command line options
	 *
	 * @return	void
	 */
	public function __construct() {
		for ($i = 2; $i < $GLOBALS['argc']; $i++) {
			$option = $this->fetchOption($i);
			$this->options[$option] = $this->fetchOptionParameter($option, $i);
		}
	}

	/**
	 * Obtains the value of the option
	 *
	 * @param	string	$option	Option
	 * @return	string
	 */
	public function getOptionParameter($option) {
		if (!$this->hasOption($option)) {
			throw new Exception(sprintf('Command line option "%s" is not set', $option));
		}
		return $this->options[$option];
	}

	/**
	 * Checks if option exists
	 *
	 * @param	string	$option
	 * @return	boolean
	 */
	public function hasOption($option) {
		return isset($this->options[$option]);
	}

	/**
	 * Gets the next option from the command line
	 *
	 * @param	int	$optionPosition
	 * @return	string
	 */
	protected function fetchOption($optionPosition) {
		$option = $GLOBALS['argv'][$optionPosition];
		if (!$this->isValidOption($option)) {
			throw new Exception(sprintf('Bad command line option: "%s" at position %d', $GLOBALS['argv'][$optionPosition], $optionPosition));
		}
		return $option;
	}

	/**
	 * Fetches option parameter (if any) and advances option position (if
	 * necessary).
	 *
	 * @param	string	$option
	 * @param	int	$optionPosition
	 * @return	string
	 */
	protected function fetchOptionParameter($option, &$optionPosition) {
		$result = '';
		if ($this->hasParameter($option)) {
			$result = $GLOBALS['argv'][++$optionPosition];
		}
		return $result;
	}

	/**
	 * Checks if the option has parameters
	 *
	 * @param	string	$option
	 */
	protected function hasParameter($option) {
		return (strchr('pvx', $option{1}) !== false);
	}

	/**
	 * Checks if the option is valid
	 *
	 * @param	string	$option
	 * @return	boolean
	 */
	protected function isValidOption($option) {
		if ($option{0} != '-') {
			return false;
		}

		if ($option == '--dry-run') {
			return true;
		}

		return (strchr('?cdnpvwxz', $option{1}) !== false);
	}
}

if (defined('TYPO3_MODE') && $TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/cli/class.tx_mnogosearch_clioptions.php'])	{
	include_once($TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/cli/class.tx_mnogosearch_clioptions.php']);
}

?>