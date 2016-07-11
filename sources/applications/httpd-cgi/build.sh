#!/bin/bash

if [ "$1" == "" ]; then
        INSTALL_PATH=`pwd`/_install
else
        if [ -d $1 ]; then
                INSTALL_PATH=$1
        else
                exit 1
        fi
fi

mkdir -p $INSTALL_PATH/www/cgi-bin/

make clean
if make; then
	cp -af *.cgi 	$INSTALL_PATH/www/cgi-bin/
	exit 0
fi
exit 1
