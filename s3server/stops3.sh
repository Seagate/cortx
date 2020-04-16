#!/bin/sh
for s3unit in `systemctl list-units | grep s3server@  | grep "loaded active running"  | awk '{print $1}'`
do
  systemctl stop $s3unit
done

out=`systemctl list-units | grep s3server | grep "loaded active running"  | awk '{print $1}'`
if [ "$out" != "" ]; then
  systemctl stop s3server
fi
