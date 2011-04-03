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
 * $Id: class.tx_mnogosearch_configgenerator.php 25946 2009-10-28 14:33:03Z dmitry $
 */

require_once(t3lib_extMgm::extPath('mnogosearch', 'cli/class.tx_mnogosearch_baseconfig.php'));
require_once(t3lib_extMgm::extPath('mnogosearch', 'cli/class.tx_mnogosearch_serverconfig.php'));
require_once(t3lib_extMgm::extPath('mnogosearch', 'cli/class.tx_mnogosearch_realmconfig.php'));
require_once(t3lib_extMgm::extPath('mnogosearch', 'cli/class.tx_mnogosearch_recordconfig.php'));

/**
 * This class generates configuration file for the current indexer's session.
 *
 * @author	Dmitry Dulepov <dmitry.dulepov@gmail.com>
 * @package	TYPO3
 * @subpackage	tx_mnogosearch
 */
class tx_mnogosearch_configgenerator {

	/**
	 * Allowed URLs for the processed configurations
	 *
	 * @var	array
	 */
	protected $allowedURLs = array();

	/**
	 * Command line options
	 *
	 * @var	tx_mnogosearch_clioptions
	 */
	protected $commandLineOptions;

	/**
	 * A number of found indexing configurations
	 *
	 * @var int
	 */
	protected $configurationCount = 0;

	/**
	 * Generated configuration content
	 *
	 * @var	string
	 */
	protected $generatedContent = '';

	/**
	 * Creates indexer configuration file
	 *
	 * @return	string	File anme
	 */
	public function createIndexerConfigurationFile() {
		$this->generatedContent = '';
		$this->createIndexerBasicConfig();
		$this->processIndexingConfigurations();

		$fileName = tempnam(sys_get_temp_dir(), 'mnogo-');
		if (!@file_put_contents($fileName, $this->generatedContent)) {
			throw new Exception(sprintf('Cannot write configuration to %s', $fileName));
		}

		return $fileName;
	}

	/**
	 * Obtains a list of allowed URLs
	 *
	 * @return	array
	 */
	public function getAllowedURLs() {
		return $this->allowedURLs;
	}

	/**
	 * Obtains a number of ound indexing configurations
	 *
	 * @return	void
	 */
	public function getConfigurationCount() {
		return $this->configurationCount;
	}

