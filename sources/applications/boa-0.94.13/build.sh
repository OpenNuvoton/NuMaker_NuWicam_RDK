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

mkdir -p $INSTALL_PATH

cd src
make clean
if make; then
	mkdir $INSTALL_PATH/bin
	mkdir $INSTALL_PATH/etc

	cp -af boa 	$INSTALL_PATH/bin/
	cp ../boa.conf 	$INSTALL_PATH/etc
	exit 0
fi
exit 1
