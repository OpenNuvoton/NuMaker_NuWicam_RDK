#!/bin/sh
# Copyright (c) Nuvoton Technology Corp. All rights reserved.
# Description:	RTL8189FS		module enable script
#		W.C  Lin	wclin@nuvoton.com

# startup network service
if [ -f "RTL8189FS/8189fs.ko" ]; then
	bloaded=`lsmod | grep 8189fs | awk '{print $1}'`
	if [ "$bloaded" = "" ]; then
		insmod RTL8189FS/8189fs.ko
		if [ $? != 0 ]; then 
			exit 1
		fi
	fi
fi

#Concurrent mode
if ifconfig wlan0 up; then
   if ifconfig wlan1 up; then
        exit 0
   fi
fi

exit 1
