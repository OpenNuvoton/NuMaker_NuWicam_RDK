#!/bin/sh
# Copyright (c) Nuvoton Technology Corp. All rights reserved.
# Description:	WiFi startup script
# Version:	2013-06-30	first release version
#		2015-11-18      Auto-select chipset
#		W.C  Lin

if [ -z "$MSGTOTERMINAL" ]; then MSGTOTERMINAL="/dev/ttyS1"; fi

NETCONFIG_FILE=/tmp/etc/network_config

WPA_CONF_FILE=/tmp/wpa.conf
HOSTAPD_CONF_FILE=/tmp/hostapd.conf
DHCPD_CONF_FILE=/tmp/dnsmasq.conf
if [ ! -f $NETCONFIG_FILE ]; then
        echo "Can't find $NETCONFIG_FILE"
        exit 1
fi

CTRL_INTERFACE=`awk -F= '{if ($1=="ctrl_interface") {print $2}}' wpa.conf.default`

VALID_CHIPSET=`ls RTL*/*ko | awk -F/ 'NR == 1 {print $1}'`
echo "Auto-select $VALID_CHIPSET as a successor."
if [ "$VALID_CHIPSET" == "" ]; then
	echo "Can't find RTL folder."
	exit 1
fi

# set DEVICE to wlan0
STA_DEVICE=wlan0
# set WiFi Chipset to RTL*
STA_CHIPSET=$VALID_CHIPSET
# set BOOTPROTO to DHCP/STATIC
STA_BOOTPROTO=`awk '{if ($1=="BOOTPROTO") {print $2}}' $NETCONFIG_FILE`
# set IP address, only usful to STATIC IP
STA_IPADDR=`awk '{if ($1=="IPADDR") {print $2}}' $NETCONFIG_FILE`
# set GATEWAY address, only usful to STATIC IP
STA_GATEWAY=`awk '{if ($1=="GATEWAY") {print $2}}' $NETCONFIG_FILE`
# set Wireless AP's SSID
STA_SSID=`awk '{if ($1=="SSID") { print $2 }}' $NETCONFIG_FILE`
if [ "$(echo "$STA_SSID" | cut -c1)" = '"' ]; then
	STA_SSID=`awk -F\" '{if ($1=="SSID ") { print $2 }}' $NETCONFIG_FILE`
fi

# set AUTH_MODE to OPEN/SHARED/WPAPSK/WPA2PSK
STA_AUTH_MODE=`awk '{if ($1=="AUTH_MODE") {print $2}}' $NETCONFIG_FILE`
# set ENCRYPT_TYPE to NONE/WEP/TKIP/AES
STA_ENCRYPT_TYPE=`awk '{if ($1=="ENCRYPT_TYPE") {print $2}}' $NETCONFIG_FILE`
# set authentication key to be either
# WEP-HEX example: 4142434445
# WEP-ASCII example: ABCDE
# TKIP/AES-ASCII: 8~63 ASCII
STA_AUTH_KEY=`awk '{if ($1=="AUTH_KEY") {print $2}}' $NETCONFIG_FILE`
# Trigger Key
STA_WPS_TRIG_KEY=`awk '{if ($1=="WPS_TRIG_KEY") {print $2}}' $NETCONFIG_FILE`

# set DEVICE to wlan1
AP_DEVICE=wlan1
AP_CHIPSET=$STA_CHIPSET
AP_BOOTPROTO=STATIC
AP_IPADDR=`awk '{if ($1=="AP_IPADDR") {print $2}}' $NETCONFIG_FILE`
AP_SSID=`awk '{if ($1=="AP_SSID") { print $2}}' $NETCONFIG_FILE`
if [ "$(echo "$AP_SSID" | cut -c1)" = '"' ]; then
	AP_SSID=`awk -F\" '{if ($1=="AP_SSID ") { print $2 }}' $NETCONFIG_FILE`
fi
AP_AUTH_MODE=`awk '{if ($1=="AP_AUTH_MODE") {print $2}}' $NETCONFIG_FILE`
AP_ENCRYPT_TYPE=`awk '{if ($1=="AP_ENCRYPT_TYPE") {print $2}}' $NETCONFIG_FILE`
AP_AUTH_KEY=`awk '{if ($1=="AP_AUTH_KEY") {print $2}}' $NETCONFIG_FILE`
AP_CHANNEL=`awk '{if ($1=="AP_CHANNEL") {print $2}}' $NETCONFIG_FILE` 

