#!/usr/bin/env bash

#This script helps to create a device configuration file viz. devices.conf.
#The file uses yaml format, as desired by the m0d program.
#The script uses the fdisk command and extracts only the unused devices
#present on the system (i.e without valid partition table).
#Below illustration describes a typical devices.conf entry,
#Device:
#       - id: 1
#	  filename: /dev/sda

i=0
echo "Device:" > ./devices.conf
sudo fdisk -l 2>&1 |  grep "doesn't contain a valid partition table"  | awk '{ print $2 }'| grep "/dev" | grep [a-z]$ | while read line
do
echo "       - id: $i" >> ./devices.conf
echo "         filename: $line" >> ./devices.conf
i=`expr $i + 1`
done
