<?php
/***************************************************************
*  Copyright notice
*
*  (c) 2008 Dmitry Dulepov <dmitry@typo3.org>
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
 * $Id: cli_mnogosearch.php 45325 2011-03-21 10:05:51Z fedir $
 */
if (!defined('TYPO3_cliMode')) {
	die('This script cannot be executed directly. Use "typo3/cli_dispatch.phpsh mnogosearch help" for details.');
}

/**
 * This class implements mnoGoSearch command line processing script.
 *
 * @author	Dmitry Dulepov	<dmitry@typo3.org>
 */
class tx_mnogosearch_cli {

	/** By default make the indexer completely silent */
	protected $silenceOption = '-l';

	/**
	 * Extension configuration
	 *
	 * @var	array
	 */
	protected $sysconf;

	/** Configuration file name. This file is in the temporary directory */
	protected $configFileName;

	/** "Server" directive methods */
	protected $server_methods = array(1 => 'Disallow', 2 => 'HrefOnly', 3 => 'CheckOnly ', 4 => 'Skip');

	/** "Realm" subsections */
	protected $realm_subsections = array(1 => 'site', 2 => 'world', 3 => 'page');

	/** Extra arguments to pass to the indexer */
	protected $extraIndexerArguments = '';

	/**
	 * List of aloowed Server URLs. Etries from this list are compared to the
	 * URL log. If URL in the log does not match, it will be kept in the log.
	 * It is useful if pid is specified.
	 *
	 * @var	array
	 */
	protected $allowedServerURLs = array();

	/**
	 * Creates an instance of this class.
	 *
	 * @return	void
	 */
	public function __construct() {
		$this->sysconf = unserialize($GLOBALS['TYPO3_CONF_VARS']['EXT']['extConf']['mnogosearch']);
		$this->configFileName = $this->getTempFileName();
		$this->getSilenceOption();
		$this->getExtraIndexerArguments();
	}

	/**
	 * Creates temporary file name in the system temporary directory
	 *
	 * @return	string	Temporary file name
	 */
	protected function getTempFileName() {
		return tempnam(is_callable('sys_get_temp_dir') ? sys_get_temp_dir() : '/tmp', 'mnogosearch-');
	}

	/**
	 * Checks if silence option is passed and sets internal variable accordingly
	 *
	 * @return void
	 */
	protected function getSilenceOption() {
		if (($key = array_search('-v', $GLOBALS['argv']))) {
			if (t3lib_div::testInt($GLOBALS['argv'][$key + 1]{0})) {
				if ($GLOBALS['argv'][$key + 1] != '0') {
					$this->silenceOption = '-v ' . max(1, min(5, intval($GLOBALS['argv'][$key + 1])));
				}
			}
		}
	}

	/**
	 * Retrieves additional indexer argumants
	 *
	 * @return	void
	 */
	protected function getExtraIndexerArguments() {
		if (($key = array_search('-x', $GLOBALS['argv']))) {
			$this->extraIndexerArguments = trim($GLOBALS['argv'][$key + 1], '"');
		}
	}

	/**
	 * Cheks that mnoGoSearch database tables exist and create it if necessary.
	 *
	 * @return	void
	 */
	protected function checkAndCreateDatabase() {
		$parts = parse_url($this->sysconf['dbaddr']);
		$new_link = true;
		$hasTables = false;
		if ($parts['scheme'] == 'mysql') {
			$conn = mysql_connect($parts['host'], $parts['user'], $parts['pass'], $new_link);
			if (mysql_errno() == 0) {
				$dbname = trim($parts['path'], '/');
				$rs = mysql_query('SHOW TABLES FROM `' . mysql_real_escape_string($dbname, $conn) . '` LIKE \'qcache\'', $conn);
				if (mysql_errno() == 0) {
					$hasTables = (mysql_num_rows($rs) > 0);
					mysql_free_result($rs);
				}
				else {
					printf("MySQL error #%d: %s\n", mysql_errno($conn), mysql_error($conn));
					mysql_close($conn);
					exit;
				}
				mysql_close($conn);
			}
		}
		if (!$hasTables) {
			$cmdLine = $this->sysconf['mnoGoSearchPath'] . '/sbin/indexer ' . $this->silenceOption . ' -Ecreate -d ' . $this->configFileName . ' ' . $GLOBALS['extraOptions'];
			if (in_array('--dry-run', $GLOBALS['argv'])) {
				echo 'Tables are missing from database, will be created!';
				echo 'Executing: ' . $cmdLine . chr(10);
			}
			else {
				@exec($cmdLine);
			}
		}
	}

