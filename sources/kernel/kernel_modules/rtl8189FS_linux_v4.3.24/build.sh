#!/bin/bash

BUILD_PATH=`pwd`
KERNEL_PATH=../../
WIFI_DRIVER_PATH=../../../app_fs/nuwicam/wifi/RTL8189FS/

# build wifi driver
cd $BUILD_PATH
make clean
if make -j4; then
	if arm-linux-strip --strip-unneeded *.ko; then
		cp -a *.ko	$WIFI_DRIVER_PATH
		echo "installed driver to app root fs."
	fi
else
	echo "Make failure."
fi

