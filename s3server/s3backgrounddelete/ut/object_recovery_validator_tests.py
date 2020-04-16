"""
Unit Test for ObjectRecoveryValidator.
"""
import logging
import json
from unittest.mock import Mock, MagicMock
import pytest

from s3backgrounddelete.eos_core_config import EOSCoreConfig
from s3backgrounddelete.eos_core_index_api import EOSCoreIndexApi
from s3backgrounddelete.eos_core_kv_api import EOSCoreKVApi
from s3backgrounddelete.eos_core_object_api import EOSCoreObjectApi
from s3backgrounddelete.eos_core_error_respose import EOSCoreErrorResponse
from s3backgrounddelete.eos_get_kv_response import EOSCoreGetKVResponse
from s3backgrounddelete.eos_list_index_response import EOSCoreListIndexResponse
from s3backgrounddelete.object_recovery_validator import ObjectRecoveryValidator


def test_list_instance_index_fail():
    """Test if ObjectRecoveryValidator skips processing successfully if listing instance index fails"""
    index_api_mock = Mock(spec=EOSCoreIndexApi)
    object_api_mock = Mock(spec=EOSCoreObjectApi)

    index_api_mock.list.return_value = False, {}

    config = EOSCoreConfig()
    probable_delete_records = {'Key': 'TAcGAQAAAAA=-AwAAAAAAhEs=', \
        'Value':'{"clovis_process_fid":"<0x7200000000000000:0>","create_timestamp":"2020-03-16T16:24:04.000Z", \
        "force_delete":"false","global_instance_id":"TAifBwAAAAA=-AAAAAAAA2lk=","is_multipart":"false", \
        "object_key_in_index":"object_1","object_layout_id":9,"object_list_index_oid":"TAifBwAAAHg=-AQAAAAAA2lk=", \
        "objects_version_list_index_oid":"TAifBwAAAHg=-AwAAAAAA2lk=","old_oid":"AAAAAAAAAAA=-AAAAAAAAAAA=", \
        "version_key_in_index":"object_1/18446742489333709430"}'}

    validator = ObjectRecoveryValidator(
                    config, probable_delete_records, objectapi = object_api_mock, indexapi = index_api_mock)
    inst_id = json.loads(probable_delete_records['Value'])

    ret = validator.check_instance_is_nonactive(inst_id['global_instance_id'])

    # Assert that object oid delete is skipped if instance id listing fails.
    object_api_mock.assert_not_called
    assert ret == False, "value was True, should be False"


def test_object_metadata_not_exists():
    """Test to check if ObjectRecoveryValidator attempts delete for object oid """
    index_api_mock = Mock(spec=EOSCoreIndexApi)
    kv_api_mock = Mock(spec=EOSCoreKVApi)
    object_api_mock = Mock(spec=EOSCoreObjectApi)

    ol_res_val = {'ACL':'','Bucket-Name':'mybucket','Object-Name':'test_object','Object-URI':'mybucket\\test_object',
        'create_timestamp':'2020-03-17T11:02:13.000Z','layout_id':9,'mero_oid':'Tgj8AgAAAAA=-dQAAAAAABCY='}

    index_content = {'Delimiter': '', 'Index-Id': 'AAAAAAAAAHg=-BAAQAAAAAAA=',
                    'IsTruncated': 'false', 'Keys': [{'Key': 'test_object', 'Value': ol_res_val}],
                    'Marker': '', 'MaxKeys': '1000', 'NextMarker': '', 'Prefix': ''}
    index_response = EOSCoreListIndexResponse(json.dumps(index_content).encode())
    error_response = EOSCoreErrorResponse(404, "Not found", "Not found")

    index_api_mock.list.return_value = True, index_response
    kv_api_mock.get.return_value = False, error_response
    object_api_mock.delete.return_value = True, {}


    config = EOSCoreConfig()
    probable_delete_records = {'Key': 'TAcGAQAAAAA=-AwAAAAAAhEs=', \
        'Value':'{"clovis_process_fid":"<0x7200000000000000:0>","create_timestamp":"2020-03-16T16:24:04.000Z", \
        "force_delete":"false","global_instance_id":"TAifBwAAAAA=-AAAAAAAA2lk=","is_multipart":"false", \
        "object_key_in_index":"object_1","object_layout_id":9,"object_list_index_oid":"TAifBwAAAHg=-AQAAAAAA2lk=", \
        "objects_version_list_index_oid":"TAifBwAAAHg=-AwAAAAAA2lk=","old_oid":"Tgj8AgAAAAA=-kwAAAAAABCY=", \
        "version_key_in_index":"object_1/18446742489333709430"}\n'}

    validator = ObjectRecoveryValidator(
                          config, probable_delete_records, objectapi = object_api_mock, kvapi = kv_api_mock, indexapi = index_api_mock)
    #Mock call to 'process_probable_delete_record'
    validator.process_probable_delete_record = MagicMock(return_value=True)

    validator.process_results()

    # Assert that object oid delete and leak index entry delete
    # is triggered if metadata doesn't exists.
    validator.process_probable_delete_record.assert_called_with(True, True)


