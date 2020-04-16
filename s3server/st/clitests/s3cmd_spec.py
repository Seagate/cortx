#!/usr/bin/python

from framework import Config
from framework import S3PyCliTest
from s3cmd import S3cmdTest
from jclient import JClientTest
from s3client_config import S3ClientConfig
from s3kvstool import S3kvTest
from auth import AuthTest
import s3kvs

# Helps debugging
# Config.log_enabled = True
# Config.dummy_run = True
# Config.client_execution_timeout = 300 * 1000
# Config.request_timeout = 300 * 1000
# Config.socket_timeout = 300 * 1000

# Set time_readable_format to False if you want to display the time in milli seconds.
# Config.time_readable_format = False

# TODO
# DNS-compliant bucket names should not contains underscore or other special characters.
# The allowed characters are [a-zA-Z0-9.-]*
#
# Add validations to S3 server and write system tests for the same.

#  ***MAIN ENTRY POINT

# Run before all to setup the test environment.
print("Configuring LDAP")
S3PyCliTest('Before_all').before_all()

# Set pathstyle =false to run jclient for partial multipart upload
S3ClientConfig.pathstyle = False
S3ClientConfig.access_key_id = 'AKIAJPINPFRBTPAYOGNA'
S3ClientConfig.secret_key = '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'

# Path style tests.
Config.config_file = "pathstyle.s3cfg"

# ************ Test post removal of data (removal/creation of root index) ************
s3kvs.clean_all_data()
s3kvs.create_s3root_index()
S3cmdTest('Create bucket post root index deletion').create_bucket("seagatebucket").execute_test().command_is_successful().command_response_should_not_have('WARNING')
S3cmdTest('s3cmd can delete bucket').delete_bucket("seagatebucket").execute_test().command_is_successful()

s3kvs.clean_all_data()
s3kvs.create_s3root_index()
S3cmdTest('s3cmd should not have bucket').list_buckets().execute_test().command_is_successful().command_response_should_not_have('WARNING')

s3kvs.clean_all_data()
s3kvs.create_s3root_index()
S3cmdTest('s3cmd cannot fetch info for nonexistent bucket').info_bucket("seagate-bucket").execute_test(negative_case=True).command_should_fail().command_error_should_have("NoSuchBucket").command_response_should_not_have('WARNING')

# Create Account
# Extract the response elements from response which has the following format
# <Key 1> = <Value 1>, <Key 2> = <Value 2> ... <Key n> = <Value n>
def get_response_elements(response):
    response_elements = {}
    key_pairs = response.split(',')

    for key_pair in key_pairs:
        tokens = key_pair.split('=')
        response_elements[tokens[0].strip()] = tokens[1].strip()

    return response_elements

account_args = {}
account_args['AccountName'] = 's3secondaccount'
account_args['Email'] = 's3secondaccount@seagate.com'
account_args['ldapuser'] = 'sgiamadmin'
account_args['ldappasswd'] = 'ldapadmin'

test_msg = "Create account s3secondaccount"
account_response_pattern = "AccountId = [\w-]*, CanonicalId = [\w-]*, RootUserName = [\w+=,.@-]*, AccessKeyId = [\w-]*, SecretKey = [\w/+]*$"
auth_test = AuthTest(test_msg)
result = auth_test.create_account(**account_args).execute_test()
result.command_should_match_pattern(account_response_pattern)
account_response_elements = get_response_elements(result.status.stdout)

# ************ Create bucket in s3secondaccount account************
S3cmdTest('s3cmd can create bucket in s3secondaccount account')\
    .with_credentials(account_response_elements['AccessKeyId'], account_response_elements['SecretKey'])\
    .create_bucket("s3secondaccount").execute_test().command_is_successful()

# ************ List buckets ************
S3cmdTest('s3cmd can list buckets from s3secondaccount account')\
    .with_credentials(account_response_elements['AccessKeyId'], account_response_elements['SecretKey'])\
    .list_buckets().execute_test().command_is_successful().command_response_should_have('s3://s3secondaccount')

# ************ Create bucket ************
S3cmdTest('s3cmd can create bucket').create_bucket("seagatebucket").execute_test().command_is_successful()

# ************ List buckets of specific account************
S3cmdTest('s3cmd can list buckets').list_buckets().execute_test()\
    .command_is_successful().command_response_should_have('s3://seagatebucket')\
    .command_is_successful().command_response_should_not_have('s3://s3secondaccount')

S3cmdTest('s3cmd can list buckets')\
    .with_credentials(account_response_elements['AccessKeyId'], account_response_elements['SecretKey'])\
    .list_buckets().execute_test().command_is_successful().command_response_should_not_have('s3://seagatebucket')\
    .command_is_successful().command_response_should_have('s3://s3secondaccount')

# ************ create bucket that already exsting and owned by another account************
S3cmdTest('s3cmd can not create bucket with name exsting in other account')\
    .with_credentials(account_response_elements['AccessKeyId'], account_response_elements['SecretKey'])\
    .create_bucket("seagatebucket").execute_test(negative_case=True).command_should_fail()\
    .command_error_should_have("BucketAlreadyExists")

S3cmdTest('s3cmd can not create bucket with name exsting in other account').create_bucket("s3secondaccount")\
    .execute_test(negative_case=True).command_should_fail().command_error_should_have("BucketAlreadyExists")

#TODO  Enable this tests when authorization is enabled for this api calls.

# ************ info_bucket owned by another account************
#S3cmdTest('s3cmd can not access bucket owned by another account')\
#    .with_credentials(account_response_elements['AccessKeyId'], account_response_elements['SecretKey'])\
#    .info_bucket("seagatebucket").execute_test(negative_case=True).command_should_fail()\
#    .command_error_should_have("AccessDenied")

#S3cmdTest('s3cmd can not access bucket owned by another account').info_bucket("s3secondaccount")\
#    .execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")

# ************ delete bucket owned by another account************
#S3cmdTest('s3cmd can not delete bucket owned by another account')\
#    .with_credentials(account_response_elements['AccessKeyId'], account_response_elements['SecretKey'])\
#    .delete_bucket("seagatebucket").execute_test(negative_case=True).command_should_fail()\
#    .command_error_should_have("AccessDenied")

