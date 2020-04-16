#!/usr/bin/python3.6

from framework import Config
from framework import S3PyCliTest
from s3cmd import S3cmdTest
from s3fi import S3fiTest
from jclient import JClientTest
from s3client_config import S3ClientConfig
from s3kvstool import S3kvTest
import s3kvs
import yaml

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

    # Create bucket list index failure when creating bucket
    S3fiTest('s3cmd enable FI create index fail').enable_fi("enable", "always", "clovis_idx_create_fail").execute_test().command_is_successful()
    S3cmdTest('s3cmd cannot create bucket').create_bucket("seagatebucket").execute_test(negative_case=True).command_should_fail().command_error_should_have("InternalError")
    S3fiTest('s3cmd disable Fault injection').disable_fi("clovis_idx_create_fail").execute_test().command_is_successful()

    # Create object list index failure when creating bucket
    S3fiTest('s3cmd enable FI create index fail').enable_fi_offnonm("enable", "clovis_idx_create_fail", "1", "1").execute_test().command_is_successful()
    S3cmdTest('s3cmd cannot create bucket').create_bucket("seagatebucket").execute_test(negative_case=True).command_should_fail().command_error_should_have("InternalError")
    S3fiTest('s3cmd disable Fault injection').disable_fi("clovis_idx_create_fail").execute_test().command_is_successful()

    # Create multipart list index failure when creating bucket
    S3fiTest('s3cmd enable FI create index fail').enable_fi_offnonm("enable", "clovis_idx_create_fail", "1", "99").execute_test().command_is_successful()
    S3cmdTest('s3cmd cannot create bucket').create_bucket("seagatebucket").execute_test(negative_case=True).command_should_fail().command_error_should_have("InternalError")
    S3fiTest('s3cmd disable Fault injection').disable_fi("clovis_idx_create_fail").execute_test().command_is_successful()


    # ************ Create bucket ************
    S3cmdTest('s3cmd can create bucket').create_bucket("seagatebucket").\
        execute_test().command_is_successful()

    # ************ List buckets ************
    S3cmdTest('s3cmd can list buckets').list_buckets().execute_test().\
        command_is_successful().command_response_should_have('s3://seagatebucket')

    # ************ BUCKET METADATA CORRUPTION TEST ***********
    # Bucket listing shouldn't list corrupted bucket
    S3fiTest('s3cmd enable FI bucket_metadata_corrupted').\
        enable_fi("enable", "always", "bucket_metadata_corrupted").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can not list corrupted bucket metadata').list_buckets().\
        execute_test().command_is_successful().command_response_should_have('')
    S3fiTest('s3cmd can disable FI bucket_metadata_corrupted').\
        disable_fi("bucket_metadata_corrupted").\
        execute_test().command_is_successful()

    # ************ BUCKET METADATA CORRUPTION TEST ***********
    # Bucket listing shouldn't list corrupted bucket
    S3cmdTest('s3cmd can create bucket').create_bucket("seagatebucket123").\
        execute_test().command_is_successful()
    S3fiTest('s3cmd enable FI bucket_metadata_corrupted').\
        enable_fi_enablen("enable", "bucket_metadata_corrupted", "2").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd does not list corrupted bucket').list_buckets().\
        execute_test().command_is_successful().\
        command_response_should_not_have('s3://seagatebucket123').\
        command_response_should_have('s3://seagatebucket')
    S3fiTest('s3cmd can disable FI bucket_metadata_corrupted').\
        disable_fi("bucket_metadata_corrupted").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can delete bucket').delete_bucket("seagatebucket123").\
        execute_test().command_is_successful()

    # If bucket metadata is corrupted then object listing within bucket shall
    # return an error
    S3fiTest('s3cmd enable FI bucket_metadata_corrupted').\
        enable_fi("enable", "always", "bucket_metadata_corrupted").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can not list objects within bucket').list_objects('seagatebucket').\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("InternalError")
    S3fiTest('s3cmd can disable FI bucket_metadata_corrupted').\
        disable_fi("bucket_metadata_corrupted").\
        execute_test().command_is_successful()

    # ************ OBJECT METADATA CORRUPTION TEST ***********
    # Object listing shouldn't list corrupted objects
    S3cmdTest('s3cmd can upload 3K file').\
        upload_test("seagatebucket", "3Kfile", 3000).\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can upload 9K file').\
        upload_test("seagatebucket", "9Kfile", 9000).\
        execute_test().command_is_successful()
    S3fiTest('s3cmd can enable FI object_metadata_corrupted').\
        enable_fi_enablen("enable", "object_metadata_corrupted", "2").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd does not list corrupted objects').list_objects('seagatebucket').\
        execute_test().command_is_successful().\
        command_response_should_not_have('9Kfile').\
        command_response_should_have('3Kfile')
    S3fiTest('s3cmd can disable FI object_metadata_corrupted').\
        disable_fi("object_metadata_corrupted").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can delete 3K file').\
        delete_test("seagatebucket", "3Kfile").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can delete 9K file').\
        delete_test("seagatebucket", "9Kfile").\
        execute_test().command_is_successful()

    # `Get Object` for corrupted object shall return an error
    S3cmdTest('s3cmd can upload 3K file').\
        upload_test("seagatebucket", "3Kfile", 3000).\
        execute_test().command_is_successful()
    S3fiTest('s3cmd can enable FI object_metadata_corrupted').\
        enable_fi("enable", "always", "object_metadata_corrupted").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can not download corrupted object').\
        download_test("seagatebucket", "3Kfile").\
        execute_test(negative_case=True).command_should_fail()
    S3fiTest('s3cmd can disable FI object_metadata_corrupted').\
        disable_fi("object_metadata_corrupted").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can delete 3K file').\
        delete_test("seagatebucket", "3Kfile").\
        execute_test().command_is_successful()

    # clovis_open_entity fails
    S3fiTest('s3cmd can enable FI clovis_enity_open').\
        enable_fi("enable", "always", "clovis_entity_open_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can not upload 18MBfile object').\
        upload_test("seagatebucket", "18MBfile", 18000000).\
        execute_test(negative_case=True).command_should_fail()

    # test for delete bucket with multipart object
    S3cmdTest('s3cmd cannot delete bucket').delete_bucket("seagatebucket").\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")

    S3fiTest('s3cmd can disable FI clovis_enity_open_fail').\
        disable_fi("clovis_entity_open_fail").\
        execute_test().command_is_successful()
    result = S3cmdTest('s3cmd can list multipart uploads in progress').\
             list_multipart_uploads("seagatebucket").execute_test()
    result.command_response_should_have('18MBfile')
    upload_id = result.status.stdout.split('\n')[2].split('\t')[2]

    result = S3cmdTest('S3cmd can list parts of multipart upload.').\
             list_parts("seagatebucket", "18MBfile", upload_id).\
             execute_test().command_is_successful()

    S3cmdTest('S3cmd can abort multipart upload').\
    abort_multipart("seagatebucket", "18MBfile", upload_id).\
    execute_test().command_is_successful()

    #clovis_enity_open failure and chunk upload
    S3fiTest('s3cmd can enable FI clovis_enity_open').\
        enable_fi("enable", "always", "clovis_entity_open_fail").\
        execute_test().command_is_successful()
    JClientTest('Jclient can upload 3k file in chunked mode').\
        put_object("seagatebucket", "3Kfile", 3000, chunked=True).\
        execute_test().command_is_successful()
    S3fiTest('s3cmd can disable FI clovis_enity_open_fail').\
        disable_fi("clovis_entity_open_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can delete 3k file').\
        delete_test("seagatebucket", "3Kfile").\
        execute_test().command_is_successful()



    # clovis_open_entity fails read failure
    S3cmdTest('s3cmd can upload 3K file').\
        upload_test("seagatebucket", "3Kfile", 3000).\
        execute_test().command_is_successful()
    S3fiTest('s3cmd can enable FI clovis_enity_open').\
        enable_fi("enable", "always", "clovis_entity_open_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd cannot download 3k file').\
        download_test("seagatebucket", "3kfile").\
        execute_test(negative_case=True).command_should_fail()
    S3fiTest('s3cmd can disable FI clovis_enity_open_fail').\
        disable_fi("clovis_entity_open_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can delete 3k file').\
        delete_test("seagatebucket", "3Kfile").\
        execute_test().command_is_successful()

   # clovis_idx_op failure
    S3cmdTest('s3cmd can upload 3K file').\
        upload_test("seagatebucket", "3Kfile", 3000).\
        execute_test().command_is_successful()
    S3fiTest('s3cmd can enable FI clovis_idx_op_fail').\
        enable_fi("enable", "always", "clovis_idx_op_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd cannot download 3K file').\
        download_test("seagatebucket", "3Kfile").\
        execute_test(negative_case=True).command_should_fail()
    S3fiTest('s3cmd can disable FI clovis_idx_op_fail').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can delete 3k file').\
        delete_test("seagatebucket", "3Kfile").\
        execute_test().command_is_successful()

    save_max_retry = Config.s3cmd_max_retries
    Config.s3cmd_max_retries = 1
    S3fiTest('s3cmd can enable FI clovis_idx_op_fail').\
        enable_fi("enable", "always", "clovis_idx_op_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can not create bucket').create_bucket("seagatebucket123").\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3cmdTest('s3cmd cannot upload 3K file').\
        upload_test("seagatebucket", "3Kfile", 3000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3cmdTest('s3cmd can not set acl on bucket').\
        setacl_bucket("seagatebucket","read:123").\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3cmdTest('s3cmd can not upload 18MBfile file').\
        upload_test("seagatebucket", "18MBfile", 18000000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3cmdTest('s3cmd can not list buckets').list_buckets().\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    JClientTest('Jclient can not upload 3k file in chunked mode').\
        put_object("seagatebucket", "3Kfile", 3000, chunked=True).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    JClientTest('Jclient can verify object does not exist').\
        head_object("seagatebucket", "3kfile").\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("Service Unavailable")
    S3fiTest('s3cmd can disable FI clovis_idx_op_fail').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()

    # Don't trigger FI first time, then trigger FI next 99 times, then
    # repeat the cycle
    S3fiTest('s3cmd can enable FI clovis_idx_op_fail').\
        enable_fi_offnonm("enable", "clovis_idx_op_fail", "1", "99").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd cannot upload 3K file').\
        upload_test("seagatebucket", "3Kfile", 3000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3cmdTest('s3cmd can not upload 18MBfile file').\
        upload_test("seagatebucket", "18MBfile", 18000000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    JClientTest('Jclient can not upload 3k file in chunked mode').\
        put_object("seagatebucket", "3Kfile", 3000, chunked=True).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable FI clovis_idx_op_fail').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()

    # Don't trigger FI first two times, then trigger FI next 99 times, then
    # repeat the cycle
    S3fiTest('s3cmd can enable FI clovis_idx_op_fail').\
        enable_fi_offnonm("enable", "clovis_idx_op_fail", "2", "99").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can not upload 18MBfile file').\
        upload_test("seagatebucket", "18MBfile", 18000000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable FI clovis_idx_op_fail').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()

    # Don't trigger FI first three times, then trigger FI next 99 times, then
    # repeat the cycle
    S3fiTest('s3cmd can enable FI clovis_idx_op_fail').\
        enable_fi_offnonm("enable", "clovis_idx_op_fail", "3", "99").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can not upload 18MBfile file').\
        upload_test("seagatebucket", "18MBfile", 18000000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable FI clovis_idx_op_fail').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()

    S3fiTest('s3cmd can enable FI clovis_idx_op_fail').\
        enable_fi_offnonm("enable", "clovis_idx_op_fail", "4", "99").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can not upload 18MBfile file').\
        upload_test("seagatebucket", "18MBfile", 18000000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable FI clovis_idx_op_fail').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()

    is_object_leak_track_enabled=yaml.load(open("/opt/seagate/s3/conf/s3config.yaml"))["S3_SERVER_CONFIG"]["S3_SERVER_ENABLE_OBJECT_LEAK_TRACKING"]
    fi_off="5"
    if is_object_leak_track_enabled:
        fi_off="6"
    S3fiTest('s3cmd can enable FI clovis_idx_op_fail').\
        enable_fi_offnonm("enable", "clovis_idx_op_fail", fi_off, "99").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can not upload 18MBfile file').\
        upload_test("seagatebucket", "18MBfile", 18000000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable FI clovis_idx_op_fail').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()
    result = JClientTest('Jclient can list all multipart uploads.').\
                list_multipart("seagatebucket").execute_test()
    result.command_response_should_have('18MBfile')
    upload_id = result.status.stdout.split("id - ")[1]
    JClientTest('Jclient can abort multipart upload').\
        abort_multipart("seagatebucket", "18MBfile", upload_id).\
        execute_test().command_is_successful()

    fi_off="6"
    if is_object_leak_track_enabled:
        fi_off="7"
    S3fiTest('s3cmd can enable FI clovis_idx_op_fail').\
        enable_fi_offnonm("enable", "clovis_idx_op_fail", fi_off, "99").\
        execute_test().command_is_successful()
    # S3PutMultiObjectAction::fetch_multipart_metadata
    S3cmdTest('s3cmd can not upload 18MBfile file').\
        upload_test("seagatebucket", "18MBfile", 18000000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable FI clovis_idx_op_fail').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()
    result = JClientTest('Jclient can list all multipart uploads.').\
                list_multipart("seagatebucket").execute_test()
    result.command_response_should_have('18MBfile')
    upload_id = result.status.stdout.split("id - ")[1]
    JClientTest('Jclient can abort multipart upload').\
        abort_multipart("seagatebucket", "18MBfile", upload_id).\
        execute_test().command_is_successful()

    fi_off="7"
    if is_object_leak_track_enabled:
        fi_off="9"
    S3fiTest('s3cmd can enable FI clovis_idx_op_fail').\
        enable_fi_offnonm("enable", "clovis_idx_op_fail", fi_off, "99").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can not upload 18MBfile file').\
        upload_test("seagatebucket", "18MBfile", 18000000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("InternalError")
    S3fiTest('s3cmd can disable FI clovis_idx_op_fail').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()
    result = JClientTest('Jclient can list all multipart uploads.').\
                list_multipart("seagatebucket").execute_test()
    result.command_response_should_have('18MBfile')
    upload_id = result.status.stdout.split("id - ")[1]
    JClientTest('Jclient can abort multipart upload').\
        abort_multipart("seagatebucket", "18MBfile", upload_id).\
        execute_test().command_is_successful()

    S3fiTest('s3cmd can enable FI clovis_idx_op_fail').\
        enable_fi_offnonm("enable", "clovis_idx_op_fail", "25", "99").\
        execute_test().command_is_successful()
    #Post complete operation -- fetch_multipart_info
    S3cmdTest('s3cmd can not upload 18MBfile file').\
        upload_test("seagatebucket", "18MBfile", 18000000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable FI clovis_idx_op_fail').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()
    result = JClientTest('Jclient can list all multipart uploads.').\
                list_multipart("seagatebucket").execute_test()
    result.command_response_should_have('18MBfile')
    upload_id = result.status.stdout.split("id - ")[1]
    JClientTest('Jclient can abort multipart upload').\
        abort_multipart("seagatebucket", "18MBfile", upload_id).\
        execute_test().command_is_successful()

    S3fiTest('s3cmd can enable FI clovis_idx_op_fail').\
        enable_fi_offnonm("enable", "clovis_idx_op_fail", "26", "99").\
        execute_test().command_is_successful()
    #Post complete operation -- fetch_multipart_info
    S3cmdTest('s3cmd can not upload 18MBfile file').\
        upload_test("seagatebucket", "18MBfile", 18000000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd can disable FI clovis_idx_op_fail').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()
    result = JClientTest('Jclient can list all multipart uploads.').\
                list_multipart("seagatebucket").execute_test()
    result.command_response_should_have('18MBfile')
    upload_id = result.status.stdout.split("id - ")[1]
    JClientTest('Jclient can abort multipart upload').\
        abort_multipart("seagatebucket", "18MBfile", upload_id).\
        execute_test().command_is_successful()

    Config.s3cmd_max_retries = save_max_retry

    # clovis_enity_create fails for object upload
    S3fiTest('s3cmd can enable FI clovis_enity_create').\
        enable_fi("enable", "always", "clovis_entity_create_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can not upload 3K file').\
        upload_test("seagatebucket", "3Kfile", 3000).\
        execute_test(negative_case=True).command_should_fail()
    S3fiTest('s3cmd can disable FI clovis_enity_create').\
        disable_fi("clovis_entity_create_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can upload 3K file').\
        upload_test("seagatebucket", "3Kfile", 3000).\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can delete 3k file').\
        delete_test("seagatebucket", "3Kfile").\
        execute_test().command_is_successful()

    #clovis_enity_create failure and chunk upload
    S3fiTest('s3cmd can enable FI clovis_enity_create').\
        enable_fi("enable", "always", "clovis_entity_create_fail").\
        execute_test().command_is_successful()
    JClientTest('Jclient can not upload 3k file in chunked mode').\
        put_object("seagatebucket", "3Kfile", 3000, chunked=True).\
        execute_test(negative_case=True).command_should_fail()
    S3fiTest('s3cmd can disable FI clovis_enity_create').\
        disable_fi("clovis_entity_create_fail").\
        execute_test().command_is_successful()
    JClientTest('Jclient can upload 3k file in chunked mode').\
        put_object("seagatebucket", "3Kfile", 3000, chunked=True).\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can delete 3k file').\
        delete_test("seagatebucket", "3Kfile").\
        execute_test().command_is_successful()

    # clovis_enity_create failure with multipart object
    S3fiTest('s3cmd can enable FI clovis_enity_create').\
        enable_fi("enable", "always", "clovis_entity_create_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can not upload 18MBfile file').\
        upload_test("seagatebucket", "18MBfile", 18000000).\
        execute_test(negative_case=True).command_should_fail()
    S3fiTest('s3cmd can disable FI clovis_enity_create').\
        disable_fi("clovis_entity_create_fail").\
        execute_test().command_is_successful()

    # clovis_enity_delete fails delete failure
    S3cmdTest('s3cmd can upload 3K file').\
        upload_test("seagatebucket", "3Kfile", 3000).\
        execute_test().command_is_successful()
    S3fiTest('s3cmd can enable FI clovis_enity_delete').\
        enable_fi("enable", "always", "clovis_entity_delete_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can delete 3k file').\
        delete_test("seagatebucket", "3Kfile").\
        execute_test().command_is_successful()
    S3fiTest('s3cmd can disable FI clovis_enity_delete').\
        disable_fi("clovis_entity_delete_fail").\
        execute_test().command_is_successful()

     #clovis_enity_delete failure and chunk upload
    JClientTest('Jclient can upload 3k file in chunked mode').\
        put_object("seagatebucket", "3Kfile", 3000, chunked=True).\
        execute_test().command_is_successful()
    S3fiTest('s3cmd can enable FI clovis_entity_delete').\
        enable_fi("enable", "always", "clovis_entity_delete_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can delete 3k file').\
        delete_test("seagatebucket", "3Kfile").\
        execute_test().command_is_successful()
    S3fiTest('s3cmd can disable FI clovis_entity_delete').\
        disable_fi("clovis_entity_delete_fail").\
        execute_test().command_is_successful()

     # clovis_enity_delete failure with multipart object
    S3cmdTest('s3cmd can upload 18MBfile file').\
        upload_test("seagatebucket", "18MBfile", 18000000).\
        execute_test().command_is_successful()
    S3fiTest('s3cmd can eneble FI clovis_enity_delete').\
        enable_fi("enable", "always", "clovis_entity_delete_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can delete 18MB file').\
        delete_test("seagatebucket", "18MBfile").\
        execute_test().command_is_successful()
    S3fiTest('s3cmd can disable FI clovis_enity_delete').\
        disable_fi("clovis_entity_delete_fail").\
        execute_test().command_is_successful()

    # clovis_enity_create failure for Bucket metadata
    S3fiTest('s3cmd can enable FI clovis_enity_create').\
        enable_fi("enable", "always", "clovis_entity_create_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can not create bucket').create_bucket("seagatebucket123").\
        execute_test(negative_case=True).command_should_fail()
    S3fiTest('s3cmd can disable FI clovis_enity_create').\
        disable_fi("clovis_entity_create_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd does not list corrupted bucket').list_buckets().\
        execute_test().command_is_successful().\
        command_response_should_not_have('s3://seagatebucket123').\
        command_response_should_have('s3://seagatebucket')

    # negative tests cases for put_keyval
    # set and delete policy negative testing
    S3fiTest('s3cmd enable FI clovis idx op fail').\
        enable_fi_offnonm("enable", "clovis_idx_op_fail", "3", "99").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd cannot set acl on bucket').\
        setacl_bucket("seagatebucket","read:123").\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd disable Fault injection').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()
    S3fiTest('s3cmd enable FI clovis idx op fail').\
        enable_fi_offnonm("enable", "clovis_idx_op_fail", "2", "99").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd cannot set policy on bucket').\
        setpolicy_bucket("seagatebucket","policy.txt").\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd disable Fault injection').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can set policy on bucket').\
        setpolicy_bucket("seagatebucket","policy.txt").\
        execute_test().command_is_successful()
    S3fiTest('s3cmd enable FI clovis idx op fail').\
        enable_fi_offnonm("enable", "clovis_idx_op_fail", "2", "99").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd cannot delete policy on bucket').\
        delpolicy_bucket("seagatebucket").\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd disable Fault injection').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()

    is_object_leak_track_enabled=yaml.load(open("/opt/seagate/s3/conf/s3config.yaml"))["S3_SERVER_CONFIG"]["S3_SERVER_ENABLE_OBJECT_LEAK_TRACKING"]
    fi_off="20"
    if is_object_leak_track_enabled:
        fi_off="22"
    S3fiTest('s3cmd enable FI clovis idx op fail').\
        enable_fi_offnonm("enable", "clovis_idx_op_fail", fi_off, "99").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can not upload 18MBfile file').\
        upload_test("seagatebucket", "18MBfile", 18000000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("InternalError")
    S3fiTest('s3cmd disable Fault injection').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()
    result = S3cmdTest('s3cmd can list multipart uploads in progress').\
             list_multipart_uploads("seagatebucket").execute_test()
    result.command_response_should_have('18MBfile')
    upload_id = result.status.stdout.split('\n')[2].split('\t')[2]
    S3cmdTest('S3cmd can abort multipart upload').\
    abort_multipart("seagatebucket", "18MBfile", upload_id).\
    execute_test().command_is_successful()
#    S3cmdTest('s3cmd can delete policy on bucket').\
 #       delpolicy_bucket("seagatebucket").\
  #      execute_test().command_is_successful()

    # object metadata save negative testing
    S3fiTest('s3cmd enable FI clovis idx op fail').\
        enable_fi_offnonm("enable", "clovis_idx_op_fail", "3", "99").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can not upload 3K file').\
        upload_test("seagatebucket", "3Kfile", 3000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd disable Fault injection').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()

    # bucket metadata save negative testing
    S3fiTest('s3cmd enable FI clovis idx op fail').\
        enable_fi_offnonm("enable", "clovis_idx_op_fail", "2", "99").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can not create bucket').create_bucket("seagatebucket123").\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd disable Fault injection').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()

    # multipart object metadata negative test
    S3fiTest('s3cmd enable FI clovis idx op fail').\
        enable_fi_offnonm("enable", "clovis_idx_op_fail", "4", "99").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can not upload 18MBfile file').\
        upload_test("seagatebucket", "18MBfile", 18000000).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd disable Fault injection').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()

    # Multipart listing shall return an error for corrupted object
    JClientTest('Jclient can upload partial parts').\
        partial_multipart_upload("seagatebucket", "18MBfile", 18000000, 1, 2).\
        execute_test().command_is_successful()

    result = JClientTest('Jclient can list all multipart uploads.').\
                list_multipart("seagatebucket").execute_test()
    result.command_response_should_have('18MBfile')
    upload_id = result.status.stdout.split("id - ")[1]
    print(upload_id)

    S3fiTest('s3cmd can enable FI object_metadata_corrupted').\
        enable_fi("enable", "always", "object_metadata_corrupted").\
        execute_test().command_is_successful()
    JClientTest('Jclient can not list multipart uploads of corrupted object').\
        list_parts("seagatebucket", "18MBfile", upload_id).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("InternalError")
    S3fiTest('s3cmd can disable FI object_metadata_corrupted').\
        disable_fi("object_metadata_corrupted").\
        execute_test().command_is_successful()
    JClientTest('Jclient can abort multipart upload').\
        abort_multipart("seagatebucket", "18MBfile", upload_id).\
        execute_test().command_is_successful()

    # negative tests cases for next_keyval
    # bucket deletion negative test
    S3cmdTest('s3cmd can create bucket').create_bucket("seagatebucket123").\
        execute_test().command_is_successful()
    S3fiTest('s3cmd enable FI clovis idx op fail').\
        enable_fi_offnonm("enable", "clovis_idx_op_fail", "2", "99").\
        execute_test().command_is_successful()
    # fetch_first_object_metadata_failed clovis idx op fail
    S3cmdTest('s3cmd can not delete bucket').delete_bucket("seagatebucket123").\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd disable Fault injection').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()
    S3fiTest('s3cmd enable FI clovis idx op fail').\
        enable_fi_offnonm("enable", "clovis_idx_op_fail", "3", "99").\
        execute_test().command_is_successful()
    # fetch_first_multipart_object_metadata_failed clovis idx op fail
    S3cmdTest('s3cmd can not delete bucket').delete_bucket("seagatebucket123").\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd disable Fault injection').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can delete bucket').delete_bucket("seagatebucket123").\
        execute_test().command_is_successful()

    # object list negative test
    S3fiTest('s3cmd enable FI clovis idx op fail').\
        enable_fi_offnonm("enable", "clovis_idx_op_fail", "2", "99").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can not list objects').list_objects('seagatebucket').\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd disable Fault injection').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()

    # list bucket negative test
    S3fiTest('s3cmd enable FI clovis idx op fail').\
        enable_fi("enable", "always", "clovis_idx_op_fail").\
        execute_test().command_is_successful()
    S3cmdTest('s3cmd can not list buckets').list_buckets().\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd disable Fault injection').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()

    # multipart object metadata negative test
    # Multipart listing shall return an error on clovis_idx_op
    JClientTest('Jclient can upload partial parts').\
        partial_multipart_upload("seagatebucket", "18MBfile", 18000000, 1, 2).\
        execute_test().command_is_successful()

    result = JClientTest('Jclient can list all multipart uploads.').\
                list_multipart("seagatebucket").execute_test()
    result.command_response_should_have('18MBfile')
    upload_id = result.status.stdout.split("id - ")[1]
    S3fiTest('s3cmd enable FI clovis idx op fail').\
        enable_fi_offnonm("enable", "clovis_idx_op_fail", "2", "99").\
        execute_test().command_is_successful()
    JClientTest('Jclient can not list multipart uploads of corrupted object').\
        list_parts("seagatebucket", "18MBfile", upload_id).\
        execute_test(negative_case=True).command_should_fail().\
        command_error_should_have("ServiceUnavailable")
    S3fiTest('s3cmd disable Fault injection').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()
    JClientTest('Jclient can abort multipart upload').\
        abort_multipart("seagatebucket", "18MBfile", upload_id).\
        execute_test().command_is_successful()

    # ************ PART METADATA CORRUPTION TEST ***********
    # Multipart listing shouldn't list corrupted parts
    JClientTest('Jclient can upload partial parts').\
        partial_multipart_upload("seagatebucket", "18MBfile", 18000000, 1, 2).\
        execute_test().command_is_successful()

    result = JClientTest('Jclient can list all multipart uploads.').\
                list_multipart("seagatebucket").execute_test()
    result.command_response_should_have('18MBfile')
    upload_id = result.status.stdout.split("id - ")[1]
    print(upload_id)

    S3fiTest('s3cmd can enable FI part_metadata_corrupted').\
        enable_fi_enablen("enable", "part_metadata_corrupted", "2").\
        execute_test().command_is_successful()
    result = JClientTest('Jclient does not list corrupted part').\
        list_parts("seagatebucket", "18MBfile", upload_id).\
        execute_test()
    result.command_response_should_have("part number - 1").\
           command_response_should_not_have("part number - 2")
    S3fiTest('s3cmd can disable FI part_metadata_corrupted').\
        disable_fi("part_metadata_corrupted").\
        execute_test().command_is_successful()
    JClientTest('Jclient can abort multipart upload').\
        abort_multipart("seagatebucket", "18MBfile", upload_id).\
        execute_test().command_is_successful()

    # ************ Delete bucket ************
    S3cmdTest('s3cmd can delete bucket').delete_bucket("seagatebucket").\
        execute_test().command_is_successful()
