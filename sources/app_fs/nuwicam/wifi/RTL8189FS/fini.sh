#!/bin/sh
# Copyright (c) Nuvoton Technology Corp. All rights reserved.
# Description:	RTL8188		module enable script
#		W.C  Lin	wclin@nuvoton.com

# Stop network service

ifconfig wlan0 down
ifconfig wlan1 down

bloaded=`lsmod | grep 8189fs | awk '{print $1}'`
if [ "$bloaded" != "" ]; then
        if rmmod 8189fs; then
                exit 0
        fi
fi

exit 1

