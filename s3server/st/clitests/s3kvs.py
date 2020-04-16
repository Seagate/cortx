# Helper defines for metadata operations
import sys
import json
import base64
import re
from s3kvstool import S3kvTest, S3OID

class S3LeakRecord(object):
    """
    Represents S3 Probable delete record
    """
    def __init__(self, key=None, jsonLeakRec=None):
        self.key = key
        if (jsonLeakRec):
            self.leak_info = json.loads(jsonLeakRec)
        else:
            self.leak_info = {}

    def get_leak_info(self):
        return self.leak_info

    def get_part_index_oid(self):
        if (self.leak_info):
            return self.leak_info["part_list_idx_oid"]
        else:
            return None

# Find record in record list, return empty string otherwise
def _find_record(fkey, record_list):
    for entry in record_list:
        if fkey in entry:
            return entry
    return ""

# Extract json formatted value from given record
def _find_keyval_json(record):
    record_split = record.split('\n')
    for entry in record_split:
        if entry.startswith("Val:"):
            json_keyval_string = entry[5:]
            return json_keyval_string
    return ""

# Extract account id
def _extract_account_id(json_keyval):
    keyval = json.loads(json_keyval)
    account_id = keyval['account_id']
    return account_id

# Extract object id for bucket/object
def _extract_oid(json_keyval, bucket=True):
    keyval = json.loads(json_keyval)
    sbyteorder = sys.byteorder
    oid_val = S3OID()
    if bucket:
        string_oid = keyval['bucket_list_index_oid']
        oid_list = string_oid.split("-")
        dec_string_oid_hi = base64.b64decode(oid_list[0])
        dec_string_oid_lo = base64.b64decode(oid_list[1])
    else:
        string_oid = keyval['mero_object_list_index_oid']
        oid_list = string_oid.split("-")
        dec_string_oid_hi = base64.b64decode(oid_list[0])
        dec_string_oid_lo = base64.b64decode(oid_list[1])
    int_oid_hi = int.from_bytes(dec_string_oid_hi,byteorder=sbyteorder)
    int_oid_lo = int.from_bytes(dec_string_oid_lo,byteorder=sbyteorder)
    oid_val.set_oid(hex(int_oid_hi), hex(int_oid_lo))
    return oid_val


# Fetch leak record from probable delete index corresponding to key 'rec_key'
def _fetch_leak_record(rec_key):
    if (rec_key is None):
        return None

    probable_delete_index_oid = S3kvTest('Kvtest fetch probable delete index').root_probable_dead_object_list_index()
    result = S3kvTest('Get key-value from probable delete index').get_keyval(probable_delete_index_oid, rec_key)\
            .execute_test(ignore_err=True)
    leak_index_records = result.status.stdout.split('----------------------------------------------')
    found_leak_value = _find_record(rec_key, leak_index_records)
    if (found_leak_value is None):
        return None

    found_leak_jsonval = _find_keyval_json(found_leak_value)
    if (found_leak_jsonval is None):
        return None

    s3_leak_obj = S3LeakRecord(rec_key, found_leak_jsonval)
    return s3_leak_obj


# Helper to fetch System Test user record from kvs
def _fetch_test_bucket_account_info(bucket_name):
    result = S3kvTest('kvtest can list user records').root_bucket_account_index_records().execute_test(ignore_err=True)
    root_index_records = result.status.stdout.split('----------------------------------------------')
    bucket_json_keyval_string = _find_record(bucket_name, root_index_records)
    test_user_record_json = _find_keyval_json(bucket_json_keyval_string)
    account_id = _extract_account_id(test_user_record_json)
    assert account_id, "account id not found for bucket:%s" % bucket_name
    return account_id

# Fetch given bucket record for System Test user account
def _fetch_bucket_info(bucket_name):
    root_bucket_metadata_oid = S3kvTest('Kvtest fetch bucket metadata index').root_bucket_metadata_index()
    result = S3kvTest('Kvtest list user bucket(s)').next_keyval(root_bucket_metadata_oid, bucket_name, 5).execute_test(ignore_err=True)
    test_bucket_list = result.status.stdout.split('----------------------------------------------')
    bucket_record = _find_record(bucket_name, test_bucket_list)
    return bucket_record

