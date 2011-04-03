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
 * [CLASS/FUNCTION INDEX of SCRIPT]
 *
 * $Id: class.tx_mnogosearch_tsfepostproc.php 25985 2009-10-29 12:35:59Z dmitry $
 */

/**
 * Content post-processing functions.
 *
 * @author	Dmitry Dulepov <dmitry@typo3.org>
 * @package	TYPO3
 * @subpackage	tx_mnogosearch
 */
class tx_mnogosearch_tsfepostproc {

	/**
	 * Called by TSFE when the content is ready for output. Updates the
	 * content for the indexer. Note: this hook runs only if TYPO3 is
	 * called from the indexer! See ext_localconf.php for the hook definition.
	 *
	 * @return	void
	 */
	public function contentPostProcOutput() {
		if ($this->isMnogosearchEnabled()) {
			if ($this->canIndex()) {
				$this->updateContentForIndexer();
			}
			else {
				$this->disallowIndexing();
			}
		}
	}

	/**
	 * Checkes if mnogosearch extension is enabled.
	 *
	 * @return	boolean
	 */
	protected function isMnogosearchEnabled() {
		return !intval($GLOBALS['TSFE']->config['config']['tx_mnogosearch_enable']) || $GLOBALS['TSFE']->type != 0;
	}

	/**
	 * Updates current content for the indexer
	 *
	 * @return	void
	 */
	protected function updateContentForIndexer() {
		if ($GLOBALS['TSFE']->content) {
			if (strpos($GLOBALS['TSFE']->content, '<!--TYPO3SEARCH_begin-->')) {
				// Remove parts that should not be indexed
				$GLOBALS['TSFE']->content = $this->processContent($GLOBALS['TSFE']->content);
			}
			$this->replaceTitleIfNecessary();
			$this->addLastModified();
			$this->addUserGroupMeta();
		}
	}

	/**
	 * Checks if the page can be indexed.
	 *
	 * @return	boolean
	 */
	protected function canIndex() {
		return !$GLOBALS['TSFE']->page['no_search'];
	}

	/**
	 * Replaces title tag if necessary
	 *
	 * @return	void
	 */
	protected function replaceTitleIfNecessary() {
		if (!$GLOBALS['TSFE']->config['config']['tx_mnogosearch_keepSiteTitle']) {
			$title = ($GLOBALS['TSFE']->indexedDocTitle ? $GLOBALS['TSFE']->indexedDocTitle :
						($GLOBALS['TSFE']->altPageTitle ? $GLOBALS['TSFE']->altPageTitle : $GLOBALS['TSFE']->page['title']));
			$GLOBALS['TSFE']->content = preg_replace('/<title>[^<]*<\/title>/', '<title>' . htmlspecialchars($title) . '</title>', $GLOBALS['TSFE']->content);
		}
	}

	/**
	 * Disallows indexing for the current content
	 *
	 * @return	void
	 */
	protected function disallowIndexing() {
		$this->disallowIndexingThroughMeta();
		$this->removeMnogosearchTags();
	}

	/**
	 * Removes mnogosearch tags from the content
	 *
	 * @return	void
	 */
	protected function removeMnogosearchTags() {
		$GLOBALS['TSFE']->content = preg_replace('/<!--\/?UdmComment-->/ims', '', $GLOBALS['TSFE']->content);
	}

	/**
	 * Disallows indexing by setting the <meta> tag no "noindex,nofollow"
	 *
	 * @return	void
	 */
	protected function disallowIndexingThroughMeta() {
		$metaTag = '<meta name="robots" content="noindex,nofollow" />';
		if ($this->hasRobotsTag()) {
			$GLOBALS['TSFE']->content = preg_replace('/<meta\s[^>]*name="robots"[^>]*>/ims', $metaTag, $GLOBALS['TSFE']->content);
		}
		else {
			$this->insertIntoHead($metaTag);
		}
	}

