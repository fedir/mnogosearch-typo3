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

/**
 * Base renderer for mnoGoSearch plugin
 *
 * @author	Dmitry Dulepov <dmitry@typo3.org>
 * @package	TYPO3
 * @subpackage	tx_mnogosearch
 */
class tx_mnogosearch_view {

	/** Template file content */
	protected $templateCode;

	/**
	 * Parent object
	 *
	 * @var tx_mnogosearch_pi1
	 */
	protected $pObj;

	/**
	 * Initializes the renderer
	 *
	 * @param	tx_mnogosearch_pi1	$pObj	Calling object
	 * @return	boolean	true on success
	 */
	public function init(tx_mnogosearch_pi1 &$pObj) {
		$this->pObj = $pObj;

		$templateFile = $pObj->pi_getFFvalue($pObj->cObj->data['pi_flexform'], 'field_templateFile');
		if (!$templateFile) {
			$templateFile = $pObj->conf['templateFile'];
		}
		$this->templateCode = $pObj->cObj->fileResource($templateFile);

		// Add header parts if there are any
		$headerParts = $this->pObj->cObj->getSubpart($this->templateCode, '###HEADER_ADDITIONS###');
		$key = $pObj->extKey . '_' . md5($headerParts);
		if ($headerParts && !isset($GLOBALS['TSFE']->additionalHeaderData[$key])) {
			$headerParts = $pObj->cObj->substituteMarker($headerParts, '###SITE_REL_PATH###', t3lib_extMgm::siteRelPath($pObj->extKey));
			$GLOBALS['TSFE']->additionalHeaderData[$key] = $headerParts;
		}
		return true;
	}

	/**
	 * Retrieves the controller.
	 *
	 * @return	tx_mnogosearch_pi1
	 */
	public function getController() {
		return $this->pObj;
	}

	/**
	 * Renders simple search form
	 *
	 * @return	string	Generated HTML
	 */
	public function render_simpleSearchForm() {
		$template = $this->pObj->cObj->getSubpart($this->templateCode, '###SHORT_SEARCH_FORM###');
		$result = $this->pObj->cObj->substituteMarkerArray($template, array(
			'###SHORT_SEARCH_FORM_ACTION###' => $this->pObj->pi_getPageLink($this->getResultPage()),
			'###SHORT_SEARCH_FORM_VALUE###' => htmlspecialchars($this->pObj->piVars['q']),
			'###TEXT_SEARCH###' => $this->pObj->pi_getLL('text_submit_short', '', true),
			));
		return $result;
	}

	/**
	 * Renders normal search form
	 *
	 * @return	string	Generated HTML
	 */
	public function render_searchForm() {
		$template = $this->pObj->cObj->getSubpart($this->templateCode, '###LONG_SEARCH_FORM###');
		$result = $this->pObj->cObj->substituteMarkerArray($template, array(
			'###TEXT_SEARCH_FOR###' => $this->pObj->pi_getLL('text_search_text', '', true),
			'###LONG_SEARCH_FORM_ACTION###' => $this->pObj->pi_getPageLink($this->getResultPage()),
			'###LONG_SEARCH_FORM_VALUE###' => htmlspecialchars($this->pObj->piVars['q']),
			'###TEXT_SEARCH###' => $this->pObj->pi_getLL('text_submit_long', '', true),
		));

		// Site selector
		$result = $this->render_searchForm_siteSelector($result);

		return $result;
	}

	/**
	 * Obtains page uid where results are displayed
	 *
	 * @return	int	Page id
	 */
	protected function getResultPage() {
		$resultPage = intval($this->pObj->conf['form.']['resultsPage']);
		if (!$resultPage) {
			$resultPage = $GLOBALS['TSFE']->id;
		}
		return $resultPage;
	}

