#!/usr/bin/python3.6

from framework import Config
from framework import S3PyCliTest
from s3cmd import S3cmdTest
from s3fi import S3fiTest
from jclient import JClientTest
from s3client_config import S3ClientConfig

# Helps debugging
# Config.log_enabled = True
# Config.dummy_run = True
# Config.client_execution_timeout = 300 * 1000
# Config.request_timeout = 300 * 1000
# Config.socket_timeout = 300 * 1000
# Enable retry flag to limit retries on failure
Config.s3cmd_max_retries = 2

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

config_types = ["pathstyle.s3cfg", "virtualhoststyle.s3cfg"]
for i, type in enumerate(config_types):
    Config.config_file = type

    # ************ Create bucket ************
    S3cmdTest('s3cmd can create bucket').create_bucket("seagatebucket").\
        execute_test().command_is_successful()
    # ************ List buckets ************
    S3cmdTest('s3cmd can list buckets').list_buckets().execute_test().\
        command_is_successful().command_response_should_have('s3://seagatebucket')
    # *********** Inform S3 that shutdown tests are in progress ******
    S3fiTest('s3cmd enable FI shutdown_system_tests_in_progress').\
        enable_fi("enable", "always", "shutdown_system_tests_in_progress").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')

    # ************ PUT/GET OBJECT TEST ***********
    S3fiTest('s3cmd enable FI put_object_action_fetch_bucket_info_shutdown_fail').\
        enable_fi("enable", "always", "put_object_action_fetch_bucket_info_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')
    S3cmdTest('s3cmd can not upload 4MB file').\
        upload_test("seagatebucket", "4MBfile", 4000000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable Fault injection').\
        disable_fi("put_object_action_fetch_bucket_info_shutdown_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd should not have objects').list_objects('seagatebucket').\
        execute_test().command_is_successful().\
        command_response_should_not_have('4MBfile')

    S3fiTest('s3cmd enable FI put_object_action_create_object_shutdown_fail').\
        enable_fi("enable", "always", "put_object_action_create_object_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')
    S3cmdTest('s3cmd can not upload 4MB file').\
        upload_test("seagatebucket", "4MBfile", 4000000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable Fault injection').\
        disable_fi("put_object_action_create_object_shutdown_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd should not have objects').list_objects('seagatebucket').\
        execute_test().command_is_successful().\
        command_response_should_not_have('4MBfile')

    S3fiTest('s3cmd enable FI put_object_action_consume_incoming_content_shutdown_fail').\
        enable_fi_enablen("enable", "put_object_action_consume_incoming_content_shutdown_fail", "2").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')
    S3cmdTest('s3cmd can not upload 4MB file').\
        upload_test("seagatebucket", "4MBfile", 4000000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable Fault injection').\
        disable_fi("put_object_action_consume_incoming_content_shutdown_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd should not have objects').list_objects('seagatebucket').\
        execute_test().command_is_successful().\
        command_response_should_not_have('4MBfile')

    S3fiTest('s3cmd enable FI put_object_action_save_metadata_pass').\
        enable_fi("enable", "always", "put_object_action_save_metadata_pass").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')
    S3cmdTest('s3cmd can upload 4MB file').\
        upload_test("seagatebucket", "4MBfile", 4000000).\
        execute_test().command_is_successful()
    S3fiTest('s3cmd can disable Fault injection').\
        disable_fi("put_object_action_save_metadata_pass").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd should list objects').list_objects('seagatebucket').\
        execute_test().command_is_successful().\
        command_response_should_have('s3://seagatebucket/4MBfile')

    S3fiTest('s3cmd enable FI get_object_action_fetch_bucket_info_shutdown_fail').\
        enable_fi("enable", "always", "get_object_action_fetch_bucket_info_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')
    S3cmdTest('s3cmd can not download 4MB file').\
        download_test("seagatebucket", "4MBfile").\
        execute_test(negative_case=True).command_should_fail()
    S3fiTest('s3cmd can disable Fault injection').\
        disable_fi("get_object_action_fetch_bucket_info_shutdown_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can delete 4MB file').\
        delete_test("seagatebucket", "4MBfile").execute_test().\
        command_is_successful()

    # ************ PUT/GET BUCKET TEST ***********
    S3fiTest('s3cmd enable FI put_bucket_action_validate_request_shutdown_fail').\
        enable_fi("enable", "always", "put_bucket_action_validate_request_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')
    S3cmdTest('s3cmd can not create bucket').create_bucket("seagate-bucket").\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable Fault injection').\
        disable_fi("put_bucket_action_validate_request_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')
    S3cmdTest('s3cmd should not list bucket').list_buckets().execute_test().\
        command_is_successful().\
        command_response_should_not_have('s3://seagate-bucket')

    S3cmdTest('s3cmd can upload 3K file').\
        upload_test("seagatebucket", "3Kfile", 3000).\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can upload 9K file').\
        upload_test("seagatebucket", "9Kfile", 9000).\
        execute_test().command_is_successful()
    S3fiTest('s3cmd enable FI get_bucket_action_get_next_objects_shutdown_fail').\
        enable_fi("enable", "always", "get_bucket_action_get_next_objects_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')
    S3cmdTest('s3cmd can not list objects').list_objects('seagatebucket').\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable Fault injection').\
        disable_fi("get_bucket_action_get_next_objects_shutdown_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can delete 3K file').\
        delete_test("seagatebucket", "3Kfile").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can delete 9K file').\
        delete_test("seagatebucket", "9Kfile").\
        execute_test().command_is_successful()

    S3fiTest('s3cmd enable FI get_service_action_get_next_buckets_shutdown_fail').\
        enable_fi("enable", "always", "get_service_action_get_next_buckets_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')
    S3cmdTest('s3cmd can not list buckets').list_buckets().\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable Fault injection').\
        disable_fi("get_service_action_get_next_buckets_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')

    # ************ ACL/POLICY TEST ***************
    S3fiTest('s3cmd enable FI put_bucket_acl_action_fetch_bucket_info_shutdown_fail').\
        enable_fi("enable", "always", "put_bucket_acl_action_fetch_bucket_info_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')
    S3cmdTest('s3cmd can not set acl on bucket').\
        setacl_bucket("seagatebucket","read:123").\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable Fault injection').\
        disable_fi("put_bucket_acl_action_fetch_bucket_info_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')

    S3cmdTest('s3cmd can upload 3k file').\
        upload_test("seagatebucket", "3kfile", 3000).\
        execute_test().command_is_successful()
    S3fiTest('s3cmd enable FI put_object_acl_action_fetch_bucket_info_shutdown_fail').\
        enable_fi("enable", "always", "put_object_acl_action_fetch_bucket_info_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')
    S3cmdTest('s3cmd can not set acl on object').\
        setacl_object("seagatebucket","3kfile", "read:123").\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable Fault injection').\
        disable_fi("put_object_acl_action_fetch_bucket_info_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')
    S3cmdTest('s3cmd can delete 3kfile').\
        delete_test("seagatebucket", "3kfile").\
        execute_test().command_is_successful()

    S3fiTest('s3cmd enable FI put_bucket_policy_action_get_metadata_shutdown_fail').\
        enable_fi("enable", "always", "put_bucket_policy_action_get_metadata_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')
    S3cmdTest('s3cmd can not set policy on bucket').\
        setpolicy_bucket("seagatebucket","policy.txt").\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable Fault injection').\
        disable_fi("put_bucket_policy_action_get_metadata_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')

    # ************ MULTI-PART TEST ***************
    S3fiTest('s3cmd enable FI post_multipartobject_action_create_object_shutdown_fail').\
        enable_fi("enable", "always", "post_multipartobject_action_create_object_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')
    S3cmdTest('s3cmd can not upload 18MB file').\
        upload_test("seagatebucket", "18MBfile1", 18000000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable Fault injection').\
        disable_fi("post_multipartobject_action_create_object_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')

    # S3fiTest('s3cmd enable FI post_multipartobject_action_save_upload_metadata_shutdown_fail').\
    #    enable_fi("enable", "always", "post_multipartobject_action_save_upload_metadata_shutdown_fail").\
    #    execute_test().command_is_successful()
    # S3cmdTest('s3cmd can not upload 18MB file').\
    #    upload_test("seagatebucket", "18MBfile2", 18000000).\
    #    execute_test(negative_case=True).command_should_fail().\
    #    command_error_should_have("ServiceUnavailable")
    # S3fiTest('s3cmd can disable Fault injection').\
    #    disable_fi("post_multipartobject_action_save_upload_metadata_shutdown_fail").\
    #    execute_test().command_is_successful()

    S3fiTest('s3cmd enable FI put_multiobject_action_fetch_bucket_info_shutdown_fail').\
        enable_fi("enable", "always", "put_multiobject_action_fetch_bucket_info_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')
    S3cmdTest('s3cmd can not upload 19MB file').\
        upload_test("seagatebucket", "19MBfile", 19000000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable Fault injection').\
        disable_fi("put_multiobject_action_fetch_bucket_info_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')

    S3fiTest('s3cmd enable FI put_multiobject_action_consume_incoming_content_shutdown_fail').\
        enable_fi_enablen("enable", "put_multiobject_action_consume_incoming_content_shutdown_fail", "2").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')
    S3cmdTest('s3cmd can not upload 17MB file').\
        upload_test("seagatebucket", "17MBfile", 17000000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable Fault injection').\
        disable_fi("put_multiobject_action_consume_incoming_content_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')

    # ************ CHUNK UPLOAD TEST ***************
    S3fiTest('s3cmd enable FI put_multiobject_action_fetch_bucket_info_shutdown_fail').\
        enable_fi("enable", "always", "put_multiobject_action_fetch_bucket_info_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')
    JClientTest('Jclient can not upload 18MB file (multipart) in chunked mode').\
        put_object_multipart("seagatebucket", "18MBfilec", 18000000, 15, chunked=True).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable Fault injection').\
        disable_fi("put_multiobject_action_fetch_bucket_info_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')

    S3fiTest('s3cmd enable FI put_chunk_upload_object_action_consume_incoming_content_shutdown_fail').\
        enable_fi_enablen("enable", "put_chunk_upload_object_action_consume_incoming_content_shutdown_fail", "2").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')
    JClientTest('Jclient can not upload 4MB file in chunked mode').\
        put_object("seagatebucket", "4MBfilec", 4000000, chunked=True).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable Fault injection').\
        disable_fi("put_chunk_upload_object_action_consume_incoming_content_shutdown_fail").\
        execute_test().command_is_successful().\
        command_response_should_not_have('ServiceUnavailable')

    # *********** Done with shutdown tests, Inform S3 ********
    S3fiTest('s3cmd can disable Fault injection').\
        disable_fi("shutdown_system_tests_in_progress").execute_test()\
        .command_is_successful().command_response_should_not_have('ServiceUnavailable')

    # ************ Delete bucket TEST ************
    S3cmdTest('s3cmd can delete bucket').delete_bucket("seagatebucket").\
        execute_test().command_is_successful()
