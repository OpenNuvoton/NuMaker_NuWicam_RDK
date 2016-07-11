#!/bin/sh
#
# Copyright (c) 2013 Nuvoton Technology Corp. All rights reserved.
#

PATH_ROOT=/mnt/nuwicam

echo "Stop ser2net..."
killall -9 ser2net
if [ ! -f /etc/ser2net.conf ]; then
	cp -af $PATH_ROOT/etc/ser2net.conf /etc/
	sync
fi

echo "Start ser2net..."
$PATH_ROOT/bin/ser2net -u
