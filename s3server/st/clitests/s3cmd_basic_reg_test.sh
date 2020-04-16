#!/bin/bash

set -e

##################################################
#     S3 Basic s3cmd regression test script      #
##################################################

# 4k - 1, 4k, 4k + 1, 1mb - 1, 1mb, 1mb + 1
byte_sizes=(4095 4096 4097 1048575 1048576 1048577)
# 1mb - 1k, 1mb, 1mb + 1k
kb_sizes=(1023 1024 1025)
mb_sizes=(1 2 4 8 9 14 16 20 32 33)
mb_sizes_multipart=(64 127 128 129 500 700 1023)
gb_sizes=(1 2 3 4)

USAGE="USAGE: bash $(basename "$0") --config /path/to/config [--help]
Run S3 simple regression tests for varying object sizes.
where:
    --config     path to s3cmd configuration file
    --cont_zero  use obj filled with zeros; if not provided random content is used
    --help       display this help and exit

Operations performed:
  * Create Buckets
  * List Buckets
  * Put Object of various sizes
  * Get Object the uploaded objects
  * Delete Object
  * Delete Bucket"

config_path=""
cont_zero=0

while [ "$1" != "" ]; do
    case "$1" in
        --cont_zero ) cont_zero=1;
                      echo "Use objects filled with zeros";
                      ;;
        --config ) shift;
                   if [ ! -r "$1" ]
                   then
                       echo "File $1 cannot be read";
                       exit 1
                   fi
                   config_path="$1"
                   ;;
        --help | -h )
            echo "$USAGE"
            exit 1
            ;;
        * )
            echo "Invalid argument passed";
            echo "$USAGE"
            exit 1
            ;;
    esac
    shift
done

if [ ! -r "$config_path" ]
then
    echo "Config should be provided";
    echo "$USAGE"
    exit 1
fi

data_device=/dev/urandom
if [ $cont_zero -eq 1 ]
then
    data_device=/dev/zero
fi
echo "Generate data from $data_device"


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
  test_file_output=/tmp/"$object_name".out

  # create a test file
  echo -e "\n\tCreating test file of size [$object_size]$obj_size_unit"
  dd if=$data_device of=$test_file_input bs=1${obj_size_unit} count=$object_size 2> /dev/null
  content_md5_before=$(md5sum $test_file_input | cut -d ' ' -f 1)

  echo -e "\n\t$TEST_CMD put $test_file_input "s3://$bucket_name/$object_name $multipart_option": "
  $TEST_CMD_WITH_CREDS put $test_file_input "s3://$bucket_name/$object_name" $multipart_option \
      > /dev/null && echo -e '\t=> Passed.' || { echo -e "\t=> Failed" && exit 1; }

  echo -e "\n\t$TEST_CMD ls "s3://$bucket_name/$object_name": "
  $TEST_CMD_WITH_CREDS ls "s3://$bucket_name/$object_name"  \
      > /dev/null && echo -e '\t=> Passed.' || { echo -e "\t=> Failed" && exit 1; }

  echo -e "\n\t$TEST_CMD get "s3://$bucket_name/$object_name" $test_file_output: "
  $TEST_CMD_WITH_CREDS get "s3://$bucket_name/$object_name" $test_file_output \
      > /dev/null && echo -e '\t=> Passed.' || { echo -e "\t=> Failed" && exit 1; }
  content_md5_after=$(md5sum $test_file_output | cut -d ' ' -f 1)

  echo -en "\tData integrity check: "
  [[ $content_md5_before == $content_md5_after ]] && echo 'Passed.' || echo 'Failed.'
  # Delete the test files.
  rm -f $test_file_input $test_file_output
  rm -rf /tmp/$object_name
  echo -e "\n\t$TEST_CMD del "s3://$bucket_name/$object_name": "
  $TEST_CMD_WITH_CREDS del "s3://$bucket_name/$object_name" \
      > /dev/null && echo -e '\t=> Passed.' || { echo -e "\t=> Failed" && exit 1; }

  echo -e "\n]]"
}

bucket_name=$(head /dev/urandom | tr -dc a-z | head -c 8)

echo -e "\n\n***** S3: Test for varying object sizes using simple and multipart upload *****"

TEST_CMD="s3cmd"
TEST_CMD_WITH_CREDS="s3cmd -c $config_path"

echo -e "\nList Buckets: "
$TEST_CMD_WITH_CREDS ls || { echo "=> Failed" && exit 1; }

echo -e "\nCreate bucket - '$bucket_name': "
$TEST_CMD_WITH_CREDS mb "s3://$bucket_name" || { echo "=> Failed" && exit 1; }

# Small objects
for obj_size in "${byte_sizes[@]}"
do
    perform_object_test $bucket_name $obj_size '' 0

    # delete test files in case of error
    rm -f $test_file_input $test_file_output
    rm -rf /tmp/$object_name
done

# Objects sizes KB to MBs
for obj_size in "${kb_sizes[@]}"
do
    perform_object_test $bucket_name $obj_size 'KB' 0

    # delete test files in case of error
    rm -f $test_file_input $test_file_output
    rm -rf /tmp/$object_name
done

# # Objects sizes with few MBs
# for obj_size in "${mb_sizes[@]}"
# do
#     perform_object_test $bucket_name $obj_size 'MB' 0

#    # delete test files in case of error
#    rm -f $test_file_input $test_file_output
#    rm -rf /tmp/$object_name
# done

# Multipart object sizes
for obj_size in "${mb_sizes_multipart[@]}"
do
    perform_object_test $bucket_name $obj_size 'MB' 64

    # delete test files in case of error
    rm -f $test_file_input $test_file_output
    rm -rf /tmp/$object_name
done

# # Object sizes in GBs
# for obj_size in "${gb_sizes[@]}"
# do
#     perform_object_test $bucket_name $obj_size 'GB' 128

#    # delete test files in case of error
#    rm -f $test_file_input $test_file_output
#    rm -rf /tmp/$object_name
# done

echo -e "\nDelete bucket - '$bucket_name': "
$TEST_CMD_WITH_CREDS rb "s3://$bucket_name" || { echo "=> Failed" && exit 1; }

echo -e "\nList Buckets: "
$TEST_CMD_WITH_CREDS ls || { echo "=> Failed" && exit 1; }

set +e

echo -e "\n\n***** S3: TEST SUCCESSFULLY COMPLETED *****\n"
