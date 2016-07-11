#!/bin/sh
set -e

CUR_PATH=`pwd`
OUTPUT_PATH=$CUR_PATH"/../app_fs/nuwicam"

mkdir -p $OUTPUT_PATH

BUILD_SET="boa-0.94.13 httpd-cgi RTSPServer ser2net-2.10.0"

for i in $BUILD_SET; do
	cd $CUR_PATH
	echo "Enter  $CUR_PATH/$i"
	cd $CUR_PATH/$i
	if ! ./build.sh $OUTPUT_PATH; then
	        echo "[Failure] $i"
	        exit 1
	fi
done

arm-linux-strip $OUTPUT_PATH/bin/*
ls -al $OUTPUT_PATH