BRIF=`awk '{if ($1=="BRIF") {print $2}}' $NETCONFIG_FILE`        

MONITOR_IF="wlan0"

IsWPS=0

DeviceInit()
{
	if [ "$BRIF" != "" ]; then
		if ! ifconfig $BRIF; then
			./brctl addbr $BRIF
			if ! ifconfig $BRIF; then
				BRIF=""
				return 3
			fi
		fi
	        ifconfig $BRIF up
        fi

	CHIPSET=$1
	if [ ! -f ./$CHIPSET/init.sh ]; then
       		echo "cannot find file $PWD/$CHIPSET/init.sh" > $MSGTOTERMINAL
        	return 1
        fi

	./$CHIPSET/init.sh
	if [ $? != 0 ]; then
	        echo "Initialize $CHIPSET failure" > $MSGTOTERMINAL
	        return 2
	fi

        return 0
}

DeviceFini()
{
	CHIPSET=$1
	if [ ! -f ./$CHIPSET/fini.sh ]; then
       		echo "cannot find file $PWD/$CHIPSET/init.sh" > $MSGTOTERMINAL
        	return 1
        fi

	./$CHIPSET/fini.sh
	if [ $? != 0 ]; then
	        echo "Finalize $CHIPSET failure" > $MSGTOTERMINAL
	        return 2
	fi

        return 0
}

WaitingForConnected()
{
   IsProcRun=`ps | grep "wpa_supplicant" | grep -v "grep" | sed -n '1P' | awk '{print $1}'`
   while [ 1 ]; do
         if [ "$IsProcRun" != "" ]; then break; fi
         IsProcRun=`ps | grep "wpa_supplicant" | grep -v "grep" | sed -n '1P' | awk '{print $1}'`
   done

   counter=0

   while [ 1 ]; do

      WPSStatus=`./wpa_cli -p $CTRL_INTERFACE -i $STA_DEVICE status | awk -F= '{if ($1=="wpa_state") {print $2}}'`

      echo "Wait.. ($counter/30) " > $MSGTOTERMINAL

      if [ "$WPSStatus" == "COMPLETED" ] && [ "$WPSStatus" != "" ]; then

		echo "Save configuration to file ..."
		./wpa_cli -p $CTRL_INTERFACE save_config

		echo "Show AP information"
		./wpa_cli -p $CTRL_INTERFACE status

                if [ $IsWPS = 1 ]; then
			echo "Restore to network_config"
			./wpa_conf_restore -f $WPA_CONF_FILE -o $NETCONFIG_FILE -t 0
		fi

		break;

      elif [ "$WPSStatus" == "INACTIVE" ]; 

		then return 5;

      fi

      counter=`expr $counter + 1`
      if [ $counter = 30 ]; then 
		echo "Timeout!!"
		return 6; 
      fi
      sleep 1

   done

   return 0
}


