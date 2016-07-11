#!/bin/bash

PARTITION_SIZE=0x0600000
if [ $# -gt 0 ]; then SRC_TARGET=$1; DST_TARGET=$2; else SRC_TARGET="nuwicam"; DST_TARGET="../image/app_fs"; fi
if mkfs.jffs2 -r $SRC_TARGET -o $DST_TARGET.jffs2 -e 0x10000 --pad=$PARTITION_SIZE; then
	sumtool -l -e 0x10000 -p -i "$DST_TARGET".jffs2 -o "$DST_TARGET"_jffs2.img
	tar zcf $SRC_TARGET".tar.gz" $SRC_TARGET
	ls -al ../image/app_*
fi
