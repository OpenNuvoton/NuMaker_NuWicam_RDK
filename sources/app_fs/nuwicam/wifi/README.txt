Please copy whole "wifi" folder to /mnt/nand1-1/, 
and follow below steps to create a WiFi connection.

  1. type "cd /mnt/nand1-1/wifi" to change path to wifi directory
  2. edit network_config to set network configuration at first time.
     Please refer below descriptions to know what options you can choice.
  3. type "./network.sh" to startup a WiFi connection

[Example of network_config for STA Infra mode]
# set STA_BOOTPROTO to DHCP/STATIC
# set STA_IPADDR is only usful to STATIC IP
# set STA_GATEWAY is only usful to STATIC IP, a space means does not set it
# set STA_NETWORK_TYPE to Infra/Adhoc, only Infra is supported now
# set STA_SSID to Wireless AP's SSID
# set STA_AUTH_MODE to OPEN/SHARED/WPAPSK/WPA2PSK
# set STA_ENCRYPT_TYPE to NONE/WEP/TKIP/AES
# set STA_AUTH_KEY to authentication key, it could be
# 	WEP-HEX example: 4142434445
# 	WEP-ASCII example: "ABCDE"
# 	TKIP/AES-ASCII: 8~63 ASCII

[for WPA2PSK/AES authentication]
BOOTPROTO DHCP
IPADDR 192.168.1.99 
GATEWAY 192.168.1.1
NETWORK_TYPE Infra
SSID ASUS_RT-N53
AUTH_MODE WPA2PSK
ENCRYPT_TYPE AES
AUTH_KEY 12345678

[for SHARED/WEP authentication]
BOOTPROTO DHCP
IPADDR 192.168.1.99
GATEWAY 192.168.1.1
NETWORK_TYPE Infra
SSID ASUS_RT-N53
AUTH_MODE SHARED
ENCRYPT_TYPE WEP
AUTH_KEY "ABCDE"

[Example of network_config for AP mode]
# set AP_IPADDR to IP address
# set AP_SSID to Wireless AP's SSID
# set AP_AUTH_MODE to OPEN/SHARED/WEPAUTO/WPAPSK/WPA2PSK
# set AP_ENCRYPT_TYPE to NONE/WEP/TKIP/AES
# set AP_AUTH_KEY to authentication key, it could be
# 	TKIP/AES-ASCII: 8~63 ASCII
# set AP_CHANNEL to the channel number of AP, is only usful to STA_ENABLE is NO
#	If station mode is enabled, this channel number must be the same to station's AP

[for WPA2PSK authentication]
AP_IPADDR 192.168.100.1
AP_SSID "Sky Eye"
AP_AUTH_MODE WPA2PSK
AP_ENCRYPT_TYPE AES
AP_AUTH_KEY 12345678
AP_CHANNEL AUTO

[for Open/None authentication]
AP_IPADDR 192.168.100.1
AP_SSID "Sky Eye"
AP_AUTH_MODE OPEN
AP_AUTH_KEY 
AP_CHANNEL 11

Notice:
1. If SSID contain a space character, please use double quote to identify the name. For example,
     STA_SSID "BUFFALO G300N"
2. Channel 1-14 is 2.4 GHz ; Channel 36, 40, 44, 46, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 
   120, 124, 128, 132, 136, 140, 149, 153, 157, 161 is 5GHz. The channels that are available 
   for use in a particular country differ according to the regulations of that country.
