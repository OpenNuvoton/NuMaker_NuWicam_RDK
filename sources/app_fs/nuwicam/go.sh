#!/bin/sh
#
# Copyright (c) 2013 Nuvoton Technology Corp. All rights reserved.
#

PATH_ROOT=/mnt/nuwicam
cd $PATH_ROOT/bin

echo "clock setting"
/usr/nvtio -w 0xb0000204 0x71a1099f

echo "Wi-Fi network start"
rm -f /usr/wifi
ln -s $PATH_ROOT/wifi /usr/wifi
ln -s $PATH_ROOT/etc	/tmp/etc
./go_networking.sh &

echo "http server start"
./go_boa.sh  &

echo "Ser2net start"
./go_ser2net.sh &

echo "RTSP server start"     
./go_stream.sh &