#S3cmdTest('s3cmd can not deelte bucket owned by another account').delete_bucket("s3secondaccount")\
#    .execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")

# ************ upload object to a bucket owned by another account************
#S3cmdTest('s3cmd can not upload 3k file to bucket owned by another account')\
#    .with_credentials(account_response_elements['AccessKeyId'], account_response_elements['SecretKey'])\
#    .upload_test("seagatebucket", "3kfile", 3000).execute_test(negative_case=True).command_should_fail()\
#    .command_error_should_have("AccessDenied")

#S3cmdTest('s3cmd can not upload 3k file to bucket owned by another account')\
#    .upload_test("s3secondaccount", "3kfile", 3000).execute_test(negative_case=True)\
#    .command_should_fail().command_error_should_have("AccessDenied")

# Overwriting values of access key and secret key given by
S3ClientConfig.access_key_id = account_response_elements['AccessKeyId']
S3ClientConfig.secret_key = account_response_elements['SecretKey']

# ************ try to delete account which is having bucket ************
test_msg = "Cannot delete account s3secondaccount with buckets"
account_args = {'AccountName': 's3secondaccount'}
AuthTest(test_msg).delete_account(**account_args).execute_test(negative_case=True)\
    .command_response_should_have("Account cannot be deleted")

# ************ delete bucket using owner account************
S3cmdTest('s3cmd can delete bucket')\
    .with_credentials(account_response_elements['AccessKeyId'], account_response_elements['SecretKey'])\
    .delete_bucket("s3secondaccount").execute_test().command_is_successful()

# ************ List buckets of specific account************
S3cmdTest('s3cmd can list buckets of s3secondaccount account')\
    .with_credentials(account_response_elements['AccessKeyId'], account_response_elements['SecretKey'])\
    .list_buckets().execute_test().command_is_successful()\
    .command_is_successful().command_response_should_have('')

# ************ try to delete empty account ************
test_msg = "Cannot delete account s3secondaccount with buckets"
account_args = {'AccountName': 's3secondaccount'}
AuthTest(test_msg).delete_account(**account_args).execute_test()\
    .command_response_should_have("Account deleted successfully")

# restore access key and secret key.
S3ClientConfig.access_key_id = 'AKIAJPINPFRBTPAYOGNA'
S3ClientConfig.secret_key = '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'

# ************ List buckets ************
S3cmdTest('s3cmd can list buckets').list_buckets().execute_test().command_is_successful().command_response_should_have('s3://seagatebucket')


# ************ 3k FILE TEST ************
S3cmdTest('s3cmd can upload 3k file').upload_test("seagatebucket", "3kfile", 3000).execute_test().command_is_successful()

S3cmdTest('s3cmd can download 3k file').download_test("seagatebucket", "3kfile").execute_test().command_is_successful().command_created_file("3kfile")

S3cmdTest('s3cmd cannot copy object 3k file').upload_copy_test("seagatebucket", "3kfile", "3kfile.copy").execute_test(negative_case=True).command_should_fail()

S3cmdTest('s3cmd cannot move object 3k file').upload_move_test("seagatebucket", "3kfile", "seagatebucket", "3kfile.moved").execute_test(negative_case=True).command_should_fail()

# ************ FILE Overwrite TEST ************
S3cmdTest('s3cmd can upload 3k file with size 3072').upload_test("seagatebucket", "3kfile", 3072).execute_test().command_is_successful()

S3cmdTest('s3cmd can upload 3k file with size 3000').upload_test("seagatebucket", "3kfile", 3000).execute_test().command_is_successful()

# ************ 3k FILE TEST ************
S3cmdTest('s3cmd can upload 3k file with special chars in filename.').upload_test("seagatebucket/2016-04:32:21/3kfile", "3kfile", 3000).execute_test().command_is_successful()

S3cmdTest('s3cmd can download 3k file with special chars in filename.').download_test("seagatebucket/2016-04:32:21", "3kfile").execute_test().command_is_successful().command_created_file("3kfile")


# ************ 8k FILE TEST ************
S3cmdTest('s3cmd can upload 8k file').upload_test("seagatebucket", "8kfile", 8192).execute_test().command_is_successful()

S3cmdTest('s3cmd can download 8k file').download_test("seagatebucket", "8kfile").execute_test().command_is_successful().command_created_file("8kfile")

# ************ OBJECT LISTING TEST ************
S3cmdTest('s3cmd can list objects').list_objects('seagatebucket').execute_test().command_is_successful().command_response_should_have('s3://seagatebucket/3kfile').command_response_should_have('s3://seagatebucket/8kfile')

S3cmdTest('s3cmd can list specific objects').list_specific_objects('seagatebucket', '3k').execute_test().command_is_successful().command_response_should_have('s3://seagatebucket/3kfile').command_response_should_not_have('s3://seagatebucket/8kfile')

S3cmdTest('s3cmd can create bucket').create_bucket("seagatebucket2").execute_test().command_is_successful()

S3cmdTest('s3cmd can upload 8k file').upload_test("seagatebucket2", "8kfile", 8192).execute_test().command_is_successful()

S3cmdTest('s3cmd can list all objects').list_all_objects().execute_test().command_is_successful().command_response_should_have('s3://seagatebucket/3kfile').command_response_should_have('s3://seagatebucket2/8kfile')

# ************ Disk Usage TEST ************
S3cmdTest('s3cmd can show disk usage').disk_usage_bucket("seagatebucket").execute_test().command_is_successful()

# ************ DELETE OBJECT TEST ************
S3cmdTest('s3cmd can delete 3k file').delete_test("seagatebucket", "3kfile").execute_test().command_is_successful()

S3cmdTest('s3cmd should not have object after its delete').list_objects('seagatebucket').execute_test().command_is_successful().command_response_should_not_have('3kfile')


S3cmdTest('s3cmd can delete 3k file').delete_test("seagatebucket/2016-04:32:21", "3kfile").execute_test().command_is_successful()