	/**
	 * Renders search results
	 *
	 * @see	tx_mnogosearch_renderer::render_searchResults()
	 * @param	tx_mnogosearch_results	$results
	 * @return	string	Generated content
	 */
	public function render_searchResults(&$results) {
		/* @var $results tx_mnogosearch_results */
		// Setup variables
		$result = '';
		$rpp = intval($this->pObj->conf['search.']['resultsPerPage']);
		if (!$rpp) {
			$rpp = 20;
		}
		$page = intval($results->firstDoc/$rpp);
		$numPages = (intval($results->totalResults/$rpp) + (($results->totalResults % $rpp) == 0 ? 0 : 1));

		// Get template for this function
		$template = $this->pObj->cObj->getSubpart($this->templateCode, '###SEARCH_RESULTS###');

		// Results
		if ($results->numRows == 0) {
			$resultTemplate = $this->pObj->cObj->getSubpart($this->templateCode, '###SEARCH_RESULTS_EMPTY###');
			$resultList = $this->pObj->cObj->substituteMarker($resultTemplate, '###TEXT_NOTHING_FOUND###', $this->pObj->pi_getLL('text_nothing_found'));
		}
		else {
			$resultTemplate = $this->pObj->cObj->getSubpart($this->templateCode, '###SEARCH_RESULTS_RESULT###');
			$linksTemplate = $this->pObj->cObj->getSubpart($resultTemplate, '###SEARCH_RESULTS_RESULT_ALT_LINK###');
			$resultList = ''; $i = 0;
			foreach ($results->results as $result) {
				/* @var $result tx_mnogosearch_result */
				// Basic fields
				$iconSrc = $this->getIconSrc($result->url);
				$t = $this->pObj->cObj->substituteMarkerArray($resultTemplate, array(
						'###SEARCH_RESULTS_RESULT_NUMBER###' => $results->firstDoc + ($i++),
						'###SEARCH_RESULTS_RESULT_URL###' => htmlspecialchars($result->url),
						'###SEARCH_RESULTS_RESULT_TITLE###' => $result->title,
						'###SEARCH_RESULTS_RESULT_RELEVANCY###' => sprintf('%.2f', $result->rating),
						'###SEARCH_RESULTS_RESULT_EXCERPT###' => $result->excerpt,
						'###SEARCH_RESULTS_RESULT_SIZE###' => $this->formatSize($result->documentSize),
						'###SEARCH_RESULTS_RESULT_LASTMOD###' => $this->pObj->cObj->stdWrap($result->lastModified, $this->pObj->conf['search.']['resultTime_stdWrap.']),
						'###TEXT_ADDITIONAL_RESULTS###' => $this->pObj->pi_getLL('text_additional_results'),
						'###UNIQID###' => uniqid('c'),
						'###ICON_SRC###' => $iconSrc,
					));
				// Make links
				$links = '';
				foreach ($result->clones as $r) {
					$links .= $this->pObj->cObj->substituteMarkerArray($linksTemplate, array(
						'###SEARCH_RESULTS_RESULT_ALT_LINK_URL###' => $r->url,
						'###SEARCH_RESULTS_RESULT_ALT_LINK_TITLE###' => $r->url,
						));
				}
				if ($iconSrc == '') {
					$t = $this->pObj->cObj->substituteSubpart($t, '###ICON###', '');
				}
				if ($links != '') {
					$resultList .= $this->pObj->cObj->substituteSubpart($t, '###SEARCH_RESULTS_RESULT_ALT_LINK###', $links);
				}
				else {
					$resultList .= $this->pObj->cObj->substituteSubpart($t, '###SEARCH_RESULTS_RESULT_LINKS###', '');
				}
			}
			// Wrap
			$resultTemplate = $this->pObj->cObj->getSubpart($this->templateCode, '###SEARCH_RESULTS_CONTENT###');
			$resultList = $this->pObj->cObj->substituteSubpart($resultTemplate, '###SEARCH_RESULTS_RESULT###', $resultList);
			$resultList = $this->pObj->cObj->substituteMarker($resultList, '###SEARCH_RESULTS_FIRST###', $results->firstDoc);
		}

		// Put all together
		$totalPages = intval($results->totalResults/$rpp) + ($results->totalResults % $rpp ? 1 : 0);
		$wordInfo = str_replace(' / ', '/', $results->wordInfo);
		$wordInfo = str_replace(' :', ':', $wordInfo);
		// TODO stdWrap numbers inside $wordInfo
		$timeStr = sprintf($this->pObj->conf['search.']['time_format'], $results->searchTime);
		$content = $this->pObj->cObj->substituteMarkerArray($template, array(
				// Older markers (partially for compatibility)
				'###SEARCH_RESULTS_TERMS###' => htmlspecialchars($this->pObj->piVars['q']),
				'###SEARCH_RESULTS_STATISTICS###' => htmlspecialchars($wordInfo),
				'###SEARCH_RESULTS_TIME###' => $timeStr,
				'###SEARCH_RESULTS_FIRST###' => $results->firstDoc,
				'###SEARCH_RESULTS_LAST###' => $results->lastDoc,
				'###SEARCH_RESULTS_TOTAL###' => $results->totalResults,
				'###SEARCH_RESULTS_CURRENT_PAGE###' => $page + 1,
				'###SEARCH_RESULTS_PAGE_TOTAL###' => $totalPages,
				// Newer markers
				'###TEXT_SEARCH_TEXT###' => $this->pObj->pi_getLL('text_search_text'),
				'###TEXT_SEARCH_RESULTS###' => $this->pObj->pi_getLL('text_search_results'),
				'###SEARCH_TOOK###' => sprintf($this->pObj->pi_getLL('search_took'),
											$this->pObj->cObj->stdWrap($timeStr, $this->pObj->conf['number_stdWrap.'])),
				'###SEARCH_TOOK_SHORT###' => sprintf($this->pObj->pi_getLL('search_took_short'),
											$this->pObj->cObj->stdWrap($timeStr, $this->pObj->conf['number_stdWrap.'])),
				'###RESULT_RANGE###' => sprintf($this->pObj->pi_getLL('result_range'),
											$this->pObj->cObj->stdWrap($results->firstDoc, $this->pObj->conf['number_stdWrap.']),
											$this->pObj->cObj->stdWrap($results->lastDoc, $this->pObj->conf['number_stdWrap.']),
											$this->pObj->cObj->stdWrap($results->totalResults, $this->pObj->conf['number_stdWrap.'])
											),
				'###PAGE_RANGE###' => sprintf($this->pObj->pi_getLL('page_range'),
											$this->pObj->cObj->stdWrap($page + 1, $this->pObj->conf['number_stdWrap.']),
											$this->pObj->cObj->stdWrap($totalPages, $this->pObj->conf['number_stdWrap.'])
											),
				'###PAGE_BROWSER###' => $this->getPageBrowser($numPages),
			));
		$content = $this->pObj->cObj->substituteSubpart($content, '###SEARCH_RESULTS_CONTENT###', $resultList);
		$content = $this->pObj->cObj->substituteSubpart($content, '###SEARCH_RESULTS_EMPTY###', '');
		return $content;
	}

