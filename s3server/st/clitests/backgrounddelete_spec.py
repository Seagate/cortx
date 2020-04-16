#!/usr/bin/python3.6
import sys
import os
import yaml
import shutil
import re
import json
from framework import Config
from framework import S3PyCliTest
from s3client_config import S3ClientConfig
from auth import AuthTest
from s3fi import S3fiTest
from awss3api import AwsTest
from s3cmd import S3cmdTest
import s3kvs
import time

sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__),  '../../s3backgrounddelete/s3backgrounddelete')))
from s3backgrounddelete.object_recovery_scheduler import ObjectRecoveryScheduler
from s3backgrounddelete.object_recovery_processor import ObjectRecoveryProcessor
from s3backgrounddelete.eos_core_config import EOSCoreConfig
from s3backgrounddelete.eos_core_object_api import EOSCoreObjectApi
from s3backgrounddelete.eos_core_index_api import EOSCoreIndexApi
from s3backgrounddelete.eos_core_error_respose import EOSCoreErrorResponse

# Run before all to setup the test environment.
def before_all():
    print("Configuring LDAP")
    S3PyCliTest('Before_all').before_all()

# Run before all to setup the test environment.
before_all()

# Set basic properties
Config.config_file = "pathstyle.s3cfg"
S3ClientConfig.pathstyle = False
S3ClientConfig.ldapuser = 'sgiamadmin'
S3ClientConfig.ldappasswd = 'ldapadmin'

# Config files used by scheduler and processor
origional_bgdelete_config_file = os.path.join(os.path.dirname(__file__), 's3_background_delete_config_test.yaml')
bgdelete_config_dir = os.path.join('/', 'opt', 'seagate', 's3', 's3backgrounddelete')
bgdelete_config_file = os.path.join(bgdelete_config_dir, 'config.yaml')
backup_bgdelete_config_file = os.path.join(bgdelete_config_dir, 'backup_config.yaml')


# Update S3 Background delete config file with account access key and secretKey.
def load_and_update_config(access_key_value, secret_key_value):
    # Update config file
    if os.path.isfile(bgdelete_config_file):
       shutil.copy2(bgdelete_config_file, backup_bgdelete_config_file)
    else:
       try:
           os.stat(bgdelete_config_dir)
       except:
           os.mkdir(bgdelete_config_dir)
       shutil.copy2(origional_bgdelete_config_file, bgdelete_config_file)

    with open(bgdelete_config_file, 'r') as f:
            config = yaml.safe_load(f)
            config['eos_core']['access_key'] = access_key_value
            config['eos_core']['secret_key'] = secret_key_value
            config['eos_core']['daemon_mode'] = "False"
            config['leakconfig']['leak_processing_delay_in_mins'] = 0
            config['leakconfig']['version_processing_delay_in_mins'] = 0

    with open(bgdelete_config_file, 'w') as f:
            yaml.dump(config, f)

    os.environ["AWS_ACCESS_KEY_ID"] = access_key_value
    os.environ["AWS_SECRET_ACCESS_KEY"] = secret_key_value

    # Update values of access key and secret key for s3iamcli commands
    S3ClientConfig.access_key_id = access_key_value
    S3ClientConfig.secret_key = secret_key_value


# Restore previous configurations
def restore_configuration():
    # Restore/Delete config file
    if os.path.isfile(backup_bgdelete_config_file):
       shutil.copy2(backup_bgdelete_config_file, bgdelete_config_file)
       os.remove(backup_bgdelete_config_file)
    else:
       os.remove(bgdelete_config_file)

    # Restore access key and secret key.
    del os.environ["AWS_ACCESS_KEY_ID"]
    del os.environ["AWS_SECRET_ACCESS_KEY"]


