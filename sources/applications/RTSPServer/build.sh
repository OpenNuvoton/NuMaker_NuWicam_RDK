#!/bin/bash

set -e

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
if make CROSS_COMPILE=arm-linux- SYS=posix PREFIX=$INSTALL_PATH; then
	make install PREFIX=$INSTALL_PATH
fi