	/**
	 * Obtains page browser.
	 *
	 * @param	int	$numberOfPages	Number of pages
	 * @return	string	HTML from page browser
	 */
	protected function getPageBrowser($numberOfPages) {
		$pageBrowserKind = $this->pObj->conf['search.']['pageBrowser'];
		$pageBrowserConfig = (array)$this->pObj->conf['search.']['pageBrowser.'];
		$pageBrowserConfig += array(
			'pageParameterName' => $this->pObj->prefixId . '|page',
			'numberOfPages' => $numberOfPages,
		);
		// Get page browser
		$cObj = t3lib_div::makeInstance('tslib_cObj');
		/* @var $cObj tslib_cObj */
		$cObj->start(array(), '');
		return $cObj->cObjGetSingle($pageBrowserKind, $pageBrowserConfig);
	}

	protected function getLink($page) {
		return $this->pObj->pi_linkTP_keepPIvars_url(array('page' => $page), 1);
	}

	/**
	 * Obtains icon for the file in the URL from TS setup
	 *
	 * @param	string	$url	URL of the file
	 * @return	string	Icon or empty string if there is not icon
	 */
	protected function getIconSrc($url) {
		$parts = pathinfo($url);
		$icon = '';
		if ($parts['extension']) {
			$extension = strtolower($parts['extension']);
			$icon = (isset($this->pObj->conf['icons.'][$extension]) ?
					$this->pObj->conf['icons.'][$extension] :
					$this->pObj->conf['defaultFileIcon']);
			if ($icon) {
				$icon = $GLOBALS['TSFE']->tmpl->getFileName($icon);
			}
		}
		return $icon;
	}

	protected function formatSize($size) {
		$sizes = explode(',', $this->pObj->pi_getLL('file_sizes'));
		if ($size < 1024) {
			$result = '<1' . $sizes[1];
		}
		else {
			$result = '';
			for ($i = 0; $i < 4; $i++) {
				$result = intval($size) . $sizes[$i];
				$size /= 1024;
				if ($size < 1) {
					break;
				}
			}
		}
		return $result;
	}

	/**
	 * Renders site selector for the advanced search form. For the selector to
	 * appear it must be enabled in the TS configuration and at least two
	 * indexing configs must present in the search limit (or one limit + search
	 * of the whole site enabled)
	 *
	 * @param	string	$template	HTML template to use
	 * @return	string	$template with updated search limit subparts
	 */
	protected function render_searchForm_siteSelector($template) {
		$siteSelectorTags = array(
			'###SITESEL_SELECT###',
			'###SITESEL_RADIO###',
			'###SITESEL_CHECK###'
		);
		$tag = '';
		$conf = &$this->pObj->conf['form.']['advanced.'];
		if (isset($conf['siteSelector'])) {
			$sections = $this->getSearchSections($conf);
			// We go only if there are at least two sections (i.e. there are options to choose from)
			if ($sections > 1) {
				$tag = $this->getSiteSelectorTag($conf);
				if ($tag) {
					$subpart = $this->pObj->cObj->getSubpart($template, $tag);
					$optionSubpart = $this->pObj->cObj->getSubpart($subpart, '###SITESEL_OPTION###');
					$options = '';
					$default = $conf['siteSelector.']['default'];
					foreach ($sections as $uid => $section) {
						if ($conf['siteSelector.']['exclude'] == '' || !t3lib_div::inList($conf['siteSelector.']['exclude'], $uid)) {
							if (is_array($this->pObj->piVars['l']) && in_array($uid, $this->pObj->piVars['l']) ||
								!is_array($this->pObj->piVars['l']) && $uid == $default) {
								$selected = ' selected="selected" ';
								$checked = ' checked="checked" ';
							}
							else {
								$selected = $checked = '';
							}
							$options .= $this->pObj->cObj->substituteMarkerArray($optionSubpart,
								array(
									'###VALUE###' => htmlspecialchars($uid),
									'###TEXT###' => htmlspecialchars($section),
									'###SELECTED###' => $selected,
									'###CHECKED###' => $checked
								)
							);
						}
					}
					$subpart = $this->pObj->cObj->substituteSubpart($subpart, '###SITESEL_OPTION###', $options);
					$template = $this->pObj->cObj->substituteSubpart($template, $tag, $subpart);
				}
			}
		}
		// Remove all unused selectors
		foreach ($siteSelectorTags as $selectorTag) {
			if ($tag != $selectorTag) {
				$template = $this->pObj->cObj->substituteSubpart($template, $selectorTag, '');
			}
		}
		$template = $this->pObj->cObj->substituteMarker($template, '###TEXT_SEARCH_WHERE###',
			$this->pObj->pi_getLL('search_where'));
		return $template;
	}

