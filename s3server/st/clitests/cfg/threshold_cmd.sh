#!/bin/bash

# Parameters passed by callback
# $1 = %fsName, The name of the file system experiencing the event
fsname=$1
echo `date` > /tmp/threshold.log 2>&1
/usr/lpp/mmfs/bin/mmapplypolicy $fsname --single-instance >> /tmp/threshold.log 2>&1