	/**
	 * Checks if content has robots tag
	 *
	 * @return	boolean
	 */
	protected function hasRobotsTag() {
		return preg_match('/<meta\s[^>]*name="robots"[^>]*>/ims', $GLOBALS['TSFE']->content);
	}

	/**
	 * Inserts data into the <head> part of the content (just before the closing tag)
	 *
	 * @param string	$data	Data to insert
	 * @return	void
	 */
	protected function insertIntoHead($data) {
		$pos = stripos($GLOBALS['TSFE']->content, '</head>');
		if ($pos > 0) {
			$GLOBALS['TSFE']->content = substr_replace($GLOBALS['TSFE']->content, $data . chr(10), $pos, 0);
		}
	}

	/**
	 * Processes content by removing everything except links from parts of
	 * the content that should not be searched.
	 *
	 * @param	string	$html	HTML to process
	 * @return	string	Processing content
	 */
	protected function processContent($html) {
		list($part1, $content, $part2) = preg_split('/<\/?body[^>]*>/', $html);

		$process = true;
		$result = '';
		$regexp = '/(<!--TYPO3SEARCH_(?:begin|end)-->)/';
		$blocks = preg_split($regexp, $content, -1, PREG_SPLIT_DELIM_CAPTURE);

		foreach ($blocks as $block) {
			if ($block == '<!--TYPO3SEARCH_begin-->') {
				$process = false;
			}
			elseif ($block == '<!--TYPO3SEARCH_end-->') {
				$process = true;
			}
			elseif ($process) {
				$result .= $this->processBlock($block);
			}
			else {
				$result .= $block;
			}
		}
		return $part1 . '<body>' . $result . '</body>' . $part2;
	}

	/**
	 * Processes a single block of text, removes everything execept links.
	 *
	 * @param	string	$block	Content part
	 * @return	string	Processed block
	 */
	protected function processBlock($block) {
		$regexp = '/(<a[^>]+href=[^>]+>)/';
		$matches = array();
		$result = '';
		if (preg_match_all($regexp, $block, $matches)) {
			foreach ($matches[0] as $match) {
				$result .= $match . ' </a>';
			}
		}
		return $result;
	}

	/**
	 * Adds the "Last-modified" header to the page. Last-modified is necessary
	 * to show it in search results. TYPO3 never sends "Not modified" response,
	 * so we are safe for "If-modified-since" requests.
	 *
	 * @param	tslib_fe $GLOBALS['TSFE']	Parent object
	 * @return 	void
	 */
	protected function addLastModified() {
		// See if we have this header already
		$headers = headers_list();
		foreach ($headers as $header) {
			if (stripos($header, 'Last-modified:') === 0) {
				$parts = t3lib_div::trimExplode(':', $header, true, 2);
				if (count($parts == 2)) {
					$time = strtotime($parts[1]);
					if ($time >= time() - 300) {
						// The page was just generated. Do not trust this header!
						break;
					}
				}
				return;
			}
		}
		$time = (($GLOBALS['TSFE']->register['SYS_LASTCHANGED'] < time() - 300) ?
			$GLOBALS['TSFE']->register['SYS_LASTCHANGED'] : $GLOBALS['TSFE']->page['tstamp']);
		header('Last-modified: ' . gmdate('D, d M Y H:i:s T', $time));
	}

	/**
	 * Adds a special meta tag for the indexer with the user group content. This
	 * tag is always added to allow filtering by group during search.
	 *
	 * @return	void
	 */
	protected function addUserGroupMeta() {
		$userGroup = intval(t3lib_div::_GET('tx_mnogosearch_gid'));
		$metaTag = '<meta name="usergroup" content="' . $userGroup . '" />';
		$this->insertIntoHead($metaTag);
	}
}

if (defined('TYPO3_MODE') && $TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/hooks/class.tx_mnogosearch_tsfepostproc.php'])	{
	include_once($TYPO3_CONF_VARS[TYPO3_MODE]['XCLASS']['ext/mnogosearch/hooks/class.tx_mnogosearch_tsfepostproc.php']);
}

?>