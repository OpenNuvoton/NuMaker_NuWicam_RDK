#!/bin/sh
#
# Copyright (c) 2013 Nuvoton Technology Corp. All rights reserved.
#

PATH_ROOT=/mnt/nuwicam
PATH_CONFIG=/mnt/nuwicam/etc/stream_config

while [ 1 ]; do
	
	if  [ -f "/mnt/nuwicam/etc/stream_config" ]; then
		sync
		
		# set video-in width
		STREAM_VINWIDTH=`awk '{if ($1=="VINWIDTH") {print $2}}' $PATH_CONFIG`
		
		# set video-in height
		STREAM_VINHEIGHT=`awk '{if ($1=="VINHEIGHT") {print $2}}' $PATH_CONFIG`

		# set JPEG encoding width
		STREAM_JPEGENCWIDTH=`awk '{if ($1=="JPEGENCWIDTH") {print $2}}' $PATH_CONFIG`

		# set JPEG encoding height
		STREAM_JPEGENCHEIGHT=`awk '{if ($1=="JPEGENCHEIGHT") {print $2}}' $PATH_CONFIG`

		# set RTSP transmitted bitrate
		STREAM_BITRATE=`awk '{if ($1=="BITRATE") {print $2}}' $PATH_CONFIG`
		STREAM_BITRATE=$(expr $STREAM_BITRATE \* 1000)

		echo "Start RTSP server..."
		
		echo "$PATH_ROOT/bin/RTSPServer -w $STREAM_VINWIDTH -d $STREAM_VINHEIGHT -i $STREAM_JPEGENCWIDTH -e $STREAM_JPEGENCHEIGHT -b $STREAM_BITRATE"

		$PATH_ROOT/bin/RTSPServer -w $STREAM_VINWIDTH -d $STREAM_VINHEIGHT -i $STREAM_JPEGENCWIDTH -e $STREAM_JPEGENCHEIGHT -b $STREAM_BITRATE
		
		echo "Stop RTSP server..."

		sleep 1
		
	else
		echo "$PATH_CONFIG is non-exist"
		break
	fi
	
done