	/**
	 * Obtains site selector subpart name
	 *
	 * @param	array	$conf	Configuration of the advanced form
	 * @return	string	Subpart name
	 */
	protected function getSiteSelectorTag(array $conf) {
		$tag = '';
		switch ($conf['siteSelector']) {
			case 'select':
				$tag = '###SITESEL_SELECT###';
				break;
			case 'radio':
				$tag = '###SITESEL_RADIO###';
				break;
			case 'checkboxes':
				$tag = '###SITESEL_CHECK###';
				break;
		}
		return $tag;
	}

	/**
	 * Obtains search sections of the site from the configuration
	 *
	 * @param	array	$conf	Confiuration of the advanced search form
	 * @return	array	Key is uid of the indexing configuration, value is title
	 * @see	tx_mnogosearch_pi1::addSearchRestrictions()
	 */
	protected function getSearchSections(array $conf) {
		$result = array();
		if ($this->pObj->conf['siteList']) {

			// Limit domains
			$domainList = $GLOBALS['TYPO3_DB']->exec_SELECTgetRows(
				'tx_mnogosearch_type,tx_mnogosearch_url,tx_mnogosearch_table,uid,title',
				'tx_mnogosearch_indexconfig',
				'uid IN (' . $this->pObj->conf['siteList'] . ') AND ' .
				// Server
				'((tx_mnogosearch_type=0 AND tx_mnogosearch_method<=0) OR ' .
				// Records
				'tx_mnogosearch_type=11)' .
				$this->pObj->cObj->enableFields('tx_mnogosearch_indexconfig'));
			$lang = null;
			foreach ($domainList as $domain) {
				if ($domain['title']) {
					$result[$domain['uid']] = $domain['title'];
				}
				elseif ($domain['tx_mnogosearch_type'] != 11) {
					// Server
					$result[$domain['uid']] = $domain['tx_mnogosearch_url'];
				}
				else {
					// htdb domains
					if ($lang == null) {
						t3lib_div::requireOnce(t3lib_extMgm::extPath('lang', 'lang.php'));
						$lang = t3lib_div::makeInstance('language');
						/* @var $lang language */
						$lang->init($GLOBALS['TSFE']->lang);
					}
					$result[$domain['uid']] = $lang->sL($GLOBALS['TCA'][$domain['tx_mnogosearch_table']]['ctrl']['title']);
				}
			}

			// Sort
			$sorted = array();
			// Add "Search all" if necessary
			if ($conf['siteSelector.']['searchAll'] && $conf['siteSelector'] != 'checkboxes') {
				$sorted[''] = $this->pObj->pi_getLL('search_all');
			}
			foreach (explode(',', $this->pObj->conf['siteList']) as $id) {
				if (isset($result[$id])) {
					$sorted[$id] = $result[$id];
				}
			}
			$result = $sorted;

			// Call hook for custom sections
			if (is_array($GLOBALS['TYPO3_CONF_VARS']['EXTCONF']['mnogosearch']['getCustomSearchLimit'])) {
				foreach($GLOBALS['TYPO3_CONF_VARS']['EXTCONF']['mnogosearch']['getCustomSearchLimit'] as $userFunc) {
					$params = array(
						'pObj' => &$this,
						'limits' => &$result
					);
					t3lib_div::callUserFunction($userFunc, $params, $this);
				}
			}
		}
		return $result;
	}
}

if (defined('TYPO3_MODE') && $TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/view/class.tx_mnogosearch_view.php'])	{
	include_once($TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/view/class.tx_mnogosearch_view.php']);
}

?>