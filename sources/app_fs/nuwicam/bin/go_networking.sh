#!/bin/sh
# Copyright (c) Nuvoton Technology Corp. All rights reserved.
# Description:	WiFi startup script

export PATH=/sbin:$PATH

PATH_PRJNAME=/mnt/nuwicam

cd $PATH_PRJNAME/wifi
./network.sh &
