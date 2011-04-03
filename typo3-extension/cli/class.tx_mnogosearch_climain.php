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
 * $Id: class.tx_mnogosearch_climain.php 25754 2009-10-22 14:36:03Z dmitry $
 */

if (!defined('TYPO3_MODE')) {
	echo 'Please use "./typo3/cli_dispatch.phpsh mnogosearch -?" for help. Thank you.' . chr(10);
	exit;
}

require_once(t3lib_extMgm::extPath('mnogosearch', 'cli/class.tx_mnogosearch_clioptions.php'));
require_once(t3lib_extMgm::extPath('mnogosearch', 'cli/class.tx_mnogosearch_configgenerator.php'));
require_once(t3lib_extMgm::extPath('mnogosearch', 'cli/class.tx_mnogosearch_dboperations.php'));
require_once(t3lib_extMgm::extPath('mnogosearch', 'cli/class.tx_mnogosearch_indexer.php'));

/**
 * This class is a main mnoGoSearch CLI handler
 *
 * @author	Dmitry Dulepov <dmitry.dulepov@gmail.com>
 * @package	TYPO3
 * @subpackage	tx_mnogosearch
 */
class tx_mnogosearch_climain {

	/**
	 * Command line options object
	 *
	 * @var tx_mnogosearch_clioptions
	 */
	protected $commandLineOptions;

	/**
	 * Shows if we are in "dry-run" mode
	 *
	 * @var boolean
	 */
	protected $dryRun;

	/**
	 * mnoGoSearch indexer instance
	 *
	 * @var	tx_mnogosearch_indexer
	 */
	protected $indexer;

	/**
	 * Creates an instance of this class
	 *
	 * @return	void
	 */
	public function __construct() {
		$this->createCommandLineOptions();
		$this->dryRun = $this->commandLineOptions->hasOption(tx_mnogosearch_clioptions::DRYRUN);
	}

	/**
	 * Runs mnoGoSearch command line handler
	 *
	 * @return	void
	 */
	public function run() {
		$this->checkForHelp();

		try {
			$this->getAndRunTasks();
		}
		catch(Exception $e) {
			$this->printException($e);
		}
	}

	/**
	 * Checks if help message should be displayed. If yes, shows this message
	 * and aborts execution
	 *
	 * @return	void
	 */
	protected function checkForHelp() {
		if ($this->commandLineOptions->hasOption(tx_mnogosearch_clioptions::HELP)) {
			$this->showUsageAndExit();
		}
	}

	/**
	 * Converts blobs
	 *
	 * @return	void
	 */
	protected function convertBlobs() {
		$sysconf = unserialize($GLOBALS['TYPO3_CONF_VARS']['EXT']['extConf']['mnogosearch']);
		if (stristr($this->sysconf['dbaddr'], 'dbmode=blob')) {
			$this->showProgressMessage('Going to convert blobs.');
			$this->indexer->convertBlobs();
		}
	}

	/**
	 * Parses command line options.
	 *
	 * @return	void
	 */
	protected function createCommandLineOptions() {
		try {
			$this->commandLineOptions = t3lib_div::makeInstance('tx_mnogosearch_clioptions');
		}
		catch(Exception $e) {
			echo $e->getMessage() . chr(10);
			$this->showUsageAndExit();
		}
	}

	/**
	 * Creates indexer object
	 *
	 * @return	void
	 */
	protected function createIndexer() {
		$this->showProgressMessage('Going to create indexer.');
		$this->indexer = t3lib_div::makeInstance('tx_mnogosearch_indexer');
		$this->indexer->setCommandLineOptions($this->commandLineOptions);
		$this->indexer->setConfigurationGenerator($this->getIndexerConfigurationGenerator());
	}

	/**
	 * Creates statistics for misspelled words if necessary
	 *
	 * @return	void
	 */
	protected function createWordStatistics() {
		if ($this->commandLineOptions->hasOption(tx_mnogosearch_clioptions::SPELLSTATS)) {
			$this->showProgressMessage('Going to create word statistics.');
			$this->indexer->createWordStatistics();
		}
	}

	/**
	 * Displays configuration
	 *
	 * @return	void
	 */
	protected function displayConfiguration() {
		$this->showProgressMessage('Going to retrieve indexing configurations.');
		$generator = $this->getIndexerConfigurationGenerator();
		echo $generator->getIndexerConfiguration() . chr(10);
	}

	/**
	 * Generates URL list file and returns its name or an empty string if there are no URLs
	 *
	 * @return	string
	 */
	protected function generateURLListFile() {
		$content = '';
		$allowedURLs = $this->indexer->getAllowedURLs();
		$recordSet = $GLOBALS['TYPO3_DB']->exec_SELECTquery('uid,tx_mnogosearch_url',
			'tx_mnogosearch_urllog', '');
		$GLOBALS['TYPO3_DB']->sql_query('START TRANSACTION');
		while (false !== ($row = $GLOBALS['TYPO3_DB']->sql_fetch_assoc($recordSet))) {
			$addUrl = false;
			foreach ($allowedURLs as $allowedURL) {
				if (substr($allowedURL, 0, 6) == 'htdb:/') {
					$addUrl = ($allowedURL == dirname($row['tx_mnogosearch_url']));
				}
				elseif (substr($row['tx_mnogosearch_url'], 0, 6) != 'htdb:/') {
					if ($allowedURL{0} == '/') {
						// regexp
						$addUrl = preg_match($allowedURL, $row['tx_mnogosearch_url']);
					}
					elseif (strpos($allowedURL, '*')) {
						// wildcard
						$regexp = str_replace('/', '\/', str_replace('*', '.*', $allowedURL));
						$addUrl = preg_match('/' . $regexp . '/', $row['tx_mnogosearch_url']);
					}
					elseif (strpos($row['tx_mnogosearch_url'], $allowedURL) === 0) {
						// normal string
						$addUrl = true;
					}
				}
				if ($addUrl) {
					$content .= $row['tx_mnogosearch_url'] . chr(10);
					$GLOBALS['TYPO3_DB']->exec_DELETEquery('tx_mnogosearch_urllog', 'uid=' . $row['uid']);
					break;
				}
			}
		}
		$GLOBALS['TYPO3_DB']->sql_free_result($recordSet);
		$name = '';
		if ($content) {
			$name = tempnam(sys_get_temp_dir(), 'mnogo-');
			if (!@file_put_contents($name, $content)) {
				$GLOBALS['TYPO3_DB']->sql_query('ROLLBACK');
				throw new Exception(sprintf('Unable to create URL list file "%s"', $name));
			}
		}
		$GLOBALS['TYPO3_DB']->sql_query('COMMIT');

		return $name;
	}