ConfigurationSta()
{   

   if DeviceInit $STA_CHIPSET; then 

	ifconfig $STA_DEVICE down
	ifconfig $STA_DEVICE up

        cat ./wpa.conf.default 				> $WPA_CONF_FILE
	
	echo "network={"			>>$WPA_CONF_FILE
	echo "		ssid=\"$STA_SSID\""	>>$WPA_CONF_FILE
        echo "          scan_ssid=1"     	>>$WPA_CONF_FILE
	
	# set AUTH_MODE to OPEN/SHARED/WEPAUTO/WPAPSK/WPA2PSK/WPANONE
	case $STA_AUTH_MODE in
        "NONE")
		echo "		key_mgmt=NONE"          >>$WPA_CONF_FILE
        ;;
        "WEPAUTO")
                echo "  key_mgmt=NONE"   >>$WPA_CONF_FILE               
                echo "  auth_alg=OPEN SHARED"   >>$WPA_CONF_FILE         
                if [ "$STA_ENCRYPT_TYPE" != "NONE" ]; then                      
                    echo "  wep_key0=\"$STA_AUTH_KEY\"" >>$WPA_CONF_FILE
                    echo "  wep_tx_keyidx=0"        	>>$WPA_CONF_FILE
                fi
        ;;
        "OPEN"|"SHARED")
		echo "	key_mgmt=NONE"		>>$WPA_CONF_FILE
		if [ "$STA_AUTH_MODE" == "SHARED" ]; then
                        echo "  auth_alg=SHARED"      >>$WPA_CONF_FILE
                else
	                echo "  auth_alg=OPEN"        >>$WPA_CONF_FILE
	        fi
                if [ "$STA_ENCRYPT_TYPE" != "NONE" ]; then
		        echo "  wep_key0=\"$STA_AUTH_KEY\""   >>$WPA_CONF_FILE
		        echo "  wep_tx_keyidx=0"        >>$WPA_CONF_FILE
		fi
	;;
        "WPAPSK"|"WPA2PSK")
		if [ "$STA_AUTH_MODE" == "WPAPSK" ]; then
	        	echo "	proto=WPA"	>>$WPA_CONF_FILE
		else
			echo "	proto=WPA2"	>>$WPA_CONF_FILE
		fi

                echo "	key_mgmt=WPA-PSK"       >>$WPA_CONF_FILE
	        case $STA_ENCRYPT_TYPE in
		"TKIP")	
	                echo "	pairwise=TKIP"  >>$WPA_CONF_FILE
		;;
                "AES")
                	echo "	pairwise=CCMP"  >>$WPA_CONF_FILE
                ;;			
		esac	
		echo "	psk=\"$STA_AUTH_KEY\""	>>$WPA_CONF_FILE	
        ;;

	*)
	        echo "The mode wasn't supported!!"
                return 1
	;;
        esac
	
	echo "}"	>>$WPA_CONF_FILE

	sync

  	killall -9 wpa_supplicant
	rm -f $CTRL_INTERFACE"/"$STA_DEVICE

	if [ "$BRIF" != "" ]; then
		./wpa_supplicant -c $WPA_CONF_FILE -i$STA_DEVICE -Dwext -b $BRIF &
	else
		./wpa_supplicant -c $WPA_CONF_FILE -i$STA_DEVICE -Dwext &
	fi
	
	sleep 1
 	counter=0
	while [ 1 ]; do
        	IsProcRun=`ps | grep "wpa_supplicant" | grep -v "grep" | sed -n '1P' | awk '{print $1}'`
	       	if [ "$IsProcRun" != "" ] || [ $counter = 10 ]; then break; fi
	       	counter=`expr $counter + 1`
	done

	if [ $counter != 10 ]; then 
	  	if WaitingForConnected; then return 0; fi
	fi
   fi
  
   killall wpa_supplicant

   
   ifconfig $STA_DEVICE down
   #DeviceFini $STA_CHIPSET

   return 1

}

