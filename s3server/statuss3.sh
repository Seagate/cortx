#!/bin/sh
out=`systemctl list-units | grep s3server[@]*[0-9]*.service`
if [ "$out" != "" ]; then
  echo "Status of s3 service(s)"
  echo "======================="
  systemctl list-units | grep s3server[@]*[0-9]*.service
else
  echo "S3 service is stopped"
fi