S3cmdTest('s3cmd should not have object after its delete').list_objects('seagatebucket/2016-04:32:21').execute_test().command_is_successful().command_response_should_not_have('3kfile')

S3cmdTest('s3cmd can delete 8k file').delete_test("seagatebucket", "8kfile").execute_test().command_is_successful()

S3cmdTest('s3cmd can delete 8k file').delete_test("seagatebucket2", "8kfile").execute_test().command_is_successful()

host_ip = Config.use_ipv6 and "[::1]" or "127.0.0.1"

# ************ Some Basic tests with hostname ************
S3cmdTest('s3cmd can create bucket').create_bucket("seagate-bucket", host=host_ip, no_check_hostname=True).execute_test().command_is_successful()
S3cmdTest('s3cmd can list buckets').list_buckets(host=host_ip, no_check_hostname=True).execute_test().command_is_successful().command_response_should_have('s3://seagate-bucket')
S3cmdTest('s3cmd can upload 3k file').upload_test("seagate-bucket", "3kfile", 3000, host=host_ip, no_check_hostname=True).execute_test().command_is_successful()
S3cmdTest('s3cmd can list objects').list_objects('seagate-bucket', host=host_ip, no_check_hostname=True).execute_test().command_is_successful().command_response_should_have('s3://seagate-bucket/3kfile')
S3cmdTest('s3cmd can download 3k file').download_test("seagate-bucket", "3kfile", host=host_ip, no_check_hostname=True).execute_test().command_is_successful().command_created_file("3kfile")
S3cmdTest('s3cmd can delete 3k file').delete_test("seagate-bucket", "3kfile", host=host_ip, no_check_hostname=True).execute_test().command_is_successful()
S3cmdTest('s3cmd can delete bucket').delete_bucket("seagate-bucket").execute_test().command_is_successful()

# ************ Some Basic tests with hostname:port, client connecting to s3server directly ************
if Config.no_ssl:
    S3cmdTest('s3cmd can create bucket').create_bucket("seagate-bucket", host=host_ip + ":8081").execute_test().command_is_successful()
    S3cmdTest('s3cmd can list buckets').list_buckets(host=host_ip + ":8081").execute_test().command_is_successful().command_response_should_have('s3://seagate-bucket')
    S3cmdTest('s3cmd can upload 3k file').upload_test("seagate-bucket", "3kfile", 3000, host=host_ip + ":8081").execute_test().command_is_successful()
    S3cmdTest('s3cmd can list objects').list_objects('seagate-bucket', host=host_ip + ":8081").execute_test().command_is_successful().command_response_should_have('s3://seagate-bucket/3kfile')
    S3cmdTest('s3cmd can download 3k file').download_test("seagate-bucket", "3kfile", host=host_ip + ":8081").execute_test().command_is_successful().command_created_file("3kfile")
    S3cmdTest('s3cmd can delete 3k file').delete_test("seagate-bucket", "3kfile", host=host_ip + ":8081").execute_test().command_is_successful()
    S3cmdTest('s3cmd can delete bucket').delete_bucket("seagate-bucket").execute_test().command_is_successful()

    # ************ Some Basic tests with idns style hostname:port, client connecting to s3server directly ************
    S3cmdTest('s3cmd can create bucket').create_bucket("seagate-bucket", host="s3.seagate.com:8081").execute_test().command_is_successful()
    S3cmdTest('s3cmd can list buckets').list_buckets(host="s3.seagate.com:8081").execute_test().command_is_successful().command_response_should_have('s3://seagate-bucket')
    S3cmdTest('s3cmd can upload 3k file').upload_test("seagate-bucket", "3kfile", 3000, host="s3.seagate.com:8081").execute_test().command_is_successful()
    S3cmdTest('s3cmd can list objects').list_objects('seagate-bucket', host="s3.seagate.com:8081").execute_test().command_is_successful().command_response_should_have('s3://seagate-bucket/3kfile')
    S3cmdTest('s3cmd can download 3k file').download_test("seagate-bucket", "3kfile", host="s3.seagate.com:8081").execute_test().command_is_successful().command_created_file("3kfile")
    S3cmdTest('s3cmd can delete 3k file').delete_test("seagate-bucket", "3kfile", host="s3.seagate.com:8081").execute_test().command_is_successful()
    S3cmdTest('s3cmd can delete bucket').delete_bucket("seagate-bucket").execute_test().command_is_successful()

# ************ Some Basic tests with dns style hostname:port, client connecting to nginx ************
S3cmdTest('s3cmd can create bucket').create_bucket("seagate-bucket", host="s3.seagate.com").execute_test().command_is_successful()
S3cmdTest('s3cmd can list buckets').list_buckets(host="s3.seagate.com").execute_test().command_is_successful().command_response_should_have('s3://seagate-bucket')
S3cmdTest('s3cmd can upload 3k file').upload_test("seagate-bucket", "3kfile", 3000, host="s3.seagate.com").execute_test().command_is_successful()
S3cmdTest('s3cmd can list objects').list_objects('seagate-bucket', host="s3.seagate.com").execute_test().command_is_successful().command_response_should_have('s3://seagate-bucket/3kfile')
S3cmdTest('s3cmd can download 3k file').download_test("seagate-bucket", "3kfile", host="s3.seagate.com").execute_test().command_is_successful().command_created_file("3kfile")
S3cmdTest('s3cmd can delete 3k file').delete_test("seagate-bucket", "3kfile", host="s3.seagate.com").execute_test().command_is_successful()
S3cmdTest('s3cmd can delete bucket').delete_bucket("seagate-bucket").execute_test().command_is_successful()

# ************ 700K FILE TEST ************
S3cmdTest('s3cmd can upload 700K file').upload_test("seagatebucket", "700Kfile", 716800).execute_test().command_is_successful()

S3cmdTest('s3cmd can download 700K file').download_test("seagatebucket", "700Kfile").execute_test().command_is_successful().command_created_file("700Kfile")

S3cmdTest('s3cmd can delete 700K file').delete_test("seagatebucket", "700Kfile").execute_test().command_is_successful()