	/**
	 * Checks for tasks and runs them.
	 *
	 * @return	void
	 */
	protected function getAndRunTasks() {
		if ($this->commandLineOptions->hasOption(tx_mnogosearch_clioptions::DISPLAYCONFIG)) {
			$this->displayConfiguration();
		}
		else {
			$this->createIndexer();

			if ($this->commandLineOptions->hasOption(tx_mnogosearch_clioptions::CLEARDB)) {
				$this->indexer->clearDatabase();
			}
			elseif ($this->commandLineOptions->hasOption(tx_mnogosearch_clioptions::CREATEDB)) {
				$this->runDatabaseCheck();
			}
			else {
				$this->index();
			}
		}
	}

	/**
	 * Obtains indexer configuration file name
	 *
	 * @return	string
	 */
	protected function getIndexerConfigurationGenerator() {
		$this->showProgressMessage('Going to get indexing configurations.');
		$configGenerator = t3lib_div::makeInstance('tx_mnogosearch_configgenerator');
		/* @var $configGenerator tx_mnogosearch_configgenerator */
		$configGenerator->setCommandLineOptions($this->commandLineOptions);
		return $configGenerator;
	}

	/**
	 * Runs regular indexing process
	 *
	 * @return	void
	 */
	protected function index() {
		$this->showProgressMessage('Going to index.');
		$this->indexer->index();
		$this->reindexNewURLs();
		$this->convertBlobs();
		$this->createWordStatistics();
	}

	/**
	 * Prints exception information. If verbosity level is highest, prints also
	 * stack trace.
	 *
	 * @param	Exception	$e
	 * @return	void
	 */
	protected function printException(Exception $e) {
		echo $e->getMessage() . chr(10);

		if ($this->commandLineOptions->hasOption(tx_mnogosearch_clioptions::VERBOSE)) {
			if ($this->commandLineOptions->getOptionParameter(tx_mnogosearch_clioptions::VERBOSE) >= 5) {
				print_r($e->getTrace());
			}
		}
	}

	/**
	 * Reindexes new or modified URLs
	 *
	 * @return	void
	 */
	protected function reindexNewURLs() {
		if ($this->commandLineOptions->hasOption(tx_mnogosearch_clioptions::REINDEX)) {
			$this->showProgressMessage('Going to reindex new URLs.');
			$urlFileName = $this->generateURLListFile();
			if ($urlFileName != '') {
				$this->indexer->indexURLs($urlFileName);
				@unlink($urlFileName);
			}
			else {
				$this->showProgressMessage('No new URLs found.');
			}
		}
	}

	/**
	 * Runs database check over mnoGoSearch database
	 *
	 * @return	void
	 */
	protected function runDatabaseCheck() {
		$this->showProgressMessage('Going to run database check.');

		$dbOperations = t3lib_div::makeInstance('tx_mnogosearch_dboperations');
		/* @var $dbOperations tx_mnogosearch_dboperations */
		$dbOperations->setIndexer($this->indexer);
		$dbOperations->checkAndCreate();
	}

	/**
	 * Shows progress message in case of a dry-run mode. This function accepts
	 * variable number of parameters and will format than sprintf-like.
	 *
	 * @param	string	$message
	 * @return	void
	 */
	protected function showProgressMessage($message) {
		if ($this->dryRun) {
			$arguments = func_get_args();
			if (count($arguments) > 1) {
				array_shift($arguments);
				$message = vsprintf($message, $arguments);
			}
			echo $message . chr(10);
		}
	}

	/**
	 * Shows script usage information and aborts execution.
	 *
	 * @return	void
	 */
	protected function showUsageAndExit() {
			echo 'Usage: cli_mnogosearch.phpsh [-n] [-w]

This script reindexes TYPO3 web sites as defined by current TYPO3 configuration.
It accepts the following options:

-c                Only check and create database if necessary. Do not reindex.
-d                Display generated indexer configuration and exit.
-n                Force reindexing of new URLs (normally should be set)
-p pid            Process indexing configuration only from this pid
-w                Create statistic for misspelled words. Useful only if
.                   Ispell dictionaries are included to mnoGoSearch
.                   configuration (see mnoGoSearch documentation)
--dry-run         Show what will be done (not applicable to -d and -E)
-h, --help, -?    Display this help message
-x                Pass the argument to mnoGoSearch indexer
-v level          Be verbose. Level is 0-5. Default is 0 (complete silence)
-z                Remove all indexed data from the database
';
			exit(1);
	}

}

if (defined('TYPO3_MODE') && $TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/cli/class.tx_mnogosearch_climain.php'])	{
	include_once($TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/cli/class.tx_mnogosearch_climain.php']);
}

$cli = t3lib_div::makeInstance('tx_mnogosearch_climain');
/* @var $cli tx_mnogosearch_climain */
$cli->run();

?>