# Call HEAD object api on oids
def perform_head_object(oid_dict):
    print("Validating non-existence of oids in probable dead list using HEAD object api")
    print("Probable dead list should not contain :" + str(list(oid_dict.keys())))
    config = EOSCoreConfig()
    for oid,layout_id in oid_dict.items():
        response = EOSCoreObjectApi(config).head(oid, layout_id)
        assert response is not None
        assert response[0] is False
        assert isinstance(response[1], EOSCoreErrorResponse)
        assert response[1].get_error_status() == 404
        assert response[1].get_error_reason() == "Not Found"
        print("Object oid \"" + oid + "\" is not present in list..")
    print("HEAD object validation completed..")


# *********************Create account s3-background-delete-svc************************
test_msg = "Create account s3-background-delete-svc"
account_args = {'AccountName': 's3-background-delete-svc',\
                'Email': 's3-background-delete-svc@seagate.com',\
                'ldapuser': S3ClientConfig.ldapuser,\
                'ldappasswd': S3ClientConfig.ldappasswd}
account_response_pattern = "AccountId = [\w-]*, CanonicalId = [\w-]*, RootUserName = [\w+=,.@-]*, AccessKeyId = [\w-]*, SecretKey = [\w/+]*$"
result = AuthTest(test_msg).create_account(**account_args).execute_test()
result.command_should_match_pattern(account_response_pattern)
account_response_elements = AuthTest.get_response_elements(result.status.stdout)
print(account_response_elements)


# ********** Update s3background delete config file with AccesskeyId and SecretKey*****************
load_and_update_config(account_response_elements['AccessKeyId'], account_response_elements['SecretKey'])

#Clear probable delete list index
s3kvs.clean_all_data()
s3kvs.create_s3root_index()

# ********** Create test bucket in s3-background-delete-svc account********
AwsTest('Create Bucket "seagatebucket" using s3-background-delete-svc account')\
    .create_bucket("seagatebucket").execute_test().command_is_successful()

# Initialising the scheduler and processor
scheduler = ObjectRecoveryScheduler()
processor = ObjectRecoveryProcessor()


"""
Test Scenario : 1
Scenario: Delete object leak test (DELETE api test)
   1. create object and the get OID, layout_id from response
   2. enable fault point
   3. delete object
   4. disable fault point
   5. run background delete schedular
   6. run background delete processor
   7. verify cleanup of OID using HEAD api
   8. verify cleanup of Object using aws s3api head-object api
"""
# ********** Upload objects in bucket*************************
result = AwsTest('Upload Object "object1" to bucket "seagatebucket"')\
    .put_object("seagatebucket", "object1", 3000, debug_flag="True")\
    .execute_test(ignore_err=True).command_is_successful()

object1_oid_dict = s3kvs.extract_headers_from_response(result.status.stderr)

# ********** Delete objects with fault injection enabled*******
S3fiTest('Enable FI clovis entity delete fail')\
   .enable_fi("enable", "always", "clovis_entity_delete_fail")\
   .execute_test().command_is_successful()

AwsTest('Delete Object "object1" from bucket "seagatebucket"')\
   .delete_object("seagatebucket", "object1").execute_test().command_is_successful()

# wait till cleanup process completes and s3server sends response to client
time.sleep(1)

S3fiTest('Disable FI clovis entity delete').disable_fi("clovis_entity_delete_fail")\
   .execute_test().command_is_successful()

# ************ Start Schedular*****************************
print("Running scheduler...")
scheduler.add_kv_to_queue()
print("Scheduler has stopped...")
# ************* Start Processor****************************
print("Running Processor...")
processor.consume()
print("Processor has stopped...")

# ************* Verify OID is not present in list*******
perform_head_object(object1_oid_dict)

# ************* Verify cleanup of Object using aws s3api head-object api******
AwsTest('Do head-object for "object1" on bucket "seagatebucket"')\
   .head_object("seagatebucket", "object1").execute_test(negative_case=True)\
   .command_should_fail().command_error_should_have("Not Found")