	/**
	 * Obtains generated indexed configuration as a string
	 *
	 * @return	string
	 */
	public function getIndexerConfiguration() {
		$this->generatedContent = '';
		$this->createIndexerBasicConfig();
		$this->processIndexingConfigurations();

		return $this->generatedContent;
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
	 * Creates basic indexer configuration
	 *
	 * @return	void
	 */
	protected function createIndexerBasicConfig() {
		$sysconf = unserialize($GLOBALS['TYPO3_CONF_VARS']['EXT']['extConf']['mnogosearch']);
		$this->generatedContent .= '# Defaults
DBAddr ' . $sysconf['dbaddr'] . '
VarDir ' . PATH_site . 'typo3temp/mnogosearch/var
PopRankUseShowCnt yes
PopRankUseTracking yes
PopRankFeedBack yes
PopRankSkipSameSite no
Section body 1 8092 html cdon
Section title 2 256 text cdoff
Section meta.keywords 3 64 text cdoff
Section meta.description 4 64 text cdoff
SaveSectionSize yes
LocalCharset ' . ($sysconf['LocalCharset'] ? $sysconf['LocalCharset'] : 'UTF-8') . '
BrowserCharset ' . ($sysconf['BrowserCharset'] ? $sysconf['BrowserCharset'] : 'UTF-8') . '

#Allow *.htm *.html *.php *.txt */ *.pdf *.doc *.odt *.swx
Mime message/rfc822 text/plain "cat $1"
Mime application/msword text/plain "catdoc -a $1"
Mime application/pdf text/plain "pdftotext -enc UTF-8 $1 -"
Mime application/vnd.ms-excel text/html "xltohtml $1"
Mime application/vnd.ms-powerpoint text/html "pptohtml $1"
' . $this->getOpenOfficeCommands() . '

Disallow *.rdf *.xml *.rss *.js *.css *.jpg *.png *.gif *.svg *.swf *.tif *.tiff
Disallow *.bz2 *.gz *.zip *.xpi *.dmg *.exe *.scr *.avi *.jpeg *.ods *.psd *.rar
Disallow *.fla Thumbs.db *.rpm *.rm *.qt *.mov *.pfb *.ttf* *.fon *.mpg *.mpeg *.ai
Disallow *.t3x *.t3d *.dll *.dat *.cab *.inf

AddType application/vnd.oasis.opendocument.text *.sxw *.odt
AddType text/plain *.txt
AddType text/html *.html *.htm
AddType text/rtf *.rtf
AddType application/pdf *.pdf
AddType message/rfc822 *.eml
AddType application/msword *.doc *.dot
AddType application/vnd.ms-excel *.xls
AddType application/vnd.ms-powerpoint *.ppt

HoldBadHrefs 2d
DetectClones yes
HTTPHeader "X-TYPO3-mnogosearch: ' . md5('mnogosearch' . $GLOBALS['TYPO3_CONF_VARS']['SYS']['encryptionKey']) . '"
DefaultContentType "text/html' .
			($GLOBALS['TYPO3_CONF_VARS']['BE']['forceCharset'] ? '; charset=' .
				$GLOBALS['TYPO3_CONF_VARS']['BE']['forceCharset'] : '') . '"' . chr(10);

		if ($GLOBALS['TYPO3_CONF_VARS']['BE']['forceCharset']) {
			$this->generatedContent .= 'RemoteCharset ' . $GLOBALS['TYPO3_CONF_VARS']['BE']['forceCharset'] . chr(10);
		}
		if ($sysconf['IncludeFile']) {
			$this->generatedContent .= 'Include ' . $sysconf['IncludeFile'] . chr(10);
		}
	}

	/**
	 * Obtains all indexing configurations from the database.
	 *
	 * @return	array
	 */
	protected function getIndexingConfigurations() {
		$rows = $GLOBALS['TYPO3_DB']->exec_SELECTgetRows('*', 'tx_mnogosearch_indexconfig',
					$this->getPidCondition() . t3lib_BEfunc::deleteClause('tx_mnogosearch_indexconfig'),
					'', 'sorting');
		$result = array();
		foreach ($rows as $row) {
			$result[] = $this->getIndexingConfigFromRow($row);
		}

		return $result;
	}

	/**
	 * Obtains configuration class name from the configuration type
	 *
	 * @param	int	$type
	 * @param	int	$id
	 * @return	string
	 */
	protected function getConfigClassFromType($type, $id) {
		switch ($type) {
			case 0:
				$className = 'tx_mnogosearch_serverconfig';
				break;
			case 1:
				$className = 'tx_mnogosearch_realmconfig';
				break;
			case 11:
				$className = 'tx_mnogosearch_recordconfig';
				break;
			default:
				throw new Exception(sprintf('Unknown indexing configuration ' .
					'type %1$d for the record with uid=%2$d', $type, $id));
		}
		return $className;
	}

	/**
	 * Creates indexing configuration from a database row
	 *
	 * @return	tx_mnogosearch_baseconfig
	 */
	protected function getIndexingConfigFromRow(array $row) {
		$className = $this->getConfigClassFromType($row['tx_mnogosearch_type'], $row['uid']);
		$model= new $className();
		/* @var $model tx_mnogosearch_baseconfig */
		$model->setData($row);
		return $model;
	}

	/**
	 * Obtains OpenOffice indexer command (if possible)
	 *
	 * @return void
	 */
	protected function getOpenOfficeCommands() {
		// Get path to PHP interpreter
		if (basename($_ENV['_']) == 'php') {
			$phpPath = $_ENV['_'];
		}
		else {
			$phpPath = trim(`which php`);
		}
		$openOfficeCmd = '';
		if ($phpPath) {
			$openOfficeCmd = 'Mime application/vnd.oasis.opendocument.text text/plain ' .
						'"' . $phpPath . ' -q ' .
						escapeshellarg(dirname(__FILE__) . '/catoo.php') .
						' \'$1\'"';
		}
		return $openOfficeCmd;
	}

	/**
	 * Sets indexing period to the current configuration block. If period is set
	 * by the configuration record, $resetPeriod will be set to reset it for the
	 * next block
	 *
	 * @param	tx_mnogosearch_baseconfig	$configuration
	 * @param	boolean	$resetPeriod
	 * @return	void
	 */
	protected function getPeriod(tx_mnogosearch_baseconfig &$configuration, &$resetPeriod) {
		if ($configuration->hasPeriod()) {
			$this->generatedContent .= 'Period ' . $configuration->getPeriod();
			$resetPeriod = true;
		}
		elseif ($resetPeriod) {
			$this->generatedContent .= 'Period 10y';
			$resetPeriod = false;
		}
		$this->generatedContent .= chr(10);
	}

	/**
	 * Obtains pid from the command line and generates WHERE condition accordingly.
	 *
	 * @return	string
	 */
	protected function getPidCondition() {
		$pidCondition = '1=1';
		if ($this->commandLineOptions->hasOption(tx_mnogosearch_clioptions::PID)) {
			$pid = $this->commandLineOptions->getOptionParameter(tx_mnogosearch_clioptions::PID);
			if (t3lib_div::testInt($pid)) {
				$pidCondition = 'pid=' . $pid;
			}
			else {
				throw new Exception('-p accepts only integer page id numbers');
			}
		}
		return $pidCondition;
	}

	/**
	 * Walks list of servers and generates indexer configuration for them
	 *
	 * @return	string	Configuration text
	 */
	protected function processIndexingConfigurations() {
		$configurations = $this->getIndexingConfigurations();
		$this->configurationCount = count($configurations);
		$resetPeriod = true;
		foreach ($configurations as $configuration) {
			/* @var $configuration tx_mnogosearch_baseconfig */
			$this->generatedContent .= sprintf('# uid=%d' . chr(10), $configuration->getId());
			$this->generatedContent .= $this->getPeriod($configuration, $resetPeriod);
			$this->generatedContent .= $configuration->generate();
			$this->allowedURLs += $configuration->getAllowedServerURLs();
		}
	}
}

if (defined('TYPO3_MODE') && $TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/cli/class.tx_mnogosearch_configgenerator.php'])	{
	include_once($TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/cli/class.tx_mnogosearch_configgenerator.php']);
}

?>