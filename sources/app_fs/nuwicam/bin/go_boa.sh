#!/bin/sh
#
# Copyright (c) 2013 Nuvoton Technology Corp. All rights reserved.
#

PATH_ROOT=/mnt/nuwicam

while [ 1 ]; do
killall -9 boa
if [ ! -f /etc/boa/boa.conf ]; then
	mkdir -p /etc/boa
	cp -af $PATH_ROOT/etc/mime.types /etc/
	cp -af $PATH_ROOT/etc/boa.conf /etc/boa/
	sync
fi

echo "Start boa http server..."
$PATH_ROOT/bin/boa -d
echo "Stop boa http server..."
done
