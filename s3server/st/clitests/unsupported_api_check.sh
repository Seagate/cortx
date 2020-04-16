#!/bin/sh
# Script to test unsupported APIs. Calling these APIs should return NotImplemented message.
check_501_response()
{
   METHOD=$1
   QUERY=$2
   DISPLAY_METHOD=$3
   ADDITIONAL_HEADER=$4

   #Execute curl query on provided METHOD and QUERY.
   printf "%s  %-40s" "Checking NotImplemented return code for " "[$DISPLAY_METHOD]"
   if [ -z "$4" ] ;then
     echo $(curl -s -X $METHOD -H "Keep-Alive: 60" -H "Connection: close" "http://s3.seagate.com/testb/$QUERY") \
       | grep -q "NotImplemented"
   else
      echo $(curl -s -X $METHOD -H "Keep-Alive: 60" -H "Connection: close" -H "$ADDITIONAL_HEADER" "http://s3.seagate.com/testb/$QUERY") \
       | grep -q "NotImplemented"
   fi
}

###
# Main body of test script.
###
test_failed=false

echo "Verification of un-implemented S3 APIs....Starting"

# DELETE Bucket analytics.
check_501_response 'DELETE'  '?analytics&id=1234' 'DELETE Bucket analytics' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# DELETE Bucket cors.
check_501_response 'DELETE'  '?cors' 'DELETE Bucket cors' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# DELETE Bucket inventory.
check_501_response 'DELETE'  '?inventory&id=1234' 'DELETE Bucket inventory' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# DELETE Bucket lifecycle.
check_501_response 'DELETE'  '?lifecycle' 'DELETE Bucket lifecycle' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# DELETE Bucket metrics.
check_501_response 'DELETE'  '?metrics&id=1234' 'DELETE Bucket metrics' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# DELETE Bucket replication.
check_501_response 'DELETE'  '?replication' 'DELETE Bucket replication' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# DELETE Bucket tagging.
check_501_response 'DELETE'  '?tagging' 'DELETE Bucket tagging' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# DELETE Bucket website.
check_501_response 'DELETE'  '?website' 'DELETE Bucket website' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# GET Bucket accelerate.
check_501_response 'GET'  '?accelerate' 'GET Bucket accelerate' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# GET Bucket analytics.
check_501_response 'GET'  '?analytics&id=1234' 'GET Bucket analytics' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# GET Bucket cors.
check_501_response 'GET'  '?cors' 'GET Bucket cors' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# GET Bucket inventory.
check_501_response 'GET'  '?inventory&id=1234' 'GET Bucket inventory' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# GET Bucket lifecycle.
check_501_response 'GET'  '?lifecycle' 'GET Bucket lifecycle' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# GET Bucket logging.
check_501_response 'GET'  '?logging' 'GET Bucket logging' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# GET Bucket metrics.
check_501_response 'GET'  '?metrics&id=1234' 'GET Bucket metrics' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# GET Bucket notification.
check_501_response 'GET'  '?notification' 'GET Bucket notification' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# GET Bucket requestPayment.
check_501_response 'GET'  '?requestPayment' 'GET Bucket requestPayment' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# GET Bucket replication.
check_501_response 'GET'  '?replication' 'GET Bucket replication' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# GET Bucket tagging.
check_501_response 'GET'  '?tagging' 'GET Bucket tagging' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# GET Bucket Object versions.
check_501_response 'GET'  '?versions' 'GET Bucket Object versions' && echo  "Successful" || { echo  "Failed" ; test_failed=true; }
# GET Bucket versioning.
check_501_response 'GET'  '?versioning' 'GET Bucket versioning' && echo  "Successful" || { echo  "Failed" ; test_failed=true; }
# GET Bucket website.
check_501_response 'GET'  '?website' 'GET Bucket website' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# List Bucket Analytics Configurations.
check_501_response 'GET'  '?analytics' 'List Bucket Analytics Configurations' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# List Bucket Metrics Configurations.
check_501_response 'GET'  '?metrics' 'List Bucket Metrics Configurations' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# List Bucket Inventory Configurations.
check_501_response 'GET'  '?inventory' 'List Bucket Inventory Configurations' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# PUT Bucket accelerate.
check_501_response 'PUT'  '?accelerate' 'PUT Bucket accelerate' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# PUT Bucket analytics.
check_501_response 'PUT'  '?analytics&id=1234' 'PUT Bucket analytics' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# PUT Bucket cors.
check_501_response 'PUT'  '?cors' 'PUT Bucket cors' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# PUT Bucket inventory.
check_501_response 'PUT'  '?inventory&id=1234' 'PUT Bucket inventory' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# PUT Bucket lifecycle.
check_501_response 'PUT'  '?lifecycle' 'PUT Bucket lifecycle' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# PUT Bucket logging.
check_501_response 'PUT'  '?logging' 'PUT Bucket logging' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# PUT Bucket metrics.
check_501_response 'PUT'  '?metrics&id=1234' 'PUT Bucket metrics' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# PUT Bucket notification.
check_501_response 'PUT'  '?notification' 'PUT Bucket notification' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# PUT Bucket replication.
check_501_response 'PUT'  '?replication' 'PUT Bucket replication' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# PUT Bucket requestPayment.
check_501_response 'PUT'  '?requestPayment' 'PUT Bucket requestPayment' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# PUT Bucket tagging.
check_501_response 'PUT'  '?tagging' 'PUT Bucket tagging' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# PUT Bucket versioning.
check_501_response 'PUT'  '?versioning' 'PUT Bucket versioning' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# PUT Bucket website.
check_501_response 'PUT'  '?website' 'PUT Bucket website' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# PUT Object tagging.
check_501_response 'PUT'  'testfile1.txt?tagging' 'PUT Object tagging' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# DELETE Object tagging.
check_501_response 'DELETE'  'testfile1.txt?tagging' 'DELETE Object tagging' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# GET Object tagging.
check_501_response 'GET'  'testfile1.txt?tagging' 'GET Object tagging' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# GET Object torrent.
check_501_response 'GET'  'testfile1.txt?torrent' 'PUT Object torrent' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# OPTIONS object.
check_501_response 'OPTIONS'  'testfile.txt' 'OPTIONS object' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# POST Object.
check_501_response 'POST'  '' 'POST Object' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# POST Object restore.
check_501_response 'POST'  'testfile.txt?restore&versionId=1' 'POST Object restore' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# PUT Object - Copy.
check_501_response 'PUT'  'testfile1.txt' 'PUT Object - Copy' 'x-amz-copy-source: /testb/testfile.txt' && echo  "Successful" || { echo  "Failed"; test_failed=true; }
# Upload Part - Copy.
check_501_response 'PUT'  'testfile.txt?partNumber=1&uploadId=1' 'Upload Part - Copy' 'x-amz-copy-source: /testb/testfile.txt' && echo  "Successful" || { echo  "Failed"; test_failed=true; }

if [[ $test_failed  = true ]] ;then
     echo "Verification of un-implemented S3 APIs....Failed"
     exit 1
else
     echo "Verification of un-implemented S3 APIs....OK"
     exit 0
fi
