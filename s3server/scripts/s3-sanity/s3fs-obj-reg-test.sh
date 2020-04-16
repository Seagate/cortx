#!/bin/bash

set -e

#############################################################
#     S3 Object size regression test script using s3fs      #
#############################################################

# 4k - 1, 4k, 4k + 1, 1mb - 1, 1mb, 1mb + 1
byte_sizes=(3999 4000 4001 1048575 1048576 1048577)
# 1mb - 1k, 1mb, 1mb + 1k
kb_sizes=(1023 1024 1025)
mb_sizes=(1 2 4 8 9 14 16 20 32 33)
mb_sizes_multipart=(64 127 128 129 500 700 1023)
gb_sizes=(1 2 3 4)

USAGE="USAGE: bash $(basename "$0") [--help]
Run S3 regression tests for varying object sizes using s3fs.
where:
    --help      display this help and exit

Operations performed:
  * Create file to s3fs mount point
  * Copy files of various sizes to s3fs mount point
  * Copy files of various sizes from s3fs mount point
  * Delete file from s3fs mount point"

case "$1" in
    --help )
        echo "$USAGE"
        exit 0
        ;;
esac

function create_mount_point() {
 bucketname=$1
 s3fs_mountpoint=$2
 multipart_size=$3
 if [[ $multipart_size -eq 0 ]]
 then
   multipart_size=15 #default multipart size
 fi

 if ! [ -d "${s3fs_mountpoint}" ]
  then
  mkdir -p ${s3fs_mountpoint}
 fi

 echo -e "\nList Buckets: "
 $TEST_CMD_WITH_CREDS ls || { echo "=> Failed" && exit 1; }

 echo -e "\nCreate bucket - '$bucketname': "
 $TEST_CMD_WITH_CREDS mb "s3://$bucketname" || { echo "=> Failed" && exit 1; }

 # mount s3fs for given bucket with given access key and secret key
 echo "s3fs mounting at: ${s3fs_mountpoint}"
 s3fs $bucketname ${s3fs_mountpoint} -o passwd_file=${passwd_file} -o \
 url=http://s3.seagate.com/ -o use_path_request_style -o multipart_size=${multipart_size} -o dbglevel=info

 df -h | grep s3fs | awk '{print $6}' | grep "${s3fs_mountpoint}"
 if [ $? -nq 0 ]
 then
  echo "s3fs mount point not found"
  exit 0
 fi
}

# Assumes $TEST_CMD is setup
function perform_object_test() {
  s3fs_mountpoint=$1
  object_size=$2
  obj_size_unit=$3  #'' = bytes, else "KB", "MB", "GB"

  object_name=$(mktemp /tmp/s3test.XXXXXXXXX | xargs basename)
  test_file_input=/tmp/"$object_name".in
  test_output_file=/tmp/"$object_name".out

  # create a test file
  echo -e "\n\tCreating test file of size [$object_size]$obj_size_unit"
  dd if=/dev/urandom of=$test_file_input bs=1${obj_size_unit} count=$object_size 2> /dev/null
  content_md5_before=$(md5sum $test_file_input | cut -d ' ' -f 1)

  echo -e "\n\tCopy file to s3fs mount point operation: cp $test_file_input ${s3fs_mountpoint}/$object_name: "
  cp $test_file_input ${s3fs_mountpoint}/${object_name} > /dev/null
  [ $? -eq 0 ] && echo -e '\t=> Passed.' || { echo -e "\t=> Failed" && exit 1; }

  echo -e "\n\tList file operation: ls -l ${s3fs_mountpoint}/$object_name: "
  ls -l ${s3fs_mountpoint}/${object_name} > /dev/null
  [ $? -eq 0 ] && echo -e '\t=> Passed.' || { echo -e "\t=> Failed" && exit 1; }

  echo -e "\n\tCopy file from s3fs mount point operation: cp ${s3fs_mountpoint}/$object_name $test_output_file: "
  cp ${s3fs_mountpoint}/${object_name} $test_output_file > /dev/null
  [ $? -eq 0 ] && echo -e '\t=> Passed.' || { echo -e "\t=> Failed" && exit 1; }
  content_md5_after=$(md5sum $test_output_file | cut -d ' ' -f 1)

  echo -en "\tData integrity check: "
  [[ $content_md5_before == $content_md5_after ]] && echo 'Passed.' || echo 'Failed.'
  # Delete the test files.
  rm -f $test_file_input $test_output_file
  rm -f /tmp/$object_name
  echo -e "\n\tDelete object opearation: rm -rf ${s3fs_mountpoint}/$object_name: "
  rm -f ${s3fs_mountpoint}/${object_name} > /dev/null
  [ $? -eq 0 ] && echo -e '\t=> Passed.' || { echo -e "\t=> Failed" && exit 1; }

  echo -e "\n]]"
}

