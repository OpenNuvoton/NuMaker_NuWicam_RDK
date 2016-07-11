#!/bin/bash

BUILD_PATH=`pwd`
KERNEL_PATH=../../
DRIVER_PATH=$ROOTFS/usr/

cd $BUILD_PATH
# build wifi driver
make clean
if make; then
	if arm-linux-strip --strip-unneeded *.ko; then
		cp -a *.ko	$DRIVER_PATH
		echo "installed driver to app root fs."
	fi
else
	echo "Make failure."
fi

