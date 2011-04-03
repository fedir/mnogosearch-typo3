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
 * $Id: class.tx_mnogosearch_indexer.php 25754 2009-10-22 14:36:03Z dmitry $
 */


/**
 * This class contains mnoGoSearch indexer
 *
 * @author	Dmitry Dulepov <dmitry.dulepov@gmail.com>
 * @package	TYPO3
 * @subpackage	tx_mnogosearch
 */
class tx_mnogosearch_indexer {

	/**
	 * Command line options
	 *
	 * @var	tx_mnogosearch_clioptions
	 */
	protected $commandLineOptions;

	/**
	 * Configuration file name
	 *
	 * @var	string
	 */
	protected $configurationFileName;

	/**
	 * Configuration generator class
	 *
	 * @var	tx_mnogosearch_configgenerator
	 */
	protected $configurationGenerator;

	/**
	 * Path to the indexer binary
	 *
	 * @var	string
	 */
	protected $indexerPath;

	/**
	 * Creates an instance of this class
	 *
	 * @return	void
	 */
	public function __construct() {
		$sysconf = unserialize($GLOBALS['TYPO3_CONF_VARS']['EXT']['extConf']['mnogosearch']);
		$this->indexerPath = $sysconf['mnoGoSearchPath'] . '/sbin/indexer';
	}

	/**
	 * Cleans up
	 *
	 * @return	void
	 */
	public function __destruct() {
		if (!empty($this->configurationFileName)) {
			@unlink($this->configurationFileName);
		}
	}

	/**
	 * Runs a "convert blob" mnoGoSearch operation
	 *
	 * @return	void
	 */
	public function convertBlobs() {
		$this->execute('-Eblob');
	}

	/**
	 * Clears mnoGoSearch database
	 *
	 * @return	void
	 */
	public function clearDatabase() {
		$this->execute('-Cw');
	}

	/**
	 * Creates mnoGoSearch database
	 *
	 * @return	void
	 */
	public function createDatabase() {
		$this->execute('-Ecreate');
	}

	/**
	 * Creates misspelled word statistics.
	 *
	 * @return	void
	 */
	public function createWordStatistics() {
		$this->execute('-Ewordstat');
	}

	/**
	 * Obtains allowed indexing URLs for the current indexer
	 *
	 * @return	array
	 */
	public function getAllowedURLs() {
		return $this->configurationGenerator->getAllowedURLs();
	}

	/**
	 * Runs a regular indexing process
	 *
	 * @return	void
	 */
	public function index() {
		$this->checkIndexingConfigurations();
		$this->execute('-r');
	}

	/**
	 * Indexes URLs from the given file
	 *
	 * @param	string	$urlFileName
	 * @return	void
	 */
	public function indexURLs($urlFileName) {
		$this->checkIndexingConfigurations();
		$command = sprintf('-a -i -r -f %s', escapeshellarg($urlFileName));
		$this->execute($command);
	}

	/**
	 * Sets command line options object. We have to use this function instead of
	 * a constructor because of the stupid way TYPO3 4.3 deprecated
	 * t3lib_div::makeInstanceClassName().
	 *
	 * @param	tx_mnogosearch_clioptions	$options
	 * @return	void
	 */
	public function setCommandLineOptions(tx_mnogosearch_clioptions &$commandLineOptions) {
		$this->commandLineOptions = $commandLineOptions;
	}

	/**
	 * Sets configuration object and obtains configuration file name from it
	 *
	 * @param	string	$fileName
	 * @return	void
	 */
	public function setConfigurationGenerator(tx_mnogosearch_configgenerator &$configurationGenerator) {
		$this->configurationGenerator = $configurationGenerator;
		$this->configurationFileName = $configurationGenerator->createIndexerConfigurationFile();
	}

	/**
	 * Creates command line for the indexer
	 *
	 * @param	string	Indexer options for this command line (like -Ecreate)
	 * @return	string	Generated command line
	 */
	protected function buildCommandLine($options) {
		$extraOptions = '';
		if ($this->commandLineOptions->hasOption(tx_mnogosearch_clioptions::EXTFLAGS)) {
			$extraOptions = $this->commandLineOptions->getOptionParameter(tx_mnogosearch_clioptions::EXTFLAGS);
		}
		$commandLine = sprintf('%s %s %s -d %s %s',
			$this->indexerPath, $this->getSilenceOption(), $options,
			escapeshellarg($this->configurationFileName), $extraOptions);
		return trim($commandLine);
	}

	/**
	 * Checks that there is one or more indexing configurations. If not, throws
	 * an exception.
	 *
	 * @return	void
	 */
	protected function checkIndexingConfigurations() {
		if ($this->configurationGenerator->getConfigurationCount() == 0) {
			throw new Exception('No indexing configurations found. There is nothing to index.');
		}
	}

	/**
	 * Executes indexer binary with the given options
	 *
	 * @param	string	$options
	 * @return	void
	 */
	protected function execute($options) {
		$commandLine = $this->buildCommandLine($options);
		if ($this->commandLineOptions->hasOption(tx_mnogosearch_clioptions::DRYRUN)) {
			echo sprintf('Executing: %s' . chr(10), $commandLine);
		}
		else {
			@exec($commandLine);
		}
	}

	/**
	 * Checks if silence option is passed and sets internal variable accordingly
	 *
	 * @return void
	 */
	protected function getSilenceOption() {
		if ($this->commandLineOptions->hasOption(tx_mnogosearch_clioptions::VERBOSE)) {
			$verbosityLevel = intval($this->commandLineOptions->getOptionParameter(tx_mnogosearch_clioptions::VERBOSE));
			if ($verbosityLevel > 0) {
				return '-v ' . max(1, min(5, $verbosityLevel));
			}
		}
		return '';
	}
}

if (defined('TYPO3_MODE') && $TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/cli/class.tx_mnogosearch_indexer.php'])	{
	include_once($TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/cli/class.tx_mnogosearch_indexer.php']);
}

?>