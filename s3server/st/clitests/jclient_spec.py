#!/usr/bin/python3.6

import os
from framework import Config
from framework import S3PyCliTest
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

    JClientTest('Jclient can verify bucket does not exist').check_bucket_exists("seagatebucket").execute_test().command_is_successful().command_response_should_have('Bucket seagatebucket does not exist')

    JClientTest('Jclient cannot get bucket location for nonexistent bucket').get_bucket_location("seagatebucket").execute_test(negative_case=True).command_should_fail().command_error_should_have('No such bucket')

    # get-bucket-acl: no bucket exists
    JClientTest('Jclient can (not) get bucket ACL').get_acl("seagatebucket").execute_test(negative_case=True).command_should_fail().command_error_should_have('No such bucket')

    # ************ Create bucket ************
    JClientTest('Jclient can create bucket').create_bucket("seagatebucket", "us-west-2").execute_test().command_is_successful()

    JClientTest('Jclient cannot create bucket if it exists').create_bucket("seagatebucket", "us-west-2").execute_test(negative_case=True).command_should_fail().command_error_should_have("BucketAlreadyExists")

    JClientTest('Jclient can get bucket location').get_bucket_location("seagatebucket").execute_test().command_is_successful().command_response_should_have('us-west-2')

    JClientTest('Jclient can verify bucket existence').check_bucket_exists("seagatebucket").execute_test().command_is_successful().command_response_should_have('Bucket seagatebucket exists')

    # ************ List buckets ************
    JClientTest('Jclient can list buckets').list_buckets().execute_test().command_is_successful().command_response_should_have('seagatebucket')
    JClientTest('Jclient can call list objects on empty bucket').list_objects('seagatebucket').execute_test().command_is_successful()

    # get-bucket-acl
    JClientTest('Jclient can get bucket ACL').get_acl("seagatebucket").execute_test().command_is_successful().command_response_should_have('C12345: FULL_CONTROL')

    # ************ 3k FILE TEST ************
    JClientTest('Jclient can verify object does not exist').head_object("seagatebucket", "3kfile").execute_test(negative_case=True).command_should_fail().command_error_should_have('Bucket or Object does not exist')

    JClientTest('Jclient cannot verify object in nonexistent bucket').head_object("seagate-bucket", "3kfile").execute_test(negative_case=True).command_should_fail().command_error_should_have("Bucket or Object does not exist")

    JClientTest('Jclient can (not) get object acl').get_acl("seagatebucket", "3kfile").execute_test(negative_case=True).command_should_fail().command_error_should_have('No such object')

    JClientTest('Jclient can upload 3k file').put_object("seagatebucket", "3kfile", 3000).execute_test().command_is_successful()

    JClientTest('Jclient can upload 3k file in chunked mode').put_object("seagatebucket", "3kfilec", 3000, chunked=True).execute_test().command_is_successful()

    JClientTest('Jclient can get object acl').get_acl("seagatebucket", "3kfilec").execute_test().command_is_successful().command_response_should_have('C12345: FULL_CONTROL')


    # ************* File Overwrite Test **********
    JClientTest('Jclient can upload 3k file in chunked mode').put_object("seagatebucket", "3kfilec", 3072, chunked=True).execute_test().command_is_successful()


    JClientTest('Jclient cannot upload file to nonexistent bucket').put_object("seagate-bucket", "3kfile", 3000).execute_test(negative_case=True).command_should_fail().command_error_should_have("The specified bucket does not exist.")

    JClientTest('Jclient cannot upload chunked file to nonexistent bucket').put_object("seagate-bucket", "3kfilec", 3000, chunked=True).execute_test(negative_case=True).command_should_fail().command_error_should_have("The specified bucket does not exist.")

    JClientTest('Jclient cannot delete bucket which is not empty').delete_bucket("seagatebucket").execute_test(negative_case=True).command_should_fail().command_error_should_have("BucketNotEmpty")

    JClientTest('Jclient can verify object existence').head_object("seagatebucket", "3kfile").execute_test().command_is_successful().command_response_should_have("3kfile")

    JClientTest('Jclient can get object acl').get_acl("seagatebucket", "3kfile").execute_test().command_is_successful().command_response_should_have('C12345: FULL_CONTROL')

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
    s3secondaccountId = s3secondaccount_response_elements['CanonicalId']

    # ************ Create bucket in s3secondaccount account************
    S3ClientConfig.access_key_id = s3secondaccount_response_elements['AccessKeyId']
    S3ClientConfig.secret_key = s3secondaccount_response_elements['SecretKey']
    JClientTest('Jclient can create bucket in s3secondaccount account')\
        .create_bucket("seagate-bucket").execute_test().command_is_successful()

    # ************ List buckets ************
    JClientTest('Jclient can list buckets from s3secondaccount account')\
        .list_buckets().execute_test().command_is_successful().command_response_should_have('seagate-bucket')

    # ************ List buckets of specific account************
    S3ClientConfig.access_key_id = 'AKIAJPINPFRBTPAYOGNA'
    S3ClientConfig.secret_key = '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'
    JClientTest('Jclient can list buckets').list_buckets().execute_test()\
        .command_is_successful().command_response_should_have('seagatebucket')\
        .command_is_successful().command_response_should_not_have('seagate-bucket')

    S3ClientConfig.access_key_id = s3secondaccount_response_elements['AccessKeyId']
    S3ClientConfig.secret_key = s3secondaccount_response_elements['SecretKey']
    JClientTest('Jclient can list buckets from seagate-bucket')\
        .list_buckets().execute_test().command_is_successful().command_response_should_not_have('seagatebucket')\
        .command_is_successful().command_response_should_have('seagate-bucket')

    # ************ create bucket that already exsting and owned by another account************
    S3ClientConfig.access_key_id = s3secondaccount_response_elements['AccessKeyId']
    S3ClientConfig.secret_key = s3secondaccount_response_elements['SecretKey']
    JClientTest('Jclient can not create bucket with name exsting in other account')\
        .create_bucket("seagatebucket").execute_test(negative_case=True).command_should_fail()\
        .command_error_should_have("BucketAlreadyExists")

    S3ClientConfig.access_key_id = 'AKIAJPINPFRBTPAYOGNA'
    S3ClientConfig.secret_key = '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'
    JClientTest('Jclient can not create bucket with name exsting in other account').create_bucket("seagate-bucket")\
        .execute_test(negative_case=True).command_should_fail().command_error_should_have("BucketAlreadyExists")

    # ************ delete bucket owned by another account************
    S3ClientConfig.access_key_id = s3secondaccount_response_elements['AccessKeyId']
    S3ClientConfig.secret_key = s3secondaccount_response_elements['SecretKey']
    JClientTest('Jclient can not delete bucket owned by another account')\
        .delete_bucket("seagatebucket").execute_test(negative_case=True).command_should_fail()\
        .command_error_should_have("Access Denied")

    S3ClientConfig.access_key_id = 'AKIAJPINPFRBTPAYOGNA'
    S3ClientConfig.secret_key = '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'
    JClientTest('Jclient can not deelte bucket owned by another account').delete_bucket("seagate-bucket")\
        .execute_test(negative_case=True).command_should_fail().command_error_should_have("Access Denied")

    # ************ upload object to a bucket owned by another account************
    S3ClientConfig.access_key_id = s3secondaccount_response_elements['AccessKeyId']
    S3ClientConfig.secret_key = s3secondaccount_response_elements['SecretKey']
    JClientTest('Jclient can not upload 3k file to bucket owned by another account')\
        .put_object("seagatebucket", "3kfile", 3000).execute_test(negative_case=True).command_should_fail()\
        .command_error_should_have("Access Denied")

    S3ClientConfig.access_key_id = 'AKIAJPINPFRBTPAYOGNA'
    S3ClientConfig.secret_key = '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'
    JClientTest('Jclient can not upload 3k file to bucket owned by another account')\
        .put_object("seagate-bucket", "3kfile", 3000).execute_test(negative_case=True)\
        .command_should_fail().command_error_should_have("Access Denied")

    # ************ try to delete account which is having bucket ************
    S3ClientConfig.access_key_id = s3secondaccount_response_elements['AccessKeyId']
    S3ClientConfig.secret_key = s3secondaccount_response_elements['SecretKey']
    test_msg = "Jclient Cannot delete account s3secondaccount with buckets"
    account_args = {'AccountName': 's3secondaccount'}
    AuthTest(test_msg).delete_account(**account_args).execute_test(negative_case=True)\
        .command_response_should_have("Account cannot be deleted")

    # ************ delete bucket using owner account************
    JClientTest('Jclient can delete bucket')\
        .delete_bucket("seagate-bucket").execute_test().command_is_successful()

    # ************ List buckets of specific account************
    JClientTest('Jclient can list buckets of s3secondaccount account')\
        .list_buckets().execute_test().command_is_successful()\
        .command_is_successful().command_response_should_have('')

    # restore default access key and secret key.
    S3ClientConfig.access_key_id = 'AKIAJPINPFRBTPAYOGNA'
    S3ClientConfig.secret_key = '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'

    # ACL Tests.
    # Bucket ACL Tests.
    JClientTest('Jclient can set public ACL on bucket').set_acl("seagatebucket", action="acl-public")\
        .execute_test().command_is_successful().command_response_should_have("ACL set to Public")

    JClientTest('Jclient cannot set ACL on nonexistent bucket').set_acl("seagate-bucket", action="acl-public")\
        .execute_test(negative_case=True).command_should_fail()\
        .command_error_should_have("The specified bucket does not exist")

    JClientTest('Jclient can verify public ACL on bucket').get_acl("seagatebucket")\
        .execute_test().command_is_successful().command_response_should_have("*anon*: READ")

    JClientTest('Jclient cannot verify ACL on nonexistent bucket').get_acl("seagate-bucket")\
        .execute_test(negative_case=True).command_should_fail()\
        .command_error_should_have("No such bucket")

    JClientTest('Jclient can set private ACL on bucket').set_acl("seagatebucket", action="acl-private")\
        .execute_test().command_is_successful().command_response_should_have("ACL set to Private")

    JClientTest('Jclient can verify private ACL on bucket').get_acl("seagatebucket")\
        .execute_test().command_is_successful().command_response_should_not_have("*anon*: READ")

    JClientTest('Jclient can not grant READ permission on bucket with InvalidId').set_acl("seagatebucket",
        action="acl-grant", permission="READ:123:root")\
        .execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidArgument")


    JClientTest('Jclient can not grant WRITE permission on bucket with InvalidId').set_acl("seagatebucket",
        action="acl-grant", permission="WRITE:123")\
        .execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidArgument")

    JClientTest('Jclient can not grant WRITE permission on bucket with InvalidId').set_acl("seagatebucket",
        action="acl-grant", permission="WRITE:123")\
        .execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidArgument")

    JClientTest('Jclient can grant READ permission on bucket to another account').set_acl("seagatebucket",
        action="acl-grant", permission="READ:" + s3secondaccountId)\
        .execute_test().command_is_successful().command_response_should_not_have(s3secondaccountId + ": READ")

    JClientTest('Jclient can revoke READ permission on bucket').set_acl("seagatebucket",
        action="acl-revoke", permission="READ:" + s3secondaccountId)\
        .execute_test().command_is_successful().command_response_should_have("Revoke ACL successful")

    # Object ACL Tests.

    JClientTest('Jclient can set public ACL on object').set_acl("seagatebucket", "3kfile",
        action="acl-public")\
        .execute_test().command_is_successful().command_response_should_have("ACL set to Public")

    JClientTest('Jclient cannot set ACL on nonexistent object').set_acl("seagatebucket", "abc",
        action="acl-public")\
        .execute_test(negative_case=True).command_should_fail()\
        .command_error_should_have("The specified key does not exist")

    JClientTest('Jclient can verify public ACL on object').get_acl("seagatebucket", "3kfile")\
        .execute_test().command_is_successful().command_response_should_have("*anon*: READ")

    JClientTest('Jclient cannot verify ACL on nonexistent object').get_acl("seagatebucket", "abc")\
        .execute_test(negative_case=True).command_should_fail()\
        .command_error_should_have("No such object")

    JClientTest('Jclient can set private ACL on object').set_acl("seagatebucket", "3kfile",
        action="acl-private")\
        .execute_test().command_is_successful().command_response_should_have("ACL set to Private")

    JClientTest('Jclient can verify private ACL on object').get_acl("seagatebucket", "3kfile")\
        .execute_test().command_is_successful().command_response_should_not_have("*anon*: READ")

    JClientTest('Jclient can grant READ permission on object').set_acl("seagatebucket", "3kfile",
        action="acl-grant", permission="READ:"+ s3secondaccountId + ":s3secondaccount")\
        .execute_test().command_is_successful().command_response_should_have("Grant ACL successful")

    JClientTest('Jclient can verify READ ACL on object').get_acl("seagatebucket", "3kfile")\
        .execute_test().command_is_successful().command_response_should_have(s3secondaccountId + ": READ")\
        .command_response_should_not_have("WRITE")

    JClientTest('Jclient can grant READ_ACP permission on object').set_acl("seagatebucket", "3kfile",
        action="acl-grant", permission="READ_ACP:" + s3secondaccountId + ":root")\
        .execute_test().command_is_successful().command_response_should_have("Grant ACL successful")

    JClientTest('Jclient can verify READ ACL on object').get_acl("seagatebucket", "3kfile")\
        .execute_test().command_is_successful().command_response_should_have(s3secondaccountId + ": READ_ACP")\
        .command_response_should_not_have("WRITE")

    JClientTest('Jclient can grant WRITE_ACP permission on object').set_acl("seagatebucket", "3kfile",
        action="acl-grant", permission="WRITE_ACP:" + s3secondaccountId + ":s3secondaccount")\
        .execute_test().command_is_successful().command_response_should_have("Grant ACL successful")

    JClientTest('Jclient can verify WRITE_ACP ACL on object').get_acl("seagatebucket", "3kfile")\
        .execute_test().command_is_successful().command_response_should_have(s3secondaccountId + ": WRITE_ACP")

    JClientTest('Jclient can grant WRITE permission on object').set_acl("seagatebucket", "3kfile",
        action="acl-grant", permission="WRITE:" + s3secondaccountId)\
        .execute_test().command_is_successful().command_response_should_have("Grant ACL successful")

    JClientTest('Jclient can verify WRITE ACL on object').get_acl("seagatebucket", "3kfile")\
        .execute_test().command_is_successful().command_response_should_have(s3secondaccountId + ": READ")\
        .command_response_should_have(s3secondaccountId + ": WRITE")

    JClientTest('Jclient can revoke WRITE permission on object').set_acl("seagatebucket", "3kfile",
        action="acl-revoke", permission="WRITE:" + s3secondaccountId)\
        .execute_test().command_is_successful().command_response_should_have("Revoke ACL successful")

    JClientTest('Jclient can revoke WRITE_ACP permission on object').set_acl("seagatebucket", "3kfile",
        action="acl-revoke", permission="WRITE_ACP:" + s3secondaccountId)\
        .execute_test().command_is_successful().command_response_should_have("Revoke ACL successful")

    JClientTest('Jclient can verify WRITE ACL is revoked on object').get_acl("seagatebucket", "3kfile")\
        .execute_test().command_is_successful().command_response_should_not_have("WRITE")

    JClientTest('Jclient can download 3k file').get_object("seagatebucket", "3kfile").execute_test().command_is_successful().command_created_file("3kfile")

    JClientTest('Jclient can download 3k file uploaded in chunked mode').get_object("seagatebucket", "3kfilec").execute_test().command_is_successful().command_created_file("3kfilec")

    JClientTest('Jclient cannot download nonexistent file').get_object("seagatebucket", "nonexistent").execute_test(negative_case=True).command_should_fail().command_error_should_have("The specified key does not exist")

    JClientTest('Jclient cannot download file in nonexistent bucket').get_object("seagate-bucket", "nonexistent").execute_test(negative_case=True).command_should_fail().command_error_should_have("The specified bucket does not exist")

    # ************ 8k FILE TEST ************
    JClientTest('Jclient can upload 8k file').put_object("seagatebucket", "8kfile", 8192).execute_test().command_is_successful()

    JClientTest('Jclient can upload 8k file in chunked mode').put_object("seagatebucket", "8kfilec", 8192).execute_test().command_is_successful()

    JClientTest('Jclient can download 8k file').get_object("seagatebucket", "8kfile").execute_test().command_is_successful().command_created_file("8kfile")

    JClientTest('Jclient can download 8k file uploaded in chunked mode').get_object("seagatebucket", "8kfilec").execute_test().command_is_successful().command_created_file("8kfilec")

    # ************ OBJECT LISTING TEST ************
    JClientTest('Jclient can list objects').list_objects('seagatebucket').execute_test().command_is_successful().command_response_should_have('3kfile').command_response_should_have('8kfile')

    JClientTest('Jclient cannot list objects for nonexistent bucket').list_objects('seagate-bucket').execute_test(negative_case=True).command_should_fail().command_error_should_have("The specified bucket does not exist")

    JClientTest('Jclient can list specific objects').list_specific_objects('seagatebucket', '3k').execute_test().command_is_successful().command_response_should_have('3kfile').command_response_should_not_have('8kfile')

    # ************ 700K FILE TEST ************
    JClientTest('Jclient can upload 700K file').put_object("seagatebucket", "700Kfile", 716800).execute_test().command_is_successful()

    JClientTest('Jclient can upload 700K file in chunked mode').put_object("seagatebucket", "700Kfilec", 716800, chunked=True).execute_test().command_is_successful()

    JClientTest('Jclient can download 700K file').get_object("seagatebucket", "700Kfile").execute_test().command_is_successful().command_created_file("700Kfile")

    JClientTest('Jclient can download 700K file uploaded in chunked mode').get_object("seagatebucket", "700Kfilec").execute_test().command_is_successful().command_created_file("700Kfilec")

    # ************ 18MB FILE TEST (Without multipart) ************
    JClientTest('Jclient can upload 18MB file').put_object("seagatebucket", "18MBfile", 18000000).execute_test().command_is_successful()

    JClientTest('Jclient can get object acl').get_acl("seagatebucket", "18MBfile").execute_test().command_is_successful().command_response_should_have('C12345: FULL_CONTROL')

    JClientTest('Jclient can delete 18MB file').delete_object("seagatebucket", "18MBfile").execute_test().command_is_successful()

    JClientTest('Jclient should not have object after its delete').list_objects('seagatebucket').execute_test().command_is_successful().command_response_should_not_have('18MBfile')

    # ************ 18MB FILE Multipart Upload TEST ***********
    JClientTest('Jclient can upload 18MB file (multipart)').put_object_multipart("seagatebucket", "18MBfile", 18000000, 15)\
            .execute_test().command_is_successful()

    JClientTest('Jclient can get object acl').get_acl("seagatebucket", "18MBfile").execute_test().command_is_successful().command_response_should_have('C12345: FULL_CONTROL')

    JClientTest('Jclient cannot upload 18MB file (multipart) to nonexistent bucket').put_object_multipart("seagate-bucket", "18MBfile", 18000000, 15)\
            .execute_test(negative_case=True).command_should_fail().command_error_should_have("The specified bucket does not exist")

    JClientTest('Jclient can download 18MB file').get_object("seagatebucket", "18MBfile")\
            .execute_test().command_is_successful().command_created_file("18MBfile")

    JClientTest('Jclient can delete 18MB file').delete_object("seagatebucket", "18MBfile").execute_test().command_is_successful()

    JClientTest('Jclient cannot upload 18MB file (multipart) in chunked mode to nonexistent bucket')\
            .put_object_multipart("seagate-bucket", "18MBfilec", 18000000, 15, chunked=True)\
            .execute_test(negative_case=True).command_should_fail().command_error_should_have("The specified bucket does not exist")

    JClientTest('Jclient can upload 18MB file (multipart) in chunked mode').put_object_multipart("seagatebucket", "18MBfilec", 18000000, 15, chunked=True)\
            .execute_test().command_is_successful()

    JClientTest('Jclient can get object acl').get_acl("seagatebucket", "18MBfilec").execute_test().command_is_successful().command_response_should_have('C12345: FULL_CONTROL')


    JClientTest('Jclient can download 18MB file uploaded in chunked mode').get_object("seagatebucket", "18MBfilec")\
            .execute_test().command_is_successful().command_created_file("18MBfilec")

    # Partial multipart upload tests
    JClientTest('Jclient cannot list parts of multipart upload on nonexistent object.').list_parts("seagatebucket", "INVALID.file", "UPLOAD-ID")\
            .execute_test(negative_case=True).command_should_fail().command_error_should_have("NoSuchUpload")

    JClientTest('Jclient cannot upload partial parts to nonexistent bucket.').partial_multipart_upload("seagate-bucket", "18MBfile", 18000000, 1, 2)\
            .execute_test(negative_case=True).command_should_fail().command_error_should_have("The specified bucket does not exist")

    JClientTest('Jclient can upload partial parts to test abort and list multipart.').partial_multipart_upload("seagatebucket", "18MBfile", 18000000, 1, 2)\
            .execute_test().command_is_successful()

    JClientTest('Jclient cannot upload partial parts when partial upload is already in progress.').partial_multipart_upload("seagatebucket", "18MBfile", 18000000, 1, 2)\
            .execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidObjectState")

    JClientTest('Jclient cannot list all multipart uploads on nonexistent bucket.').list_multipart("seagate-bucket")\
            .execute_test(negative_case=True).command_should_fail().command_error_should_have("The specified bucket does not exist")

    result = JClientTest('Jclient can list all multipart uploads.').list_multipart("seagatebucket").execute_test()
    result.command_response_should_have('18MBfile')

    upload_id = result.status.stdout.split("id - ")[1]
    print(upload_id)

    JClientTest('Jclient cannot list parts of multipart upload on invalid bucket.').list_parts("seagate-bucket", "18MBfile", upload_id)\
            .execute_test(negative_case=True).command_should_fail().command_error_should_have("The specified bucket does not exist")

    JClientTest('Jclient cannot list parts of multipart upload on invalid object.').list_parts("seagatebucket", "INVALID.file", upload_id)\
            .execute_test(negative_case=True).command_should_fail().command_error_should_have("NoSuchUpload")

    JClientTest('Jclient cannot list parts of multipart upload on invalid upload-id.').list_parts("seagatebucket", "18MBfile", "INVALID-UPLOAD-ID")\
            .execute_test(negative_case=True).command_should_fail().command_error_should_have("NoSuchUpload")

    result = JClientTest('Jclient can list parts of multipart upload.').list_parts("seagatebucket", "18MBfile", upload_id).execute_test()
    result.command_response_should_have("part number - 1").command_response_should_have("part number - 2")

    JClientTest('Jclient cannot abort multipart upload on invalid bucket.').abort_multipart("seagate-bucket", "18MBfile", upload_id)\
            .execute_test(negative_case=True).command_should_fail().command_error_should_have("The specified bucket does not exist")

    JClientTest('Jclient cannot abort multipart upload on invalid object.').abort_multipart("seagatebucket", "INVALID.file", upload_id)\
            .execute_test(negative_case=True).command_should_fail()

    JClientTest('Jclient cannot abort multipart upload on invalid upload_id').abort_multipart("seagatebucket", "18MBfile", "INVALID-UPLOAD-ID")\
            .execute_test(negative_case=True).command_should_fail().command_error_should_have("NoSuchUpload")

    JClientTest('Jclient can abort multipart upload').abort_multipart("seagatebucket", "18MBfile", upload_id)\
            .execute_test().command_is_successful()

    JClientTest('Jclient can get object acl').get_acl("seagatebucket", "18MBfile").execute_test(negative_case=True).command_should_fail().command_error_should_have('No such object')

    JClientTest('Jclient can test the multipart was aborted.').list_multipart('seagatebucket')\
            .execute_test().command_is_successful().command_response_should_not_have('18MBfile')

    # ************ DELETE OBJECT TEST ************
    JClientTest('Jclient can delete 3k file').delete_object("seagatebucket", "3kfile").execute_test().command_is_successful()

    JClientTest('Jclient should not have object after its deletion').list_objects('seagatebucket').execute_test().command_is_successful().command_response_should_not_have('3kfile ')

    JClientTest('Jclient cannot delete file in nonexistent bucket').delete_object("seagate-bucket", "3kfile").execute_test(negative_case=True).command_should_fail().command_error_should_have("The specified bucket does not exist")

    JClientTest('Jclient can delete nonexistent file').delete_object("seagatebucket", "3kfile").execute_test().command_is_successful()

    # ************ DELETE MULTIPLE OBJECTS TEST ************
    JClientTest('Jclient can delete 8k, 700k and 18MB files and non existent 1MB file').delete_multiple_objects("seagatebucket", ["8kfile", "700Kfile", "18MBfile", "1MBfile", "3kfilec", "8kfilec", "700Kfilec", "18MBfilec"]).execute_test().command_is_successful()

    JClientTest('Jclient should not list deleted objects').list_objects('seagatebucket').execute_test().command_is_successful().command_response_should_not_have('8kfile').command_response_should_not_have('700Kfile').command_response_should_not_have('18MBfile').command_response_should_not_have('3kfilec').command_response_should_not_have('8kfilec').command_response_should_not_have('700Kfilec').command_response_should_not_have('18MBfilec')

    JClientTest('Jclient multiple delete should succeed when objects not present').delete_multiple_objects("seagatebucket", ["8kfile", "700Kfile", "18MBfile"]).execute_test().command_is_successful()

    JClientTest('Jclient cannot delete multiple files when bucket does not exists').delete_multiple_objects("seagate-bucket", ["8kfile", "700Kfile", "18MBfile", "1MBfile"]).execute_test(negative_case=True).command_should_fail().command_error_should_have("The specified bucket does not exist")

    # ************ Bucket Policy Tests ************
    JClientTest('Jclient can get bucket policy').get_bucket_policy("seagatebucket").execute_test().command_is_successful().command_response_should_have("Bucket policy not found")

    JClientTest('Jclient can set bucket policy').set_bucket_policy("seagatebucket", "../policy.txt").execute_test().command_is_successful().command_response_should_have("Bucket policy set successfully")

    #JClientTest('Jclient can get bucket policy').get_bucket_policy("seagatebucket").execute_test().command_is_successful().command_response_should_have("s3:ListBucket").command_response_should_have("Allow")

    #JClientTest('Jclient can delete bucket policy').delete_bucket_policy("seagatebucket").execute_test().command_is_successful().command_response_should_have("Successfully deleted bucket policy")

    #JClientTest('Jclient can not set bucket policy on non existent bucket').set_bucket_policy("seagate-bucket", "../policy.txt").execute_test(negative_case=True).command_should_fail().command_error_should_have("No such bucket")

    #JClientTest('Jclient can not get bucket policy for non existent bucket').get_bucket_policy("seagate-bucket").execute_test(negative_case=True).command_should_fail().command_error_should_have("No such bucket")

    #JClientTest('Jclient can not delete bucket policy of non existent bucket').delete_bucket_policy("seagate-bucket").execute_test(negative_case=True).command_should_fail().command_error_should_have("No such bucket")

    # ************ Delete bucket TEST ************
    JClientTest('Jclient can delete bucket').delete_bucket("seagatebucket").execute_test().command_is_successful()

    JClientTest('Jclient should not have bucket after its deletion').list_buckets().execute_test().command_is_successful().command_response_should_not_have('seagatebucket')

    JClientTest('Jclient cannot delete nonexistent bucket').delete_bucket("seagatebucket").execute_test(negative_case=True).command_should_fail().command_error_should_have("The specified bucket does not exist")

    # ************ try to delete empty secondary account ************
    S3ClientConfig.access_key_id = s3secondaccount_response_elements['AccessKeyId']
    S3ClientConfig.secret_key = s3secondaccount_response_elements['SecretKey']
    test_msg = "Jclient can delete empty account s3secondaccount"
    account_args = {'AccountName': 's3secondaccount'}
    AuthTest(test_msg).delete_account(**account_args).execute_test(negative_case=True)\
        .command_response_should_have("Account deleted successfully")

    # restore default access key and secret key.
    S3ClientConfig.access_key_id = 'AKIAJPINPFRBTPAYOGNA'
    S3ClientConfig.secret_key = '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'

    # ********** Tests to verify objects with special characters
    JClientTest('JClient can create bucket').create_bucket("seagatebucket").execute_test().command_is_successful()
    JClientTest('JClient can upload objec+t file').put_object("seagatebucket", "objec+t", 0).execute_test().command_is_successful()
    JClientTest('JClient can list objects').list_specific_objects('seagatebucket','objec+').execute_test().command_is_successful().command_response_should_have('objec+t')
    JClientTest('JClient can download objec+t file').get_object("seagatebucket", "objec+t").execute_test().command_is_successful().command_created_file("objec+t")
    JClientTest('JClient can delete objec+t file').delete_object("seagatebucket", "objec+t").execute_test().command_is_successful()
    JClientTest('JClient can upload objec@t file').put_object("seagatebucket", "objec@t", 0).execute_test().command_is_successful()
    JClientTest('JClient can list objects').list_specific_objects('seagatebucket','objec@').execute_test().command_is_successful().command_response_should_have('objec@t')
    JClientTest('JClient can download objec@t file').get_object("seagatebucket", "objec@t").execute_test().command_is_successful().command_created_file("objec@t")
    JClientTest('JClient can delete objec@t file').delete_object("seagatebucket", "objec@t").execute_test().command_is_successful()
    JClientTest('JClient can upload objec,t file').put_object("seagatebucket", "objec,t", 0).execute_test().command_is_successful()
    JClientTest('JClient can list objects').list_specific_objects('seagatebucket','objec,').execute_test().command_is_successful().command_response_should_have('objec,t')
    JClientTest('JClient can download objec,t file').get_object("seagatebucket", "objec,t").execute_test().command_is_successful().command_created_file("objec,t")
    JClientTest('JClient can delete objec,t file').delete_object("seagatebucket", "objec,t").execute_test().command_is_successful()
    JClientTest('JClient can upload objec:t file').put_object("seagatebucket", "objec:t", 0).execute_test().command_is_successful()
    JClientTest('JClient can list objects').list_specific_objects('seagatebucket','objec:').execute_test().command_is_successful().command_response_should_have('objec:t')
    JClientTest('JClient can download objec:t file').get_object("seagatebucket", "objec:t").execute_test().command_is_successful().command_created_file("objec:t")
    JClientTest('JClient can delete objec:t file').delete_object("seagatebucket", "objec:t").execute_test().command_is_successful()
    JClientTest('JClient can upload objec;t file').put_object("seagatebucket", "objec;t", 0).execute_test().command_is_successful()
    JClientTest('JClient can list objects').list_specific_objects('seagatebucket','objec;').execute_test().command_is_successful().command_response_should_have('objec;t')
    JClientTest('JClient can download objec;t file').get_object("seagatebucket", "objec;t").execute_test().command_is_successful().command_created_file("objec;t")
    JClientTest('JClient can delete objec;t file').delete_object("seagatebucket", "objec;t").execute_test().command_is_successful()
    JClientTest('JClient can upload objec?t file').put_object("seagatebucket", "objec?t", 0).execute_test().command_is_successful()
    JClientTest('JClient can list objects').list_specific_objects('seagatebucket','objec?').execute_test().command_is_successful().command_response_should_have('objec?t')
    JClientTest('JClient can download objec?t file').get_object("seagatebucket", "objec?t").execute_test().command_is_successful().command_created_file("objec?t")
    JClientTest('JClient can delete objec?t file').delete_object("seagatebucket", "objec?t").execute_test().command_is_successful()
    JClientTest('JClient can upload objec t file').put_object("seagatebucket", "objec&t", 0).execute_test().command_is_successful()
    JClientTest('JClient can list objects').list_specific_objects('seagatebucket','objec&').execute_test().command_is_successful().command_response_should_have('objec&t')
    JClientTest('JClient can download objec&t file').get_object("seagatebucket", "objec&t").execute_test().command_is_successful().command_created_file("objec&t")
    JClientTest('JClient can delete objec&t file').delete_object("seagatebucket", "objec&t").execute_test().command_is_successful()
    JClientTest('JClient can upload objec t file').put_object("seagatebucket", "objec t", 0).execute_test().command_is_successful()
    JClientTest('JClient can list objects').list_specific_objects('seagatebucket','objec ').execute_test().command_is_successful().command_response_should_have('objec t')
    JClientTest('JClient can download objec t file').get_object("seagatebucket", "objec t").execute_test().command_is_successful().command_created_file("objec t")
    JClientTest('JClient can delete objec t file').delete_object("seagatebucket", "objec t").execute_test().command_is_successful()
    JClientTest('JClient can delete bucket').delete_bucket("seagatebucket").execute_test().command_is_successful()

    #******************Multipart upload and list part for object names with special characters**************************************
    JClientTest('JClient can create bucket').create_bucket("seagate.bucket").execute_test().command_is_successful()

    JClientTest('Jclient can upload tes@t file with 18MB (multipart)').put_object_multipart("seagate.bucket", "tes@t", 18000000, 15)\
            .execute_test().command_is_successful()
    JClientTest('Jclient can upload partial parts to test abort and list multipart.').partial_multipart_upload("seagate.bucket", "tes@t", 18000000, 1, 2)\
            .execute_test().command_is_successful()
    result = JClientTest('Jclient can list all multipart uploads.').list_multipart("seagate.bucket").execute_test()
    result.command_response_should_have('tes@t')
    upload_id = result.status.stdout.split("id - ")[1]
    result = JClientTest('Jclient can list parts of multipart upload.').list_parts("seagate.bucket", "tes@t", upload_id).execute_test()
    result.command_response_should_have("part number - 1").command_response_should_have("part number - 2")
    JClientTest('Jclient can abort multipart upload').abort_multipart("seagate.bucket", "tes@t", upload_id)\
            .execute_test().command_is_successful()
    JClientTest('Jclient can delete object').delete_object("seagate.bucket", "tes@t").execute_test().command_is_successful()
    JClientTest('Jclient can upload tes.t file with 18MB (multipart)').put_object_multipart("seagate.bucket", "tes.t", 18000000, 15)\
            .execute_test().command_is_successful()
    JClientTest('Jclient can upload partial parts to test abort and list multipart.').partial_multipart_upload("seagate.bucket", "tes.t", 18000000, 1, 2)\
            .execute_test().command_is_successful()
    result = JClientTest('Jclient can list all multipart uploads.').list_multipart("seagate.bucket").execute_test()
    result.command_response_should_have('tes.t')
    upload_id = result.status.stdout.split("id - ")[1]
    result = JClientTest('Jclient can list parts of multipart upload.').list_parts("seagate.bucket", "tes.t", upload_id).execute_test()
    result.command_response_should_have("part number - 1").command_response_should_have("part number - 2")
    JClientTest('Jclient can abort multipart upload').abort_multipart("seagate.bucket", "tes.t", upload_id)\
            .execute_test().command_is_successful()
    JClientTest('Jclient can delete object').delete_object("seagate.bucket", "tes.t").execute_test().command_is_successful()
    JClientTest('Jclient can upload tes,t file with 18MB (multipart)').put_object_multipart("seagate.bucket", "tes,t", 18000000, 15)\
            .execute_test().command_is_successful()
    JClientTest('Jclient can upload partial parts to test abort and list multipart.').partial_multipart_upload("seagate.bucket", "tes,t", 18000000, 1, 2)\
            .execute_test().command_is_successful()
    result = JClientTest('Jclient can list all multipart uploads.').list_multipart("seagate.bucket").execute_test()
    result.command_response_should_have('tes,t')
    upload_id = result.status.stdout.split("id - ")[1]
    result = JClientTest('Jclient can list parts of multipart upload.').list_parts("seagate.bucket", "tes,t", upload_id).execute_test()
    result.command_response_should_have("part number - 1").command_response_should_have("part number - 2")
    JClientTest('Jclient can abort multipart upload').abort_multipart("seagate.bucket", "tes,t", upload_id)\
            .execute_test().command_is_successful()
    JClientTest('Jclient can delete object').delete_object("seagate.bucket", "tes,t").execute_test().command_is_successful()
    JClientTest('Jclient can upload tes:t file with 18MB (multipart)').put_object_multipart("seagate.bucket", "tes:t", 18000000, 15)\
            .execute_test().command_is_successful()
    JClientTest('Jclient can upload partial parts to test abort and list multipart.').partial_multipart_upload("seagate.bucket", "tes:t", 18000000, 1, 2)\
            .execute_test().command_is_successful()
    result = JClientTest('Jclient can list all multipart uploads.').list_multipart("seagate.bucket").execute_test()
    result.command_response_should_have('tes:t')
    upload_id = result.status.stdout.split("id - ")[1]
    result = JClientTest('Jclient can list parts of multipart upload.').list_parts("seagate.bucket", "tes:t", upload_id).execute_test()
    result.command_response_should_have("part number - 1").command_response_should_have("part number - 2")
    JClientTest('Jclient can abort multipart upload').abort_multipart("seagate.bucket", "tes:t", upload_id)\
            .execute_test().command_is_successful()
    JClientTest('Jclient can delete object').delete_object("seagate.bucket", "tes:t").execute_test().command_is_successful()
    JClientTest('Jclient can upload tes?t file with 18MB (multipart)').put_object_multipart("seagate.bucket", "tes?t", 18000000, 15)\
            .execute_test().command_is_successful()
    JClientTest('Jclient can upload partial parts to test abort and list multipart.').partial_multipart_upload("seagate.bucket", "tes?t", 18000000, 1, 2)\
            .execute_test().command_is_successful()
    result = JClientTest('Jclient can list all multipart uploads.').list_multipart("seagate.bucket").execute_test()
    result.command_response_should_have('tes?t')
    upload_id = result.status.stdout.split("id - ")[1]
    result = JClientTest('Jclient can list parts of multipart upload.').list_parts("seagate.bucket", "tes?t", upload_id).execute_test()
    result.command_response_should_have("part number - 1").command_response_should_have("part number - 2")
    JClientTest('Jclient can abort multipart upload').abort_multipart("seagate.bucket", "tes?t", upload_id)\
            .execute_test().command_is_successful()
    JClientTest('Jclient can delete object').delete_object("seagate.bucket", "tes?t").execute_test().command_is_successful()
    JClientTest('Jclient can upload tes+t file with 18MB (multipart)').put_object_multipart("seagate.bucket", "tes+t", 18000000, 15)\
            .execute_test().command_is_successful()
    JClientTest('Jclient can upload partial parts to test abort and list multipart.').partial_multipart_upload("seagate.bucket", "tes+t", 18000000, 1, 2)\
            .execute_test().command_is_successful()
    result = JClientTest('Jclient can list all multipart uploads.').list_multipart("seagate.bucket").execute_test()
    result.command_response_should_have('tes+t')
    upload_id = result.status.stdout.split("id - ")[1]
    result = JClientTest('Jclient can list parts of multipart upload.').list_parts("seagate.bucket", "tes+t", upload_id).execute_test()
    result.command_response_should_have("part number - 1").command_response_should_have("part number - 2")
    JClientTest('Jclient can abort multipart upload').abort_multipart("seagate.bucket", "tes+t", upload_id)\
            .execute_test().command_is_successful()
    JClientTest('Jclient can delete object').delete_object("seagate.bucket", "tes+t").execute_test().command_is_successful()
    JClientTest('Jclient can call list objects on empty bucket').list_objects('seagate.bucket').execute_test().command_is_successful()
    JClientTest('JClient can delete bucket').delete_bucket("seagate.bucket").execute_test().command_is_successful()

    # ************ Listing with prefix ************
    JClientTest('Jclient can create bucket seagatebucket').create_bucket("seagatebucket").execute_test().command_is_successful()
    JClientTest('Jclient can upload a/3kfile file').put_object("seagatebucket", "3kfile", 3000, prefix="a").execute_test().command_is_successful()
    JClientTest('Jclient can upload b/3kfile file').put_object("seagatebucket", "3kfile", 3000, prefix="b").execute_test().command_is_successful()
    JClientTest('Jclient can list specific objects with prefix a/').list_specific_objects('seagatebucket', 'a/').execute_test().command_is_successful().command_response_should_have('a/3kfile').command_response_should_not_have('b/3kfile')
    JClientTest('Jclient can list specific objects with prefix b/').list_specific_objects('seagatebucket', 'b/').execute_test().command_is_successful().command_response_should_have('b/3kfile').command_response_should_not_have('a/3kfile')
    JClientTest('Jclient can delete a/3kfile, b/3kfile file').delete_multiple_objects("seagatebucket", ["a/3kfile", "b/3kfile"]).execute_test().command_is_successful()
    JClientTest('Jclient can delete bucket').delete_bucket("seagatebucket").execute_test().command_is_successful()

    # ************ Delete bucket even if parts are present(multipart) ************
    JClientTest('Jclient can create bucket seagatebucket').create_bucket("seagatebucket").execute_test().command_is_successful()
    JClientTest('Jclient can upload partial parts to test abort and list multipart.').partial_multipart_upload("seagatebucket", "18MBfile", 18000000, 1, 2).execute_test().command_is_successful()
    JClientTest('Jclient can delete bucket even if parts are present').delete_bucket("seagatebucket").execute_test().command_is_successful()

    # ************ Signing algorithm test ************
    JClientTest('Jclient can create bucket seagate-bucket').create_bucket("seagate-bucket").execute_test().command_is_successful()
    JClientTest('Jclient can create bucket seagatebucket123').create_bucket("seagatebucket123").execute_test().command_is_successful()
    JClientTest('Jclient can create bucket seagate.bucket').create_bucket("seagate.bucket").execute_test().command_is_successful()
    JClientTest('Jclient can delete bucket seagate-bucket').delete_bucket("seagate-bucket").execute_test().command_is_successful()
    JClientTest('Jclient can delete bucket seagatebucket123').delete_bucket("seagatebucket123").execute_test().command_is_successful()
    JClientTest('Jclient can delete bucket seagate.bucket').delete_bucket("seagate.bucket").execute_test().command_is_successful()
    JClientTest('Jclient should not list bucket after its deletion').list_buckets().execute_test().command_is_successful().command_response_should_not_have('seagatebucket').command_response_should_not_have('seagatebucket123').command_response_should_not_have('seagate.bucket').command_response_should_not_have('seagate-bucket')

    # ************ TEST: INIT-MPU && PARTIAL-MPU ***********
    JClientTest('Jclient can create bucket seagatebucket').create_bucket("seagatebucket").execute_test().command_is_successful()

    JClientTest('Jclient can initiate multpart upload').init_mpu("seagatebucket", "18MBfile", 18000000)\
            .execute_test().command_is_successful()

    result = JClientTest('Jclient can list all multipart uploads.').list_multipart("seagatebucket").execute_test()
    result.command_response_should_have('18MBfile')

    upload_id = result.status.stdout.split("18MBfile, Upload id - ")[1].strip()
    print("UPLOAD-ID: ", upload_id)

    JClientTest('JClient cannot upload other parts before uploading part-1')\
            .partial_multipart_upload("seagatebucket", "18MBfile", 18000000, 1, 1, with_upload_id=upload_id, from_part=2)\
            .execute_test(negative_case=True).command_should_fail().command_error_should_have("Partial upload failed")

    JClientTest('JClient can upload part-1')\
            .partial_multipart_upload("seagatebucket", "18MBfile", 18000000, 1, 1, with_upload_id=upload_id)\
            .execute_test().command_is_successful().command_response_should_have("Partial upload successful")

    JClientTest('JClient can upload part-1')\
            .partial_multipart_upload("seagatebucket", "18MBfile", 18000000, 1, 1, with_upload_id=upload_id)\
            .execute_test().command_is_successful().command_response_should_have("Partial upload successful")

    JClientTest('JClient can upload part-3')\
            .partial_multipart_upload("seagatebucket", "18MBfile", 18000000, 1, 1, with_upload_id=upload_id, from_part=3)\
            .execute_test().command_is_successful().command_response_should_have("Partial upload successful")

    JClientTest('JClient can upload part-4-5-6')\
            .partial_multipart_upload("seagatebucket", "18MBfile", 18000000, 1, 3, with_upload_id=upload_id, from_part=4)\
            .execute_test().command_is_successful().command_response_should_have("Partial upload successful")

    JClientTest('Jclient can abort multipart upload').abort_multipart("seagatebucket", "18MBfile", upload_id)\
            .execute_test().command_is_successful()

    JClientTest('Jclient can test the multipart was aborted.').list_multipart('seagatebucket')\
            .execute_test().command_is_successful().command_response_should_not_have('18MBfile')

    JClientTest('Jclient can initiate multpart upload').init_mpu("seagatebucket", "FILE_L0", 18000000)\
            .execute_test().command_is_successful()

    JClientTest('Jclient can initiate multpart upload').init_mpu("seagatebucket/L1/", "FILE_L1_A", 18000000)\
            .execute_test().command_is_successful()

    JClientTest('Jclient can initiate multpart upload').init_mpu("seagatebucket/L1/", "FILE_L1_B", 18000000)\
            .execute_test().command_is_successful()

    JClientTest('Jclient can initiate multpart upload').init_mpu("seagatebucket/L1/L2/", "FILE_L2_C", 18000000)\
            .execute_test().command_is_successful()

    JClientTest('Jclient can initiate multpart upload').init_mpu("seagatebucket/L1/L2/", "FILE_L2_D", 18000000)\
            .execute_test().command_is_successful()

    result = JClientTest('Jclient can list all multipart uploads.').list_multipart("seagatebucket").execute_test()
    uid_0 = result.status.stdout.split("FILE_L0, Upload id - ")[1].strip()
    uid_a = result.status.stdout.split("FILE_L1_A, Upload id - ")[1].strip()
    uid_b = result.status.stdout.split("FILE_L1_B, Upload id - ")[1].strip()
    uid_c = result.status.stdout.split("FILE_L2_C, Upload id - ")[1].strip()
    uid_d = result.status.stdout.split("FILE_L2_D, Upload id - ")[1].strip()

    JClientTest('Jclient can list 3 multipart uploads.')\
            .list_multipart("seagatebucket", max_uploads=3).execute_test().command_is_successful()\
            .command_response_should_have("FILE_L0")\
            .command_response_should_have("L1/FILE_L1_A").command_response_should_have("L1/FILE_L1_B")\
            .command_response_should_not_have("L1/L2/FILE_L2_C").command_response_should_not_have("L1/L2/FILE_L2_D")

    JClientTest('Jclient can list multipart uploads with prefix.')\
            .list_multipart("seagatebucket", prefix="L1/L2/").execute_test().command_is_successful()\
            .command_response_should_have("L1/L2/FILE_L2_C").command_response_should_have("L1/L2/FILE_L2_D")\
            .command_response_should_not_have("L1/FILE_L1_A").command_response_should_not_have("L1/FILE_L1_B")\
            .command_response_should_not_have("FILE_L0")

    JClientTest('Jclient can list multipart uploads with delimiter.')\
            .list_multipart("seagatebucket", delimiter="/").execute_test().command_is_successful()\
            .command_response_should_have("FILE_L0").command_response_should_have("CommonPrefix - L1/")\
            .command_response_should_not_have("L1/FILE_L1_A").command_response_should_not_have("L1/FILE_L1_B")\
            .command_response_should_not_have("L1/L2/FILE_L2_C").command_response_should_not_have("L1/L2/FILE_L2_D")

    JClientTest('Jclient can list multipart uploads with prefix and delimiter.')\
            .list_multipart("seagatebucket", prefix="L1/L2/", delimiter="/").execute_test().command_is_successful()\
            .command_response_should_have("L1/L2/FILE_L2_C").command_response_should_have("L1/L2/FILE_L2_D")\
            .command_response_should_not_have("L1/FILE_L1_A").command_response_should_not_have("L1/FILE_L1_B")\
            .command_response_should_not_have("CommonPrefix")

    JClientTest('Jclient can list multipart uploads with max-uploads && prefix')\
            .list_multipart("seagatebucket", prefix="L1/", delimiter="/", max_uploads=1).execute_test().command_is_successful()\
            .command_response_should_have("L1/FILE_L1_A").command_response_should_not_have("L1/FILE_L1_B")\
            .command_response_should_not_have("L1/L2/FILE_L2_C").command_response_should_not_have("L1/L2/FILE_L2_D")\
            .command_response_should_not_have("FILE_L0")

    JClientTest('Jclient can list multipart uploads with max-uploads, prefix and delimiter.')\
            .list_multipart("seagatebucket", prefix="L1/", delimiter="/", max_uploads=1).execute_test().command_is_successful()\
            .command_response_should_have("L1/FILE_L1_A").command_response_should_not_have("L1/FILE_L1_B")\
            .command_response_should_not_have("L1/L2/FILE_L2_C").command_response_should_not_have("L1/L2/FILE_L2_D")\
            .command_response_should_not_have("FILE_L0")

    JClientTest('Jclient can list multipart uploads with max-uploads ')\
            .list_multipart("seagatebucket", max_uploads=1, show_next=True).execute_test().command_is_successful()\
            .command_response_should_have("L1/FILE_L1_A").command_response_should_have("L1/FILE_L1_B")\
            .command_response_should_have("L1/L2/FILE_L2_C").command_response_should_have("L1/L2/FILE_L2_D")\
            .command_response_should_have("FILE_L0")

    JClientTest('Jclient can abort multipart upload').abort_multipart("seagatebucket", "FILE_L0", uid_0)\
            .execute_test().command_is_successful()

    JClientTest('Jclient can abort multipart upload').abort_multipart("seagatebucket/L1", "FILE_L1_A", uid_a)\
            .execute_test().command_is_successful()

    JClientTest('Jclient can abort multipart upload').abort_multipart("seagatebucket/L1", "FILE_L1_B", uid_b)\
            .execute_test().command_is_successful()

    JClientTest('Jclient can abort multipart upload').abort_multipart("seagatebucket/L1/L2", "FILE_L2_C", uid_c)\
            .execute_test().command_is_successful()

    JClientTest('Jclient can abort multipart upload').abort_multipart("seagatebucket/L1/L2", "FILE_L2_D", uid_d)\
            .execute_test().command_is_successful()

    JClientTest('Jclient can delete bucket').delete_bucket("seagatebucket").execute_test().command_is_successful()


# Add tests which are specific to Path style APIs

S3ClientConfig.pathstyle = True

# ************ Signing algorithm test ************
# /etc/hosts should not contains nondnsbucket. This is to test the path style APIs.
JClientTest('Jclient can create bucket nondnsbucket').create_bucket("nondnsbucket").execute_test().command_is_successful()
JClientTest('Jclient can delete bucket nondnsbucket').delete_bucket("nondnsbucket").execute_test().command_is_successful()

