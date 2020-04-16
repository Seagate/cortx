#!/bin/sh
# Script to add leaked objects to probable delete index-id.

aws s3 rb s3://mybucket --force
echo Creating new bucket "mybucket"
aws s3 mb s3://mybucket
for value in {1..2}
do
echo Creating test object
dd if=/dev/urandom of=test_object bs=1M count=1 iflag=fullblock
echo Adding object_$value to S3server
curl -sS --header "x-seagate-faultinjection: enable,always,clovis_entity_delete_fail" -X PUT https://s3.seagate.com --cacert /etc/ssl/stx-s3-clients/s3/ca.crt
aws s3 cp test_object s3://mybucket/test_object
done
rm -rf test_object
curl -sS --header "x-seagate-faultinjection: disable,noop,clovis_entity_delete_fail" -X PUT https://s3.seagate.com --cacert /etc/ssl/stx-s3-clients/s3/ca.crt
