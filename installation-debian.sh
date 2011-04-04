#!/bin/bash

##############################################
# mnoGoSearch installation script for Debian 4/5, Ubuntu
# Author : Fedir Rykhtik <fedir.rykhtik@gmail.com>
# License : GNU General Public License (http://www.gnu.org/licenses/gpl.html)
##############################################

ok() {
	MSG=$1
	echo "[OK] ${MSG}"
}

apt-get install automake autoconf gcc php5-dev mysql-server libmysqlclient15-dev zlib1g zlib1g-dev
ok "Dependencies installation for mnoGoSearch compilation"

# Install search engine
LOCAL_SOURCES=
echo -n "Would You like to use local mnogosearch sources ? [y/N] "
read query
if [ "${LOCAL_SOURCES}" = "y" ] || [ "${LOCAL_SOURCES}" = "Y" ]; then
	cd mnogosearch-sources
else
	cd ~/
	wget http://www.mnogosearch.org/Download/mnogosearch-3.3.11.tar.gz
	tar xzf mnogosearch-3.3.11.tar.gz
	cd mnogosearch-3.3.11
	ok "Sources download"
fi

sed -i "s/\[HAVE_PGSQL\],\ \[1\]/\[HAVE_PGSQL\],\ \[0\]/" configure.in
./configure --prefix=/opt/mnogosearch --disable-mp3 --disable-news --without-debug --with-pgsql=no --with-freetds=no --with-oracle8=no --with-oracle8i=no --with-iodbc=no --with-unixODBC=no --with-db2=no --with-solid=no --with-openlink=no --with-easysoft=no --with-sapdb=no --with-ibase=no --with-ctlib=no --with-zlib --with-mysql --disable-syslog
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
