#!/bin/bash

##############################################
# mnoGoSearch installation script for Debian 4/5, Ubuntu
# Author : Fedir Rykhtik <fedir.rykhtik@gmail.com>
# License : GNU General Public License (http://www.gnu.org/licenses/gpl.html)
##############################################

CURRENT_VERSION="3.3.15"

ok() {
	MSG=$1
	echo "[OK] ${MSG}"
}

# Install mnoGoSearch search engine
apt-get update
apt-get install build-essential make automake autoconf gcc php5-dev mysql-server libmysqlclient15-dev zlib1g zlib1g-dev
ok "Dependencies installation for mnoGoSearch compilation"
echo -n "Would You like to use local mnogosearch sources ? [y/N] "
read LOCAL_SOURCES
if [ "${LOCAL_SOURCES}" = "y" ] || [ "${LOCAL_SOURCES}" = "Y" ]; then
	cd mnogosearch-sources
else
	cd ~/
	wget http://www.mnogosearch.org/Download/mnogosearch-${CURRENT_VERSION}.tar.gz
	tar xzf mnogosearch-${CURRENT_VERSION}.tar.gz
	cd mnogosearch-${CURRENT_VERSION}
	ok "Sources download"
fi
sed -i "s/\[HAVE_PGSQL\],\ \[1\]/\[HAVE_PGSQL\],\ \[0\]/" configure.in
./configure --prefix=/opt/mnogosearch --disable-mp3 --disable-news --without-debug --with-pgsql=no --with-freetds=no --with-oracle8=no --with-oracle8i=no --with-iodbc=no --with-unixODBC=no --with-db2=no --with-solid=no --with-openlink=no --with-easysoft=no --with-sapdb=no --with-ibase=no --with-ctlib=no --with-zlib --with-mysql --disable-syslog --with-openssl
sed -i "s/\/\*\ #undef\ HAVE_PGSQL\ \*\//#undef\ HAVE_PGSQL/" include/udm_autoconf.h
ok "Sources patching"
make
make install
ok "Search engine installation"

# Install php extension
cd php
phpize
./configure --with-mnogosearch=/opt/mnogosearch
sed -i "s/#include\ \"php.h\"/#include\ \"php.h\"\n\#undef\ HAVE_PGSQL/" php_mnogo.c
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/mnogosearch/lib/
make
ok "PHP extension compilation"
make install

echo 'extension=mnogosearch.so' > /etc/php5/mods-available/mnogosearch.ini
chmod a+r /etc/php5/mods-available/mnogosearch.ini
cd /etc/php5/apache2/conf.d && ln -s ../../mods-available/mnogosearch.ini 30-mnogosearch.ini
cd /etc/php5/cli/conf.d && ln -s ../../mods-available/mnogosearch.ini 30-mnogosearch.ini
cd /etc/php5/cgi/conf.d && ln -s ../../mods-available/mnogosearch.ini 30-mnogosearch.ini
apache2ctl configtest && apache2ctl graceful

ok "PHP extension installation"