# Fetch bucket record, assert on failure
def _fail_fetch_bucket_info(bucket_name):
    bucket_record = _fetch_bucket_info(bucket_name)
    assert bucket_record, "bucket:%s not found!" % bucket_name
    return bucket_record

# Given bucket record, fetch key value pair, if exist
def _fetch_object_info(key_name, bucket_record):
    bucket_json_keyval = _find_keyval_json(bucket_record)
    oid_decoded = _extract_oid(bucket_json_keyval, bucket=False)
    result = S3kvTest('Kvtest list user bucket(s)').next_keyval(oid_decoded, "", 10).execute_test(ignore_err=True)
    bucket_entries = result.status.stdout.split('----------------------------------------------')
    file_record = _find_record(key_name, bucket_entries)
    return file_record

# Test for record in bucket
def expect_object_in_bucket(bucket_name, key):
    test_account_id = _fetch_test_bucket_account_info(bucket_name)
    test_bucket_name = test_account_id + "/" + bucket_name
    bucket_record = _fail_fetch_bucket_info(test_bucket_name)
    file_record = _fetch_object_info(key, bucket_record)
    assert file_record, "key:%s not found in bucket %s!" % key % bucket_name
    return file_record

# Fetch acl from metadata for given bucket
def _fetch_bucket_acl(bucket_name):
    bucket_record = _fail_fetch_bucket_info(bucket_name)
    bucket_json_keyval = _find_keyval_json(bucket_record)
    bucket_keyval = json.loads(bucket_json_keyval)
    return bucket_keyval['ACL']


# Check if bucket is empty by checking against mero_object_list_index_oid_u_*
def _check_bucket_not_empty(bucket_record):
    default_empty_object_list_index_oid = "AAAAAAAAAAA="
    bucket_json_keyval = _find_keyval_json(bucket_record)
    bucket_keyval = json.loads(bucket_json_keyval)
    string_oid = bucket_keyval['mero_object_list_index_oid']
    oid_list = string_oid.split("-")
    mero_object_list_index_oid_u_hi = base64.b64decode(oid_list[0])
    mero_object_list_index_oid_u_lo = base64.b64decode(oid_list[1])

    if (mero_object_list_index_oid_u_hi == default_empty_object_list_index_oid and
        mero_object_list_index_oid_u_lo == default_empty_object_list_index_oid):
        return False
    return True

# Fetch object acl from metadata
def _fetch_object_acl(bucket_name, key):
    file_record = expect_object_in_bucket(bucket_name, key)
    file_json_keyval = _find_keyval_json(file_record)
    file_keyval = json.loads(file_json_keyval)
    return file_keyval['ACL']

# Check against (default) acl for given bucket/object
def check_object_acl(bucket_name, key="", acl="", default_acl_test=False):
    default_acl = "PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVRGLTgiIHN0YW5kYWxvbmU9Im5vIj8+PEFjY2Vzc0NvbnRyb2xQb2xpY3kgeG1sbnM9Imh0dHA6Ly9zMy5hbWF6b25hd3MuY29tL2RvYy8yMDA2LTAzLTAxLyI+CiA8T3duZXI+CiAgPElEPkMxMjM0NTwvSUQ+CiAgPERpc3BsYXlOYW1lPnMzX3Rlc3Q8L0Rpc3BsYXlOYW1lPgogPC9Pd25lcj4KIDxBY2Nlc3NDb250cm9sTGlzdD4KICA8R3JhbnQ+CiAgIDxHcmFudGVlIHhtbG5zOnhzaT0iaHR0cDovL3d3dy53My5vcmcvMjAwMS9YTUxTY2hlbWEtaW5zdGFuY2UiIHhzaTp0eXBlPSJDYW5vbmljYWxVc2VyIj4KICAgIDxJRD5DMTIzNDU8L0lEPgogICAgPERpc3BsYXlOYW1lPnMzX3Rlc3Q8L0Rpc3BsYXlOYW1lPgogICA8L0dyYW50ZWU+CiAgIDxQZXJtaXNzaW9uPkZVTExfQ09OVFJPTDwvUGVybWlzc2lvbj4KICA8L0dyYW50PgogPC9BY2Nlc3NDb250cm9sTGlzdD4KPC9BY2Nlc3NDb250cm9sUG9saWN5Pg=="

    if default_acl_test:
        test_against_acl = default_acl
    else:
        test_against_acl = acl

    object_acl = _fetch_object_acl(bucket_name, key)

    if (object_acl == test_against_acl):
        print("Success")
        return
    else:
        raise AssertionError("Default ACL not matched for " + key + " in bucket " + bucket_name )
        return