	/**
	 * Walks list of servers and generates indexer configuration for them
	 *
	 * @return	string	Configuration text
	 */
	protected function getServers() {
		$content = ''; $pidCondition = '1=1';
		if (($key = array_search('-p', $GLOBALS['argv']))) {
			if (t3lib_div::testInt($GLOBALS['argv'][$key + 1])) {
				$pidCondition = 'pid=' . $GLOBALS['argv'][$key + 1];
			}
		}
		$rows = $GLOBALS['TYPO3_DB']->exec_SELECTgetRows('*', 'tx_mnogosearch_indexconfig',
					$pidCondition . t3lib_BEfunc::deleteClause('tx_mnogosearch_indexconfig') /*.
						t3lib_BEfunc::BEenableFields('tx_mnogosearch_indexconfig')*/,
					'', 'sorting');

		$hasPeriod = false;
		foreach ($rows as $row) {
			$content .= '# uid=' . $row['uid'] . chr(10);
			if ($row['tx_mnogosearch_period'] != '0' && $row['tx_mnogosearch_period'] != '') {
				$content .= 'Period ' .
					(t3lib_div::testInt($row['tx_mnogosearch_period']) ?
						$row['tx_mnogosearch_period'] . 'h' :
						$row['tx_mnogosearch_period']) . chr(10);
				$hasPeriod = true;
			}
			elseif (!$hasPeriod) {
				$content .= 'Period 10y' . chr(10);
				$hasPeriod = true;
			}

			// Add any extra config. This must go *before* servers!
			if (trim($row['tx_mnogosearch_additional_config'])) {
				$content .= $row['tx_mnogosearch_additional_config'] . chr(10);
			}

			// Add servers
			switch ($row['tx_mnogosearch_type']) {
				case 0:
					// Server
					$content .= $this->getServersGetServer($row);
					break;
				case 1:
					// Realm
					$content .= $this->getServersGetRealm($row);
					break;
				case 11:
					// Database table
					$content .= $this->getServersGetRecords($row);
					break;
				default:
					@unlink($this->configFileName);
					die(sprintf('Unknown configuration type %d for configuration entry with id %d' . chr(10), $row['tx_mnogosearch_type'], $row['uid']));
			}
		}
		if ($content == '') {
			@unlink($this->configFileName);
			die('There is nothing to index. Aborting.');
		}
		return $content;
	}

	/**
	 * Creates configuration for "Server"
	 *
	 * @param	array	$row	Row
	 * @return	string	Generated configuration
	 */
	protected function getServersGetServer($row) {
		$content = 'Server ';
		$content .= isset($this->server_methods[$row['tx_mnogosearch_method']]) ? $this->server_methods[$row['tx_mnogosearch_method']] . ' ' : '';
		$content .= isset($this->realm_subsections[$row['tx_mnogosearch_subsection']]) ? $this->realm_subsections[$row['tx_mnogosearch_subsection']] . ' ' : '';
		$content .= $row['tx_mnogosearch_url'] . chr(10);
		if ($row['tx_mnogosearch_method'] != 1 && $row['tx_mnogosearch_method'] != 4) {
			$this->allowedServerURLs[] = $row['tx_mnogosearch_url'];
		}
		return $content;
	}

