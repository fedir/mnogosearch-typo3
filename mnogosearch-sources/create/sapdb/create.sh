#!/bin/sh
# Edit env for you database instance
# create-all.ins to create all database structre for ALL DBMode
# 
 
DBROOT=/usr/sapdb-srv
USER=udms
PASS=udms
SID=WWW

$DBROOT/bin/xload -u ${USER},${PASS} -d $SID -b create-all.ins

