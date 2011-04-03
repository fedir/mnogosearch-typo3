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
 * $Id: catoo.php 25752 2009-10-22 14:34:42Z dmitry $
 *
 * This script converts content of OpenOffice sxw/odt files to text and writes
 * it to standard output.
 */

if ($GLOBALS['argc'] < 2) {
	die('Format: php catoo.php sxw_or_odt_file_path');
}
$unzip = trim(`which unzip`);
if ($unzip == '') {
	die('Cannot find unzip');
}
$tempName = tempnam(sys_get_temp_dir(), '');
@unlink($tempName);
mkdir($tempName);
@exec($unzip . ' -d' . escapeshellarg($tempName) . ' ' . escapeshellarg($GLOBALS['argv'][1]));
if (file_exists($tempName . '/content.xml')) {
	echo preg_replace(array('/<[^>]+>/m', '/\s{2,}/m'), ' ', file_get_contents($tempName . '/content.xml'));
}

$rm = trim(`which rm`);
@exec($rm . ' -Rf ' . escapeshellarg($tempName));

?>