#!/bin/bash

set -e
set -x

#########################
# S3 sanity test script #
#########################

if command -v s3cmd >/dev/null 2>&1;then
    printf "\nCheck S3CMD...OK"
else
    printf "\nPlease install patched version built from <s3server src>/rpms/s3cmd/"
    exit 0
fi

# Check s3iamcli is installed
if command -v s3iamcli >/dev/null 2>&1; then
    printf "\nCheck s3iamcli...OK"
else
    printf "\ns3iamcli not installed"
    printf "\nPlease install s3iamcli using rpm built from <s3server repo>/rpms/s3iamcli/"
    exit 0
fi

USAGE="USAGE: bash $(basename "$0") [--help]
Run S3 sanity test.
where:
    --help      display this help and exit
    --clean     clean s3 resources if present

Operations performed:
  * Create Account
  * Create User
  * Create Bucket
  * Put Object
  * Delete Object
  * Delete Bucket
  * Delete User
  * Delete Account"

cleanup() {

    if [ "$externalcleanup" = true ]
    then
        output=$(s3iamcli resetaccountaccesskey -n SanityAccountToDeleteAfterUse --ldapuser sgiamadmin --ldappasswd ldapadmin)
        echo $output
        access_key=$(echo -e "$output" | tr ',' '\n' | grep "AccessKeyId" | awk '{print $3}')
        secret_key=$(echo -e "$output" | tr ',' '\n' | grep "SecretKey" | awk '{print $3}')
        TEST_CMD="s3cmd --access_key=$access_key --secret_key=$secret_key"
    fi

    $TEST_CMD del "s3://sanitybucket/SanityObjectToDeleteAfterUse" || echo "Failed"
    $TEST_CMD rb "s3://sanitybucket" || echo "Failed"
    s3iamcli deleteuser -n SanityUserToDeleteAfterUse --access_key $access_key --secret_key $secret_key || echo "Failed"
    s3iamcli deleteaccount -n SanityAccountToDeleteAfterUse --access_key $access_key --secret_key $secret_key || echo "Failed"
    rm -f $test_file_input $test_output_file || echo "failed"
    exit 0
}

case "$1" in
    --help )
        echo "$USAGE"
        exit 0
        ;;
    --clean )
        echo "start clean up"
        externalcleanup=true
        cleanup
        ;;

esac

trap "cleanup" ERR

echo -e "\n\n*** S3 Sanity ***"
echo -e "\n\n**** Create Account *******"

output=$(s3iamcli createaccount -n SanityAccountToDeleteAfterUse  -e SanityAccountToDeleteAfterUse@sanitybucket.com --ldapuser sgiamadmin --ldappasswd ldapadmin)

echo $output
access_key=$(echo -e "$output" | tr ',' '\n' | grep "AccessKeyId" | awk '{print $3}')
secret_key=$(echo -e "$output" | tr ',' '\n' | grep "SecretKey" | awk '{print $3}')

s3iamcli CreateUser -n SanityUserToDeleteAfterUse --access_key $access_key --secret_key $secret_key

TEST_CMD="s3cmd --access_key=$access_key --secret_key=$secret_key"

echo -e "\nCreate bucket - 'sanitybucket': "
$TEST_CMD mb "s3://sanitybucket"

# create a test file
test_file_input=/tmp/SanityObjectToDeleteAfterUse.input
test_output_file=/tmp/SanityObjectToDeleteAfterUse.out

dd if=/dev/urandom of=$test_file_input bs=5MB count=1
content_md5_before=$(md5sum $test_file_input | cut -d ' ' -f 1)

echo -e "\nUpload '$test_file_input' to 'sanitybucket': "
$TEST_CMD put $test_file_input "s3://sanitybucket/SanityObjectToDeleteAfterUse"

echo -e "\nList uploaded SanityObjectToDeleteAfterUse in 'sanitybucket': "
$TEST_CMD ls "s3://sanitybucket/SanityObjectToDeleteAfterUse"

echo -e "\nDownload 'SanityObjectToDeleteAfterUse' from 'sanitybucket': "
$TEST_CMD get "s3://sanitybucket/SanityObjectToDeleteAfterUse" $test_output_file
content_md5_after=$(md5sum $test_output_file | cut -d ' ' -f 1)

echo -en "\nData integrity check: "
[[ $content_md5_before == $content_md5_after ]] && echo 'Passed.'

echo -e "\nDelete 'SanityObjectToDeleteAfterUse' from 'sanitybucket': "
$TEST_CMD del "s3://sanitybucket/SanityObjectToDeleteAfterUse"

echo -e "\nDelete bucket - 'sanitybucket': "
$TEST_CMD rb "s3://sanitybucket"

echo -e "\nDelete User - 'SanityUserToDeleteAfterUse': "
s3iamcli deleteuser -n SanityUserToDeleteAfterUse --access_key $access_key --secret_key $secret_key

echo -e "\nDelete Account - 'SanityAccountToDeleteAfterUse': "
s3iamcli deleteaccount -n SanityAccountToDeleteAfterUse --access_key $access_key --secret_key $secret_key

set +e

# delete test files file
rm -f $test_file_input $test_output_file

echo -e "\n\n***** S3: SANITY TEST SUCCESSFULLY COMPLETED *****\n"
