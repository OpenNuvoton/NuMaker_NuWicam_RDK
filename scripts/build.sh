#!/bin/bash

DEV_ROOT=`dirname $0`/..
DEV_ROOT=`cd $DEV_ROOT; pwd`

REV_ID=`git rev-parse HEAD`
REV_ID=${REV_ID:0:7}

HOST_CPU_NUM=$((`grep '^processor' /proc/cpuinfo | wc -l` * 2))

APT_GET="sudo apt-get -y -q"
RM="rm -rf"
TAR="tar -v"
MKDIR="mkdir -p -v"
MAKE="make"
CP="cp -v"
CHMOD="sudo chmod -v"
STRIP="arm-linux-strip -v"
LN="ln -s"
ZIP="zip -r -v"
LS="ls -al"
SYM_LINK="ln"
TOOLCHAIN_ABS_PATH="/usr/local/arm_linux_4.8"

SOURCES_PATH=$DEV_ROOT/sources/
IMAGES_PATH=$DEV_ROOT/sources/image/

export PATH=$DEV_ROOT/compiler/arm_linux_4.8/bin:$PATH
sensor_set="gc0308 tvp5150 gm7150"

toolchain_make()
{
    echo "$FUNCNAME"
    echo "Updating package list..."
    $APT_GET update

    echo "Installing tools..."
    $APT_GET install lzop zip automake autoconf bison flex mtd-tools

    if [ `uname -m` == 'x86_64' ]; then
        echo Installing 32bit libraries...
        $APT_GET install --force-yes ia32-libs ia32-libs-multiarch liblzo2-2:i386 liblzma5:i386
	$APT_GET install --force-yes lib32ncurses5 lib32z1 lib32bz2
	$APT_GET install --force-yes lib32stdc++6
    fi

    if [[ -L "$TOOLCHAIN_ABS_PATH" && -d "$TOOLCHAIN_ABS_PATH" ]]; then
                echo "Toolchain already exist!"
    else
	    if [ ! -d "$DEV_ROOT/compiler/arm_linux_4.8" ]; then
	        $MKDIR $DEV_ROOT/compiler
	        cd $DEV_ROOT/compiler

	        echo "Uncompressing gcc toolchain..."
	        $TAR -zxf arm_linux_4.8.tar.gz
	    fi
	    echo "To make symbol link"
  	    sudo $LN $DEV_ROOT/compiler/arm_linux_4.8/  $TOOLCHAIN_ABS_PATH
    fi

    arm-linux-gcc -v
}

toolchain_clean()
{
	echo "$FUNCNAME"
	$RM $DEV_ROOT/compiler/arm_linux_4.8/
	sudo $RM $TOOLCHAIN_ABS_PATH
}

initramfs_make()
{	
	echo "$FUNCNAME"
	if [ -f $SOURCES_PATH/"rootfs-2.6.35.tar.gz" ]; then
		cd $SOURCES_PATH
		sudo $TAR -zxf rootfs-2.6.35.tar.gz
		sudo chown -R `whoami` rootfs-2.6.35
		sudo chgrp -R `whoami` rootfs-2.6.35
		$LS $SOURCES_PATH/rootfs-2.6.35
		cd $DEV_ROOT
	fi
}

initramfs_clean()
{
	echo "$FUNCNAME"
	if [ -d "$SOURCES_PATH/rootfs-2.6.35" ]; then
		$RM "$SOURCES_PATH/rootfs-2.6.35"
	fi
}

busybox_make()
{	
	echo "$FUNCNAME"
	if [ "$SOURCES_PATH/applications/busybox-1.15.2/" ]; then
		cd $SOURCES_PATH/applications/busybox-1.15.2/
		cp .config_nuwicam .config
		if make -j $HOST_CPU_NUM; then make install;	fi
		cd $DEV_ROOT
	fi
}

busybox_clean()
{
	echo "$FUNCNAME"
	if [ "$SOURCES_PATH/applications/busybox-1.15.2/" ]; then
		cd $SOURCES_PATH/applications/busybox-1.15.2/
		make clean
		cd $DEV_ROOT
	fi
}

appfs_make()
{	
	echo "$FUNCNAME"
	if [ ! -f "$SOURCES_PATH/app_fs/nuwicam" ]; then
		cd $SOURCES_PATH/app_fs
		tar zxvf nuwicam.tar.gz
		cd $DEV_ROOT
	fi
}

appfs_clean()
{
	echo "$FUNCNAME"
}

applicatons_make()
{	
	echo "$FUNCNAME"
        if [ "$SOURCES_PATH/applications/" ]; then
                cd $SOURCES_PATH/applications/
                ./build_all.sh
                cd $DEV_ROOT
        fi
}

applicatons_clean()
{	
	echo "$FUNCNAME"
}

kernel_make()
{
	echo "$FUNCNAME"
	if [ "$SOURCES_PATH/kernel" ]; then
		cd $SOURCES_PATH/kernel
		for sensor in $sensor_set; do
			make ARCH=arm "nuwicam_"$sensor"_defconfig"
			if ./build spi; then
				ls -al ../image/conprog.gz
				mv ../image/conprog.gz ../image/conprog.gz.$sensor
			fi
		done
		cd $DEV_ROOT
	fi	
}

kernel_clean()
{	
	echo "$FUNCNAME"
        if [ "$SOURCES_PATH/kernel" ]; then
                cd $SOURCES_PATH/kernel
                make distclean
                cd $DEV_ROOT
        fi
}

image_pack()
{	
	echo "$FUNCNAME"
	cd $DEV_ROOT
	mkdir -p $DEV_ROOT/output

        if [ "$SOURCES_PATH/app_fs" ]; then
                cd $SOURCES_PATH/app_fs
                ./build_jffs2_image.sh
        fi

        for sensor in $sensor_set; do
		if [ -f $IMAGES_PATH"/conprog.gz."$sensor ]; then
			if [ -d $DEV_ROOT"/output/autowriter_"$sensor ]; then
				rm -rf $DEV_ROOT"/output/autowriter_"$sensor
			fi
			cp -af $DEV_ROOT"/tool/autowriter" $DEV_ROOT"/output/autowriter_"$sensor
			cp -af $IMAGES_PATH"/conprog.gz."$sensor   	$DEV_ROOT"/output/autowriter_"$sensor/conprog.gz
			cp -af $IMAGES_PATH"/app_fs_jffs2.img"   	$DEV_ROOT"/output/autowriter_"$sensor/
			sed -i 's/NUWICAM_UVC_VideoIn.bin/NUWICAM_UVC_VideoIn_'$sensor'.bin/g' $DEV_ROOT"/output/autowriter_"$sensor"/AutoWriter.ini"
			ls -al $DEV_ROOT"/output/autowriter_"$sensor/
		fi
        done

	echo "Please refer user guide to programming these firmwares."
	ls -al $DEV_ROOT"/output/"
        cd $DEV_ROOT
}

all_make()
{
	$MKDIR $DEV_ROOT/output
	toolchain_make
	initramfs_make
	busybox_make
	appfs_make
	applicatons_make
	kernel_make
	image_pack
}

all_clean()
{
	toolchain_clean
	busybox_clean
	applicatons_clean
	initramfs_clean
	appfs_clean
 	kernel_clean
    	$RM $DEV_ROOT/output
    	git status -s --ignored
}

usage()
{
    echo "Usage: $0 [-v] [clean]"
    echo "Build NuWicam Firmware".
}

all_make
#all_clean