# Check against (default) acl for given bucket/object
def check_bucket_acl(bucket_name, acl="", default_acl_test=False):
    default_acl = "PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVRGLTgiIHN0YW5kYWxvbmU9Im5vIj8+PEFjY2Vzc0NvbnRyb2xQb2xpY3kgeG1sbnM9Imh0dHA6Ly9zMy5hbWF6b25hd3MuY29tL2RvYy8yMDA2LTAzLTAxLyI+CiA8T3duZXI+CiAgPElEPkMxMjM0NTwvSUQ+CiAgPERpc3BsYXlOYW1lPnMzX3Rlc3Q8L0Rpc3BsYXlOYW1lPgogPC9Pd25lcj4KIDxBY2Nlc3NDb250cm9sTGlzdD4KICA8R3JhbnQ+CiAgIDxHcmFudGVlIHhtbG5zOnhzaT0iaHR0cDovL3d3dy53My5vcmcvMjAwMS9YTUxTY2hlbWEtaW5zdGFuY2UiIHhzaTp0eXBlPSJDYW5vbmljYWxVc2VyIj4KICAgIDxJRD5DMTIzNDU8L0lEPgogICAgPERpc3BsYXlOYW1lPnMzX3Rlc3Q8L0Rpc3BsYXlOYW1lPgogICA8L0dyYW50ZWU+CiAgIDxQZXJtaXNzaW9uPkZVTExfQ09OVFJPTDwvUGVybWlzc2lvbj4KICA8L0dyYW50PgogPC9BY2Nlc3NDb250cm9sTGlzdD4KPC9BY2Nlc3NDb250cm9sUG9saWN5Pg=="

    if default_acl_test:
        test_against_acl = default_acl
    else:
        test_against_acl = acl

    test_account_id = _fetch_test_bucket_account_info(bucket_name)
    test_bucket_name = test_account_id + "/" + bucket_name
    bucket_acl = _fetch_bucket_acl(test_bucket_name)
    if (bucket_acl == test_against_acl):
        print("Success")
        return
    else:
        raise AssertionError("Default ACL not matched for " + bucket_name)
        return

# Delete key from given index table
def delete_file_info(bucket_name,key):
    bucket_record = _fail_fetch_bucket_info(bucket_name)
    bucket_json_keyval = _find_keyval_json(bucket_record)
    oid_decoded = _extract_oid(bucket_json_keyval, bucket=False)
    result = S3kvTest('Kvtest delete key from bucket').delete_keyval(oid_decoded,key).execute_test(ignore_err=True)
    file_record = _fetch_object_info(key,bucket_record)
    assert not file_record,"key:%s not deleted from bucket %s!" % key % bucket_name
    return file_record