"""
# Test Scenario : 2
Scenario: New object oid leak test(PUT api test)
    1. enable fault points
    2. Upload object and the get OID1 from response
    3. disable fault points
    4. run background delete schedular
    5. run background delete processor
    6. verify cleanup of OID1 using HEAD api
    7. verify cleanup of Object using aws s3api head-object api
"""
# *********** Upload Object in bucket*************************
S3fiTest('Enable FI clovis object write fail')\
   .enable_fi("enable", "always", "clovis_obj_write_fail").execute_test()\
   .command_is_successful()
S3fiTest('Enable FI clovis entity delete fail')\
   .enable_fi("enable", "always", "clovis_entity_delete_fail").execute_test()\
   .command_is_successful()

result = AwsTest('Upload Object "object2" to bucket "seagatebucket"')\
    .put_object("seagatebucket", "object2", 3000, debug_flag="True")\
    .execute_test(ignore_err=True, negative_case=True)\
    .command_should_fail().command_error_should_have("InternalError")

# wait till cleanup process completes and s3server sends response to client
time.sleep(1)

S3fiTest('Disable FI clovis obj write fail').disable_fi("clovis_obj_write_fail")\
   .execute_test().command_is_successful()
S3fiTest('Disable FI clovis entity delete fail').disable_fi("clovis_entity_delete_fail")\
   .execute_test().command_is_successful()

object2_oid_dict = s3kvs.extract_headers_from_response(result.status.stderr)

# ************ Start Schedular*****************************
print("Running scheduler...")
scheduler.add_kv_to_queue()
print("Schdeuler has stopped...")
# ************* Start Processor****************************
print("Running Processor...")
processor.consume()
print("Processor has stopped...")

# *********** Validate OID is not present in list**********
perform_head_object(object2_oid_dict)

# ************* Verify cleanup of Object using aws s3api head-object api******
AwsTest('Do head-object for "object2" on bucket "seagatebucket"')\
   .head_object("seagatebucket", "object2").execute_test(negative_case=True)\
   .command_should_fail().command_error_should_have("Not Found")


"""
# Test Scenario : 3
Scenario: Old object oid leak test (PUT api test)
    1. Upload object and the get OID1 from response
    2. enable fault point
    3. Upload same object with same name and the get OID2 from response
    4. disable fault point
    5. run background delete schedular
    6. run background delete processor
    7. verify cleanup of OID1 using HEAD api
    8. delete new object
"""
# *********** Upload Object in bucket*************************
result = AwsTest('Upload Object "object3" to bucket "seagatebucket"')\
    .put_object("seagatebucket", "object3", 3000, debug_flag="True")\
    .execute_test(ignore_err=True).command_is_successful()

# wait till cleanup process completes and s3server sends response to client
time.sleep(1)

object3_old_oid_dict = s3kvs.extract_headers_from_response(result.status.stderr)

# ********** Upload same object again in same bucket*************************
S3fiTest('Enable FI clovis object write fail')\
   .enable_fi("enable", "always", "clovis_obj_write_fail").execute_test()\
   .command_is_successful()
S3fiTest('Enable FI clovis entity delete fail')\
   .enable_fi("enable", "always", "clovis_entity_delete_fail")\
   .execute_test().command_is_successful()

result = AwsTest('Upload Object "object3" to bucket "seagatebucket"')\
    .put_object("seagatebucket", "object3", 3000, debug_flag="True")\
    .execute_test(ignore_err=True, negative_case=True).command_should_fail()\
    .command_error_should_have("InternalError")

# wait till cleanup process completes and s3server sends response to client
time.sleep(1)

S3fiTest('Disable FI clovis object write fail').disable_fi("clovis_obj_write_fail")\
   .execute_test().command_is_successful()
S3fiTest('Disable FI clovis entity delete fail').disable_fi("clovis_entity_delete_fail")\
   .execute_test().command_is_successful()

object3_new_oid_dict = s3kvs.extract_headers_from_response(result.status.stderr)

# ************ Start Schedular*****************************
print("Running scheduler...")
scheduler.add_kv_to_queue()
print("Scheduler has stopped...")
# ************* Start Processor****************************
print("Running Processor...")
processor.consume()
print("Processor has stopped...")