# ************ 18MB FILE Multipart Upload TEST ***********
S3cmdTest('s3cmd can upload 18MB file').upload_test("seagatebucket", "18MBfile", 18000000).execute_test().command_is_successful()

S3cmdTest('s3cmd can download 18MB file').download_test("seagatebucket", "18MBfile").execute_test().command_is_successful().command_created_file("18MBfile")

S3cmdTest('s3cmd can delete 18MB file').delete_test("seagatebucket", "18MBfile").execute_test().command_is_successful()

#################################################
JClientTest('Jclient can upload partial parts to test abort and list multipart.').partial_multipart_upload("seagatebucket", "18MBfile", 18000000, 1, 2).execute_test().command_is_successful()

result = S3cmdTest('s3cmd can list multipart uploads in progress').list_multipart_uploads("seagatebucket").execute_test()
result.command_response_should_have('18MBfile')

upload_id = result.status.stdout.split('\n')[2].split('\t')[2]

result = S3cmdTest('S3cmd can list parts of multipart upload.').list_parts("seagatebucket", "18MBfile", upload_id).execute_test().command_is_successful()
assert len(result.status.stdout.split('\n')) == 4

S3cmdTest('S3cmd can abort multipart upload').abort_multipart("seagatebucket", "18MBfile", upload_id).execute_test().command_is_successful()

S3cmdTest('s3cmd can test the multipart was aborted.').list_multipart_uploads('seagatebucket').execute_test().command_is_successful().command_response_should_not_have('18MBfile')

S3cmdTest('s3cmd cannot list parts from a nonexistent bucket').list_parts("seagate-bucket", "18MBfile", upload_id).execute_test(negative_case=True).command_should_fail().command_error_should_have("NoSuchBucket")

S3cmdTest('s3cmd abort on nonexistent bucket should fail').abort_multipart("seagate-bucket", "18MBfile", upload_id).execute_test(negative_case=True).command_should_fail().command_error_should_have("NoSuchBucket")

#################################################

# ************ Multiple Delete bucket TEST ************
file_name = "3kfile"
for num in range(0, 4):
  new_file_name = '%s%d' % (file_name, num)
  S3cmdTest('s3cmd can upload 3k file').upload_test("seagatebucket", new_file_name, 3000).execute_test().command_is_successful()

S3cmdTest('s3cmd can delete multiple objects').multi_delete_test("seagatebucket").execute_test().command_is_successful().command_response_should_have('delete: \'s3://seagatebucket/3kfile0\'').command_response_should_have('delete: \'s3://seagatebucket/3kfile3\'')

S3cmdTest('s3cmd should not have objects after multiple delete').list_objects('seagatebucket').execute_test().command_is_successful().command_response_should_not_have('3kfile')

# ************ Delete bucket TEST ************
S3cmdTest('s3cmd can delete bucket').delete_bucket("seagatebucket").execute_test().command_is_successful()
S3cmdTest('s3cmd can delete bucket').delete_bucket("seagatebucket2").execute_test().command_is_successful()

# ************ Signing algorithm test ************
S3cmdTest('s3cmd can create bucket nondnsbucket').create_bucket("nondnsbucket").execute_test().command_is_successful()
S3cmdTest('s3cmd can create bucket seagate-bucket').create_bucket("seagate-bucket").execute_test().command_is_successful()
S3cmdTest('s3cmd can create bucket seagatebucket123').create_bucket("seagatebucket123").execute_test().command_is_successful()
S3cmdTest('s3cmd can create bucket seagate.bucket').create_bucket("seagate.bucket").execute_test().command_is_successful()
S3cmdTest('s3cmd can delete bucket nondnsbucket').delete_bucket("nondnsbucket").execute_test().command_is_successful()
S3cmdTest('s3cmd can delete bucket seagate-bucket').delete_bucket("seagate-bucket").execute_test().command_is_successful()
S3cmdTest('s3cmd can delete bucket seagatebucket123').delete_bucket("seagatebucket123").execute_test().command_is_successful()
S3cmdTest('s3cmd can delete bucket seagate.bucket').delete_bucket("seagate.bucket").execute_test().command_is_successful()

# ************ Create bucket in region ************
S3cmdTest('s3cmd can create bucket').create_bucket("seagatebucket", "eu-west-1").execute_test().command_is_successful()
S3cmdTest('s3cmd cannot fetch info for nonexistent bucket').info_bucket("seagate-bucket").execute_test(negative_case=True).command_should_fail().command_error_should_have("NoSuchBucket")
S3cmdTest('s3cmd created bucket in specific region').info_bucket("seagatebucket").execute_test().command_is_successful().command_response_should_have('eu-west-1')
S3cmdTest('s3cmd can upload 3k file').upload_test("seagatebucket", "3kfile", 3000).execute_test().command_is_successful()
S3cmdTest('s3cmd can retrieve obj info').info_object("seagatebucket", "3kfile").execute_test().command_is_successful().command_response_should_have('3kfile')
S3cmdTest('s3cmd can delete 3k file').delete_test("seagatebucket", "3kfile").execute_test().command_is_successful()
S3cmdTest('s3cmd can delete bucket').delete_bucket("seagatebucket").execute_test().command_is_successful()

# ********** Tests to verify behaviour against InvalidBucketNames.
S3cmdTest('s3cmd cannot create bucket').create_bucket("seagatebucket_1").execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidBucketName")
S3cmdTest('s3cmd cannot create bucket').create_bucket("seagatebucket-").execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidBucketName")
S3cmdTest('s3cmd cannot create bucket').create_bucket("seagatebu.-cket1").execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidBucketName")
S3cmdTest('s3cmd cannot create bucket').create_bucket("Seagatebucket1").execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidBucketName")
S3cmdTest('s3cmd cannot create bucket').create_bucket("se").execute_test(negative_case=True).command_should_fail()
S3cmdTest('s3cmd cannot create bucket').create_bucket("*eagatebucket1").execute_test(negative_case=True).command_should_fail()
S3cmdTest('s3cmd cannot create bucket').create_bucket("12.12.12.12").execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidBucketName")
S3cmdTest('s3cmd cannot create bucket').create_bucket("0.0.0.0").execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidBucketName")
S3cmdTest('s3cmd cannot create bucket').create_bucket("seagate#bucket").execute_test(negative_case=True).command_should_fail()
S3cmdTest('s3cmd cannot create bucket').create_bucket("seagate...bucket").execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidBucketName")
S3cmdTest('s3cmd cannot create bucket').create_bucket("-seagatebucket").execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidBucketName")

