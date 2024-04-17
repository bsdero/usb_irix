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

echo "Loading USB drivers "
echo "loading usbcore"
/sbin/sync 
/sbin/sync
/sbin/ml ld -v -c usbcore.o -p usbcore_


echo "loading usbehci"
/sbin/sync 
/sbin/sync
/sbin/ml ld -v -c usbehci.o -p usbehci_

echo "loading usbuhci"
/sbin/sync 
/sbin/sync
/sbin/ml ld -v -c usbuhci.o -p usbuhci_