# *********** Validate old oid is not present in list**********************************
perform_head_object(object3_new_oid_dict)

# ********** Delete object "object3" *************
AwsTest('Delete Object "object3"').delete_object("seagatebucket", "object3")\
   .execute_test().command_is_successful()


"""
Test Scenario : 4
Scenario: multidelete objects leak test (DELETE api test)
    1. upload first object and get oid1, layout_id1 from response
    2. upload second object and get oid2, layout_id2 from response
    3. enable fault point
    4. perform multi_delete_test using bucketname
    5. disable fault point
    6. run background delete schedular
    7. run background delete processor
    8. verify cleanup of both oids using HEAD object api
    9. verify cleanup of both Objects using aws s3api head-object api

"""
# ********** Upload objects in bucket*************************
result = AwsTest('Upload Object "object4" to bucket "seagatebucket"')\
    .put_object("seagatebucket", "object4", 3000, debug_flag="True")\
    .execute_test(ignore_err=True).command_is_successful()
object4_oid_dict = s3kvs.extract_headers_from_response(result.status.stderr)

result = AwsTest('Upload Object "object5" to bucket "seagatebucket"')\
    .put_object("seagatebucket", "object5", 3000, debug_flag="True")\
    .execute_test(ignore_err=True).command_is_successful()
object5_oid_dict = s3kvs.extract_headers_from_response(result.status.stderr)

# ********** Delete objects with fault injection enabled*******
S3fiTest('Enable FI clovis entity delete fail')\
    .enable_fi("enable", "always", "clovis_entity_delete_fail")\
    .execute_test().command_is_successful()

S3cmdTest('s3cmd can delete multiple objects "object4" and "object5"')\
    .with_credentials(account_response_elements['AccessKeyId'], account_response_elements['SecretKey'])\
    .multi_delete_test("seagatebucket").execute_test().command_is_successful()

# wait till cleanup process completes and s3server sends response to client
time.sleep(1)

S3fiTest('Disable FI clovis entity delete fail')\
    .disable_fi("clovis_entity_delete_fail").execute_test()\
    .command_is_successful()

# ************ Start Schedular*****************************
print("Running scheduler...")
scheduler.add_kv_to_queue()
print("Schdeuler has stopped...")
# ************* Start Processor****************************
print("Running Processor...")
processor.consume()
print("Processor has stopped...")

# ************* Verify OID are not present in list*******
perform_head_object(object4_oid_dict)
perform_head_object(object5_oid_dict)

# ************* Verify cleanup of Object using aws s3api head-object api******
AwsTest('Do head-object for "object4" on bucket "seagatebucket"')\
   .head_object("seagatebucket", "object4").execute_test(negative_case=True)\
   .command_should_fail().command_error_should_have("Not Found")

AwsTest('Do head-object for "object5" on bucket "seagatebucket"')\
   .head_object("seagatebucket", "object5").execute_test(negative_case=True)\
   .command_should_fail().command_error_should_have("Not Found")



# Extract the upload id from response which has the following format
# [bucketname    objecctname    uploadid]
def get_upload_id(response):
    key_pairs = response.split('\t')
    return key_pairs[2]


"""
Test Scenario : 5
Scenario: Multipart upload objects leak test (Multipart upload test)
    1. upload 3kfile object and get oid, layout_id from response
    2. create multipart upload and get oid, layout_id from response
    3. upload firstpart
    4. upload secondpart
    5. enable fault point
    6. complete multipart upload
    7. disable fault point
    8. run background delete schedular
    9. run background delete processor
    10.verify cleanup of OID
"""
# *********** Upload Object in bucket*************************
result = AwsTest('Upload Object "object6" to bucket "seagatebucket"')\
    .put_object("seagatebucket", "object6", 3000, debug_flag="True")\
    .execute_test(ignore_err=True).command_is_successful()