ConfigurationSoftAP()
{

   if DeviceInit $AP_CHIPSET; then 

	ifconfig $AP_DEVICE down
	ifconfig $AP_DEVICE up

	if [ -f "/sys/class/net/"$AP_DEVICE"/address" ]; then
		MACADDR=`cat "/sys/class/net/"$AP_DEVICE"/address" | awk '{gsub(/:/,"-",$1); print $1}'`
		AP_SSID=`echo -e $AP_SSID"_"$MACADDR`
		echo $AP_SSID
 
		if [ "$STA_SSID" != "" ] && [ -S $CTRL_INTERFACE"/"$STA_DEVICE ]; then	 
			WPAStatus=`./wpa_cli -i $STA_DEVICE -p $CTRL_INTERFACE status | awk -F= '{if ($1=="wpa_state") {print $2}}'`
		fi

		if [ "$WPAStatus" == "COMPLETED" ]; then
  			freq=`./wpa_cli -i $STA_DEVICE -p $CTRL_INTERFACE scan_result| awk '{if (NF>5) {for(i=6;i<=NF;i++) $5=$5" "$i} if ($5=="'"$STA_SSID"'") {printf $2}}'`
			echo "freq=$freq"
			freq_off=`expr $freq - 2407`
			ch=`expr $freq_off / 5`
		else
			if [ "$AP_CHANNEL" == "AUTO" ]; then
				echo "Select a channel automatically"
				if cat /proc/net/rtl*/$AP_DEVICE/best_channel; then
					echo "To scanning..."
					./iwlist $AP_DEVICE scan > /dev/null
					cat /proc/net/rtl*/$AP_DEVICE/best_channel
					ch=`cat /proc/net/rtl*/$AP_DEVICE/best_channel | awk '{if ($1=="best_channel_24G") {printf $3}}' `
					echo "The channel $ch is best."
				else
					ch=6
				fi				
			else
				ch=$AP_CHANNEL
			fi
		fi

		echo "Channel-->$ch"

	        cat ./hostapd.conf.default		 >$HOSTAPD_CONF_FILE

		if [ $ch -gt "14" ]; then
			echo "hw_mode=a"		>>$HOSTAPD_CONF_FILE
		else
	                echo "hw_mode=g"		>>$HOSTAPD_CONF_FILE
		fi


	        if [ "$BRIF" != "" ]; then
	                echo "bridge=$BRIF"		>>$HOSTAPD_CONF_FILE
	        fi
 
		echo "ssid=$AP_SSID"			>>$HOSTAPD_CONF_FILE
		echo "interface=$AP_DEVICE"		>>$HOSTAPD_CONF_FILE
		echo "channel=$ch"	  		>>$HOSTAPD_CONF_FILE

		# set AP_AUTH_MODE to OPEN/SHARED/WPAPSK/WPA2PSK
		case $AP_AUTH_MODE in
	        "NONE")
	        	echo "auth_algs=1"		>>$HOSTAPD_CONF_FILE
	        	echo "wpa=0"			>>$HOSTAPD_CONF_FILE
	        ;;
        
	        "OPEN"|"SHARED")
	                if [ "$AP_AUTH_MODE" == "SHARED" ]; then
	                	echo "auth_algs=2"      >>$HOSTAPD_CONF_FILE
	                else
		                echo "auth_algs=1"      >>$HOSTAPD_CONF_FILE
		        fi
			if [ "$AP_ENCRYPT_TYPE" != "NONE" ]; then
		          echo "wep_default_key=0"	>>$HOSTAPD_CONF_FILE
		          echo "wep_key0=\"$AP_AUTH_KEY\"" >>$HOSTAPD_CONF_FILE
			fi
		        echo "wpa=0"            	>>$HOSTAPD_CONF_FILE
		;;
	
		"WEPAUTO")
                	echo "auth_algs=3"		>>$HOSTAPD_CONF_FILE
	                echo "wep_default_key=0"	>>$HOSTAPD_CONF_FILE
        	        echo "wep_key0=$AP_AUTH_KEY"	>>$HOSTAPD_CONF_FILE
	                echo "wpa=0"			>>$HOSTAPD_CONF_FILE
		;;

        	"WPAPSK"|"WPA2PSK")
	        	echo "auth_algs=3"		>>$HOSTAPD_CONF_FILE
        		
			if [ "$AP_AUTH_MODE" == "WPAPSK" ]; then
	        		echo "wpa=1"		>>$HOSTAPD_CONF_FILE
			else
				echo "wpa=2"		>>$HOSTAPD_CONF_FILE
			fi

	                echo "wpa_key_mgmt=WPA-PSK"	>>$HOSTAPD_CONF_FILE
		        case $AP_ENCRYPT_TYPE in
			"TKIP")	
		                echo "wpa_pairwise=TKIP" >>$HOSTAPD_CONF_FILE
			;;
	                "AES")
				echo "ieee80211n=1"	 >>$HOSTAPD_CONF_FILE
	                	echo "wpa_pairwise=CCMP" >>$HOSTAPD_CONF_FILE
	                ;;			
			esac	
			#psk_str=`./wpa_passphrase $AP_SSID $AP_AUTH_KEY`
			echo "wpa_passphrase=$AP_AUTH_KEY"	>>$HOSTAPD_CONF_FILE	
	        ;;

		*)
		        echo "The mode wasn't supported!!"
	                return 1
		;;
	        esac
		sync

		killall hostapd
		while [ 1 ]; do
			IsProcRun=`ps | grep "hostapd" | grep -v "grep" | sed -n '1P' | awk '{print $1}'`
		        if [ "$IsProcRun" == "" ]; then break; fi
		done
		
		./hostapd $HOSTAPD_CONF_FILE &
   		sleep 1
	 	counter=0
		while [ 1 ]; do
			IsProcRun=`ps | grep "hostapd" | grep -v "grep" | sed -n '1P' | awk '{print $1}'`
		        if [ "$IsProcRun" != "" ] || [ $counter = 10 ]; then break; fi
		       	counter=`expr $counter + 1`
		done

		if [ $counter != 10 ]; then return 0; fi

	fi
  fi

  ifconfig $AP_DEVICE down
  killall -9 hostapd

  return 1

}