# ********** Tests to verify objects with special characters
S3cmdTest('s3cmd can create bucket').create_bucket("seagatebucket").execute_test().command_is_successful()
S3cmdTest('s3cmd can upload objec+t file').upload_test("seagatebucket", "objec+t", 0).execute_test().command_is_successful()
S3cmdTest('s3cmd can list objects').list_specific_objects('seagatebucket','objec+').execute_test().command_is_successful().command_response_should_have('s3://seagatebucket/objec+t')
S3cmdTest('s3cmd can download objec+t file').download_test("seagatebucket", "objec+t").execute_test().command_is_successful().command_created_file("objec+t")
S3cmdTest('s3cmd can retrieve objec+t info').info_object("seagatebucket", "objec+t").execute_test().command_is_successful().command_response_should_have('objec+t')
S3cmdTest('s3cmd can delete objec+t file').delete_test("seagatebucket", "objec+t").execute_test().command_is_successful()
S3cmdTest('s3cmd can upload objec@t file').upload_test("seagatebucket", "objec@t", 0).execute_test().command_is_successful()
S3cmdTest('s3cmd can list objects').list_specific_objects('seagatebucket','objec@').execute_test().command_is_successful().command_response_should_have('s3://seagatebucket/objec@t')
S3cmdTest('s3cmd can download objec@t file').download_test("seagatebucket", "objec@t").execute_test().command_is_successful().command_created_file("objec@t")
S3cmdTest('s3cmd can retrieve objec@t info').info_object("seagatebucket", "objec@t").execute_test().command_is_successful().command_response_should_have('objec@t')
S3cmdTest('s3cmd can delete objec@t file').delete_test("seagatebucket", "objec@t").execute_test().command_is_successful()
S3cmdTest('s3cmd can upload objec,t file').upload_test("seagatebucket", "objec,t", 0).execute_test().command_is_successful()
S3cmdTest('s3cmd can list objects').list_specific_objects('seagatebucket','objec,').execute_test().command_is_successful().command_response_should_have('s3://seagatebucket/objec,t')
S3cmdTest('s3cmd can download objec,t file').download_test("seagatebucket", "objec,t").execute_test().command_is_successful().command_created_file("objec,t")
S3cmdTest('s3cmd can retrieve objec,t info').info_object("seagatebucket", "objec,t").execute_test().command_is_successful().command_response_should_have('objec,t')
S3cmdTest('s3cmd can delete objec,t file').delete_test("seagatebucket", "objec,t").execute_test().command_is_successful()
S3cmdTest('s3cmd can upload objec:t file').upload_test("seagatebucket", "objec:t", 0).execute_test().command_is_successful()
S3cmdTest('s3cmd can list objects').list_specific_objects('seagatebucket','objec:').execute_test().command_is_successful().command_response_should_have('s3://seagatebucket/objec:t')
S3cmdTest('s3cmd can download objec:t file').download_test("seagatebucket", "objec:t").execute_test().command_is_successful().command_created_file("objec:t")
S3cmdTest('s3cmd can retrieve objec:t info').info_object("seagatebucket", "objec:t").execute_test().command_is_successful().command_response_should_have('objec:t')
S3cmdTest('s3cmd can delete objec:t file').delete_test("seagatebucket", "objec:t").execute_test().command_is_successful()
S3cmdTest('s3cmd can upload objec;t file').upload_test("seagatebucket", "objec;t", 0).execute_test().command_is_successful()
S3cmdTest('s3cmd can list objects').list_specific_objects('seagatebucket','objec;').execute_test().command_is_successful().command_response_should_have('s3://seagatebucket/objec;t')
S3cmdTest('s3cmd can download objec;t file').download_test("seagatebucket", "objec;t").execute_test().command_is_successful().command_created_file("objec;t")
S3cmdTest('s3cmd can retrieve objec;t info').info_object("seagatebucket", "objec;t").execute_test().command_is_successful().command_response_should_have('objec;t')
S3cmdTest('s3cmd can delete objec;t file').delete_test("seagatebucket", "objec;t").execute_test().command_is_successful()
S3cmdTest('s3cmd can upload objec?t file').upload_test("seagatebucket", "objec?t", 0).execute_test().command_is_successful()
S3cmdTest('s3cmd can list objects').list_specific_objects('seagatebucket','objec?').execute_test().command_is_successful().command_response_should_have('s3://seagatebucket/objec?t')
S3cmdTest('s3cmd can download objec?t file').download_test("seagatebucket", "objec?t").execute_test().command_is_successful().command_created_file("objec?t")
S3cmdTest('s3cmd can retrieve objec?t info').info_object("seagatebucket", "objec?t").execute_test().command_is_successful().command_response_should_have('objec?t')
S3cmdTest('s3cmd can delete objec?t file').delete_test("seagatebucket", "objec?t").execute_test().command_is_successful()
S3cmdTest('s3cmd can upload objec\ t file').upload_test("seagatebucket", "objec&t", 0).execute_test().command_is_successful()
S3cmdTest('s3cmd can list objects').list_specific_objects('seagatebucket',"objec&").execute_test().command_is_successful().command_response_should_have('s3://seagatebucket/objec&t')
S3cmdTest('s3cmd can download objec&t file').download_test("seagatebucket", "objec&t").execute_test().command_is_successful().command_created_file("objec&t")
S3cmdTest('s3cmd can retrieve objec&t info').info_object("seagatebucket", "objec&t").execute_test().command_is_successful().command_response_should_have('objec&t')
S3cmdTest('s3cmd can delete objec&t file').delete_test("seagatebucket", "objec&t").execute_test().command_is_successful()
S3cmdTest('s3cmd can upload objec t file').upload_test("seagatebucket", "objec t", 0).execute_test().command_is_successful()
S3cmdTest('s3cmd can list objects').list_specific_objects('seagatebucket',"objec ").execute_test().command_is_successful().command_response_should_have('s3://seagatebucket/objec t')
S3cmdTest('s3cmd can download objec t file').download_test("seagatebucket", "objec t").execute_test().command_is_successful().command_created_file("objec t")
S3cmdTest('s3cmd can retrieve objec t info').info_object("seagatebucket", "objec t").execute_test().command_is_successful().command_response_should_have('objec t')
S3cmdTest('s3cmd can delete objec t file').delete_test("seagatebucket", "objec t").execute_test().command_is_successful()
S3cmdTest('s3cmd can delete bucket').delete_bucket("seagatebucket").execute_test().command_is_successful()