object6_oid_dict = s3kvs.extract_headers_from_response(result.status.stderr)

# ************** Create a multipart upload *********
result=AwsTest('Aws can upload object6 10Mb multipart file')\
    .create_multipart_upload("seagatebucket", "object6", 10485760, "domain=storage", debug_flag="True" )\
    .execute_test(ignore_err=True).command_is_successful()

upload_id = get_upload_id(result.status.stdout)
multipart_oid_dict = s3kvs.extract_headers_from_response(result.status.stderr)

#************** Upload Individual parts ********
result=AwsTest('Aws can upload 5Mb first part')\
    .upload_part("seagatebucket", "firstpart", 5242880, "object6", "1" , upload_id)\
    .execute_test().command_is_successful()
e_tag_1 = result.status.stdout

result=AwsTest('Aws can upload 5Mb second part')\
    .upload_part("seagatebucket", "secondpart", 5242880, "object6", "2" , upload_id)\
    .execute_test().command_is_successful()
e_tag_2 = result.status.stdout

parts="Parts=[{ETag="+e_tag_1.strip('\n')+",PartNumber="+str(1)+"},\
    {ETag="+e_tag_2.strip('\n')+",PartNumber="+str(2)+"}]"
print(parts)

#************** Complete multipart upload ********
S3fiTest('Enable FI clovis entity delete fail')\
    .enable_fi_offnonm("enable", "clovis_entity_delete_fail", "1", "99")\
    .execute_test().command_is_successful()

result=AwsTest('Aws can complete multipart upload object6 10Mb file')\
    .complete_multipart_upload("seagatebucket", "object6", parts, upload_id)\
    .execute_test(ignore_err=True, negative_case=True).command_is_successful()\
    .command_response_should_have("seagatebucket/object6")

# wait till cleanup process completes and s3server sends response to client
time.sleep(1)

S3fiTest('Disable FI clovis entity delete fail').disable_fi("clovis_entity_delete_fail")\
    .execute_test().command_is_successful()

# ************ Start Schedular*****************************
print("Running scheduler...")
scheduler.add_kv_to_queue()
print("Schdeuler has stopped...")
# ************* Start Processor****************************
print("Running Processor...")
processor.consume()
print("Processor has stopped...")

# ************* Verify clean up of OID's*****************
perform_head_object(object6_oid_dict)

# * ********** Delete object "object6 10Mbfile" *************
AwsTest('Delete Object "object6"').delete_object("seagatebucket", "object6")\
   .execute_test().command_is_successful()



"""
Test Scenario : 6
Scenario: Abort multipart upload objects leak test (Abort Multipart upload test)
    1. create multipart upload and get oid, layout_id from response
    2. upload firstpart
    3. enable fault point
    4. abort multipart upload
    5. disable fault point
    6. run background delete schedular
    7. run background delete processor
    8. verify cleanup of multipart oid
    9. verify cleanup of Object using aws s3api head-object api
"""
# ************** Create a multipart upload *********
result=AwsTest('Aws can upload object7 10Mb file')\
    .create_multipart_upload("seagatebucket", "object7", 10485760, "domain=storage", debug_flag="True" )\
    .execute_test(ignore_err=True).command_is_successful()

upload_id = get_upload_id(result.status.stdout)
multipart_oid_dict = s3kvs.extract_headers_from_response(result.status.stderr)

#************** Upload Individual parts ********
result=AwsTest('Aws can upload 5Mb first part')\
    .upload_part("seagatebucket", "firstpart", 5242880, "object7", "1" , upload_id)\
    .execute_test().command_is_successful()

#************** Abort multipart upload ********
S3fiTest('Enable FI clovis entity delete fail')\
    .enable_fi_offnonm("enable", "clovis_entity_delete_fail", "1", "99")\
    .execute_test().command_is_successful()

result=AwsTest('Aws can abort multipart upload object7 10Mb file')\
    .abort_multipart_upload("seagatebucket", "object7", upload_id)\
    .execute_test().command_is_successful()