# Security credentialis
read -p "Enter Access Key: " access_key
read -p "Enter Secret Key: " secret_key

read -p "Enter test bucket name: " bucket_name

echo -e "\n\n***** S3: Test for varying object sizes using simple and multipart upload *****"

TEST_CMD="s3cmd"
TEST_CMD_WITH_CREDS="s3cmd --access_key=$access_key --secret_key=$secret_key"

# check s3fs installed or not
which s3fs > /dev/null
if [ $? -nq 0 ]
then
 echo "s3fs not installed on system"
 exit 0
fi

# store access key and secret key
passwd_file="$HOME/.passwd-s3fs-test"
echo $access_key:$secret_key > ${passwd_file}
chmod 600 ${passwd_file}

S3FS_BUCKET_NAME_NO_MULTI_PART="${bucket_name}_no_mp"
S3FS_MOUNTPOINT_NO_MULTI_PART="/seagate/s3fstest_${bucket_name}_no_mp"
multi_part_size=0
create_mount_point ${S3FS_BUCKET_NAME_NO_MULTI_PART} ${S3FS_MOUNTPOINT_NO_MULTI_PART} ${multi_part_size}


S3FS_BUCKET_NAME_64MB_MULTI_PART="${bucket_name}_64mb_mp"
S3FS_MOUNTPOINT_64MB_MULTI_PART="/seagate/s3fstest_${bucket_name}_64mb_mp"
multi_part_size=64
create_mount_point ${S3FS_BUCKET_NAME_64MB_MULTI_PART} ${S3FS_MOUNTPOINT_64MB_MULTI_PART} ${multi_part_size}

S3FS_BUCKET_NAME_128MB_MULTI_PART="${bucket_name}_128mb_mp"
S3FS_MOUNTPOINT_128MB_MULTI_PART="/seagate/s3fstest_${bucket_name}_128mb_mp"
multi_part_size=128
create_mount_point ${S3FS_BUCKET_NAME_128MB_MULTI_PART} ${S3FS_MOUNTPOINT_128MB_MULTI_PART} ${multi_part_size}

# Small objects
for obj_size in "${byte_sizes[@]}"
do
    perform_object_test ${S3FS_MOUNTPOINT_NO_MULTI_PART} $obj_size ''
done

# Objects sizes KB to MBs
for obj_size in "${kb_sizes[@]}"
do
    perform_object_test ${S3FS_MOUNTPOINT_NO_MULTI_PART} $obj_size 'KB'
done

# Objects sizes with few MBs
for obj_size in "${mb_sizes[@]}"
do
    perform_object_test ${S3FS_MOUNTPOINT_NO_MULTI_PART} $obj_size 'MB'
done

# Multipart object sizes
for obj_size in "${mb_sizes_multipart[@]}"
do
    perform_object_test ${S3FS_MOUNTPOINT_64MB_MULTI_PART} $obj_size 'MB'
done

# Object sizes in GBs
for obj_size in "${gb_sizes[@]}"
do
    perform_object_test ${S3FS_MOUNTPOINT_128MB_MULTI_PART} $obj_size 'GB'
done

# Run some Object tests.


echo -e "\nDelete bucket - '$S3FS_BUCKET_NAME_NO_MULTI_PART': "
$TEST_CMD_WITH_CREDS rb "s3://$S3FS_BUCKET_NAME_NO_MULTI_PART" || { echo "=> Failed" && exit 1; }

echo -e "\nDelete bucket - '$S3FS_BUCKET_NAME_64MB_MULTI_PART': "
$TEST_CMD_WITH_CREDS rb "s3://$S3FS_BUCKET_NAME_64MB_MULTI_PART" || { echo "=> Failed" && exit 1; }

echo -e "\nDelete bucket - '$S3FS_BUCKET_NAME_128MB_MULTI_PART': "
$TEST_CMD_WITH_CREDS rb "s3://$S3FS_BUCKET_NAME_128MB_MULTI_PART" || { echo "=> Failed" && exit 1; }
set +e

umount $S3FS_MOUNTPOINT_NO_MULTI_PART
umount $S3FS_MOUNTPOINT_64MB_MULTI_PART
umount $S3FS_MOUNTPOINT_128MB_MULTI_PART
# delete test files file
rm -f $test_file_input $test_output_file

echo -e "\n\n***** S3: TEST SUCCESSFULLY COMPLETED *****\n"
