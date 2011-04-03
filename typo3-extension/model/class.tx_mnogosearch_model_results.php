<?php
/***************************************************************
*  Copyright notice
*
*  (c) 2007 Dmitry Dulepov <dmitry@typo3.org>
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

class tx_mnogosearch_model_result {
	var	$url;					// URL
	var $title = '';			// Title
	var $contentType;			// Content type
	var	$documentSize;			// Document size
	var $popularityRank = 0;	// Rank
	var $rating = 0;			// Rating
	var $excerpt = '';			// Excerpt
	var $keywords = '';			// Keywords
	var $description = '';		// Description
	var $language = '';			// Language
	var $charset = '';			// Character set
	var $category = '';			// Category

	var $clones = array();		// Clones
}

class tx_mnogosearch_model_results {
	var $numRows;				// Number of rows on current page
	var $totalResults;			// Total found results
	var $searchTime;			// Search time
	var $firstDoc;				// First document
	var $lastDoc;				// Last document
	var $wordInfo;				// Information about found words
	var	$lastModified;			// Last modified date
	//var $wordSuggest = false;	// Suggestion information
	var $results = array();		// List of results
	var	$indexConfigCache = array();	// Cache for indexing configuration entries

	/**
	 * Initializes result collection.
	 *
	 * @param resource $udmAgent	mnoGoSearch agent
	 * @param resource $res	mnoGoSearch result
	 * @param tx_mnogosearch_pi1 $pObj	Calling onject (pi1 plugin)
	 */
	function init(&$udmAgent, &$res, tx_mnogosearch_pi1 &$pObj) {
		$this->totalResults = Udm_Get_Res_Param($res, UDM_PARAM_FOUND);
		$this->numRows = Udm_Get_Res_Param($res, UDM_PARAM_NUM_ROWS);
		$this->wordInfo = Udm_Get_Res_Param($res, UDM_PARAM_WORDINFO);
		$this->searchTime = Udm_Get_Res_Param($res, UDM_PARAM_SEARCHTIME);
		$this->firstDoc = Udm_Get_Res_Param($res, UDM_PARAM_FIRST_DOC);
		$this->lastDoc = Udm_Get_Res_Param($res, UDM_PARAM_LAST_DOC);
		/*
		// No suggest mode yet
		if ($this->totalResults == 0) {
			if ($pObj->getUdmApiVersion() >= 30233 && $pObj->conf['search.']['options.']['suggest_mode']) {
				$this->wordSuggest = Udm_Get_Agent_Param_Ex($udmAgent, 'WS');
			}
		}
		*/

		// Process results
		for ($i = 0; $i < $this->numRows; $i++) {
			$urlId = Udm_Get_Res_Field($res, $i, UDM_FIELD_ORIGINID);
			if ($urlId == 0 || !$pObj->conf['search.']['options.']['detect_clones']) {
				$result = t3lib_div::makeInstance('tx_mnogosearch_model_result');
				/* @var $result tx_mnogosearch_model_results */
				$result->popularityRank = Udm_Get_Res_Field($res, $i, UDM_FIELD_POP_RANK);
				Udm_Make_Excerpt($udmAgent, $res, $i);

				$result->url = $this->processURL(Udm_Get_Res_Field($res, $i, UDM_FIELD_URL), $pObj);
				$result->contentType = Udm_Get_Res_Field($res, $i, UDM_FIELD_CONTENT);
				$result->documentSize = Udm_Get_Res_Field($res, $i, UDM_FIELD_SIZE);
				$result->rating = Udm_Get_Res_Field($res, $i, UDM_FIELD_RATING);
				$result->title = Udm_Get_Res_Field($res, $i, UDM_FIELD_TITLE);
				if (trim($result->title) == '') {
					// Check if file
					$urlParts = parse_url($result->url);
					$path = rawurldecode($urlParts['path']);
					if (@is_file(PATH_site . trim($path, '/'))) {
						$result->title = basename($path);
					}
					else {
						$result->title = $pObj->pi_getLL('no_page_title');
					}
				}
				else {
					$result->title = $this->highlight($result->title, $pObj);
				}
				$result->excerpt = $this->highlight(strip_tags(Udm_Get_Res_Field($res, $i, UDM_FIELD_TEXT)), $pObj);
				$result->keywords = $this->highlight(strip_tags(Udm_Get_Res_Field($res, $i, UDM_FIELD_KEYWORDS)), $pObj);
				$result->description = $this->highlight(strip_tags(Udm_Get_Res_Field($res, $i, UDM_FIELD_DESC)), $pObj);
				$result->language = Udm_Get_Res_Field($res, $i, UDM_FIELD_LANG);
				$result->charset = Udm_Get_Res_Field($res, $i, UDM_FIELD_CHARSET);
				$result->category = Udm_Get_Res_Field($res,$i,UDM_FIELD_CATEGORY);
				$result->lastModified = $this->convertDateTime(strval(Udm_Get_Res_Field($res, $i, UDM_FIELD_MODIFIED)));

				// Check clones if necessary
				if ($pObj->conf['search.']['options.']['detect_clones']) {
					if (0 == Udm_Get_Res_Field($res, $i, UDM_FIELD_ORIGINID)) {
						$urlId = Udm_Get_Res_Field($res, $i, UDM_FIELD_URLID);
						for ($j = 0; $j < $this->numRows; $j++) {
							if ($j != $i && $urlId == Udm_Get_Res_Field($res, $j, UDM_FIELD_ORIGINID)) {
								$url = $this->processURL(Udm_Get_Res_Field($res, $j, UDM_FIELD_URL), $pObj);
								if ($url != $result->url) {
									$clone = t3lib_div::makeInstance('tx_mnogosearch_model_result');
									$clone->url = $url;
									$clone->contentType = Udm_Get_Res_Field($res, $j, UDM_FIELD_CONTENT);
									$clone->documentSize = Udm_Get_Res_Field($res, $j, UDM_FIELD_SIZE);
									$clone->lastModified = $this->convertDateTime(strval(Udm_Get_Res_Field($res,$j,UDM_FIELD_MODIFIED)));
									$result->clones[] = $clone;
								}
							}
						}
					}
				}
				$this->results[] = $result;
			}
		}
		Udm_Free_Res($res);
	}

	/**
	 * Highlights found search words according to configuration
	 *
	 * @param string $str	String to hightlight
	 * @param tx_mnogosearch_pi1 $pObj	Calling object
	 * @return string	Processed string
	 */
	function highlight($str, &$pObj) {
		if (count($pObj->highlightParts) == 2 && $pObj->highlightParts[0] != '') {
			$str = str_replace("\2", $pObj->highlightParts[0], $str);
			$str = str_replace("\3", $pObj->highlightParts[1], $str);
			while (substr_count($str, $pObj->highlightParts[0]) > substr_count($str, $pObj->highlightParts[1])) {
				$str .= $pObj->highlightParts[1];
			}
		}

		return $str;
	}

	/**
	 * Creates results for testing template
	 *
	 * @param	tx_mnogosearch_pi1	$pObj	Parent object
	 * @return	void
	 */
	function initTest(&$pObj) {
		$pageSize = intval($pObj->conf['search.']['resultsPerPage']);
		$resultsOnTheLastPage = max(1, intval($pageSize/3));
		$page = intval($pObj->piVars['page']);
		$numPages = 4;
		$foundDocs = (($page < ($numPages-1)) ? $pageSize : $resultsOnTheLastPage);
		$excerptSize = intval($pObj->conf['search.']['excerptSize']);
		if ($pObj->conf['excerptHighlight']) {
			$pObj->highlightParts = t3lib_div::trimExplode('|', $pObj->conf['search.']['excerptHighlight']);
		}

		// fill in our own fields
		$this->totalResults = ($numPages-1) * $pageSize + $resultsOnTheLastPage;
		$this->numRows = (($page < ($numPages-1)) ? $pageSize : $resultsOnTheLastPage);
		$this->wordInfo = '"Lorem ipsum": 123, "sit amet": 456, "sed": 789';
		$this->searchTime = floatval('0.' . sprintf('%03d', rand(1, 999)));
		$this->firstDoc = $page*$pageSize + 1;
		$this->lastDoc = $this->firstDoc + $foundDocs + 1;
		$pObj->piVars['q'] = '"Lorem ipsum" || "sit amet" || sed';

		$lipsum = $this->getLoremIpsum();

		// create fake results
		for ($i = 0; $i < $foundDocs; $i++) {
			$result = t3lib_div::makeInstance('tx_mnogosearch_model_results');
			/* @var $result tx_mnogosearch_model_result */
			$result->url = '/' . uniqid(uniqid(), true);
			$result->title = ucfirst($this->getExcerpt($lipsum, rand(3, 12)));
			$result->contentType = 'text/html';
			$result->documentSize = rand(1024, 32768);
			$result->popularityRank = $this->totalResults - $this->firstDoc + 1;
			$result->rating = 0;
			$result->excerpt = $this->highlight(str_replace(
						array('Lorem ipsum', 'lorem ipsum', 'sit amet', 'sed ', 'Sed '),
						array("\2Lorem ipsum\3", "\2lorem ipsum\3", "\2sit amet\3", "\2sed\3 ", "\2Sed\3 "),
						$this->getExcerpt($lipsum, $excerptSize)), $pObj);
			$result->keywords = '';
			$result->description = '--description goes here---';
			$result->language = '';
			$result->charset = '';
			$result->category = '---category---';
			$this->results[] = $result;
		}
	}

	/**
	 * Fetches 'Lorem Ipsum' using XML feed provided by www.lipsum.org
	 *
	 * @return	array	Words of text
	 */
	function getLoremIpsum() {
		$content = t3lib_div::getURL('http://www.lipsum.com/feed/xml?what=words&amount=2048&start=false');
		$array = t3lib_div::xml2array($content);
		return is_array($array) ? explode(' ', $array['lipsum']) : array();
	}

	/**
	 * Creates a random excerpt from Lorem Ipsum array
	 *
	 * @param	array	$lipsum	Words
	 * @param	int	$size	Excerpt size
	 * @return	string	Generated excerpt
	 */
	function getExcerpt(&$lipsum, $size) {
		$offset = rand(0, count($lipsum) - $size);
		return ($offset == 0 ? '' : '...') . implode(' ', array_slice($lipsum, $offset, $size)) . '...';
	}

	/**
	 * Converts htdb:/ scheme into HTTP
	 *
	 * @param string $url	URL
	 * @param	tx_monogosearch_pi1	$pObj	Calling object
	 * @return string	Converted URL
	 */
	protected function processURL($url, tx_mnogosearch_pi1 &$pObj) {
		if (substr($url, 0, 6) == 'htdb:/') {

			// Try hooks first
			if (is_array($GLOBALS['TYPO3_CONF_VARS']['EXTCONF']['mnogosearch']['preProcessURL'])) {
				foreach($GLOBALS['TYPO3_CONF_VARS']['EXTCONF']['mnogosearch']['preProcessURL'] as $userFunc) {
					$params = array(
						'pObj' => &$this,
						'url' => &$url,
						'controller' => &$pObj,
					);
					t3lib_div::callUserFunction($userFunc, $params, $this);
					if (substr($url, 0, 6) != 'htdb:/') {
						// URL was transformed to something visible
						break;
					}
				}
			}
			// If we still have htdb:/, search it in the indexing configs
			if (substr($url, 0, 6) == 'htdb:/') {
				$newUrl = '';
				$parts = t3lib_div::trimExplode('/', $url, true);
				// $parts now should contain:
				//	- htdb:
				//	- table_name
				//	- uid of the indexing config
				//	- uid of the record from table_name
				if (count($parts) == 4) {
					// Check if we have indexing configuration in the cache and load if not
					if (!isset($this->indexConfigCache[$parts[2]])) {
						list($config) = $GLOBALS['TYPO3_DB']->exec_SELECTgetRows(
								'tx_mnogosearch_table,tx_mnogosearch_url_parameters,tx_mnogosearch_display_pid',
								'tx_mnogosearch_indexconfig',
								'uid=' . intval($parts[2])
							);
						$this->indexConfigCache[$parts[2]] = $config;
					}
					else {
						$config = $this->indexConfigCache[$parts[2]];
					}
					if ($config && $config['tx_mnogosearch_table'] == $parts[1]) {
						$typoLinkConf = array(
							'parameter' => $config['tx_mnogosearch_display_pid'],
						);
						if ($config['tx_mnogosearch_url_parameters']) {
							$typoLinkConf['useCacheHash'] = true;
							$typoLinkConf['additionalParams'] = str_replace('{field:uid}', $parts[3], $config['tx_mnogosearch_url_parameters']);
						}
						$newUrl = rawurldecode($pObj->cObj->typoLink_URL($typoLinkConf));
						// Ensure that URL is complete
						$newUrl = t3lib_div::locationHeaderUrl($newUrl);
					}
				}
				$url = $newUrl;
			}
		}
		return $url;
	}

	/**
	 * Converts data and time to the Unix time stamp. See pi1/class.tx_mnogosearch_pi1.php
	 * near line #209 where "DateFormat" is set.
	 *
	 * @param	string	$datetime	Date and time
	 * @return	int	Timestamp
	 */
	protected function convertDateTime($datetime) {
		sscanf($datetime, '%d.%d.%d.%d.%d.%d', $day, $month, $year, $hour, $minute, $second);
		$result = mktime($hour, $minute, $second, $month, $day, $year);
		if ($result < 24*60*60) {
			$result = 0;
		}
		return $result;
	}
}

if (defined('TYPO3_MODE') && $TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/model/class.tx_mnogosearch_model_results.php'])	{
	include_once($TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/model/class.tx_mnogosearch_model_results.php']);
}

?>