ConfigurationWPS()
{

if [ $# -ge 1 ]; then

   if DeviceInit $STA_CHIPSET; then 
   
   	killall -9 hostapd
        ifconfig $AP_DEVICE down
        
	ifconfig $STA_DEVICE down
	ifconfig $STA_DEVICE up

        cat ./wpa.conf.default 				> $WPA_CONF_FILE
	
  	killall -9 wpa_supplicant
	rm -f $CTRL_INTERFACE"/"$STA_DEVICE

        if [ "$BRIF" != "" ]; then                                              
                ./wpa_supplicant -c $WPA_CONF_FILE -i$STA_DEVICE -Dwext -b $BRIF
        else                                                                    
                ./wpa_supplicant -c $WPA_CONF_FILE -i$STA_DEVICE -Dwext &       
        fi     

	sleep 1
 	counter=0
	while [ 1 ]; do
        	IsProcRun=`ps | grep "wpa_supplicant" | grep -v "grep" | sed -n '1P' | awk '{print $1}'`
	       	if [ "$IsProcRun" != "" ] || [ $counter = 10 ]; then break; fi
	       	counter=`expr $counter + 1`
	done

	if [ $counter != 10 ]; then 

		if [ "$1" = "PBC" ]; then
			retry=1
			while [ $retry != 3 ]; do
				ifconfig $STA_DEVICE down
				ifconfig $STA_DEVICE up
				echo "start WPS PBC mode( $retry / 2 )"       > $MSGTOTERMINAL
				./wpa_cli -p $CTRL_INTERFACE  -i $STA_DEVICE wps_pbc	> $MSGTOTERMINAL
			  	if WaitingForConnected; then return 0; fi
			  	retry=`expr $retry + 1`
			done

		elif [ "$1" = "PINE" ]; then
			echo "start WPS PIN Enrollee mode"	> $MSGTOTERMINAL
			PIN_CODE=`./wpa_cli -p $CTRL_INTERFACE  -i $STA_DEVICE wps_pin any`
			echo -e "\033[1;32mPlease enter the pin code into AP's wps_pin setup page\033[m" > $MSGTOTERMINAL
			echo -e "PIN CODE: \033[1;33m$PIN_CODE\033[m" > $MSGTOTERMINAL
		  	if WaitingForConnected; then return 0; fi
		fi

	fi

     killall wpa_supplicant
     rm -f $CTRL_INTERFACE"/"$STA_DEVICE

     ifconfig $STA_DEVICE down
     #DeviceFini $STA_CHIPSET

  fi

fi

return 1
}

ConfigurationAirKiss()
{
	ret=1
    if [ ! -f ./$STA_CHIPSET/"monitor.sh" ]; then
		echo "Not support monitor"
    else
		# Enter monitor to sniff all IEEE802.11 packets
		./$STA_CHIPSET/monitor.sh

		cat $NETCONFIG_FILE
		if [ -f /tmp/network_config_airkiss ]; then rm -f /tmp/network_config_airkiss; fi

		# Run Airkiss
		while [ 1 ]; do
			if [ -f "/tmp/airkiss.txt" ]; then rm -f /tmp/airkiss.txt; fi
			if ! ./airkiss -i $MONITOR_IF -n 200000; then
				echo "./airkiss -i $MONITOR_IF -n 200000"
			elif [ -f "/tmp/airkiss.txt" ]; then
				ifconfig wlan0 down
				cp $NETCONFIG_FILE 	$NETCONFIG_FILE.tmp
				cp -f /tmp/network_config_airkiss $NETCONFIG_FILE
				cat $NETCONFIG_FILE
				echo "./network.sh Infra-only"
				./network.sh "Infra-only"
				ret=$?
				cp -f $NETCONFIG_FILE.tmp 	$NETCONFIG_FILE
				cat $NETCONFIG_FILE
				if [ "$ret" == "0" ]; then
					echo "Save configuration to file ..."
					./wpa_cli -p $CTRL_INTERFACE save_config
					echo "Show AP information"
			
					./wpa_cli -p $CTRL_INTERFACE status
					echo "Restore to network_config"
					./wpa_conf_restore -f $WPA_CONF_FILE -o $NETCONFIG_FILE -t 0
				
					AIRKISS_MAGIC=`awk '{if ($1=="AIRKISS_RANDOM") {print $2}}' /tmp/airkiss.txt`
					./airkiss -i wlan0 -f $AIRKISS_MAGIC
					ret=$?
				fi
				break
			else
				sleep 1
			fi	
		done 
    fi
    return $ret
}

DHCPSrv_Start()
{
	DHCPD_CONF_FILE=/tmp/dnsmasq.conf.$1
	DHCPD_RESOLV_FILE=/tmp/resolv-file.$1
	DHCPD_LEASES_FILE=/tmp/dnsmasq.leases.$1
	DHCPD_PID_FILE=/tmp/dnsmasq.pid.$1
	DHCPD_DEVICE=$AP_DEVICE
	DHCPD_IPADDR=$AP_IPADDR
			                
	echo "ifconfig $DHCPD_DEVICE $DHCPD_IPADDR netmask 255.255.255.0"
	if ifconfig $DHCPD_DEVICE $DHCPD_IPADDR netmask 255.255.255.0; then
		echo -e "My IP: \033[1;33m$DHCPD_IPADDR\033[1;33m" > $MSGTOTERMINAL
	fi

	echo "Starting DHCP server." > $MSGTOTERMINAL

	if [ -f $DHCPD_PID_FILE ]; then killall dnsmasq --pid-file=$DHCPD_PID_FILE ; fi

	if [ -f $DHCPD_CONF_FILE ] ; then rm -f $DHCPD_CONF_FILE; fi

             echo "interface=$DHCPD_DEVICE" > $DHCPD_CONF_FILE
             echo "resolv-file=$DHCPD_RESOLV_FILE" >> $DHCPD_CONF_FILE
             echo "dhcp-leasefile=$DHCPD_LEASES_FILE">> $DHCPD_CONF_FILE
             echo "dhcp-lease-max=10" >> $DHCPD_CONF_FILE

	     echo "dhcp-option=lan,3,$DHCPD_IPADDR" >> $DHCPD_CONF_FILE
	     # Append domain name
	     echo "domain=nuvoton.com" >> $DHCPD_CONF_FILE

             # Disable DNS-query server option.
             echo "dhcp-option=lan,6" >> $DHCPD_CONF_FILE

             # DHCP release on Window platform.
             echo "dhcp-option=vendor:MSFT,2,1i" >> $DHCPD_CONF_FILE

             echo "dhcp-authoritative">> $DHCPD_CONF_FILE
             SUBNET=`echo $DHCPD_IPADDR | awk '{FS="."} {print $1 "." $2 "." $3}'`
             echo "dhcp-range=lan,$SUBNET.100,$SUBNET.109,255.255.255.0,14400m" >> $DHCPD_CONF_FILE
             echo "stop-dns-rebind" >> $DHCPD_CONF_FILE
             sync

	./dnsmasq --pid-file=$DHCPD_PID_FILE --conf-file=$DHCPD_CONF_FILE  --user=root --group=root --dhcp-fqdn &

}

Concurrent_Bridge()
{
	echo "./brctl stp $BRIF off"
	./brctl stp $BRIF off

	echo "./brctl setfd $BRIF 0"
        ./brctl setfd $BRIF 0

	echo "./brctl addif $BRIF $STA_DEVICE"
	./brctl addif $BRIF $STA_DEVICE 

	echo "./brctl addif $BRIF $AP_DEVICE"
	./brctl addif $BRIF $AP_DEVICE

        echo -e "Leasing an IP address (Bridge)..." > $MSGTOTERMINAL
        echo "udhcpc -i $BRIF -q -T 2"
        udhcpc -i $BRIF -q -T 2
        echo -e "Got IP: \033[1;33m"`ifconfig $BRIF | grep inet | awk '{FS=":"} {print $2}' | sed 's/[^0-9\.]//g'`"\033[m" > $MSGTOTERMINAL
}

ConfigurationNetplug()
{
	if grep -r $STA_DEVICE /etc/netplug/netplugd.conf; then
		echo "$STA_DEVICE already in /etc/netplug/netplugd.conf"
		return 0;
	fi

	  if [ "$STA_BOOTPROTO" == "DHCP" ] ; then
		echo "auto  $STA_DEVICE" >> /etc/network/interfaces                   
		echo "	iface $STA_DEVICE inet dhcp" >> /etc/network/interfaces    
		if ! cat /etc/netplug/netplugd.conf | grep $STA_DEVICE; then
			echo $STA_DEVICE >> /etc/netplug/netplugd.conf
		fi
	elif [ "$STA_BOOTPROTO" == "STATIC" ]; then
		echo "auto  $STA_DEVICE" >> /etc/network/interfaces
		echo "	iface $STA_DEVICE inet static" >> /etc/network/interfaces
		echo "	address $STA_IPADDR" >> /etc/network/interfaces
	  	echo "	netmask 255.255.255.0" >> /etc/network/interfaces
		echo "  gateway $STA_GATEWAY" >> /etc/network/interfaces
		if ! cat /etc/netplug/netplugd.conf | grep $STA_DEVICE; then
			echo $STA_DEVICE >> /etc/netplug/netplugd.conf
		fi		
	fi
}

ConfigurationIPAddr()
{
	  if [ "$STA_BOOTPROTO" == "DHCP" ] || [  $IsWPS = 1 ] ; then
		killall udhcpc
   		echo -e "Leasing an IP address ..." > $MSGTOTERMINAL
	    	echo "udhcpc -i $STA_DEVICE"
		udhcpc -i $STA_DEVICE -q -T 2
    		echo -e "Got IP: \033[1;33m"`ifconfig $STA_DEVICE | grep inet | awk '{FS=":"} {print $2}' | sed 's/[^0-9\.]//g'`"\033[m" > $MSGTOTERMINAL
	elif [ "$STA_BOOTPROTO" == "STATIC" ]; then
		echo "ifconfig $STA_DEVICE $STA_IPADDR netmask 255.255.255.0"
		if ifconfig $STA_DEVICE $STA_IPADDR netmask 255.255.255.0; then
			echo -e "My IP: \033[1;33m$STA_IPADDR\033[1;33m" > $MSGTOTERMINAL
			if route add default gw $STA_GATEWAY; then
				echo -e "Gateway: \033[1;33m$STA_GATEWAY\033[1;33m" > $MSGTOTERMINAL
				echo "nameserver 168.95.1.1" > /etc/resolv.conf
			fi
		fi
	fi
}


# Mode
case $1 in
   "AIRKISS")
	if ConfigurationAirKiss; then
		echo "ConfigurationAirKiss Done"
        ./network.sh
        exit $?		
	fi
   ;;
   "WPS")
	IsWPS=1
	case $2 in
	"PBC"|"PINE")
		IsWPS=1
		if ConfigurationWPS $2; then 
			if [ "$BRIF" == "" ]; then
		  		if [ ! -d /usr/netplug ]; then
					ConfigurationIPAddr $STA_DEVICE
				else
					ConfigurationNetplug $STA_DEVICE
				fi
			fi				
		fi
               ./network.sh SoftAP
                exit $?
	;;
	*)
                echo "[WPS] No support $2 in $1 mode" > $MSGTOTERMINAL
		echo "Usage: ./network.sh WPS PBC|PINE" > $MSGTOTERMINAL
                exit 1
	;;
        esac
   ;;
   "SoftAP")
	if  [ "$AP_SSID" != "" ]; then
		if ConfigurationSoftAP; then 
			if [ "$BRIF" == "" ]; then
				DHCPSrv_Start $AP_DEVICE
			else
				Concurrent_Bridge
			fi
			echo "SoftAP mode is created."
		else
			exit 1
		fi
	fi
   ;;
   "Infra-only")
	 if  [ "$STA_SSID" != "" ]; then
              if ConfigurationSta; then
                     if [ "$BRIF" == "" ]; then
                            if [ ! -d /usr/netplug ]; then
		                ConfigurationIPAddr $STA_DEVICE
                            else
                                ConfigurationNetplug $STA_DEVICE
                            fi
                     fi
		     exit 0
              fi
        fi
	exit 1
   ;;
   "Infra"|*)
        if  [ "$STA_SSID" != "" ]; then
                if ConfigurationSta; then
                        if [ "$BRIF" == "" ]; then
		  		if [ ! -d /usr/netplug ]; then
                    ConfigurationIPAddr $STA_DEVICE
				else
					ConfigurationNetplug $STA_DEVICE
				fi
                        fi
                fi
        fi
        ./network.sh SoftAP
        exit $?
    ;;
esac

TS=`cat /proc/uptime | awk '{print $1}'`
echo -e "\033[1;33m[$TS] network-$1 done.\033[m"

exit 0
