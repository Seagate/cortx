#!/bin/bash

set -e

##################################################
#     S3 Object size regression test script      #
##################################################

# 4k - 1, 4k, 4k + 1, 1mb - 1, 1mb, 1mb + 1
byte_sizes=(3999 4000 4001 1048575 1048576 1048577)
# 1mb - 1k, 1mb, 1mb + 1k
kb_sizes=(1023 1024 1025)
mb_sizes=(1 2 4 8 9 14 16 20 32 33)
mb_sizes_multipart=(64 127 128 129 500 700 1023)
gb_sizes=(1 2 3 4)

USAGE="USAGE: bash $(basename "$0") [--help]
Run S3 regression tests for varying object sizes.
where:
    --help      display this help and exit

Operations performed:
  * Create Bucket
  * Put Object of various sizes
  * Get Object the uploaded objects
  * Delete Object
  * Delete Bucket"

case "$1" in
    --help )
        echo "$USAGE"
        exit 0
        ;;
esac

# Assumes $TEST_CMD is setup
function perform_object_test() {
  bucket_name=$1
  object_size=$2
  obj_size_unit=$3  #'' = bytes, else "KB", "MB", "GB"
  multipart_size_in_mb=$4 # 0: no multipart, any valid number use multipart

  echo -e "\n Testing object of size $object_size $obj_size_unit [["

  multipart_option=""
  if [[ $multipart_size_in_mb -ne 0 ]]
  then
    multipart_option=" --multipart-chunk-size-mb=$multipart_size_in_mb "
  fi

  object_name=$(mktemp /tmp/s3test.XXXXXXXXX | xargs basename)
  test_file_input=/tmp/"$object_name".in
  test_output_file=/tmp/"$object_name".out

  # create a test file
  echo -e "\n\tCreating test file of size [$object_size]$obj_size_unit"
  dd if=/dev/urandom of=$test_file_input bs=1${obj_size_unit} count=$object_size 2> /dev/null
  content_md5_before=$(md5sum $test_file_input | cut -d ' ' -f 1)

  echo -e "\n\t$TEST_CMD put $test_file_input "s3://$bucket_name/$object_name $multipart_option": "
  $TEST_CMD_WITH_CREDS put $test_file_input "s3://$bucket_name/$object_name" $multipart_option \
      > /dev/null && echo -e '\t=> Passed.' || { echo -e "\t=> Failed" && exit 1; }

  echo -e "\n\t$TEST_CMD ls "s3://$bucket_name/$object_name": "
  $TEST_CMD_WITH_CREDS ls "s3://$bucket_name/$object_name"  \
      > /dev/null && echo -e '\t=> Passed.' || { echo -e "\t=> Failed" && exit 1; }

  echo -e "\n\t$TEST_CMD get "s3://$bucket_name/$object_name" $test_output_file: "
  $TEST_CMD_WITH_CREDS get "s3://$bucket_name/$object_name" $test_output_file \
      > /dev/null && echo -e '\t=> Passed.' || { echo -e "\t=> Failed" && exit 1; }
  content_md5_after=$(md5sum $test_output_file | cut -d ' ' -f 1)

  echo -en "\tData integrity check: "
  [[ $content_md5_before == $content_md5_after ]] && echo 'Passed.' || echo 'Failed.'
  # Delete the test files.
  rm -f $test_file_input $test_output_file
  rm -rf /tmp/$object_name
  echo -e "\n\t$TEST_CMD del "s3://$bucket_name/$object_name": "
  $TEST_CMD_WITH_CREDS del "s3://$bucket_name/$object_name" \
      > /dev/null && echo -e '\t=> Passed.' || { echo -e "\t=> Failed" && exit 1; }

  echo -e "\n]]"
}

# Security credentialis
read -p "Enter Access Key: " access_key
read -p "Enter Secret Key: " secret_key

read -p "Enter test bucket name: " bucket_name

echo -e "\n\n***** S3: Test for varying object sizes using simple and multipart upload *****"

TEST_CMD="s3cmd"
TEST_CMD_WITH_CREDS="s3cmd --access_key=$access_key --secret_key=$secret_key"

echo -e "\nList Buckets: "
$TEST_CMD_WITH_CREDS ls || { echo "=> Failed" && exit 1; }

echo -e "\nCreate bucket - '$bucket_name': "
$TEST_CMD_WITH_CREDS mb "s3://$bucket_name" || { echo "=> Failed" && exit 1; }

# Small objects
for obj_size in "${byte_sizes[@]}"
do
    perform_object_test $bucket_name $obj_size '' 0
done

# Objects sizes KB to MBs
for obj_size in "${kb_sizes[@]}"
do
    perform_object_test $bucket_name $obj_size 'KB' 0
done

# Objects sizes with few MBs
for obj_size in "${mb_sizes[@]}"
do
    perform_object_test $bucket_name $obj_size 'MB' 0
done

# Multipart object sizes
for obj_size in "${mb_sizes_multipart[@]}"
do
    perform_object_test $bucket_name $obj_size 'MB' 64
done

# Object sizes in GBs
for obj_size in "${gb_sizes[@]}"
do
    perform_object_test $bucket_name $obj_size 'GB' 128
done

# Run some Object tests.


echo -e "\nDelete bucket - '$bucket_name': "
$TEST_CMD_WITH_CREDS rb "s3://$bucket_name" || { echo "=> Failed" && exit 1; }
set +e

# delete test files file
rm -f $test_file_input $test_output_file

echo -e "\n\n***** S3: TEST SUCCESSFULLY COMPLETED *****\n"