# Virtual Host style tests.
Config.config_file = "virtualhoststyle.s3cfg"

# ************ Create bucket ************
S3cmdTest('s3cmd can create bucket').create_bucket("seagatebucket").execute_test().command_is_successful()

# ************ List buckets ************
S3cmdTest('s3cmd can list buckets').list_buckets().execute_test().command_is_successful().command_response_should_have('s3://seagatebucket')

# ************ 3k FILE TEST ************
S3cmdTest('s3cmd can upload 3k file').upload_test("seagatebucket", "3kfile", 3000).execute_test().command_is_successful()

S3cmdTest('s3cmd can download 3k file').download_test("seagatebucket", "3kfile").execute_test().command_is_successful().command_created_file("3kfile")

# ************ 8k FILE TEST ************
S3cmdTest('s3cmd can upload 8k file').upload_test("seagatebucket", "8kfile", 8192).execute_test().command_is_successful()

S3cmdTest('s3cmd can download 8k file').download_test("seagatebucket", "8kfile").execute_test().command_is_successful().command_created_file("8kfile")

# ************ OBJECT LISTING TEST ************
S3cmdTest('s3cmd can list objects').list_objects('seagatebucket').execute_test().command_is_successful().command_response_should_have('s3://seagatebucket/3kfile').command_response_should_have('s3://seagatebucket/8kfile')

S3cmdTest('s3cmd can list specific objects').list_specific_objects('seagatebucket', '3k').execute_test().command_is_successful().command_response_should_have('s3://seagatebucket/3kfile').command_response_should_not_have('s3://seagatebucket/8kfile')

# ************ DELETE OBJECT TEST ************
S3cmdTest('s3cmd can delete 3k file').delete_test("seagatebucket", "3kfile").execute_test().command_is_successful()

S3cmdTest('s3cmd can delete 8k file').delete_test("seagatebucket", "8kfile").execute_test().command_is_successful()

# ************ 700K FILE TEST ************
S3cmdTest('s3cmd can upload 700K file').upload_test("seagatebucket", "700Kfile", 716800).execute_test().command_is_successful()

S3cmdTest('s3cmd can download 700K file').download_test("seagatebucket", "700Kfile").execute_test().command_is_successful().command_created_file("700Kfile")

S3cmdTest('s3cmd can delete 700K file').delete_test("seagatebucket", "700Kfile").execute_test().command_is_successful()

# ************ 18MB FILE Multipart Upload TEST ***********
S3cmdTest('s3cmd can multipart upload 18MB file').upload_test("seagatebucket", "18MBfile", 18000000).execute_test().command_is_successful()

S3cmdTest('s3cmd can download 18MB file').download_test("seagatebucket", "18MBfile").execute_test().command_is_successful().command_created_file("18MBfile")

S3cmdTest('s3cmd can delete 18MB file').delete_test("seagatebucket", "18MBfile").execute_test().command_is_successful()

#################################################

JClientTest('Jclient can upload partial parts to test abort and list multipart.').partial_multipart_upload("seagatebucket", "18MBfile", 18000000, 1, 2).execute_test().command_is_successful()

result = S3cmdTest('s3cmd can list multipart uploads in progress').list_multipart_uploads("seagatebucket").execute_test()
result.command_response_should_have('18MBfile')

upload_id = result.status.stdout.split('\n')[2].split('\t')[2]

result = S3cmdTest('S3cmd can list parts of multipart upload.').list_parts("seagatebucket", "18MBfile", upload_id).execute_test().command_is_successful()
assert len(result.status.stdout.split('\n')) == 4

S3cmdTest('S3cmd can abort multipart upload').abort_multipart("seagatebucket", "18MBfile", upload_id).execute_test().command_is_successful()

S3cmdTest('s3cmd can test the multipart was aborted.').list_multipart_uploads('seagatebucket').execute_test().command_is_successful().command_response_should_not_have('18MBfile')

S3cmdTest('s3cmd cannot list parts from a nonexistent bucket').list_parts("seagate-bucket", "18MBfile", upload_id).execute_test(negative_case=True).command_should_fail().command_error_should_have("NoSuchBucket")

S3cmdTest('s3cmd abort on nonexistent bucket should fail').abort_multipart("seagate-bucket", "18MBfile", upload_id).execute_test(negative_case=True).command_should_fail().command_error_should_have("NoSuchBucket")


############################################

# S3cmdTest('s3cmd can delete 18MB file').delete_test("seagatebucket", "18MBfile").execute_test().command_is_successful()
#
# S3cmdTest('s3cmd can abort multipart upload of 18MB file').multipartupload_abort_test("seagatebucket", "18MBfile", 18000000).execute_test(negative_case=True).command_should_fail()
#
# S3cmdTest('s3cmd can list parts of multipart upload 18MB file').multipartupload_partlist_test("seagatebucket", "18MBfile", 18000000).execute_test().command_is_successful()
#
# S3cmdTest('s3cmd can delete 18MB file').delete_test("seagatebucket", "18MBfile").execute_test().command_is_successful()

