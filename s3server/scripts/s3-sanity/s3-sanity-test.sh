#!/bin/bash

set -e

#########################
# S3 sanity test script #
#########################

USAGE="USAGE: bash $(basename "$0") [--help]
Run S3 sanity test.
where:
    --help      display this help and exit

Operations performed:
  * Create Bucket
  * List Buckets
  * Put Object
  * Get Object
  * Delete Object
  * Delete Bucket"

case "$1" in
    --help )
        echo "$USAGE"
        exit 0
        ;;
esac


# Security credentialis
read -p "Enter Access Key: " access_key
read -p "Enter Secret Key: " secret_key

read -p "Enter test bucket name: " bucket_name
read -p "Enter test object name: " object_name
read -p "Enter test object size in MB: " object_size

echo -e "\n\n***** S3: SANITY TEST *****"

TEST_CMD="s3cmd --access_key=$access_key --secret_key=$secret_key"

echo -e "\nList Buckets: "
$TEST_CMD ls || { echo "Failed" && exit 1; }

echo -e "\nCreate bucket - '$bucket_name': "
$TEST_CMD mb "s3://$bucket_name" || { echo "Failed" && exit 1; }

# create a test file
test_file_input=/tmp/"$object_name".input
test_output_file=/tmp/"$object_name".out

dd if=/dev/urandom of=$test_file_input bs=1MB count=$object_size
content_md5_before=$(md5sum $test_file_input | cut -d ' ' -f 1)

echo -e "\nUpload object '$test_file_input' to '$bucket_name': "
$TEST_CMD put $test_file_input "s3://$bucket_name/$object_name" || { echo "Failed" && exit 1; }

echo -e "\nList uploaded object in '$bucket_name': "
$TEST_CMD ls "s3://$bucket_name/$object_name" || { echo "Failed" && exit 1; }

echo -e "\nDownload object '$object_name' from '$bucket_name': "
$TEST_CMD get "s3://$bucket_name/$object_name" $test_output_file || { echo "Failed" && exit 1; }
content_md5_after=$(md5sum $test_output_file | cut -d ' ' -f 1)

echo -en "\nData integrity check: "
[[ $content_md5_before == $content_md5_after ]] && echo 'Passed.' || echo 'Failed.'

echo -e "\nDelete object '$object_name' from '$bucket_name': "
$TEST_CMD del "s3://$bucket_name/$object_name" || { echo "Failed" && exit 1; }

echo -e "\nDelete bucket - '$bucket_name': "
$TEST_CMD rb "s3://$bucket_name" || { echo "Failed" && exit 1; }
set +e

# delete test files file
rm -f $test_file_input $test_output_file

echo -e "\n\n***** S3: SANITY TEST SUCCESSFULLY COMPLETED *****\n"
