#!/bin/bash

##############################################
# mnoGoSearch installation script for Debian 5
# Author : Fedir Rykhtik
# License : GNU V2
##############################################

# Install dependecies
apt-get install automake autoconf gcc php5-dev mysql-server libmysqlclient15-dev zlib1g zlib1g-dev

# Install search engine
cd ~/
wget http://www.mnogosearch.org/Download/mnogosearch-3.3.11.tar.gz
tar xzf mnogosearch-3.3.11.tar.gz
cd mnogosearch-3.3.11
sed -i "s/\[HAVE_PGSQL\],\ \[1\]/\[HAVE_PGSQL\],\ \[0\]/" configure.in
./configure --prefix=/opt/mnogosearch --disable-mp3 --disable-news --without-debug --with-pgsql=no --with-freetds=no --with-oracle8=no --with-oracle8i=no --with-iodbc=no --with-unixODBC=no --with-db2=no --with-solid=no --with-openlink=no --with-easysoft=no --with-sapdb=no --with-ibase=no --with-ctlib=no --with-zlib --with-mysql --disable-syslog
sed -i "s/\/\*\ #undef\ HAVE_PGSQL\ \*\//#undef\ HAVE_PGSQL/" include/udm_autoconf.h
make
make install

# Install php extension
cd php
phpize
./configure --with-mnogosearch=/opt/mnogosearch
sed -i "s/#include\ \"php.h\"/#include\ \"php.h\"\n\#undef\ HAVE_PGSQL/" php_mnogo.c
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/mnogosearch/lib/
make