	/**
	 * Generates configuration for indexing "Realm" mnogosearch configuration
	 *
	 * @param	array	$row	Row from tx_mnogosearch_indexconfig
	 * @return	string	Generated configuration
	 */
	protected function getServersGetRealm($row) {
		$content = 'Realm ';
		$content .= isset($this->server_methods[$row['tx_mnogosearch_method']]) ? $this->server_methods[$row['tx_mnogosearch_method']] . ' ' : '';
		$regExp = false;
		if ($row['tx_mnogosearch_cmpoptions'] & 2) {
			$content .= 'NoMatch ';
		}
		if ($row['tx_mnogosearch_cmptype'] > 0 || ($row['tx_mnogosearch_cmpoptions'] & 1)) {
			if ($row['tx_mnogosearch_cmptype'] == 1) {
				$content .= 'Regex ';
				$regExp = true;
			}
			else {
				$content .= 'String ';
			}
		}
		$content .= $row['tx_mnogosearch_url'] . chr(10);
		if ($row['tx_mnogosearch_method'] != 1 && $row['tx_mnogosearch_method'] != 4) {
			if (!$regExp) {
				$this->allowedServerURLs[] = $row['tx_mnogosearch_url'];
			}
			else {
				$this->allowedServerURLs[] = '/' . str_replace('/', '\/', $row['tx_mnogosearch_url']) . '/';
			}
		}
		return $content;
	}

	/**
	 * Generates configuration for indexing database records
	 *
	 * @param	array	$row	Row from tx_mnogosearch_indexconfig
	 * @return	string	Generated configuration
	 */
	function getServersGetRecords($row) {
		// TODO Needs setnames in the mnogosearch query string!
		$matches = array(); $setnames = '';
		if (preg_match('/set\s+names\s+([a-z\-0-9]+)/i', $GLOBALS['TYPO3_CONF_VARS']['SYS']['setDBinit'], $matches)) {
			$setnames = '?setnames=' . $matches[1];
		}
		$this->allowedServerURLs[] = 'htdb:/' .
					$row['tx_mnogosearch_table'] . '/' .
					$row['uid'];
		$content = 'HTDBAddr mysql://' . TYPO3_db_username . ':' .
					TYPO3_db_password . '@' .
					TYPO3_db_host . '/' . TYPO3_db . '/' .
					$setnames . chr(10);
		// Make HTDBList command
		$content .= 'HTDBList "SELECT CONCAT(\'htdb:/' .
					$row['tx_mnogosearch_table'] . '/' .
					$row['uid'] .
					'/\', uid) FROM ' . $row['tx_mnogosearch_table'];
		$where = '1=1';
		if ($row['tx_mnogosearch_pid_only']) {
			if (t3lib_div::testInt($row['tx_mnogosearch_pid_only'])) {
				$where = 'pid=' . $row['tx_mnogosearch_pid_only'];
			}
			else {
				$where = 'pid IN (' . $GLOBALS['TYPO3_DB']->cleanIntList($row['tx_mnogosearch_pid_only']) . ')';
			}
		}
		$where .= t3lib_BEfunc::BEenableFields($row['tx_mnogosearch_table']) .
					t3lib_BEfunc::deleteClause($row['tx_mnogosearch_table']);
		if ($where != '') {
			$content .= ' WHERE ' . $where;
		}
		$content .= '"' . chr(10);

		$content .= 'HTDBLimit 4096' . chr(10);

		//
		// Create HTDBDoc query
		//
		$content .= 'HTDBDoc "SELECT ';

		// Add title
		$content .= ($row['tx_mnogosearch_title_field'] ? $row['tx_mnogosearch_title_field'] : '\'\'');
		$content .= ' AS title,';

		// Add last modified
		if ($row['tx_mnogosearch_lastmod_field']) {
			$content .= $row['tx_mnogosearch_lastmod_field'] . ' AS last_mod_time,';
		}

		// TODO Add status and get rid of enableFields above.
		// See http://www.mnogosearch.org/doc33/msearch-extended-indexing.html#htdb
		// See http://dev.mysql.com/doc/refman/5.0/en/case-statement.html
		// $content .= 'CASE WHEN (delete=1 OR hidden=1 OR startime<... ....) THEN 404 ELSE 200';

		// Add body
		$bodyFields = t3lib_div::trimExplode(',', $row['tx_mnogosearch_body_field'], true);
		$content .= (count($bodyFields) > 1 ? 'CONCAT(' . implode(',\' \',', $bodyFields) . ')' : $bodyFields[0]);
		$content .= ' AS body FROM ' . $row['tx_mnogosearch_table'] . ' WHERE uid=$3"' . chr(10);
		$content .= 'Server htdb:/' . $row['tx_mnogosearch_table'] . '/' . chr(10);
		return $content;
	}

