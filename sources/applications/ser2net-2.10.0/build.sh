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

if make distclean; then
	echo "Building"
else
	echo "Auto reconfiguring..."
	autoreconf -ivf
fi

if ./configure --prefix=$INSTALL_PATH --host=arm-linux --target=arm-linux; then
	if make LDFLAGS="-all-static"; then
	        #make install LDFLAGS="-all-static"
		cp -af ser2net $INSTALL_PATH/bin/
	fi
fi