# ************ Multiple Delete bucket TEST ************
file_name = "3kfile"
for num in range(0, 4):
  new_file_name = '%s%d' % (file_name, num)
  S3cmdTest('s3cmd can upload 3k file').upload_test("seagatebucket", new_file_name, 3000).execute_test().command_is_successful()

S3cmdTest('s3cmd can delete multiple objects').multi_delete_test("seagatebucket").execute_test().command_is_successful().command_response_should_have('delete: \'s3://seagatebucket/3kfile0\'').command_response_should_have('delete: \'s3://seagatebucket/3kfile3\'')

S3cmdTest('s3cmd should not have objects after multiple delete').list_objects('seagatebucket').execute_test().command_is_successful().command_response_should_not_have('3kfile')

# ************ Delete bucket TEST ************
S3cmdTest('s3cmd can delete bucket').delete_bucket("seagatebucket").execute_test().command_is_successful()

# ************ 18MB FILE Multipart Upload TEST ***********
S3cmdTest('s3cmd can create bucket seagatebucket').create_bucket("seagatebucket").execute_test().command_is_successful()
S3cmdTest('s3cmd can multipart upload 18MB file').upload_test("seagatebucket", "18MBfile", 18000000).execute_test().command_is_successful()

S3cmdTest('s3cmd can download 18MB file').download_test("seagatebucket", "18MBfile").execute_test().command_is_successful().command_created_file("18MBfile")

S3cmdTest('s3cmd can delete 18MB file').delete_test("seagatebucket", "18MBfile").execute_test().command_is_successful()
S3cmdTest('s3cmd should not have object after its delete').list_objects('seagatebucket').execute_test().command_is_successful().command_response_should_not_have('18MBfile')
S3cmdTest('s3cmd can delete bucket').delete_bucket("seagatebucket").execute_test().command_is_successful()
S3cmdTest('s3cmd should not have bucket').list_buckets().execute_test().command_is_successful().command_response_should_not_have('s3://seagatebucket')

# *******************************************************

# ************ Signing algorithm test ************
S3cmdTest('s3cmd can create bucket seagate-bucket').create_bucket("seagate-bucket").execute_test().command_is_successful()
S3cmdTest('s3cmd can create bucket seagatebucket123').create_bucket("seagatebucket123").execute_test().command_is_successful()
S3cmdTest('s3cmd can delete bucket seagate-bucket').delete_bucket("seagate-bucket").execute_test().command_is_successful()
S3cmdTest('s3cmd can delete bucket seagatebucket123').delete_bucket("seagatebucket123").execute_test().command_is_successful()
S3cmdTest('s3cmd should not have bucket').list_buckets().execute_test().command_is_successful().command_response_should_not_have('s3://seagate-bucket')
S3cmdTest('s3cmd should not have bucket').list_buckets().execute_test().command_is_successful().command_response_should_not_have('s3://seagatebucket123')
if Config.no_ssl:
    S3cmdTest('s3cmd can create bucket seagate.bucket').create_bucket("seagate.bucket").execute_test().command_is_successful()
    S3cmdTest('s3cmd can delete bucket seagate.bucket').delete_bucket("seagate.bucket").execute_test().command_is_successful()
    S3cmdTest('s3cmd should not have bucket').list_buckets().execute_test().command_is_successful().command_response_should_not_have('s3://seagate.bucket')


# ************ Create bucket in region ************
S3cmdTest('s3cmd can create bucket').create_bucket("seagatebucket", "eu-west-1").execute_test().command_is_successful()

S3cmdTest('s3cmd created bucket in specific region').info_bucket("seagatebucket").execute_test().command_is_successful().command_response_should_have('eu-west-1')

S3cmdTest('s3cmd cannot fetch info for nonexistent bucket').info_bucket("seagate-bucket").execute_test(negative_case=True).command_should_fail().command_error_should_have("NoSuchBucket")

S3cmdTest('s3cmd can delete bucket').delete_bucket("seagatebucket").execute_test().command_is_successful()
S3cmdTest('s3cmd should not have bucket').list_buckets().execute_test().command_is_successful().command_response_should_not_have('s3://seagatebucket')

# ************ Collision Resolution TEST ************
S3cmdTest('s3cmd can create bucket').create_bucket("seagatebucket").execute_test().command_is_successful()
S3cmdTest('s3cmd can upload 3k file for Collision resolution test').upload_test("seagatebucket", "3kfilecollision", 3000).execute_test().command_is_successful()

S3cmdTest('s3cmd can upload 18MB file for Collision resolution test').upload_test("seagatebucket", "18MBfilecollision", 18000000).execute_test().command_is_successful()

s3kvs.delete_bucket_info("seagatebucket")

S3cmdTest('Create bucket for Collision resolution test').create_bucket("seagatebucket").execute_test().command_is_successful()

S3cmdTest('s3cmd can upload 3k file after Collision resolution').upload_test("seagatebucket", "3kfilecollision", 3000).execute_test().command_is_successful()

s3kvs.expect_object_in_bucket("seagatebucket", "3kfilecollision")

S3cmdTest('s3cmd can download 3kfilecollision after Collision resolution upload').download_test("seagatebucket", "3kfilecollision").execute_test().command_is_successful().command_created_file("3kfilecollision")

S3cmdTest('s3cmd can delete 3kfilecollision after collision resolution').delete_test("seagatebucket", "3kfilecollision").execute_test().command_is_successful()

S3cmdTest('s3cmd can upload 18MB file after Collision resolution').upload_test("seagatebucket", "18MBfilecollision", 3000).execute_test().command_is_successful()

s3kvs.expect_object_in_bucket("seagatebucket", "18MBfilecollision")

S3cmdTest('s3cmd can download 18MBfilecollision after Collision resolution upload').download_test("seagatebucket", "18MBfilecollision").execute_test().command_is_successful().command_created_file("18MBfilecollision")

S3cmdTest('s3cmd can delete 18MBfilecollision after collision resolution').delete_test("seagatebucket", "18MBfilecollision").execute_test().command_is_successful()


S3cmdTest('s3cmd can delete bucket').delete_bucket("seagatebucket").execute_test().command_is_successful()