	/**
	 * Creates indexer configuration and writes it to a temporary file.
	 *
	 * @return	void
	 */
	protected function createIndexerConfigFile() {
		// Process extra options
		if (false !== ($pos = array_search('-x', $GLOBALS['argv'])) && $pos < sizeof($GLOBALS['argv']) - 1) {
			$GLOBALS['extraOptions'] = $GLOBALS['argv'][$pos + 1];
		}

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

		// Note when adding new sections, do not forget to adjust weight factors in pi1!
		$content = '# Defaults
DBAddr ' . $this->sysconf['dbaddr'] . '
VarDir ' . PATH_site . 'typo3temp/mnogosearch/var
PopRankUseShowCnt yes
PopRankUseTracking yes
PopRankFeedBack yes
PopRankSkipSameSite no
Section body 1 8092 html
Section title 2 256 text
Section meta.keywords 3 64 text
Section meta.description 4 64 text
SaveSectionSize yes
LocalCharset ' . ($this->sysconf['LocalCharset'] ? $this->sysconf['LocalCharset'] : 'UTF-8') . '
BrowserCharset ' . ($this->sysconf['BrowserCharset'] ? $this->sysconf['BrowserCharset'] : 'UTF-8') . '

#Allow *.htm *.html *.php *.txt */ *.pdf *.doc *.odt *.swx
Mime message/rfc822 text/plain "cat $1"
Mime application/msword text/plain "catdoc -a $1"
Mime application/pdf text/plain "pdftotext -enc UTF-8 $1 -"
Mime application/vnd.ms-excel text/html "xltohtml $1"
Mime application/vnd.ms-powerpoint text/html "pptohtml $1"
' . $openOfficeCmd . '

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
			$content .= 'RemoteCharset ' . $GLOBALS['TYPO3_CONF_VARS']['BE']['forceCharset'] . chr(10);
		}
		if ($this->sysconf['IncludeFile']) {
			$content .= 'Include ' . $this->sysconf['IncludeFile'] . chr(10);
		}
		$content .= $this->getServers();
		file_put_contents($this->configFileName, $content);
	}

	/**
	 * Creates a list of URLs changed from last indexing and writes it to temporary file
	 *
	 * @return	string	File name with URLs or empty string if there are no URLs
	 */
	protected function createNewUrlList() {
		$res = $GLOBALS['TYPO3_DB']->sql_query('SELECT uid,tx_mnogosearch_url FROM tx_mnogosearch_urllog ORDER BY uid');
		$content = '';
		while (false !== ($row = $GLOBALS['TYPO3_DB']->sql_fetch_assoc($res))) {
			$addUrl = false;
			foreach ($this->allowedServerURLs as $allowedUrl) {
				if (substr($allowedUrl, 0, 6) == 'htdb:/') {
					$addUrl = ($allowedUrl == dirname($row['tx_mnogosearch_url']));
				}
				elseif (substr($row['tx_mnogosearch_url'], 0, 6) != 'htdb:/') {
					if ($allowedUrl{0} == '/') {
						// regexp
						$addUrl = preg_match($allowedUrl, $row['tx_mnogosearch_url']);
					}
					elseif (strpos($allowedUrl, '*')) {
						// wildcard
						$regexp = str_replace('/', '\/', str_replace('*', '.*', $allowedUrl));
						$addUrl = preg_match('/' . $regexp . '/', $row['tx_mnogosearch_url']);
					}
					elseif (strpos($row['tx_mnogosearch_url'], $allowedUrl) === 0) {
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
		$GLOBALS['TYPO3_DB']->sql_free_result($res);
		if ($content) {
			$name = $this->getTempFileName();
			file_put_contents($name, $content);
			return $name;
		}
		return '';
	}

	/**
	 * Runs indexer
	 *
	 * @return	void
	 */
	protected function executeIndexer() {
		$dryRun = in_array('--dry-run', $GLOBALS['argv']);

		set_time_limit(6*60*60);

		if ($dryRun) {
			echo 'Using configuration file ' . $this->configFileName . ':
-------------------------
' .
				file_get_contents($this->configFileName) . '
-------------------------
';
		}

		$this->checkAndCreateDatabase($this->configFileName);

		if (in_array('-c', $GLOBALS['argv'])) {
			return;
		}

		$cmdLine = $this->sysconf['mnoGoSearchPath'] . '/sbin/indexer ' . $this->silenceOption . ' -r '  . $this->extraIndexerArguments . ' -d ' . $this->configFileName . ' ';
		if ($dryRun) {
			echo 'Executing: ' . $cmdLine . chr(10);
		}
		else {
			@exec($cmdLine);
		}

		// New URLs
		$newUrlFile = (in_array('-n', $GLOBALS['argv']) ? $this->createNewUrlList() : '');
		if ($newUrlFile && filesize($newUrlFile)) {
			$cmdLine = $this->sysconf['mnoGoSearchPath'] . '/sbin/indexer ' . $this->silenceOption . ' -a -i -r -d ' . $this->configFileName . ' -f ' . $newUrlFile . ' ' . $GLOBALS['extraOptions'];
			if ($dryRun) {
				echo 'Using URL configuration file ' . $newUrlFile . ':
-------------------------
' .
					file_get_contents($newUrlFile) . '
-------------------------
';
				echo 'Executing: ' . $cmdLine . chr(10);
				echo 'Removing ' . $newUrlFile . chr(10);
			}
			else {
				@exec($cmdLine);
			}
			@unlink($newUrlFile);
		}
		elseif ($dryRun) {
			echo 'No new URLs or -n is not specified, not executing update for new URLs' . chr(10);
		}
		if (stristr($this->sysconf['dbaddr'], 'dbmode=blob')) {
			$cmdLine = $this->sysconf['mnoGoSearchPath'] . '/sbin/indexer ' . $this->silenceOption . ' -Eblob -d ' . $this->configFileName . ' ' . $GLOBALS['extraOptions'];
			if ($dryRun) {
				echo 'Executing: ' . $cmdLine . chr(10);
			}
			else {
				@exec($cmdLine);
			}
		}
		if (in_array('-w', $GLOBALS['argv'])) {
			$cmdLine = $this->sysconf['mnoGoSearchPath'] . '/sbin/indexer ' . $this->silenceOption . ' -d ' . $this->configFileName . ' -Ewordstat' . ' ' . $GLOBALS['extraOptions'];
			if ($dryRun) {
				echo 'Executing: ' . $cmdLine . chr(10);
			}
			else {
				@exec($cmdLine);
			}
		}
		if ($dryRun) {
			echo 'Removing ' . $this->configFileName . chr(10);
		}
		@unlink($this->configFileName);
	}

	/**
	 * Executes mnogosearch indexing
	 *
	 * @return	void
	 */
	function main() {
		if ($GLOBALS['argc'] > 0 && $GLOBALS['argv'][2] == 'help') {
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
';
			exit;
		}

		$this->createIndexerConfigFile();
		if (in_array('-d', $GLOBALS['argv'])) {
			echo @file_get_contents($this->configFileName);
			@unlink($this->configFileName);
			exit;
		}
		$this->executeIndexer();
	}

}

if (defined('TYPO3_MODE') && $TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/cli/class.tx_mnogosearch_cli.php'])	{
	include_once($TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/cli/class.tx_mnogosearch_cli.php']);
}

$indexer = t3lib_div::makeInstance('tx_mnogosearch_cli');
/* @var $indexer tx_mnogosearch_cli */
$indexer->main();

?>
