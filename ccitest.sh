#!/bin/sh
ADB="./adb"
#ADB="/home/$(whoami)/projects/quad_src_main_4.3/myandroid/out/host/linux-x86/bin/adb"
CCI_EXE="/home/cci/cci/cci-test"
#CCI_EXE="/home/$(whoami)/projects/solo_src_main_4.0/myandroid/out/target/product/sabresd_6dq/symbols/system/bin/cci-test"
cd /home/cci/cci
while [ 1 ]
do
	clear
	echo "**************************************************"
	echo "**************************************************"
	echo "Test script for CCI connectivity on the P14-TAB"
	echo "You can exit the script using ctrl+c"
	echo "**************************************************"
	echo "**************************************************"
	echo ""
	echo "Waiting for a P14-Tab to be connected to the USB port ..."
	echo ""
	$ADB wait-for-device
#	echo -en "\033c"
	echo "**************************************************"
	echo "**************************************************"
	echo "**************************************************"
	echo "*************New device connected*****************"
	echo "**************************************************"
	echo "**************************************************"
	echo "**************************************************"
#	echo "Start - Copy cci-test for TVRC - also checks su"
	$ADB shell su -c mount -o remount,rw /dev/block/mmcblk0p5 /system
	$ADB shell su -c chmod -R 777 /system/bin/
	$ADB wait-for-device
	$ADB push $CCI_EXE /system/bin/cci-test
	$ADB shell su -c chmod 755 /system/bin/cci-test
	$ADB shell su -c chmod 777 /dev/ttymxc1
	$ADB shell su -c chmod -R 755 /system/bin/
#	echo "End - Copy cci-test for TVRC - also checks su"
	echo "Check the volume on the TV ... NOW ..."
#	$ADB shell su -c cci-test
	$ADB shell cci-test
	sleep 10
done
echo "Outside the loop ????"