# ************ ACL/Policy TESTS ************
S3cmdTest('s3cmd can create bucket').create_bucket("seagatebucket").execute_test().command_is_successful()
s3kvs.check_bucket_acl("seagatebucket", default_acl_test=True)

S3cmdTest('s3cmd can upload 3k file with default acl').upload_test("seagatebucket", "3kfile", 3000).execute_test().command_is_successful()
s3kvs.check_object_acl("seagatebucket", "3kfile", default_acl_test=True)

S3cmdTest('s3cmd can not set acl on bucket with InvalidId').setacl_bucket("seagatebucket","read:123").execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidArgument")
'''bucket_chk_acl="PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVRGLTgiPz4KPEFjY2Vzc0NvbnRyb2xQb2xpY3kgeG1sbnM9Imh0dHA6Ly9zMy5hbWF6b25hd3MuY29tL2RvYy8yMDA2LTAzLTAxLyI+PE93bmVyPjxJRD5DMTIzNDU8L0lEPjxEaXNwbGF5TmFtZT50ZXN0ZXI8L0Rpc3BsYXlOYW1lPjwvT3duZXI+PEFjY2Vzc0NvbnRyb2xMaXN0PjxHcmFudD48R3JhbnRlZSB4bWxuczp4c2k9Imh0dHA6Ly93d3cudzMub3JnLzIwMDEvWE1MU2NoZW1hLWluc3RhbmNlIiB4c2k6dHlwZT0iQ2Fub25pY2FsVXNlciI+PElEPkMxMjM0NTwvSUQ+PERpc3BsYXlOYW1lPnRlc3RlcjwvRGlzcGxheU5hbWU+PC9HcmFudGVlPjxQZXJtaXNzaW9uPkZVTExfQ09OVFJPTDwvUGVybWlzc2lvbj48L0dyYW50PjxHcmFudD48R3JhbnRlZSB4bWxuczp4c2k9Imh0dHA6Ly93d3cudzMub3JnLzIwMDEvWE1MU2NoZW1hLWluc3RhbmNlIiB4c2k6dHlwZT0iQ2Fub25pY2FsVXNlciI+PElEPjEyMzwvSUQ+PERpc3BsYXlOYW1lPnRlc3RlcjwvRGlzcGxheU5hbWU+PC9HcmFudGVlPjxQZXJtaXNzaW9uPlJFQUQ8L1Blcm1pc3Npb24+PC9HcmFudD48L0FjY2Vzc0NvbnRyb2xMaXN0PjwvQWNjZXNzQ29udHJvbFBvbGljeT4="
s3kvs.check_bucket_acl("seagatebucket", acl=bucket_chk_acl)'''

# S3cmd cli has behaviour to convert CanonicalID provided to small letters, that causes issue for finding account in ldap due to alter of canonicalid.

#S3cmdTest('s3cmd can set acl on object').setacl_object("seagatebucket","3kfile", "read:123").execute_test().command_is_successful()
#file_chk_acl="PEFjY2Vzc0NvbnRyb2xQb2xpY3kgeG1sbnM9Imh0dHA6Ly9zMy5hbWF6b25hd3MuY29tL2RvYy8yMDA2LTAzLTAxLyI+PE93bmVyPjxJRD5DMTIzNDU8L0lEPjxEaXNwbGF5TmFtZT50ZXN0ZXI8L0Rpc3BsYXlOYW1lPjwvT3duZXI+PEFjY2Vzc0NvbnRyb2xMaXN0PjxHcmFudD48R3JhbnRlZSB4bWxuczp4c2k9Imh0dHA6Ly93d3cudzMub3JnLzIwMDEvWE1MU2NoZW1hLWluc3RhbmNlIiB4c2k6dHlwZT0iQ2Fub25pY2FsVXNlciI+PElEPkMxMjM0NTwvSUQ+PERpc3BsYXlOYW1lPnRlc3RlcjwvRGlzcGxheU5hbWU+PC9HcmFudGVlPjxQZXJtaXNzaW9uPkZVTExfQ09OVFJPTDwvUGVybWlzc2lvbj48L0dyYW50PjxHcmFudD48R3JhbnRlZSB4bWxuczp4c2k9Imh0dHA6Ly93d3cudzMub3JnLzIwMDEvWE1MU2NoZW1hLWluc3RhbmNlIiB4c2k6dHlwZT0iQ2Fub25pY2FsVXNlciI+PElEPjEyMzwvSUQ+PERpc3BsYXlOYW1lPnRlc3RlcjwvRGlzcGxheU5hbWU+PC9HcmFudGVlPjxQZXJtaXNzaW9uPlJFQUQ8L1Blcm1pc3Npb24+PC9HcmFudD48L0FjY2Vzc0NvbnRyb2xMaXN0PjwvQWNjZXNzQ29udHJvbFBvbGljeT4="
#s3kvs.check_object_acl("seagatebucket", "3kfile", acl=file_chk_acl)

S3cmdTest('s3cmd can revoke acl on bucket').revoke_acl_bucket("seagatebucket","read:123").execute_test().command_is_successful()
S3cmdTest('s3cmd can revoke acl on object').revoke_acl_object("seagatebucket","3kfile","read:123").execute_test().command_is_successful()
S3cmdTest('s3cmd can set policy on bucket').setpolicy_bucket("seagatebucket","policy.txt").execute_test().command_is_successful()
S3cmdTest('s3cmd can delete 3kfile after setting acl').delete_test("seagatebucket", "3kfile").execute_test().command_is_successful()
#S3cmdTest('s3cmd can set policy on bucket').delpolicy_bucket("seagatebucket").execute_test().command_is_successful()
S3cmdTest('s3cmd can delete bucket after setting policy/acl').delete_bucket("seagatebucket").execute_test().command_is_successful()

# ********** s3cmd accesslog should return NotImplemented ***********
S3cmdTest('s3cmd accesslog should return NotImplemented/501').accesslog_bucket("seagatebucket").execute_test(negative_case=True).command_should_fail().command_error_should_have("NotImplemented")
