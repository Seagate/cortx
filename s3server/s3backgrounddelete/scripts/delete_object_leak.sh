#!/bin/sh
# Script to add leaked objects to probable delete index-id.

aws s3 rb s3://mybucket --force
echo Creating new bucket "mybucket"
aws s3 mb s3://mybucket
for value in {1..5}
do
echo Creating test object_$value
dd if=/dev/urandom of=object_$value bs=1M count=1 iflag=fullblock
echo Adding object_$value to S3server
aws s3 cp object_$value s3://mybucket/object_$value
rm -rf object_$value
done
curl -sS --header "x-seagate-faultinjection: enable,always,clovis_entity_delete_fail" -X PUT https://s3.seagate.com --cacert /etc/ssl/stx-s3-clients/s3/ca.crt
for value in {1..5}
do
echo Trying to delete object_$value from S3server
aws s3 rm s3://mybucket/object_$value
done
curl -sS --header "x-seagate-faultinjection: disable,noop,clovis_entity_delete_fail" -X PUT https://s3.seagate.com --cacert /etc/ssl/stx-s3-clients/s3/ca.crt
