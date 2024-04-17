#! /bin/sh

DESCRIPTION_FILE="usb.version"
SHORTNAME=`cat $DESCRIPTION_FILE|grep SHORTNAME|awk -F= '{print $2}'|sed "s/\n//g"`
LONGNAME=`cat $DESCRIPTION_FILE|grep LONGNAME|awk -F= '{print $2}'|sed "s/\n//g"`
VERSION=`cat $DESCRIPTION_FILE|grep VERSION|awk -F= '{print $2}'|sed "s/\n//g"`


echo "**********************************************************"
echo "* USBCORE Driver and stack for Silicon Graphics Irix 6.5 *"
echo "* By bsdero at gmail dot org, 2011/2012                  *"
echo "* Version $VERSION                                        *"
echo "**********************************************************"

echo "Unloading USB drivers "
echo "Unloading  usbehci"
DRVID=`ml| grep usbehci| awk '{ print $2}'`
/sbin/sync 
/sbin/ml unld -v $DRVID


echo "Unloading  usbuhci"
DRVID=`ml| grep usbuhci| awk '{ print $2}'`
/sbin/sync 
/sbin/ml unld -v $DRVID

echo "Unloading  usbcore"
DRVID=`ml| grep usbcore| awk '{ print $2}'`
/sbin/sync 
/sbin/ml unld -v $DRVID

