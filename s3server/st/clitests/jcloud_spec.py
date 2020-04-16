#!/usr/bin/python3.6

import os
from framework import Config
from framework import S3PyCliTest
from jcloud import JCloudTest
from jclient import JClientTest
from auth import AuthTest
from s3client_config import S3ClientConfig

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

S3ClientConfig.access_key_id = 'AKIAJPINPFRBTPAYOGNA'
S3ClientConfig.secret_key = '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'

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

# Path style tests.
pathstyle_values = [True, False]
for i, val in enumerate(pathstyle_values):
    S3ClientConfig.pathstyle = val
    print("\nPath style = " + str(val) + "\n")

    JCloudTest('Jcloud can verify bucket does not exist').check_bucket_exists("seagatebucket").execute_test().command_is_successful().command_response_should_have('Bucket seagatebucket does not exist')

    JCloudTest('Jcloud can not get location of non existent bucket').get_bucket_location("seagatebucket").execute_test(negative_case=True).command_should_fail().command_error_should_have('The specified bucket does not exist')

    # ************ Create bucket ************
    JCloudTest('Jcloud can create bucket').create_bucket("seagatebucket").execute_test().command_is_successful()

    JCloudTest('Jcloud cannot create bucket if it exists').create_bucket("seagatebucket").execute_test(negative_case=True).command_should_fail().command_error_should_have("ResourceAlreadyExists")

    JCloudTest('Jcloud can verify bucket existence').check_bucket_exists("seagatebucket").execute_test().command_is_successful().command_response_should_have('Bucket seagatebucket exists')

    JCloudTest('Jcloud can get bucket location').get_bucket_location("seagatebucket").execute_test().command_is_successful().command_response_should_have('us-west-2')

    # ************ List buckets ************
    JCloudTest('Jcloud can list buckets').list_buckets().execute_test().command_is_successful().command_response_should_have('seagatebucket')
    JCloudTest('Jcloud can call list objects on empty bucket').list_objects('seagatebucket').execute_test().command_is_successful()

    # ************ 3k FILE TEST ************
    JCloudTest('Jcloud can verify object does not exist').head_object("seagatebucket", "test/3kfile").execute_test(negative_case=True).command_should_fail().command_error_should_have('Bucket or Object does not exist')

    JCloudTest('Jcloud cannot verify object in nonexistent bucket').head_object("seagate-bucket", "test/3kfile").execute_test(negative_case=True).command_should_fail().command_error_should_have("Bucket or Object does not exist")

    JCloudTest('Jcloud can upload 3k file').put_object("seagatebucket/test/3kfile", "3kfile", 3000).execute_test().command_is_successful()

    JCloudTest('Jcloud cannot upload file to nonexistent bucket').put_object("seagate-bucket/test/3kfile", "3kfile", 3000).execute_test(negative_case=True).command_should_fail().command_error_should_have("The specified bucket does not exist")

    JCloudTest('Jcloud can verify object existence').head_object("seagatebucket", "test/3kfile").execute_test().command_is_successful().command_response_should_have('test/3kfile')

    # ************ cross account tests ********
    account_args = {}
    account_args['AccountName'] = 's3secondaccount'
    account_args['Email'] = 's3secondaccount@seagate.com'
    account_args['ldapuser'] = 'sgiamadmin'
    account_args['ldappasswd'] = 'ldapadmin'
    test_msg = "Create account s3secondaccount"
    s3secondaccount_response_pattern = "AccountId = [\w-]*, CanonicalId = [\w-]*, RootUserName = [\w+=,.@-]*, AccessKeyId = [\w-]*, SecretKey = [\w/+]*$"
    auth_test = AuthTest(test_msg)
    result = auth_test.create_account(**account_args).execute_test()
    result.command_should_match_pattern(s3secondaccount_response_pattern)
    s3secondaccount_response_elements = get_response_elements(result.status.stdout)
    s3secondaccount_canonicalid = s3secondaccount_response_elements['CanonicalId']
    s3secondaccount_displayname = 's3secondaccount'

    # ************ Create bucket in s3secondaccount account************
    S3ClientConfig.access_key_id = s3secondaccount_response_elements['AccessKeyId']
    S3ClientConfig.secret_key = s3secondaccount_response_elements['SecretKey']
    JCloudTest('JCloudTest can create bucket in s3secondaccount account')\
        .create_bucket("seagate-bucket").execute_test().command_is_successful()

    # ************ List buckets ************
    JCloudTest('JCloudTest can list buckets from s3secondaccount account')\
        .list_buckets().execute_test().command_is_successful().command_response_should_have('seagate-bucket')

    # ************ List buckets of specific account************
    S3ClientConfig.access_key_id = 'AKIAJPINPFRBTPAYOGNA'
    S3ClientConfig.secret_key = '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'
    JCloudTest('JCloudTest can list buckets').list_buckets().execute_test()\
        .command_is_successful().command_response_should_have('seagatebucket')\
        .command_is_successful().command_response_should_not_have('seagate-bucket')

    S3ClientConfig.access_key_id = s3secondaccount_response_elements['AccessKeyId']
    S3ClientConfig.secret_key = s3secondaccount_response_elements['SecretKey']
    JCloudTest('JCloudTest can list buckets from s3secondaccount')\
        .list_buckets().execute_test().command_is_successful().command_response_should_not_have('seagatebucket')\
        .command_is_successful().command_response_should_have('seagate-bucket')

    # ************ create bucket that already exsting and owned by another account************
    S3ClientConfig.access_key_id = s3secondaccount_response_elements['AccessKeyId']
    S3ClientConfig.secret_key = s3secondaccount_response_elements['SecretKey']
    JCloudTest('JCloudTest can not create bucket with name exsting in other account')\
        .create_bucket("seagatebucket").execute_test(negative_case=True).command_should_fail()\
        .command_error_should_have("ResourceAlreadyExists")

    S3ClientConfig.access_key_id = 'AKIAJPINPFRBTPAYOGNA'
    S3ClientConfig.secret_key = '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'
    JCloudTest('JCloudTest can not create bucket with name exsting in other account').create_bucket("seagate-bucket")\
        .execute_test(negative_case=True).command_should_fail().command_error_should_have("ResourceAlreadyExists")

    # ************ info_bucket owned by another account************
    S3ClientConfig.access_key_id = s3secondaccount_response_elements['AccessKeyId']
    S3ClientConfig.secret_key = s3secondaccount_response_elements['SecretKey']
    JCloudTest('JCloudTest can not access bucket owned by another account')\
        .check_bucket_exists("seagatebucket").execute_test(negative_case=True).command_should_fail()\
        .command_error_should_have('AuthorizationException')

    S3ClientConfig.access_key_id = 'AKIAJPINPFRBTPAYOGNA'
    S3ClientConfig.secret_key = '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'
    JCloudTest('JCloudTest can not access bucket owned by another account')\
        .check_bucket_exists("seagate-bucket").execute_test(negative_case=True).command_should_fail()\
        .command_error_should_have('AuthorizationException')

    # ************ delete bucket owned by another account************
    S3ClientConfig.access_key_id = s3secondaccount_response_elements['AccessKeyId']
    S3ClientConfig.secret_key = s3secondaccount_response_elements['SecretKey']
    JCloudTest('JCloudTest can not delete bucket owned by another account')\
        .delete_bucket("seagatebucket").execute_test(negative_case=True).command_should_fail()\
        .command_error_should_have("AuthorizationException")

    S3ClientConfig.access_key_id = 'AKIAJPINPFRBTPAYOGNA'
    S3ClientConfig.secret_key = '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'
    JCloudTest('JCloudTest can not deelte bucket owned by another account').delete_bucket("seagate-bucket")\
        .execute_test(negative_case=True).command_should_fail().command_error_should_have("AuthorizationException")

    # ************ upload object to a bucket owned by another account************
    S3ClientConfig.access_key_id = s3secondaccount_response_elements['AccessKeyId']
    S3ClientConfig.secret_key = s3secondaccount_response_elements['SecretKey']
    JCloudTest('JCloudTest can not upload 3k file to bucket owned by another account')\
        .put_object("seagatebucket", "3kfile", 3000).execute_test(negative_case=True).command_should_fail()\
        .command_error_should_have("AuthorizationException")

    S3ClientConfig.access_key_id = 'AKIAJPINPFRBTPAYOGNA'
    S3ClientConfig.secret_key = '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'
    JCloudTest('JCloudTest can not upload 3k file to bucket owned by another account')\
        .put_object("seagate-bucket", "3kfile", 3000).execute_test(negative_case=True)\
        .command_should_fail().command_error_should_have("AuthorizationException")

    # ************ try to delete account which is having bucket ************
    S3ClientConfig.access_key_id = s3secondaccount_response_elements['AccessKeyId']
    S3ClientConfig.secret_key = s3secondaccount_response_elements['SecretKey']
    test_msg = "JCloudTest Cannot delete account s3secondaccount with buckets"
    account_args = {'AccountName': 's3secondaccount'}
    AuthTest(test_msg).delete_account(**account_args).execute_test(negative_case=True)\
        .command_response_should_have("Account cannot be deleted")

    # ************ delete bucket using owner account************
    JCloudTest('JCloudTest can delete bucket')\
        .delete_bucket("seagate-bucket").execute_test().command_is_successful()

    # ************ List buckets of specific account************
    JCloudTest('JCloudTest can list buckets of s3secondaccount account')\
        .list_buckets().execute_test().command_is_successful()\
        .command_is_successful().command_response_should_have('')

    # restore default access key and secret key.
    S3ClientConfig.access_key_id = 'AKIAJPINPFRBTPAYOGNA'
    S3ClientConfig.secret_key = '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'

    # ACL Tests.
    # Bucket ACL Tests.
    JCloudTest('Jcloud can set public ACL on bucket').set_acl("seagatebucket", action="acl-public")\
        .execute_test().command_is_successful().command_response_should_have("ACL set to Public")

    JCloudTest('Jcloud cannot set ACL on nonexistent bucket').set_acl("seagate-bucket", action="acl-public")\
        .execute_test(negative_case=True).command_should_fail()\
        .command_error_should_have("The specified bucket does not exist")

    JCloudTest('Jcloud can verify public ACL on bucket').get_acl("seagatebucket")\
        .execute_test().command_is_successful().command_response_should_have("*anon*: READ")

    JCloudTest('Jcloud cannot verify ACL on nonexistent bucket').get_acl("seagate-bucket")\
        .execute_test(negative_case=True).command_should_fail()\
        .command_error_should_have("The specified bucket does not exist")

    JCloudTest('Jcloud can set private ACL on bucket').set_acl("seagatebucket", action="acl-private")\
        .execute_test().command_is_successful().command_response_should_have("ACL set to Private")

    JCloudTest('Jcloud can verify private ACL on bucket').get_acl("seagatebucket")\
        .execute_test().command_is_successful().command_response_should_not_have("*anon*: READ")

    JCloudTest('Jcloud can not grant READ permission on bucket with InvalidId').set_acl("seagatebucket",
        action="acl-grant", permission="READ:123:root")\
        .execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidArgument")

    JCloudTest('Jcloud can not grant WRITE permission on bucket with InvalidId').set_acl("seagatebucket",
        action="acl-grant", permission="WRITE:123")\
        .execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidArgument")

    JCloudTest('Jcloud can grant READ permission on bucket to cross account').set_acl("seagatebucket",
        action="acl-grant", permission="READ:" + s3secondaccount_canonicalid + ":s3secondaccount")\
        .execute_test().command_is_successful().command_response_should_have("Grant ACL successful")

    JCloudTest('Jcloud can verify READ permission to cross account on bucket').get_acl("seagatebucket")\
        .execute_test().command_is_successful().command_response_should_have(s3secondaccount_canonicalid + ": READ")

    JCloudTest('Jcloud can revoke READ permission to cross account on bucket').set_acl("seagatebucket",
        action="acl-revoke", permission="READ:" + s3secondaccount_canonicalid)\
        .execute_test().command_is_successful().command_response_should_have("Revoke ACL successful")

    # Object ACL Tests.
    JCloudTest('Jcloud can set public ACL on object').set_acl("seagatebucket", "test/3kfile",
        action="acl-public")\
        .execute_test().command_is_successful().command_response_should_have("ACL set to Public")

    JCloudTest('Jcloud cannot set ACL on nonexistent object').set_acl("seagatebucket", "abc",
        action="acl-public")\
        .execute_test(negative_case=True).command_should_fail()\
        .command_error_should_have("The specified key does not exist")

    JCloudTest('Jcloud can verify public ACL on object').get_acl("seagatebucket", "test/3kfile")\
        .execute_test().command_is_successful().command_response_should_have("*anon*: READ")

    JCloudTest('Jcloud cannot verify ACL on nonexistent object').get_acl("seagatebucket", "abc")\
        .execute_test(negative_case=True).command_should_fail()\
        .command_error_should_have("The specified key does not exist")

    JCloudTest('Jcloud can set private ACL on object').set_acl("seagatebucket", "test/3kfile",
        action="acl-private")\
        .execute_test().command_is_successful().command_response_should_have("ACL set to Private")

    JCloudTest('Jcloud can verify private ACL on object').get_acl("seagatebucket", "test/3kfile")\
        .execute_test().command_is_successful().command_response_should_not_have("*anon*: READ")

    JCloudTest('Jcloud can grant READ permission on object').set_acl("seagatebucket", "test/3kfile",
        action="acl-grant", permission="READ:C12345:s3_test")\
        .execute_test().command_is_successful().command_response_should_have("Grant ACL successful")

    JCloudTest('Jcloud can verify READ permission on object').get_acl("seagatebucket", "test/3kfile")\
        .execute_test().command_is_successful().command_response_should_have("C12345: READ")\
        .command_response_should_not_have("WRITE")

    JCloudTest('Jcloud can grant WRITE permission on object').set_acl("seagatebucket", "test/3kfile",
        action="acl-grant", permission="WRITE:C12345")\
        .execute_test().command_is_successful().command_response_should_have("Grant ACL successful")

    JCloudTest('Jcloud can verify WRITE permission on object').get_acl("seagatebucket", "test/3kfile")\
        .execute_test().command_is_successful().command_response_should_have("C12345: READ")\
        .command_response_should_have("C12345: WRITE")

    JCloudTest('Jcloud can revoke WRITE permission on object').set_acl("seagatebucket", "test/3kfile",
        action="acl-revoke", permission="WRITE:C12345")\
        .execute_test().command_is_successful().command_response_should_have("Revoke ACL successful")

    JCloudTest('Jcloud can verify WRITE permission is revoked on object').get_acl("seagatebucket", "test/3kfile")\
        .execute_test().command_is_successful().command_response_should_have("C12345: READ")\
        .command_response_should_not_have("WRITE")

    JCloudTest('Jcloud can grant WRITE permission on object to cross account').set_acl("seagatebucket", "test/3kfile",
        action="acl-grant", permission="WRITE:" + s3secondaccount_canonicalid)\
        .execute_test().command_is_successful().command_response_should_have("Grant ACL successful")

    JCloudTest('Jcloud can verify WRITE permission to cross account').get_acl("seagatebucket", "test/3kfile")\
        .execute_test().command_is_successful().command_response_should_have("C12345: READ")\
        .command_response_should_have(s3secondaccount_canonicalid + ": WRITE")

    JCloudTest('Jcloud can revoke WRITE permission to cross account').set_acl("seagatebucket", "test/3kfile",
        action="acl-revoke", permission="WRITE:" + s3secondaccount_canonicalid)\
        .execute_test().command_is_successful().command_response_should_have("Revoke ACL successful")

    JCloudTest('Jcloud can verify WRITE permission is revoked on object').get_acl("seagatebucket", "test/3kfile")\
        .execute_test().command_is_successful().command_response_should_have("C12345: READ")\
        .command_response_should_not_have(s3secondaccount_canonicalid + ":WRITE")

    # ************ try to delete empty account ************
    S3ClientConfig.access_key_id = s3secondaccount_response_elements['AccessKeyId']
    S3ClientConfig.secret_key = s3secondaccount_response_elements['SecretKey']
    test_msg = "Jcloud can delete account s3secondaccount"
    account_args = {'AccountName': 's3secondaccount'}
    AuthTest(test_msg).delete_account(**account_args).execute_test()\
        .command_response_should_have("Account deleted successfully")

    # restore default access key and secret key.
    S3ClientConfig.access_key_id = 'AKIAJPINPFRBTPAYOGNA'
    S3ClientConfig.secret_key = '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'

    # Current version of Jcloud does not report error in deleteContainer
    # JCloudTest('Jcloud cannot delete bucket which is not empty').delete_bucket("seagatebucket").execute_test(negative_case=True).command_should_fail().command_error_should_have("NotEmpty")

    JCloudTest('Jcloud can download 3k file').get_object("seagatebucket/test", "3kfile").execute_test().command_is_successful().command_created_file("3kfile")

    JCloudTest('Jcloud cannot download nonexistent file').get_object("seagatebucket/test", "nonexistent").execute_test(negative_case=True).command_should_fail().command_error_should_have("No such Bucket or Object")

    JCloudTest('Jcloud cannot download file in nonexistent bucket').get_object("seagate-bucket/test", "nonexistent").execute_test(negative_case=True).command_should_fail().command_error_should_have("No such Bucket or Object")

    # ************ Special Char in file name TEST ************
    JCloudTest('Jcloud can upload 3k file with special chars in filename.').put_object("seagatebucket/2016-04:32:21/3kfile", "3kfile", 3000).execute_test().command_is_successful()

    JCloudTest('Jcloud can download 3k file with special chars in filename.').get_object("seagatebucket/2016-04:32:21", "3kfile").execute_test().command_is_successful().command_created_file("3kfile")

    # ************ 8k FILE TEST ************
    JCloudTest('Jcloud can verify object does not exist').head_object("seagatebucket", "8kfile").execute_test(negative_case=True).command_should_fail().command_error_should_have('Bucket or Object does not exist')

    JCloudTest('Jcloud can upload 8k file').put_object("seagatebucket", "8kfile", 8192).execute_test().command_is_successful()

    JCloudTest('Jcloud can verify object existence').head_object("seagatebucket", "8kfile").execute_test().command_is_successful().command_response_should_have('8kfile')

    JCloudTest('Jcloud can download 8k file').get_object("seagatebucket", "8kfile").execute_test().command_is_successful().command_created_file("8kfile")

    # ************ OBJECT LISTING TEST ************
    JCloudTest('Jcloud can list objects').list_objects('seagatebucket').execute_test().command_is_successful().command_response_should_have('test').command_response_should_have('8kfile')

    JCloudTest('Jcloud cannot list objects for nonexistent bucket').list_objects('seagate-bucket').execute_test(negative_case=True).command_should_fail().command_error_should_have("The specified bucket does not exist")

    JCloudTest('Jcloud can list objects in a directory').list_specific_objects('seagatebucket', 'test').execute_test().command_is_successful().command_response_should_have('3kfile').command_response_should_not_have('8kfile')

    # ************ 700K FILE TEST ************
    JCloudTest('Jcloud can upload 700K file').put_object("seagatebucket", "700Kfile", 716800).execute_test().command_is_successful()

    JCloudTest('Jcloud can download 700K file').get_object("seagatebucket", "700Kfile").execute_test().command_is_successful().command_created_file("700Kfile")

    # ************ 18MB FILE TEST (Without multipart) ************
    JCloudTest('Jcloud can upload 18MB file').put_object("seagatebucket", "18MBfile", 18000000).execute_test().command_is_successful()

    JCloudTest('Jcloud can delete 18MB file').delete_object("seagatebucket", "18MBfile").execute_test().command_is_successful()

    JCloudTest('Jcloud should not list deleted object').list_objects('seagatebucket').execute_test().command_is_successful().command_response_should_not_have('18MBfile ')

    # ************ 18MB FILE Multipart Upload TEST ***********
    JCloudTest('Jcloud can upload 18MB file (multipart)').put_object_multipart("seagatebucket", "18MBfile", 18000000, 15).execute_test().command_is_successful()

    JCloudTest('Jcloud can download 18MB file').get_object("seagatebucket", "18MBfile").execute_test().command_is_successful().command_created_file("18MBfile")

    JCloudTest('Jcloud cannot upload partial parts to nonexistent bucket.').partial_multipart_upload("seagate-bucket", "18MBfile", 18000000, 1, 2).execute_test(negative_case=True).command_should_fail().command_error_should_have("The specified bucket does not exist")

    JCloudTest('Jcloud can upload partial parts to test abort and list multipart.').partial_multipart_upload("seagatebucket", "18MBfile", 18000000, 1, 2).execute_test().command_is_successful()

    result = JClientTest('Jclient can list all multipart uploads.').list_multipart("seagatebucket").execute_test()
    result.command_response_should_have('18MBfile')

    upload_id = result.status.stdout.split("id - ")[1]
    print(upload_id)

    result = JClientTest('Jclient can list parts of multipart upload.').list_parts("seagatebucket", "18MBfile", upload_id).execute_test()
    result.command_response_should_have("part number - 1").command_response_should_have("part number - 2")

    # Current Jcloud version does not report error if abort failed due to invalid bucket name
    # JCloudTest('Jcloud cannot abort multipart upload on invalid bucket').abort_multipart("seagate-bucket", "18MBfile", upload_id).execute_test(negative_case=True).command_should_fail().command_error_should_have("The specified bucket does not exist")

    JCloudTest('Jcloud can abort multipart upload').abort_multipart("seagatebucket", "18MBfile", upload_id).execute_test().command_is_successful()

    JClientTest('Jclient can test the multipart was aborted.').list_multipart('seagatebucket').execute_test().command_is_successful().command_response_should_not_have('18MBfile')

    # ************ DELETE OBJECT TEST ************
    JCloudTest('Jcloud can delete 3k file').delete_object("seagatebucket", "test/3kfile").execute_test().command_is_successful()

    JCloudTest('Jcloud can delete 3k file').delete_object("seagatebucket", "2016-04:32:21/3kfile").execute_test().command_is_successful()

    JCloudTest('Jcloud should not list deleted object').list_objects('seagatebucket/test').execute_test().command_is_successful().command_response_should_not_have('3kfile ')

    JCloudTest('Jcloud should not list deleted object').list_objects('seagatebucket/2016-04:32:21/').execute_test().command_is_successful().command_response_should_not_have('3kfile ')

    # ************ DELETE MULTIPLE OBJECTS TEST ************
    JCloudTest('Jcloud can delete 8k, 700k, 18MB files and non existent 1MB file').delete_multiple_objects("seagatebucket", ["8kfile", "700Kfile", "18MBfile", "1MBfile"]).execute_test().command_is_successful()

    JCloudTest('Jcloud should not list deleted objects').list_objects('seagatebucket').execute_test().command_is_successful().command_response_should_not_have('8kfile').command_response_should_not_have('700Kfile').command_response_should_not_have('18MBfile')

    JCloudTest('Jcloud should succeed in multiple objects delete when bucket is empty').delete_multiple_objects("seagatebucket", ["8kfile", "700Kfile", "18MBfile", "1MBfile"]).execute_test().command_is_successful()

    JCloudTest('Jcloud cannot delete multiple files when bucket does not exists').delete_multiple_objects("seagate-bucket", ["8kfile", "700Kfile", "18MBfile", "1MBfile"]).execute_test(negative_case=True).command_should_fail().command_error_should_have("The specified bucket does not exist")

    # ************ Delete bucket TEST ************
    JCloudTest('Jcloud can delete bucket').delete_bucket("seagatebucket").execute_test().command_is_successful()

    JCloudTest('Jcloud should not list deleted bucket').list_buckets().execute_test().command_is_successful().command_response_should_not_have('seagatebucket')

    # Current version of Jcloud does not report error in deleteContainer
    # JCloudTest('Jcloud cannot delete bucket which is not empty').delete_bucket("seagatebucket").execute_test(negative_case=True).command_should_fail().command_error_should_have("NotPresent")

    # ********** Tests to verify objects with special characters
    JCloudTest('JCloud can create bucket').create_bucket("seagatebucket").execute_test().command_is_successful()
    JCloudTest('JCloud can upload objec+t file').put_object("seagatebucket", "objec+t", 0 , prefix="a+b").execute_test().command_is_successful()
    JCloudTest('JCloud can list objects').list_specific_objects('seagatebucket','a+b').execute_test().command_is_successful().command_response_should_have('objec+t')
    JCloudTest('JCloud can download objec+t file').get_object("seagatebucket/a+b", "objec+t").execute_test().command_is_successful().command_created_file("objec+t")
    JCloudTest('JCloud can delete objec+t file').delete_object("seagatebucket", "a+b/objec+t").execute_test().command_is_successful()
    JCloudTest('JCloud can upload objec@t file').put_object("seagatebucket", "objec@t", 0, prefix="a@b").execute_test().command_is_successful()
    JCloudTest('JCloud can list objects').list_specific_objects('seagatebucket','a@b').execute_test().command_is_successful().command_response_should_have('objec@t')
    JCloudTest('JCloud can download objec@t file').get_object("seagatebucket/a@b", "objec@t").execute_test().command_is_successful().command_created_file("objec@t")
    JCloudTest('JCloud can delete objec@t file').delete_object("seagatebucket", "a@b/objec@t").execute_test().command_is_successful()
    JCloudTest('JCloud can upload objec,t file').put_object("seagatebucket", "objec,t", 0, prefix="a,b").execute_test().command_is_successful()
    JCloudTest('JCloud can list objects').list_specific_objects('seagatebucket','a,b').execute_test().command_is_successful().command_response_should_have('objec,t')
    JCloudTest('JCloud can download objec,t file').get_object("seagatebucket/a,b", "objec,t").execute_test().command_is_successful().command_created_file("objec,t")
    JCloudTest('JCloud can delete objec,t file').delete_object("seagatebucket", "a,b/objec,t").execute_test().command_is_successful()
    JCloudTest('JCloud can upload objec:t file').put_object("seagatebucket", "objec:t", 0, prefix="a:b").execute_test().command_is_successful()
    JCloudTest('JCloud can list objects').list_specific_objects('seagatebucket','a:b').execute_test().command_is_successful().command_response_should_have('objec:t')
    JCloudTest('JCloud can download objec:t file').get_object("seagatebucket/a:b", "objec:t").execute_test().command_is_successful().command_created_file("objec:t")
    JCloudTest('JCloud can delete objec:t file').delete_object("seagatebucket", "a:b/objec:t").execute_test().command_is_successful()
    JCloudTest('JCloud can upload objec;t').put_object("seagatebucket", "objec;t", 0, prefix="a;b").execute_test().command_is_successful()
    JCloudTest('JCloud can list objects').list_specific_objects('seagatebucket','a;b').execute_test().command_is_successful().command_response_should_have('objec;t')
    JCloudTest('JCloud can download objec;t file').get_object("seagatebucket/a;b", "objec;t").execute_test().command_is_successful().command_created_file("objec;t")
    JCloudTest('JCloud can delete objec;t file').delete_object("seagatebucket", "a;b/objec;t").execute_test().command_is_successful()
    JCloudTest('JCloud can upload 3k file').put_object("seagatebucket", "objec?t", 0, prefix="a?b").execute_test().command_is_successful()
    JCloudTest('JCloud can list objects').list_specific_objects('seagatebucket','a?b').execute_test().command_is_successful().command_response_should_have('objec?t')
    JCloudTest('JCloud can download objec?t file').get_object("seagatebucket/a?b", "objec?t").execute_test().command_is_successful().command_created_file("objec?t")
    JCloudTest('JCloud can delete objec?t file').delete_object("seagatebucket", "a?b/objec?t").execute_test().command_is_successful()
    #Jcloud and aws gives same kind of behaviour when tried with object name with special character &,both do not list object with &.
    #JCloudTest('JCloud can upload 3k file').put_object("seagatebucket", "objec&t", 0, prefix="a&b").execute_test().command_is_successful()
    #JCloudTest('JCloud can list objects').list_specific_objects('seagatebucket','a&b').execute_test().command_is_successful().command_response_should_have('objec&t')
    #JCloudTest('JCloud can download objec?t file').get_object("seagatebucket/a&b", "objec&t").execute_test().command_is_successful().command_created_file("objec&t")
    #JCloudTest('JCloud can delete objec?t file').delete_object("seagatebucket", "a&b/objec&t").execute_test().command_is_successful()
    JCloudTest('JCloud can upload 3k file').put_object("seagatebucket", "objec t", 0, prefix="a b").execute_test().command_is_successful()
    JCloudTest('JCloud can list objects').list_specific_objects('seagatebucket','a b').execute_test().command_is_successful().command_response_should_have('objec t')
    JCloudTest('JCloud can delete objec?t file').delete_object("seagatebucket", "a b/objec t").execute_test().command_is_successful()
    JCloudTest('JCloud can delete bucket').delete_bucket("seagatebucket").execute_test().command_is_successful()

    #************* Multipart upload and List part with object name with speacial character***************
    JCloudTest('Jcloud can create bucket').create_bucket("seagatebucket123").execute_test().command_is_successful()

    JCloudTest('Jcloud can upload tes:t file with 18MB (multipart)').put_object_multipart("seagatebucket123", "tes:t", 18000000, 15).execute_test().command_is_successful()
    JCloudTest('Jcloud can upload partial parts to test abort and list multipart.').partial_multipart_upload("seagatebucket123", "tes:t", 18000000, 1, 2).execute_test().command_is_successful()

    result = JClientTest('Jclient can list all multipart uploads.').list_multipart("seagatebucket123").execute_test()
    result.command_response_should_have('tes:t')

    upload_id = result.status.stdout.split("id - ")[1]

    result = JClientTest('Jclient can list parts of multipart upload.').list_parts("seagatebucket123", "tes:t", upload_id).execute_test()
    result.command_response_should_have("part number - 1").command_response_should_have("part number - 2")
    JCloudTest('JCloud can delete objec?t file').delete_object("seagatebucket123", "tes:t").execute_test().command_is_successful()
    JCloudTest('Jcloud can upload tes.t file with 18MB (multipart)').put_object_multipart("seagatebucket123", "tes.t", 18000000, 15).execute_test().command_is_successful()
    JCloudTest('Jcloud can upload partial parts to test abort and list multipart.').partial_multipart_upload("seagatebucket123", "tes.t", 18000000, 1, 2).execute_test().command_is_successful()

    result = JClientTest('Jclient can list all multipart uploads.').list_multipart("seagatebucket123").execute_test()
    result.command_response_should_have('tes.t')

    upload_id = result.status.stdout.split("id - ")[1]

    result = JClientTest('Jclient can list parts of multipart upload.').list_parts("seagatebucket123", "tes.t", upload_id).execute_test()
    result.command_response_should_have("part number - 1").command_response_should_have("part number - 2")
    JCloudTest('JCloud can delete objec?t file').delete_object("seagatebucket123", "tes.t").execute_test().command_is_successful()
    JCloudTest('Jcloud can upload tes,t file with 18MB (multipart)').put_object_multipart("seagatebucket123", "tes,t", 18000000, 15).execute_test().command_is_successful()
    JCloudTest('Jcloud can upload partial parts to test abort and list multipart.').partial_multipart_upload("seagatebucket123", "tes,t", 18000000, 1, 2).execute_test().command_is_successful()

    result = JClientTest('Jclient can list all multipart uploads.').list_multipart("seagatebucket123").execute_test()
    result.command_response_should_have('tes,t')

    upload_id = result.status.stdout.split("id - ")[1]

    result = JClientTest('Jclient can list parts of multipart upload.').list_parts("seagatebucket123", "tes,t", upload_id).execute_test()
    result.command_response_should_have("part number - 1").command_response_should_have("part number - 2")
    JCloudTest('JCloud can delete objec?t file').delete_object("seagatebucket123", "tes,t").execute_test().command_is_successful()
    JCloudTest('Jcloud can create bucket').create_bucket("seagate-bucket").execute_test().command_is_successful()

    JCloudTest('Jcloud can upload tes@t file with 18MB (multipart)').put_object_multipart("seagate-bucket", "tes@t", 18000000, 15).execute_test().command_is_successful()
    JCloudTest('Jcloud can upload partial parts to test abort and list multipart.').partial_multipart_upload("seagate-bucket", "tes@t", 18000000, 1, 2).execute_test().command_is_successful()

    result = JClientTest('Jclient can list all multipart uploads.').list_multipart("seagate-bucket").execute_test()
    result.command_response_should_have('tes@t')

    upload_id = result.status.stdout.split("id - ")[1]

    result = JClientTest('Jclient can list parts of multipart upload.').list_parts("seagate-bucket", "tes@t", upload_id).execute_test()
    result.command_response_should_have("part number - 1").command_response_should_have("part number - 2")
    JCloudTest('JCloud can delete objec?t file').delete_object("seagate-bucket", "tes@t").execute_test().command_is_successful()
    JCloudTest('Jcloud can upload tes+t file with 18MB (multipart)').put_object_multipart("seagate-bucket", "tes+t", 18000000, 15).execute_test().command_is_successful()
    JCloudTest('Jcloud can upload partial parts to test abort and list multipart.').partial_multipart_upload("seagate-bucket", "tes+t", 18000000, 1, 2).execute_test().command_is_successful()

    result = JClientTest('Jclient can list all multipart uploads.').list_multipart("seagate-bucket").execute_test()
    result.command_response_should_have('tes+t')

    upload_id = result.status.stdout.split("id - ")[1]

    result = JClientTest('Jclient can list parts of multipart upload.').list_parts("seagate-bucket", "tes+t", upload_id).execute_test()
    result.command_response_should_have("part number - 1").command_response_should_have("part number - 2")
    JCloudTest('JCloud can delete objec?t file').delete_object("seagate-bucket", "tes+t").execute_test().command_is_successful()
    JCloudTest('Jcloud can create bucket').create_bucket("seagate.bucket").execute_test().command_is_successful()

    JCloudTest('Jcloud can upload tes?t file with 18MB (multipart)').put_object_multipart("seagate.bucket", "tes?t", 18000000, 15).execute_test().command_is_successful()
    JCloudTest('Jcloud can upload partial parts to test abort and list multipart.').partial_multipart_upload("seagate.bucket", "tes?t", 18000000, 1, 2).execute_test().command_is_successful()

    result = JClientTest('Jclient can list all multipart uploads.').list_multipart("seagate.bucket").execute_test()
    result.command_response_should_have('tes?t')

    upload_id = result.status.stdout.split("id - ")[1]

    result = JClientTest('Jclient can list parts of multipart upload.').list_parts("seagate.bucket", "tes?t", upload_id).execute_test()
    result.command_response_should_have("part number - 1").command_response_should_have("part number - 2")
    JCloudTest('Jcloud can call list objects on empty bucket').list_objects('seagate-bucket').execute_test().command_is_successful()
    JCloudTest('Jcloud can call list objects on empty bucket').list_objects('seagate.bucket').execute_test().command_is_successful()
    JCloudTest('Jcloud can call list objects on empty bucket').list_objects('seagatebucket123').execute_test().command_is_successful()
    JCloudTest('JCloud can delete objec?t file').delete_object("seagate.bucket", "tes?t").execute_test().command_is_successful()
    JCloudTest('Jcloud can delete bucket seagate-bucket').delete_bucket("seagate-bucket").execute_test().command_is_successful()
    JCloudTest('Jcloud can delete bucket seagatebucket123').delete_bucket("seagatebucket123").execute_test().command_is_successful()
    JCloudTest('Jcloud can delete bucket seagate.bucket').delete_bucket("seagate.bucket").execute_test().command_is_successful()


    # ************ Listing with prefix ************
    JCloudTest('JCloud can create bucket seagatebucket').create_bucket("seagatebucket").execute_test().command_is_successful()
    JCloudTest('JCloud can upload a/3kfile file').put_object("seagatebucket", "3kfile", 3000, prefix="a").execute_test().command_is_successful()
    JCloudTest('JCloud can upload b/3kfile file').put_object("seagatebucket", "3kfile", 3000, prefix="b").execute_test().command_is_successful()
    JCloudTest('JCloud can list specific objects with prefix a/').list_specific_objects('seagatebucket', 'a/').execute_test().command_is_successful().command_response_should_have('a/3kfile').command_response_should_not_have('b/3kfile')
    JCloudTest('JCloud can list specific objects with prefix b/').list_specific_objects('seagatebucket', 'b/').execute_test().command_is_successful().command_response_should_have('b/3kfile').command_response_should_not_have('a/3kfile')
    JCloudTest('JCloud can delete a/3kfile, b/3kfile file').delete_multiple_objects("seagatebucket", ["a/3kfile", "b/3kfile"]).execute_test().command_is_successful()
    JCloudTest('JCloud can delete bucket').delete_bucket("seagatebucket").execute_test().command_is_successful()

    # ************ Delete bucket even if parts are present(multipart) ************
    JCloudTest('JCloud can create bucket seagatebucket').create_bucket("seagatebucket").execute_test().command_is_successful()
    JCloudTest('JCloud can upload partial parts to test abort and list multipart.').partial_multipart_upload("seagatebucket", "18MBfile", 18000000, 1, 2).execute_test().command_is_successful()
    JCloudTest('JCloud can delete bucket even if parts are present').delete_bucket("seagatebucket").execute_test().command_is_successful()

    # ************ Signing algorithm test ************
    JCloudTest('Jcloud can create bucket seagate-bucket').create_bucket("seagate-bucket").execute_test().command_is_successful()
    JCloudTest('Jcloud can create bucket seagatebucket123').create_bucket("seagatebucket123").execute_test().command_is_successful()
    JCloudTest('Jcloud can create bucket seagate.bucket').create_bucket("seagate.bucket").execute_test().command_is_successful()
    JCloudTest('Jcloud can delete bucket seagate-bucket').delete_bucket("seagate-bucket").execute_test().command_is_successful()
    JCloudTest('Jcloud can delete bucket seagatebucket123').delete_bucket("seagatebucket123").execute_test().command_is_successful()
    JCloudTest('Jcloud can delete bucket seagate.bucket').delete_bucket("seagate.bucket").execute_test().command_is_successful()
    JCloudTest('Jcloud should not list deleted buckets').list_buckets().execute_test().command_is_successful().command_response_should_not_have('seagate-bucket').command_response_should_not_have('seagatebucket123').command_response_should_not_have('seagate.bucket')


# Add tests which are specific to Path style APIs

S3ClientConfig.pathstyle = True

# ************ Signing algorithm test ************
# /etc/hosts should not contains nondnsbucket. This is to test the path style APIs.
JCloudTest('Jcloud can create bucket nondnsbucket').create_bucket("nondnsbucket").execute_test().command_is_successful()
JCloudTest('Jcloud can delete bucket nondnsbucket').delete_bucket("nondnsbucket").execute_test().command_is_successful()
JCloudTest('Jcloud should not list deleted buckets').list_buckets().execute_test().command_is_successful().command_response_should_not_have('nondnsbucket')

