#!/bin/sh

echo -e "Content-type: text/html\r\n\r\n"
echo -e "NuWicam\r\n\r\n"
echo "<br>"
echo -e "Version: \r\n\r\n"
cat /mnt/nuwicam/etc/version.txt