# wait till cleanup process completes and s3server sends response to client
time.sleep(1)

S3fiTest('Disable FI clovis entity delete fail').disable_fi("clovis_entity_delete_fail")\
    .execute_test().command_is_successful()

# ************ Start Schedular*****************************
print("Running scheduler...")
scheduler.add_kv_to_queue()
print("Schdeuler has stopped...")
# ************* Start Processor****************************
print("Running Processor...")
processor.consume()
print("Processor has stopped...")

# ************* Verify clean up of OID's*****************
perform_head_object(multipart_oid_dict)

# ************* Verify cleanup of Object using aws s3api head-object api******
AwsTest('Do head-object for "object7" on bucket "seagatebucket"')\
   .head_object("seagatebucket", "object7").execute_test(negative_case=True)\
   .command_should_fail().command_error_should_have("Not Found")


"""
Test Scenario : 7
Scenario: Create multipart upload objects leak test (Abort Multipart upload test)
    1. enable fault points
    2. create multipart upload and get oid, layout_id from response
    3. disable fault point
    4. run background delete schedular
    5. run background delete processor
    6. verify cleanup of multipart oid using HEAD api
    7. verify cleanup of Object using aws s3api head-object api
"""
# ************** Create a multipart upload *********
S3fiTest('Enable FI post multipartobject action create object shutdown fail')\
    .enable_fi("enable", "always", "post_multipartobject_action_create_object_shutdown_fail")\
    .execute_test().command_is_successful()
S3fiTest('Enable FI clovis entity delete fail').enable_fi("enable", "always", "clovis_entity_delete_fail")\
    .execute_test().command_is_successful()

result=AwsTest('Aws can upload object8 10Mb file')\
    .create_multipart_upload("seagatebucket", "object8", 10485760, "domain=storage", debug_flag="True" )\
    .execute_test(ignore_err=True, negative_case=True).command_should_fail()\
    .command_error_should_have("ServiceUnavailable")

# wait till cleanup process completes and s3server sends response to client
time.sleep(1)

S3fiTest('Disable FI  post multipartobject action create object shutdown fail')\
    .disable_fi("post_multipartobject_action_create_object_shutdown_fail")\
    .execute_test().command_is_successful()
S3fiTest('Disable FI clovis entity delete fail').disable_fi("clovis_entity_delete_fail")\
    .execute_test().command_is_successful()

multipart_oid_dict = s3kvs.extract_headers_from_response(result.status.stderr)

# ************ Start Schedular*****************************
print("Running scheduler...")
scheduler.add_kv_to_queue()
print("Schdeuler has stopped...")
# ************* Start Processor****************************
print("Running Processor...")
processor.consume()
print("Processor has stopped...")

# ************* Verify clean up of OID's*****************
perform_head_object(multipart_oid_dict)

# ************* Verify cleanup of Object using aws s3api head-object api*****************
AwsTest('Do head-object for "object8" on bucket "seagatebucket"')\
   .head_object("seagatebucket", "object8").execute_test(negative_case=True)\
   .command_should_fail().command_error_should_have("Not Found")



"""
Test Scenario : 8 [Handle part index leak]
Scenario: Abort multipart upload after deleting multipart metadata to create leak in the part index.
Along with object leak, Background delete should take care of deleting the leaked part index.
    1. create multipart upload and get oid, layout_id from response
    2. upload first part
    3. enable fault point "clovis_idx_delete_fail" to leak part index
    4. abort multipart upload
    5. Read and save probable delete record for multipart oid
    6. disable fault point
    7. run background delete schedular
    8. run background delete processor
    9. verify cleanup of Object using aws s3api head-object api
   10. verify cleanup of part index oid
"""
CONFIG = EOSCoreConfig()
# ************** Create a multipart upload *********
result=AwsTest('Aws can upload object9 10Mb file in multipart form')\
    .create_multipart_upload("seagatebucket", "object9", 10485760, "domain=storage", debug_flag="True" )\
    .execute_test(ignore_err=True).command_is_successful()