# Delete bucket record
def delete_bucket_info(bucket_name):
    # get account id for given bucket(A1/bucket_name) from global bucket account id index
    test_account_id = _fetch_test_bucket_account_info(bucket_name)
    test_bucket_name = test_account_id + "/" + bucket_name
    bucket_record = _fail_fetch_bucket_info(test_bucket_name)
    bucket_json_keyval = _find_keyval_json(bucket_record)
    oid_decoded = _extract_oid(bucket_json_keyval, bucket=False)
    # Check if bucket is empty
    if _check_bucket_not_empty(bucket_record):
        result = S3kvTest('Kvtest delete bucket record').delete_index(oid_decoded).execute_test(ignore_err=True)
    # delete given bucket(A1/bucket_name) from bucket metadata index
    root_bucket_metadata_oid = S3kvTest('Kvtest fetch bucket metadata index').root_bucket_metadata_index()
    result = S3kvTest('Kvtest delete bucket').delete_keyval(root_bucket_metadata_oid,test_bucket_name).execute_test(ignore_err=True)
    # delete the bucket information from from global bucket account id index
    root_bucket_account_index = S3kvTest('Kvtest fetch root bucket accountid index').root_bucket_account_index()
    # verification
    result = S3kvTest('Kvtest delete bucket').delete_keyval(root_bucket_account_index,bucket_name).execute_test(ignore_err=True)
    result = S3kvTest('kvtest can list user records').root_bucket_account_index_records().execute_test(ignore_err=True)
    root_index_records = result.status.stdout.split('----------------------------------------------')
    bucket_json_keyval_string = _find_record(bucket_name, root_index_records)
    assert not bucket_json_keyval_string,"bucket:%s entry not deleted!" % bucket_name
    bucket_record = _fetch_bucket_info(test_bucket_name)
    assert not bucket_record,"bucket:%s entry not deleted!" % bucket_name
    return

# Extract OID,layout-id from api response with --debug flag enabled
# Multiple response might be generated so store them in dictionary as 'x-stx-oid':'x-stx-layout-id'
# Sample response needs to be fetched from debug log is,
"""
DEBUG - Response headers: {'x-amzn-RequestId': '18d79101-45d1-47a3-ac5f-87e7\
4eca2f8f','Content-Length': '232', 'x-stx-oid': 'egZPBQAAAAA=-EwAAAAAAJKc=',\
'Server': 'SeagateS3', 'Retry-After': '1', 'Connection': 'close',\
'x-stx-layout-id': '1', 'Content-Type': 'application/xml'}
"""
def extract_headers_from_response(api_response):
    print("Extracting object oid and layout-id as \"x-stx-oid\" : \"x-stx-layout-id\" from response..")
    expected_line = "DEBUG - Response headers:.*'x-stx-layout-id'.*.'x-stx-oid'.*"
    response = re.findall(expected_line, api_response, re.MULTILINE)
    oid_dict = {}
    for response_line in response:
        response_line =  response_line.lstrip("DEBUG - Response headers: ").replace("\'", "\"")
        json_response = json.loads(response_line)
        oid = json_response["x-stx-oid"]
        layout_id = json_response["x-stx-layout-id"]
        print(oid + ":" + layout_id)
        oid_dict[oid] = layout_id
    print("Extracted \"x-stx-oid\" and \"x-stx-layout-id\"")
    return oid_dict

# Delete User record
# def delete_user_info(user_record="12345"):
#     root_oid = S3kvTest('Kvtest fetch root index').root_bucket_account_index()
#     result = S3kvTest('Kvtest delete user record').delete_keyval(root_oid,user_record).execute_test(ignore_err=True)
#     return

# Clean all data
def clean_all_data():
    result = S3kvTest('Kvtest remove global bucket account list index').delete_root_bucket_account_index().execute_test(ignore_err=True)
    result = S3kvTest('Kvtest remove global bucket metadata list index').delete_root_bucket_metadata_index().execute_test(ignore_err=True)
    result = S3kvTest('Kvtest remove global probable delete list index').delete_root_probable_dead_object_list_index().execute_test(ignore_err=True)
    return

def create_s3root_index():
    result = S3kvTest('Kvtest create global bucket account list index').create_root_bucket_account_index().execute_test(ignore_err=True)
    result = S3kvTest('Kvtest create global bucket metadata list index').create_root_bucket_metadata_index().execute_test(ignore_err=True)
    result = S3kvTest('Kvtest create global probable delete list index').create_root_probable_dead_object_list_index().execute_test(ignore_err=True)
    return
