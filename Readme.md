# mnoGoSearch for TYPO3 CMS #

## Description ##

Systematization of elements to proceed installation of mnoGoSearch search engine on TYPO3 CMS 4.x.

## Project structure ##

* mnoGoSearch searche engine original sources (current version : 3.3.11)
* TYPO3 extension sources (v.2.2.2+)
* shell script for automatic installation on Debian

## Steps ##

* Install mnogosearch

Get sources

    git clone https://github.com/fedir/mnogosearch-typo3.git

Compile mnoGoSearch search engine from sources under root account 
You could use installation script for Debian / Ubuntu systems

    bash installation-debian.sh
    
* Copy the TYPO3 extension sources into Your typo3conf/ext folder
* Configure the extension in TYPO3 BE after official manual