def test_object_metadata_exists_and_matches():
    """Test if ObjectRecoveryValidator should attempt delete for old object oid """
    index_api_mock = Mock(spec=EOSCoreIndexApi)
    kv_api_mock = Mock(spec=EOSCoreKVApi)
    object_api_mock = Mock(spec=EOSCoreObjectApi)

    ol_res_val = {'ACL':'','Bucket-Name':'mybucket','Object-Name':'test_object','Object-URI':'mybucket\\test_object',
        'create_timestamp':'2020-03-17T11:02:13.000Z','layout_id':9,'mero_oid':'Tgj8AgAAAAA=-dQAAAAAABCY='}

    index_content = {'Delimiter': '', 'Index-Id': 'AAAAAAAAAHg=-BAAQAAAAAAA=',
                    'IsTruncated': 'false', 'Keys': [{'Key': 'test_object', 'Value': ol_res_val}],
                    'Marker': '', 'MaxKeys': '1000', 'NextMarker': '', 'Prefix': ''}

    index_response = EOSCoreListIndexResponse(json.dumps(index_content).encode())
    object_key = ol_res_val["Object-Name"]
    # oid mismatches in object metadata
    object_metadata = ol_res_val

    kv_response = EOSCoreGetKVResponse(object_key, json.dumps(object_metadata).encode())

    index_api_mock.list.return_value = True, index_response
    kv_api_mock.get.return_value = True, kv_response
    object_api_mock.delete.return_value = True, {}


    config = EOSCoreConfig()
    probable_delete_records = {'Key': 'Tgj8AgAAAAA=-dQAAAAAABCY=', \
        'Value':'{"clovis_process_fid":"<0x7200000000000000:0>","create_timestamp":"2020-03-16T16:24:04.000Z", \
        "force_delete":"false","global_instance_id":"TAifBwAAAAA=-AAAAAAAA2lk=","is_multipart":"false", \
        "object_key_in_index":"object_1","object_layout_id":9,"object_list_index_oid":"TAifBwAAAHg=-AQAAAAAA2lk=", \
        "objects_version_list_index_oid":"TAifBwAAAHg=-AwAAAAAA2lk=","old_oid":"AAAAAAAAAAA=-AAAAAAAAAAA=", \
        "version_key_in_index":"object_1/18446742489333709430"}\n'}
    validator = ObjectRecoveryValidator(
                          config, probable_delete_records, objectapi = object_api_mock, kvapi = kv_api_mock, indexapi = index_api_mock)

    #Mock 'process_probable_delete_record'
    validator.process_probable_delete_record = MagicMock(return_value=True)
    validator.check_instance_is_nonactive = MagicMock(return_value=False)

    validator.process_results()

    # Assert following:
    kv_api_mock.assert_called_once
    validator.process_probable_delete_record.assert_called == False


def test_object_metadata_exists_mismatches():
    """Test if ObjectRecoveryValidator should attempt delete for object oid """
    index_api_mock = Mock(spec=EOSCoreIndexApi)
    kv_api_mock = Mock(spec=EOSCoreKVApi)
    object_api_mock = Mock(spec=EOSCoreObjectApi)

    ol_res_val = {'ACL':'','Bucket-Name':'mybucket','Object-Name':'test_object','Object-URI':'mybucket\\test_object',
        'create_timestamp':'2020-03-17T11:02:13.000Z','layout_id':9,'mero_oid':'TAifBwAAAHg=-AQAAAAAA2lk='}

    index_content = {'Delimiter': '', 'Index-Id': 'AAAAAAAAAHg=-BAAQAAAAAAA=',
                    'IsTruncated': 'false', 'Keys': [{'Key': 'test_object', 'Value': ol_res_val}],
                    'Marker': '', 'MaxKeys': '1000', 'NextMarker': '', 'Prefix': ''}

    index_response = EOSCoreListIndexResponse(json.dumps(index_content).encode())
    object_key = ol_res_val["Object-Name"]
    # oid mismatches in object metadata
    object_metadata = ol_res_val


    kv_response = EOSCoreGetKVResponse(object_key, json.dumps(object_metadata).encode())

    index_api_mock.list.return_value = True, index_response
    kv_api_mock.get.return_value = True, kv_response
    object_api_mock.delete.return_value = True, {}


    config = EOSCoreConfig()
    probable_delete_records = {'Key': 'Tgj8AgAAAAA=-dQAAAAAABCY=', \
        'Value':'{"clovis_process_fid":"<0x7200000000000000:0>","create_timestamp":"2020-03-16T16:24:04.000Z", \
        "force_delete":"false","global_instance_id":"TAifBwAAAAA=-AAAAAAAA2lk=","is_multipart":"false", \
        "object_key_in_index":"object_1","object_layout_id":9,"object_list_index_oid":"TAifBwAAAHg=-AQAAAAAA2lk=", \
        "objects_version_list_index_oid":"TAifBwAAAHg=-AwAAAAAA2lk=","old_oid":"AAAAAAAAAAA=-AAAAAAAAAAA=", \
        "version_key_in_index":"object_1/18446742489333709430"}\n'}

    validator = ObjectRecoveryValidator(
                          config, probable_delete_records, objectapi = object_api_mock, kvapi = kv_api_mock, indexapi = index_api_mock)

    #Mock 'process_probable_delete_record'
    validator.process_probable_delete_record = MagicMock(return_value=True)

    validator.process_results()

    # Assert that object oid delete and leak index entry delete
    # is triggered if metadata doesn't exists.
    validator.process_probable_delete_record.assert_called == True