multipart_oid_dict = s3kvs.extract_headers_from_response(result.status.stderr)
if (multipart_oid_dict is not None):
    obj_oid = list(multipart_oid_dict.keys())[0]

# Get upload ID
upload_id = get_upload_id(result.status.stdout)
upload_id = upload_id.rstrip(os.linesep)

# Upload Individual part
result=AwsTest('Aws can upload 5Mb first part')\
    .upload_part("seagatebucket", "firstpart", 5242880, "object9", "1" , upload_id)\
    .execute_test().command_is_successful()
e_tag_1 = result.status.stdout

# Enable fault point. Force failure in part list index deletion
S3fiTest('Enable FI clovis entity delete fail').enable_fi("enable", "always", "clovis_entity_delete_fail")\
    .execute_test().command_is_successful()

# Abort multipart upload
result=AwsTest('Aws can abort multipart upload object9 10Mb file')\
    .abort_multipart_upload("seagatebucket", "object9", upload_id)\
    .execute_test().command_is_successful()

# Read probable delete record associated with 'obj_oid'
leak_info = s3kvs._fetch_leak_record(obj_oid)
assert leak_info, "Failed. No probable delete index record"

part_index = leak_info.get_part_index_oid()

# Disable fault point
S3fiTest('Disable FI clovis entity delete fail').disable_fi("clovis_entity_delete_fail")\
    .execute_test().command_is_successful()

# wait till cleanup process completes and s3server sends response to client
time.sleep(1)

# ************ Start Schedular*****************************
print("Running scheduler...")
scheduler.add_kv_to_queue()
print("Schdeuler has stopped...")
# ************* Start Processor****************************
print("Running Processor...")
processor.consume()
print("Processor has stopped...")

# ************* Verify clean up of OID's*****************
perform_head_object(multipart_oid_dict)

# ************* Verify cleanup of Object using aws s3api head-object api*****************
AwsTest('Do head-object for "object9" on bucket "seagatebucket"')\
   .head_object("seagatebucket", "object9").execute_test(negative_case=True)\
   .command_should_fail().command_error_should_have("Not Found")

# ************* Verify part list index is deleted *************
# Currently, HEAD /indexes/<index oid> is not implemented
# Until then, we'll check if any entry containing upload ID exist in part list index
# to check for it's existence. Later, we can implement HEAD /indexes/<index oid> in s3 server.
# Check if an entry with value of "upload_id" exists in part list index
status, res = EOSCoreIndexApi(CONFIG).list(part_index)
if (status):
    if (res):
        part_indx_present = False
        parts_record = res.get_index_content()
        assert parts_record is not None, "Error. Part index listing is empty"
        parts = parts_record["Keys"]
        if (parts is None):
            pass
        else:
            for rec in parts:
                part_val = rec["Value"]
                part_val_json = json.loads(part_val)
                if (upload_id == part_val_json["Upload-ID"]):
                    part_indx_present = True
                    break
                else:
                    pass

        assert not part_indx_present, "Error. Part index still exists"
        print("Part index deleted")
else:
    assert False, "Error: Unable to verify part index leak"


# ****** Delete bucket "seagatebucket" using s3-background-delete-svc account*****
AwsTest('Delete Bucket "seagatebucket"').delete_bucket("seagatebucket")\
   .execute_test().command_is_successful()

#Clear probable delete list index
s3kvs.clean_all_data()
s3kvs.create_s3root_index()

# ************ Delete Account*******************************
test_msg = "Delete account s3-background-delete-svc"
account_args = {'AccountName': 's3-background-delete-svc',\
                'Email': 's3-background-delete-svc@seagate.com', 'force': True}
AuthTest(test_msg).delete_account(**account_args).execute_test()\
    .command_response_should_have("Account deleted successfully")

# Restore s3backgrounddelete config file.
restore_configuration()
