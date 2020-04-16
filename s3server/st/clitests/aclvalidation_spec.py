import os
import yaml
import json
from framework import Config
from framework import S3PyCliTest
from awss3api import AwsTest
from s3cmd import S3cmdTest
from s3client_config import S3ClientConfig
from aclvalidation import AclTest
from auth import AuthTest

# Helps debugging
Config.log_enabled = True
# Config.dummy_run = True
# Config.client_execution_timeout = 300 * 1000
# Config.request_timeout = 300 * 1000
# Config.socket_timeout = 300 * 1000


defaultAcp_relative = os.path.join(os.path.dirname(__file__), 'acp_files', 'default_acp.json')
defaultAcp = "file://" + os.path.abspath(defaultAcp_relative)

allGroupACP_relative = os.path.join(os.path.dirname(__file__), 'acp_files', 'group_acp.json')
allGroupACP = "file://" + os.path.abspath(allGroupACP_relative)

fullACP_relative = os.path.join(os.path.dirname(__file__), 'acp_files', 'full_acp.json')
fullACP = "file://" + os.path.abspath(fullACP_relative)

valid_acl_wihtout_displayname_relative = os.path.join(os.path.dirname(__file__), 'acp_files', 'valid_acl_wihtout_displayname.json')
valid_acl_wihtout_displayname = "file://" + os.path.abspath(valid_acl_wihtout_displayname_relative)

invalid_acl_incorrect_granteeid_relative = os.path.join(os.path.dirname(__file__), 'acp_files', 'invalid_acl_incorrect_granteeid.json')
invalid_acl_incorrect_granteeid = "file://" + os.path.abspath(invalid_acl_incorrect_granteeid_relative)

invalid_acl_incorrect_granteename_relative = os.path.join(os.path.dirname(__file__), 'acp_files', 'invalid_acl_incorrect_granteename.json')
invalid_acl_incorrect_granteename = "file://" + os.path.abspath(invalid_acl_incorrect_granteename_relative)

invalid_acl_incorrect_ownerid_relative = os.path.join(os.path.dirname(__file__), 'acp_files', 'invalid_acl_incorrect_ownerid.json')
invalid_acl_incorrect_ownerid = "file://" + os.path.abspath(invalid_acl_incorrect_ownerid_relative)

invalid_acl_incorrect_ownername_relative = os.path.join(os.path.dirname(__file__), 'acp_files', 'invalid_acl_incorrect_ownername.json')
invalid_acl_incorrect_ownername = "file://" + os.path.abspath(invalid_acl_incorrect_ownername_relative)

invalid_acl_incorrect_granteeemail_relative = os.path.join(os.path.dirname(__file__), 'acp_files', 'invalid_acl_incorrect_granteeemail.json')
invalid_acl_incorrect_granteeemail = "file://" + os.path.abspath(invalid_acl_incorrect_granteeemail_relative)

invalid_acl_owner_id = os.path.join(os.path.dirname(__file__), 'acp_files', 'invalid_acl_owner_id.json')
invalid_acl_owner_id = "file://" + os.path.abspath(invalid_acl_owner_id)

# Load test config file
def load_test_config():
    conf_file = os.path.join(os.path.dirname(__file__),'s3iamcli_test_config.yaml')
    with open(conf_file, 'r') as f:
            config = yaml.safe_load(f)
            S3ClientConfig.ldapuser = config['ldapuser']
            S3ClientConfig.ldappasswd = config['ldappasswd']

# Run before all to setup the test environment.
def before_all():
    load_test_config()
    print("Configuring LDAP")
    S3PyCliTest('Before_all').before_all()

# Run before all to setup the test environment.
before_all()

#******** Create accounts************

# Create secondary account
test_msg = "Create account secaccount"
account_args = {'AccountName': 'secaccount', 'Email': 'secaccount@seagate.com', 'ldapuser': S3ClientConfig.ldapuser, 'ldappasswd': S3ClientConfig.ldappasswd}
account_response_pattern = "AccountId = [\w-]*, CanonicalId = [\w-]*, RootUserName = [\w+=,.@-]*, AccessKeyId = [\w-]*, SecretKey = [\w/+]*$"
result = AuthTest(test_msg).create_account(**account_args).execute_test()
result.command_should_match_pattern(account_response_pattern)
account_response_elements = AuthTest.get_response_elements(result.status.stdout)

secondary_access_key = account_response_elements['AccessKeyId']
secondary_secret_key = account_response_elements['SecretKey']
secondary_id = account_response_elements['CanonicalId']
secondary_name = 'secaccount'
sec_cannonical_id = "id=" + secondary_id

#********  Get ACL **********
bucket="seagatebucketobjectacl"
AwsTest('Aws can create bucket').create_bucket(bucket).execute_test().command_is_successful()

result=AwsTest('Aws can get bucket acl').get_bucket_acl(bucket).execute_test().command_is_successful()

print("Bucket ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('validate complete acl').validate_acl(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL")
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('acl has valid Grants').validate_grant(result, "C12345", "s3_test", 1, "FULL_CONTROL")
print("ACL validation Completed..")

AwsTest('Aws can create object').put_object(bucket, "testObject").execute_test().command_is_successful()

result=AwsTest('Aws can get object acl').get_object_acl(bucket, "testObject").execute_test().command_is_successful()

#******** Validate put-bucket-acl with email which does not exists ********
AwsTest('AWS can not put bucket acl with ganteee does not exists')\
.put_bucket_acl_with_acp_file(bucket, invalid_acl_incorrect_granteeemail)\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("UnresolvableGrantByEmailAddress")

print("Object ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('validate complete acl').validate_acl(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL")
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('acl has valid Grants').validate_grant(result, "C12345", "s3_test", 1, "FULL_CONTROL")
print("ACL validation Completed..")

AwsTest('Aws can list objects in bucket').list_objects(bucket).execute_test().command_is_successful()\
    .command_response_should_have("testObject")

os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('Cross account can not list objects in bucket')\
.list_objects(bucket).execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

print("Validate put object acl with default ACP XML")
AwsTest('Aws can put object acl').put_object_acl_with_acp_file(bucket, "testObject", defaultAcp)\
    .execute_test().command_is_successful()

result=AwsTest('Aws can get object acl').get_object_acl(bucket, "testObject").execute_test().command_is_successful()

print("Object ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('validate complete acl').validate_acl(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL")
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('acl has valid Grants').validate_grant(result, "C12345", "s3_test", 1, "FULL_CONTROL")
print("ACL validation Completed..")

print("Validate put object acl with all groups ACP XML")
AwsTest('Aws can put object acl').put_object_acl_with_acp_file(bucket, "testObject", allGroupACP)\
    .execute_test().command_is_successful()

result=AwsTest('Aws can get object acl').get_object_acl(bucket, "testObject").execute_test().command_is_successful()

print("Object ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('acl has valid Grants').validate_grant(result, "C12345", "s3_test", 4, "FULL_CONTROL")
print("ACL validation Completed..")

print("Validate put object acl with complete ACP XML that includes Grants of type - CanonicalUser/Group/Email")
AwsTest('Aws can put object acl').put_object_acl_with_acp_file(bucket, "testObject", fullACP)\
    .execute_test().command_is_successful()

result=AwsTest('Aws can get object acl').get_object_acl(bucket, "testObject").execute_test().command_is_successful()

print("Object ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('acl has valid Grants').validate_grant(result, "C12345", "s3_test", 4, "FULL_CONTROL")
print("ACL validation Completed..")

print("Validate put object acl with ACP XML without DisplayName for owner/grants")
AwsTest('Aws can put object acl').put_object_acl_with_acp_file(bucket, "testObject", valid_acl_wihtout_displayname)\
    .execute_test().command_is_successful()

result=AwsTest('Aws can get object acl').get_object_acl(bucket, "testObject").execute_test().command_is_successful()

print("Object ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)

AwsTest('Aws can put object acl').put_object_acl_with_acp_file(bucket, "testObject", invalid_acl_incorrect_granteeid)\
    .execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidArgument")

AwsTest('Aws can put object acl').put_object_acl_with_acp_file(bucket, "testObject", invalid_acl_incorrect_granteename)\
    .execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidArgument")

AwsTest('Aws can put object acl').put_object_acl_with_acp_file(bucket, "testObject", invalid_acl_incorrect_ownerid)\
    .execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidArgument")

AwsTest('Aws can put object acl').put_object_acl_with_acp_file(bucket, "testObject", invalid_acl_incorrect_ownername)\
    .execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidArgument")

AwsTest('Aws can put object acl').put_object_acl_with_acp_file(bucket, "testObject", invalid_acl_owner_id)\
    .execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidArgument")

#*********** Negative case to fetch bucket acl for non-existing bucket ****************************
AwsTest('Aws can not fetch bucket acl of non-existing bucket').get_bucket_acl("seagateinvalidbucket").execute_test(negative_case=True)\
    .command_should_fail().command_error_should_have("NoSuchBucket")

#*********** Negative case to fetch object acl for non-existing object ****************************
AwsTest('Aws can not fetch object acl of non-existing object').get_object_acl(bucket, "testObjectInvalid").execute_test(negative_case=True)\
    .command_should_fail().command_error_should_have("NoSuchKey")

#*********** Negative case to fetch object acl for non-existing bucket ****************************
AwsTest('Aws can not fetch object acl of non-existing bucket').get_object_acl("seagateinvalidbucketobjectacl", "testObject").execute_test(negative_case=True)\
    .command_should_fail().command_error_should_have("NoSuchBucket")

AwsTest('Aws can delete object').delete_object(bucket,"testObject").execute_test().command_is_successful()

AwsTest('Aws can delete bucket').delete_bucket(bucket).execute_test().command_is_successful()

#******** Create Bucket with default account ********
AwsTest('Aws can create bucket').create_bucket("seagatebucketacl").execute_test().command_is_successful()
AwsTest('AWS can put bucket acl with private canned input')\
.put_bucket_acl_with_canned_input("seagatebucketacl", "private").execute_test().command_is_successful()
AwsTest('AWS can do head-bucket').head_bucket("seagatebucketacl").execute_test().command_is_successful()

#*********** validate default bucket ACL *********

# Validate get-bucket-acl
result=AwsTest('Aws can get bucket acl').get_bucket_acl("seagatebucketacl").execute_test().command_is_successful()

print("Bucket ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('validate complete acl').validate_acl(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL")
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('acl has valid Grants').validate_grant(result, "C12345", "s3_test", 1, "FULL_CONTROL")
print("ACL validation Completed..")

# secondary account can not get the default bucket acl
os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('Other AWS account can not get default bucket acl').get_bucket_acl("seagatebucketacl")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

#********** validate get default object acl *********

# Put object in bucket - seagatebucketacl
AwsTest('Aws can create object').put_object("seagatebucketacl", "testObject").execute_test().command_is_successful()

# validate get object acl for default account
result=AwsTest('Validate the object acl').get_object_acl("seagatebucketacl", "testObject").execute_test().command_is_successful()

print("Object ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('validate complete acl').validate_acl(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL")
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('acl has valid Grants').validate_grant(result, "C12345", "s3_test", 1, "FULL_CONTROL")
print("ACL validation Completed..")

# secondary account can not get default object acl
os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('Aws can get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

# secondary account can not get object
os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('Unauthorized account can not get object').get_object("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

# secondary account can not put object into bucket
os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('Unauthorized account can not put object').put_object("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

#********** Validate put-object along with canned ACL - requester is primary default account **********

# Put object with canned acl - private
AwsTest('Aws can create object with \'private\' canned acl input')\
.put_object("seagatebucketacl", "testObject", canned_acl="private").execute_test().command_is_successful()

result=AwsTest('Validate the object acl').get_object_acl("seagatebucketacl", "testObject").execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('validate complete acl').validate_acl(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL")
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('acl has valid Grants').validate_grant(result, "C12345", "s3_test", 1, "FULL_CONTROL")
print("ACL validation Completed..")

AwsTest('Owner account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test().command_is_successful()
# Change back the ACL
AwsTest('Aws can create object with \'private\' canned acl input')\
.put_object("seagatebucketacl", "testObject", canned_acl="private").execute_test().command_is_successful()

os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('Unauthorized account can not get object').get_object("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

# Put object with canned acl - public-read
AwsTest('Aws can create object with \'public-read\' canned acl input')\
.put_object("seagatebucketacl", "testObject", canned_acl="public-read").execute_test().command_is_successful()

result=AwsTest('Validate the object acl').get_object_acl("seagatebucketacl", "testObject").execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('validate complete acl').validate_acl_single_group_grant(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL",\
"http://acs.amazonaws.com/groups/global/AllUsers", "READ")
print("ACL validation Completed..")

AwsTest('Owner account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test().command_is_successful()
# Change back the ACL
AwsTest('Aws can create object with \'public-read\' canned acl input')\
.put_object("seagatebucketacl", "testObject", canned_acl="public-read").execute_test().command_is_successful()

os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('Public account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Public account can not do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

# Put object with canned acl - public-read-write
AwsTest('Aws can create object with \'public-read-write\' canned acl input')\
.put_object("seagatebucketacl", "testObject", canned_acl="public-read-write").execute_test().command_is_successful()

result=AwsTest('Validate the object acl').get_object_acl("seagatebucketacl", "testObject").execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('validate complete acl').validate_acl_dual_group_grant(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL",\
"http://acs.amazonaws.com/groups/global/AllUsers", "READ", "http://acs.amazonaws.com/groups/global/AllUsers", "WRITE")
print("ACL validation Completed..")

AwsTest('Owner account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test().command_is_successful()
# Change back the ACL
AwsTest('Aws can create object with \'public-read-write\' canned acl input')\
.put_object("seagatebucketacl", "testObject", canned_acl="public-read-write").execute_test().command_is_successful()

os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('Public account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Public account can not do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

# Put object with canned acl - authenticated-read
AwsTest('Aws can create object with \'authenticated-read\' canned acl input')\
.put_object("seagatebucketacl", "testObject", canned_acl="authenticated-read").execute_test().command_is_successful()

result=AwsTest('Validate the object acl').get_object_acl("seagatebucketacl", "testObject").execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('validate complete acl').validate_acl_single_group_grant(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL",\
"http://acs.amazonaws.com/groups/global/AuthenticatedUsers", "READ")
print("ACL validation Completed..")

AwsTest('Owner account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test().command_is_successful()
# Change back the ACL
AwsTest('Aws can create object with \'authenticated-read\' canned acl input')\
.put_object("seagatebucketacl", "testObject", canned_acl="authenticated-read").execute_test().command_is_successful()

os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('Public account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Public account can not do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

# Put object with canned acl - bucket-owner-read - requester is the bucket owner
AwsTest('Aws can create object with \'bucket-owner-read\' canned acl input')\
.put_object("seagatebucketacl", "testObject", canned_acl="bucket-owner-read").execute_test().command_is_successful()

result=AwsTest('Validate the object acl').get_object_acl("seagatebucketacl", "testObject").execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('validate complete acl').validate_acl(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL")
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('acl has valid Grants').validate_grant(result, "C12345", "s3_test", 1, "FULL_CONTROL")
print("ACL validation Completed..")

AwsTest('Owner account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test().command_is_successful()
# Change back the ACL
AwsTest('Aws can create object with \'bucket-owner-read\' canned acl input')\
.put_object("seagatebucketacl", "testObject", canned_acl="bucket-owner-read").execute_test().command_is_successful()

os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('Unauthorized account can not get object').get_object("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

# Put object with canned acl - bucket-owner-full-control - requester is the bucket owner
AwsTest('Aws can create object with \'bucket-owner-full-control\' canned acl input')\
.put_object("seagatebucketacl", "testObject", canned_acl="bucket-owner-full-control").execute_test().command_is_successful()

result=AwsTest('Validate the object acl').get_object_acl("seagatebucketacl", "testObject").execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('validate complete acl').validate_acl(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL")
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('acl has valid Grants').validate_grant(result, "C12345", "s3_test", 1, "FULL_CONTROL")
print("ACL validation Completed..")

AwsTest('Owner account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test().command_is_successful()
# Change back the ACL
AwsTest('Aws can create object with \'bucket-owner-full-control\' canned acl input')\
.put_object("seagatebucketacl", "testObject", canned_acl="bucket-owner-full-control").execute_test().command_is_successful()

os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('Unauthorized account can not get object').get_object("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

#********** Validate put-object along with canned ACL - requester is secondary account different from bucket owner **********

# Add write permission for secaccount to seagatebucketacl
AwsTest('AWS can put bucket acl with public-read-write canned input')\
.put_bucket_acl_with_canned_input("seagatebucketacl", "public-read-write").execute_test().command_is_successful()

os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
# Put object with canned acl - private
AwsTest('Aws can create object with \'private\' canned acl input')\
.put_object("seagatebucketacl", "testObject", canned_acl="private").execute_test().command_is_successful()

result=AwsTest('Validate the object acl').get_object_acl("seagatebucketacl", "testObject").execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('validate complete acl').validate_acl(result, secondary_id, secondary_name, secondary_id, secondary_name, "FULL_CONTROL")
AclTest('acl has valid Owner').validate_owner(result, secondary_id, secondary_name)
AclTest('acl has valid Grants').validate_grant(result, secondary_id, secondary_name, 1, "FULL_CONTROL")
print("ACL validation Completed..")

AwsTest('Owner account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test().command_is_successful()
# Change back the ACL
AwsTest('Aws can create object with \'private\' canned acl input')\
.put_object("seagatebucketacl", "testObject", canned_acl="private").execute_test().command_is_successful()

del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Unauthorized account can not get object').get_object("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")

# Put object with canned acl - bucket-owner-read - requester is NOT the bucket owner
os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('Aws can create object with \'bucket-owner-read\' canned acl input')\
.put_object("seagatebucketacl", "testObject", canned_acl="bucket-owner-read").execute_test().command_is_successful()

result=AwsTest('Validate the object acl').get_object_acl("seagatebucketacl", "testObject").execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('validate complete acl').validate_acl_dual_grant\
(result, secondary_id, secondary_name, secondary_id, secondary_name, "FULL_CONTROL", "C12345", "s3_test", "READ")
AclTest('acl has valid Owner').validate_owner(result, secondary_id, secondary_name)
print("ACL validation Completed..")

AwsTest('Owner account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test().command_is_successful()
# Change back the ACL
AwsTest('Aws can create object with \'bucket-owner-read\' canned acl input')\
.put_object("seagatebucketacl", "testObject", canned_acl="bucket-owner-read").execute_test().command_is_successful()

del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Bucket owner can get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Bucket owner can not do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Bucket owner can not do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")

# Put object with canned acl - bucket-owner-full-control - requester is NOT the bucket owner
os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('Aws can create object with \'bucket-owner-full-control\' canned acl input')\
.put_object("seagatebucketacl", "testObject", canned_acl="bucket-owner-full-control").execute_test().command_is_successful()

result=AwsTest('Validate the object acl').get_object_acl("seagatebucketacl", "testObject").execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('validate complete acl').validate_acl_dual_grant\
(result, secondary_id, secondary_name, secondary_id, secondary_name, "FULL_CONTROL", "C12345", "s3_test", "FULL_CONTROL")
AclTest('acl has valid Owner').validate_owner(result, secondary_id, secondary_name)
print("ACL validation Completed..")

AwsTest('Owner account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test().command_is_successful()
# Change back the ACL
AwsTest('Aws can create object with \'bucket-owner-full-control\' canned acl input')\
.put_object("seagatebucketacl", "testObject", canned_acl="bucket-owner-full-control").execute_test().command_is_successful()

del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Bucket owner can get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Bucket owner can do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Bucket owner can do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-full-control" , sec_cannonical_id)\
.execute_test().command_is_successful()

# Put object with invalid canned acl
AwsTest('Aws can not create object with invalid \'private123\' canned acl input')\
.put_object("seagatebucketacl", "testObject", canned_acl="private123").execute_test(negative_case=True)\
    .command_should_fail()

#********** Validate put-object-acl along with canned ACL **********

# Refresh operation - Put object in bucket - seagatebucketacl
AwsTest('Aws can create object').put_object("seagatebucketacl", "testObject").execute_test().command_is_successful()

# Put object acl with canned acl - private
AwsTest('Aws can put object acl with \'private\' canned acl input')\
.put_object_acl_with_canned_input("seagatebucketacl", "testObject", "private").execute_test().command_is_successful()

result=AwsTest('Validate the object acl').get_object_acl("seagatebucketacl", "testObject").execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('validate complete acl').validate_acl(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL")
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('acl has valid Grants').validate_grant(result, "C12345", "s3_test", 1, "FULL_CONTROL")
print("ACL validation Completed..")

AwsTest('Owner account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
# Change back the ACL
AwsTest('Aws can put object acl with \'private\' canned acl input')\
.put_object_acl_with_canned_input("seagatebucketacl", "testObject", "private").execute_test().command_is_successful()

os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('Unauthorized account can not get object').get_object("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

# Put object acl with canned acl - public-read
AwsTest('Aws owner can put object acl with \'public-read\' canned acl input')\
.put_object_acl_with_canned_input("seagatebucketacl", "testObject", "public-read").execute_test().command_is_successful()

result=AwsTest('Validate the object acl').get_object_acl("seagatebucketacl", "testObject").execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('validate complete acl').validate_acl_single_group_grant(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL",\
"http://acs.amazonaws.com/groups/global/AllUsers", "READ")
print("ACL validation Completed..")

AwsTest('Owner account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
# Change back the ACL
AwsTest('Aws can put object acl with \'public-read\' canned acl input')\
.put_object_acl_with_canned_input("seagatebucketacl", "testObject", "public-read").execute_test().command_is_successful()

os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('Public account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Public account can not do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

# Put object acl with canned acl - public-read-write
AwsTest('Aws owner can put object acl with \'public-read-write\' canned acl input')\
.put_object_acl_with_canned_input("seagatebucketacl", "testObject", "public-read-write").execute_test().command_is_successful()

result=AwsTest('Validate the object acl').get_object_acl("seagatebucketacl", "testObject").execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('validate complete acl').validate_acl_dual_group_grant(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL",\
"http://acs.amazonaws.com/groups/global/AllUsers", "READ", "http://acs.amazonaws.com/groups/global/AllUsers", "WRITE")
print("ACL validation Completed..")

AwsTest('Owner account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test().command_is_successful()
# Change back the ACL
AwsTest('Aws can put object acl with \'public-read-write\' canned acl input')\
.put_object_acl_with_canned_input("seagatebucketacl", "testObject", "public-read-write").execute_test().command_is_successful()

os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('Public account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Public account can not do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

# Put object acl with canned acl - authenticated-read
AwsTest('Aws can put object acl with \'authenticated-read\' canned acl input')\
.put_object_acl_with_canned_input("seagatebucketacl", "testObject", "authenticated-read").execute_test().command_is_successful()

result=AwsTest('Validate the object acl').get_object_acl("seagatebucketacl", "testObject").execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('validate complete acl').validate_acl_single_group_grant(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL",\
"http://acs.amazonaws.com/groups/global/AuthenticatedUsers", "READ")
print("ACL validation Completed..")

AwsTest('Owner account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
# Change back the ACL
AwsTest('Aws can put object acl with \'authenticated-read\' canned acl input')\
.put_object_acl_with_canned_input("seagatebucketacl", "testObject", "authenticated-read").execute_test().command_is_successful()

os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('Public account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Public account can not do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

# Put object with canned acl - bucket-owner-read - requester is the bucket owner
AwsTest('Aws can put object acl with \'bucket-owner-read\' canned acl input')\
.put_object_acl_with_canned_input("seagatebucketacl", "testObject", "bucket-owner-read").execute_test().command_is_successful()

result=AwsTest('Validate the object acl').get_object_acl("seagatebucketacl", "testObject").execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('validate complete acl').validate_acl(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL")
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('acl has valid Grants').validate_grant(result, "C12345", "s3_test", 1, "FULL_CONTROL")
print("ACL validation Completed..")

AwsTest('Owner account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test().command_is_successful()
# Change back the ACL
AwsTest('Aws can put object acl with \'bucket-owner-read\' canned acl input')\
.put_object_acl_with_canned_input("seagatebucketacl", "testObject", "bucket-owner-read").execute_test().command_is_successful()

os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('Unauthorized account can not get object').get_object("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

# Put object acl with canned acl - bucket-owner-full-control - requester, bucket owner and object owner are same
AwsTest('Aws can put object acl with \'bucket-owner-full-control\' canned acl input')\
.put_object_acl_with_canned_input("seagatebucketacl", "testObject", "bucket-owner-full-control").execute_test().command_is_successful()

result=AwsTest('Validate the object acl').get_object_acl("seagatebucketacl", "testObject").execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('validate complete acl').validate_acl(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL")
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('acl has valid Grants').validate_grant(result, "C12345", "s3_test", 1, "FULL_CONTROL")
print("ACL validation Completed..")

AwsTest('Owner account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test().command_is_successful()
# Change back the ACL
AwsTest('Aws can put object acl with \'bucket-owner-full-control\' canned acl input')\
.put_object_acl_with_canned_input("seagatebucketacl", "testObject", "bucket-owner-full-control").execute_test().command_is_successful()

os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('Unauthorized account can not get object').get_object("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

#********** Put object acl with canned acl - requestor is secaccount - different from bucket owner ************

# Refresh operation - Put object in bucket - seagatebucketacl
AwsTest('Aws can create object').put_object_with_permission_headers("seagatebucketacl", "testObject", "grant-full-control", sec_cannonical_id)\
.execute_test().command_is_successful()

os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
# Put object acl with canned acl - private
AwsTest('Secondary account can put object acl with \'private\' canned acl input')\
.put_object_acl_with_canned_input("seagatebucketacl", "testObject", "private").execute_test().command_is_successful()

AwsTest('Unauthorized account can not get object').get_object("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Public account can not do put object acl').put_object_acl("seagatebucketacl", "testObject", "grant-read" , sec_cannonical_id)\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")

del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
result=AwsTest('Validate the object acl').get_object_acl("seagatebucketacl", "testObject").execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('validate complete acl').validate_acl(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL")
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('acl has valid Grants').validate_grant(result, "C12345", "s3_test", 1, "FULL_CONTROL")
print("ACL validation Completed..")

AwsTest('Owner account can do get object').get_object("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Owner account can do get object acl').get_object_acl("seagatebucketacl", "testObject")\
.execute_test().command_is_successful()
AwsTest('Secondary account can put object acl with \'private\' canned acl input')\
.put_object_acl_with_canned_input("seagatebucketacl", "testObject", "private").execute_test().command_is_successful()
AwsTest('Do head-object for  "testObject" on bucket "seagatebucketacl"')\
.head_object("seagatebucketacl", "testObject").execute_test().command_is_successful()

# Put object acl with invalid canned acl
AwsTest('Aws can not put object acl with invalid \'private123\' canned acl input')\
.put_object_acl_with_canned_input("seagatebucketacl", "testObject", "private123").execute_test(negative_case=True)\
    .command_should_fail()

# Put bucket with bucket-owner-read canned acl - bucket should be created with default acl
AwsTest('Aws can create bucket by ignoring bucket-owner-read canned acl').put_bucket_canned_acl("seagatebucket01", "bucket-owner-read")\
    .execute_test().command_is_successful()
result=AwsTest('Validate the object acl').get_bucket_acl("seagatebucket01").execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('validate complete acl').validate_acl(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL")
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('acl has valid Grants').validate_grant(result, "C12345", "s3_test", 1, "FULL_CONTROL")
print("ACL validation Completed..")

# Put bucket with bucket-owner-full-control canned acl - bucket should be created with default acl
AwsTest('Aws can create bucket by ignoring bucket-owner-full-control canned acl').put_bucket_canned_acl("seagatebucket02", "bucket-owner-full-control")\
    .execute_test().command_is_successful()
result=AwsTest('Validate the object acl').get_bucket_acl("seagatebucket02").execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('validate complete acl').validate_acl(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL")
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('acl has valid Grants').validate_grant(result, "C12345", "s3_test", 1, "FULL_CONTROL")
print("ACL validation Completed..")

AwsTest('AWS can not put bucket acl with log-delivery-write canned input')\
.put_bucket_acl_with_canned_input("seagatebucket02", "log-delivery-write").execute_test(negative_case=True)\
    .command_should_fail().command_error_should_have("OperationNotSupported")

AwsTest('AWS can not put bucket acl with aws-exec-read canned input')\
.put_bucket_acl_with_canned_input("seagatebucket02", "aws-exec-read").execute_test(negative_case=True)\
    .command_should_fail().command_error_should_have("OperationNotSupported")

AwsTest('AWS can delete bucket seagatebucket01').delete_bucket("seagatebucket01").execute_test().command_is_successful()

AwsTest('AWS can delete bucket seagatebucket02').delete_bucket("seagatebucket02").execute_test().command_is_successful()

#********** Delete object *************

# Secondary account can not delete object from seagatebucketacl
'''os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('Aws can not delete object owned by another account without permission')\
.delete_object("seagatebucketacl", "testObject").execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]'''

# Default acount can delete object from its own bucket
AwsTest('Aws can delete object owned by itself').delete_object("seagatebucketacl","testObject").execute_test().command_is_successful()
AwsTest('Aws can delete object owned by itself').delete_object("seagatebucketacl","testObject1").execute_test().command_is_successful()
#********** Delete bucket *************

# Secondary account can not delete bucket
'''os.environ["AWS_ACCESS_KEY_ID"] = secondary_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = secondary_secret_key
AwsTest('AWS can not delete bucket owned by another account without permission')\
.delete_bucket("seagatebucketacl").execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]'''

# Default account can delete bucket seagatebucketacl
AwsTest('Account owner can delete bucket').delete_bucket("seagatebucketacl").execute_test().command_is_successful()

# ********** Cleanup ****************

# delete secondary account
test_msg = "Delete account secaccount"
account_args = {'AccountName': 'secaccount', 'Email': 'secaccount@seagate.com',  'force': True}
S3ClientConfig.access_key_id = secondary_access_key
S3ClientConfig.secret_key = secondary_secret_key
AuthTest(test_msg).delete_account(**account_args).execute_test()\
            .command_response_should_have("Account deleted successfully")

#*************** Test Case 1 ***************
# TODO Enable below tests once permission header feature available
test_msg = "Create account testAccount"
account_args = {'AccountName': 'testAccount', 'Email': 'testAccount@seagate.com', 'ldapuser': "sgiamadmin", 'ldappasswd': "ldapadmin"}
account_response_pattern = "AccountId = [\w-]*, CanonicalId = [\w-]*, RootUserName = [\w+=,.@-]*, AccessKeyId = [\w-]*, SecretKey = [\w/+]*$"
result = AuthTest(test_msg).create_account(**account_args).execute_test()
result.command_should_match_pattern(account_response_pattern)
account_response_elements = AuthTest.get_response_elements(result.status.stdout)
testAccount_access_key = account_response_elements['AccessKeyId']
testAccount_secret_key = account_response_elements['SecretKey']
testAccount_cannonicalid = account_response_elements['CanonicalId']
testAccount_email = "testAccount@seagate.com"
#
AwsTest('Aws can create bucket').create_bucket("putobjacltestbucket").execute_test().command_is_successful()
cannonical_id = "id=" + testAccount_cannonicalid
AwsTest('Aws can upload 3k file with tags').put_object_with_permission_headers("putobjacltestbucket", "3kfile", "grant-read" , cannonical_id ).execute_test().command_is_successful()
result=AwsTest('Aws can get object acl').get_object_acl("putobjacltestbucket", "3kfile").execute_test(negative_case=True).command_is_successful().command_response_should_have("testAccount")
AwsTest('Aws can upload 3k file with tags').put_object_acl("putobjacltestbucket", "3kfile", "grant-read-acp" , cannonical_id ).execute_test().command_is_successful()
result=AwsTest('Aws can get object acl').get_object_acl("putobjacltestbucket", "3kfile").execute_test().command_is_successful().command_response_should_have("testAccount")
AwsTest('Aws can delete object').delete_object("putobjacltestbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("putobjacltestbucket").execute_test().command_is_successful()
#
test_msg = "Create account testAccount2"
account_args = {'AccountName': 'testAccount2', 'Email': 'testAccount2@seagate.com', 'ldapuser': "sgiamadmin", 'ldappasswd': "ldapadmin"}
account_response_pattern = "AccountId = [\w-]*, CanonicalId = [\w-]*, RootUserName = [\w+=,.@-]*, AccessKeyId = [\w-]*, SecretKey = [\w/+]*$"
result = AuthTest(test_msg).create_account(**account_args).execute_test()
result.command_should_match_pattern(account_response_pattern)
account_response_elements = AuthTest.get_response_elements(result.status.stdout)
testAccount2_access_key = account_response_elements['AccessKeyId']
testAccount2_secret_key = account_response_elements['SecretKey']
testAccount2_cannonicalid = account_response_elements['CanonicalId']
testAccount2_email = "testAccount2@seagate.com"
##***************Test Case 2 ******************
cannonical_id = "id=" + testAccount_cannonicalid
AwsTest('Aws can create bucket').create_bucket_with_permission_headers("authorizationtestingbucket" , "grant-write", cannonical_id).execute_test().command_is_successful()
os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Aws can upload 3k file with tags').put_object("authorizationtestingbucket", "3kfile" ).execute_test().command_is_successful()
AwsTest('Aws can delete object').delete_object("authorizationtestingbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("authorizationtestingbucket").execute_test().command_is_successful()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

##**************** Test Case 3 ************

AwsTest('Aws can create bucket').create_bucket("authbucket").execute_test().command_is_successful()
cannonical_id = "id=" + testAccount_cannonicalid
AwsTest('Aws can upload 3k file with tags').put_object_with_permission_headers("authbucket", "3kfile", "grant-read" , cannonical_id ).execute_test().command_is_successful()
os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Aws can get acl').get_object("authbucket", "3kfile").execute_test().command_is_successful()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Aws can delete object').delete_object("authbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("authbucket").execute_test().command_is_successful()

##**************** Test Case 4 ************

AwsTest('Aws can create bucket').create_bucket("authbucket").execute_test().command_is_successful()
cannonical_id = "id=" + testAccount_cannonicalid
AwsTest('Aws can put bucket acl').put_bucket_acl("authbucket", "grant-read-acp" , cannonical_id ).execute_test().command_is_successful()
os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Aws can put bucket acl').get_bucket_acl("authbucket").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("authbucket").execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Aws can put bucket acl').put_bucket_acl("authbucket", "grant-full-control", cannonical_id ).execute_test().command_is_successful()
os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Aws can delete bucket').delete_bucket("authbucket").execute_test().command_is_successful()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

##************* Test Case 5 ********************
AwsTest('Aws can create bucket').create_bucket("authbucket").execute_test().command_is_successful()
cannonical_id = "id=" + testAccount_cannonicalid
AwsTest('Aws can upload 3k file with tags').put_object("authbucket", "3kfile").execute_test().command_is_successful()
os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Aws can put acl').put_object_acl("authbucket", "3kfile", "grant-full-control" , cannonical_id ).execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Aws can delete object').delete_object("authbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("authbucket").execute_test().command_is_successful()

##**************** Test Case 6 ************
#
AwsTest('Aws can create bucket').create_bucket("testbucket").execute_test().command_is_successful()
cannonical_id_assignment = "id="
AwsTest('Aws can put bucket acl').put_bucket_acl("testbucket", "grant-read" , cannonical_id_assignment ).execute_test(negative_case=True).command_should_fail()

#*************** put-bucket-acl - with more than one acl options ***********
cmd = "aws s3api put-bucket-acl --bucket testbucket --acl bucket-owner-full-control\
 --access-control-policy " + fullACP
AwsTest('Aws can not put bucket acl with canned acl and aclxml').put_acl_with_multiple_options(cmd).\
execute_test(negative_case=True).command_should_fail().command_error_should_have("UnexpectedContent")
cmd = "aws s3api put-bucket-acl --bucket testbucket --grant-read " + "emailaddress=" +\
testAccount2_email + " --access-control-policy " + fullACP
AwsTest('Aws can not put bucket acl with aclxml and permissionheader').\
put_acl_with_multiple_options(cmd).execute_test(negative_case=True).\
command_should_fail().command_error_should_have("UnexpectedContent")
cmd = "aws s3api put-bucket-acl --bucket testbucket --grant-read " + \
"emailaddress=" + testAccount2_email + " --acl bucket-owner-full-control"
AwsTest('Aws can not put bucket acl with cannedacl and permissionheader').\
put_acl_with_multiple_options(cmd).execute_test(negative_case=True).command_should_fail().\
command_error_should_have("InvalidRequest")
AwsTest('Aws can delete bucket').delete_bucket("testbucket").\
execute_test().command_is_successful()

##**************** Test Case 7 ************
AwsTest('Aws can create bucket').create_bucket("testbucket").execute_test().command_is_successful()
cannonical_id_assignment = "id=invalid"
AwsTest('Aws can put bucket acl').put_bucket_acl("testbucket", "grant-read" , cannonical_id_assignment ).execute_test(negative_case=True).command_should_fail()
AwsTest('Aws can upload 3k file with tags').put_object_with_permission_headers("testbucket", "3kfile", "grant-read" , cannonical_id_assignment).execute_test(negative_case=True).command_should_fail()
AwsTest('Aws can delete object').delete_object("testbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("testbucket").execute_test().command_is_successful()
##**************** Test Case 8 ************
#
AwsTest('Aws can create bucket').create_bucket("testbucket").execute_test().command_is_successful()
email_id_assignment = "emailaddress=invalidAddress@seagate.com"
AwsTest('Aws can put bucket acl').put_bucket_acl("testbucket", "grant-read" , email_id_assignment ).execute_test(negative_case=True).command_should_fail()
AwsTest('Aws can delete bucket').delete_bucket("testbucket").execute_test().command_is_successful()
##**************** Test Case 9 ************
email_id = "emailaddress=" + testAccount_email
AwsTest('Aws can create bucket').create_bucket("testbucket").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with tags').put_object_with_permission_headers("testbucket", "3kfile", "grant-read" , email_id ).execute_test().command_is_successful()
result=AwsTest('Aws cannot get object acl').get_object_acl("testbucket", "3kfile").execute_test().command_is_successful().command_response_should_have("testAccount")
AwsTest('Aws can delete object').delete_object("testbucket","3kfile").execute_test().command_is_successful()
##**************** Test Case 10 ************
email_id = "emailaddress=" + testAccount_email + ",emailaddress=" + "test@seagate.com"
AwsTest('Aws can upload 3k file with tags').put_object_with_permission_headers("testbucket", "3kfile", "grant-read" , email_id ).execute_test().command_is_successful()
result=AwsTest('Aws can get object acl').get_object_acl("testbucket", "3kfile").execute_test().command_is_successful().command_response_should_have("testAccount")
AwsTest('Aws can delete object').delete_object("testbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("testbucket").execute_test().command_is_successful()
##***************Test Case 11 ******************
cannonical_id = "id=" + testAccount_cannonicalid
AwsTest('Aws can create bucket').create_bucket_with_permission_headers("testbucket" , "grant-full-control", cannonical_id).execute_test().command_is_successful()
os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
result=AwsTest('Aws can get bucket acl').get_bucket_acl("testbucket").execute_test().command_is_successful().command_response_should_have("testAccount")
AwsTest('Aws can delete bucket').delete_bucket("testbucket").execute_test().command_is_successful()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
##**************** Test Case 12 ************
email_id = "emailAddress=" + testAccount_email+",EMAILaddress="+ "test@seagate.com"
AwsTest('Aws can create bucket').create_bucket("testbucket").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with permission headers').put_object_with_permission_headers("testbucket", "3kfile", "grant-read" , email_id ).execute_test().command_is_successful()
AwsTest('Aws can get object acl').get_object_acl("testbucket", "3kfile").execute_test().command_is_successful().command_response_should_have("testAccount")
os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Aws can get object').get_object("testbucket", "3kfile" ).execute_test().command_is_successful()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
os.environ["AWS_ACCESS_KEY_ID"] = testAccount2_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount2_secret_key
AwsTest('Aws can get object').get_object("testbucket", "3kfile" ).execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
AwsTest('Aws cannot delete bucket').delete_bucket("testbucket").execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Aws can get object').get_object("testbucket", "3kfile" ).execute_test().command_is_successful()
AwsTest('Aws can delete object').delete_object("testbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("testbucket").execute_test().command_is_successful()
##**************** Test Case 12 ************
ids = "id=" + testAccount_cannonicalid
AwsTest('Aws can create bucket').create_bucket_with_permission_headers("testbucket" , "grant-full-control", ids).execute_test().command_is_successful()
os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Aws can upload 3k file with tags').put_object("testbucket", "3kfile" ).execute_test().command_is_successful()
result = AwsTest('Aws can get object acl').get_object_acl("testbucket", "3kfile").execute_test().command_is_successful()
AclTest('acl has valid Owner').validate_owner(result, testAccount_cannonicalid , "testAccount")
result=AwsTest('Aws can get bucket acl').get_bucket_acl("testbucket").execute_test().command_is_successful()
AclTest('acl has valid Owner').validate_owner(result,"C12345", "s3_test")
print("Owner validation success")
AwsTest('Aws can delete object').delete_object("testbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("testbucket").execute_test().command_is_successful()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

##**************** Test Case 13 ************
grantees = "id=" + testAccount_cannonicalid +",EMAILaddress="+testAccount2_email
AwsTest('Aws can create bucket').create_bucket("testbucket").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with permission headers').put_object_with_permission_headers("testbucket", "3kfile", "grant-read" , grantees ).execute_test().command_is_successful()
AwsTest('Aws can get object acl').get_object_acl("testbucket", "3kfile").execute_test().command_is_successful().command_response_should_have("testAccount")
AwsTest('Aws can get object acl').get_object_acl("testbucket", "3kfile").execute_test().command_is_successful().command_response_should_have("testAccount2")
os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Aws can get object').get_object("testbucket", "3kfile" ).execute_test().command_is_successful()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
os.environ["AWS_ACCESS_KEY_ID"] = testAccount2_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount2_secret_key
AwsTest('Aws can get object').get_object("testbucket", "3kfile" ).execute_test().command_is_successful()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Aws can delete object').delete_object("testbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("testbucket").execute_test().command_is_successful()

##**************** Test Case 14 ************
grantees = "id=" + testAccount_cannonicalid +",EMAILaddress=invalidEmail@seagate.com"
AwsTest('Aws can create bucket').create_bucket("testbucket").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with permission headers').put_object_with_permission_headers("testbucket", "3kfile", "grant-read" , grantees ).execute_test(negative_case=True).command_should_fail()
AwsTest('Aws can delete object').delete_object("testbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("testbucket").execute_test().command_is_successful()

##**************** Test Case 15 ************
ids = "id=" + testAccount_cannonicalid
AwsTest('Aws can create bucket').create_bucket("testbucket").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with tags').put_object_with_permission_headers("testbucket", "3kfile","grant-write",ids).execute_test().command_is_successful()
result = AwsTest('Aws can get object acl').get_object_acl("testbucket", "3kfile").execute_test().command_is_successful()
AclTest('acl has valid Owner').validate_owner(result,"C12345", "s3_test")
print("Owner validation success")
AwsTest('Aws can delete object').delete_object("testbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("testbucket").execute_test().command_is_successful()

##**************** Test Case 16 ************
grantees = "id=" + testAccount_cannonicalid
AwsTest('Aws can create bucket').create_bucket("testbucket").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with permission headers').put_object_with_permission_headers("testbucket", "3kfile", "grant-read" , grantees ).execute_test().command_is_successful()
os.environ["AWS_ACCESS_KEY_ID"] = testAccount2_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount2_secret_key
AwsTest('Aws can get object').get_object("testbucket", "3kfile" ).execute_test(negative_case=True).command_should_fail()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Aws can get object').get_object("testbucket", "3kfile" ).execute_test().command_is_successful()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Aws can delete object').delete_object("testbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("testbucket").execute_test().command_is_successful()


##**************** Test Case 17 ************
bucket="testbucket1"
cannonical_id = "id=" + testAccount_cannonicalid
AwsTest('Aws can create bucket').create_bucket(bucket).execute_test().command_is_successful()
AwsTest('Aws can put bucket acl').put_bucket_acl(bucket, "grant-write-acp" , cannonical_id)\
.execute_test().command_is_successful()
#Check if second account has WRITE_ACP permission
result=AwsTest('Aws can get bucket acl').get_bucket_acl(bucket).execute_test().command_is_successful()
AclTest('validate complete acl').validate_acl(result, "C12345", "s3_test",\
 testAccount_cannonicalid, "testAccount", "WRITE_ACP")

os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Aws can put bucket acl with canned input i.e. public-read from second account')\
.put_bucket_acl_with_canned_input(bucket, "public-read").execute_test().command_is_successful()
result=AwsTest('Aws can get bucket acl from second account').get_bucket_acl(bucket)\
.execute_test().command_is_successful()
AclTest('validate complete acl').validate_acl_single_group_grant(result, "C12345", "s3_test",\
 testAccount_cannonicalid, "testAccount", "FULL_CONTROL", "http://acs.amazonaws.com/groups/global/AllUsers", "READ")
AwsTest('Aws can delete bucket from second account').delete_bucket(bucket).execute_test().command_is_successful()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

##**************** Test Case 18 ************
bucket="testbucket1"
cannonical_id = "id=" + testAccount_cannonicalid
AwsTest('Aws can create bucket').create_bucket(bucket).execute_test().command_is_successful()
AwsTest('Aws can create object with grant-write-acp to second account')\
.put_object_with_permission_headers(bucket, "testObject", "grant-write-acp", cannonical_id)\
.execute_test().command_is_successful()
#Check if second account has WRITE_ACP permission
result=AwsTest('Aws can get object acl from first account').get_object_acl(bucket, "testObject")\
.execute_test().command_is_successful()
AclTest('validate complete acl').validate_acl(result, "C12345", "s3_test",\
 testAccount_cannonicalid, "testAccount", "WRITE_ACP")

os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Validate the object acl from second account').get_object_acl(bucket, "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
# Put object with canned acl - public-read
AwsTest('Aws can create put object acl with \'public-read\' canned acl input from second account')\
.put_object_acl_with_canned_input(bucket, "testObject", "public-read").execute_test().command_is_successful()
#.put_object(bucket, "testObject", canned_acl="public-read").execute_test().command_is_successful()
result=AwsTest('Validate the object acl from second account should fail').get_object_acl(bucket, "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]

result=AwsTest('Aws can get object acl from first account').get_object_acl(bucket, "testObject")\
.execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('validate complete acl').validate_acl_single_group_grant(result, "C12345", "s3_test",\
 "C12345", "s3_test", "FULL_CONTROL", "http://acs.amazonaws.com/groups/global/AllUsers", "READ")
print("ACL validation Completed..")
AwsTest('Aws can delete object').delete_object(bucket,"testObject").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket(bucket).execute_test().command_is_successful()

##**************** Test Case 19 ************
bucket="testbucket3"
cannonical_id = "id=" + testAccount_cannonicalid
AwsTest('Aws can create bucket from first account').create_bucket(bucket).execute_test().command_is_successful()
AwsTest('Aws can upload test object from first account').put_object(bucket, "testObject").execute_test().command_is_successful()
AwsTest('Aws can put bucket acl with WRITE permission to second account')\
.put_bucket_acl(bucket, "grant-write" , cannonical_id ).execute_test().command_is_successful()
#Check if second account has WRITE permission
result=AwsTest('Aws can get bucket acl from first account').get_bucket_acl(bucket).execute_test().command_is_successful()
AclTest('validate complete bucket acl').validate_acl(result, "C12345", "s3_test",\
 testAccount_cannonicalid, "testAccount", "WRITE")
result=AwsTest('Aws can get object acl from first account').get_object_acl(bucket, "testObject")\
.execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('Object acl has valid Owner').validate_owner(result, "C12345", "s3_test")
AclTest('validate complete Object acl').validate_acl(result, "C12345", "s3_test",\
 "C12345", "s3_test", "FULL_CONTROL")
print("ACL validation Completed..")

os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Aws can create object with \'public-read\' canned acl input from second account')\
.put_object(bucket, "testObject", canned_acl="public-read").execute_test().command_is_successful()
result=AwsTest('Aws can get object acl from second account').get_object_acl(bucket, "testObject")\
.execute_test().command_is_successful()
print("Object Canned ACL validation started..")
AclTest('aws command has valid response').check_response_status(result)
AclTest('Object acl has valid Owner').validate_owner(result, testAccount_cannonicalid, "testAccount")
AclTest('validate complete object acl').validate_acl_single_group_grant(result, testAccount_cannonicalid,\
 "testAccount", testAccount_cannonicalid, "testAccount", "FULL_CONTROL",\
"http://acs.amazonaws.com/groups/global/AllUsers", "READ")
AwsTest('Second AWS account can not get bucket acl').get_bucket_acl(bucket)\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")
print("ACL validation Completed..")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Validate the object acl from first account should fail').get_object_acl(bucket, "testObject")\
.execute_test(negative_case=True).command_should_fail().command_error_should_have("AccessDenied")

os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Aws can delete object from second account').delete_object(bucket,"testObject")\
.execute_test().command_is_successful()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Aws can delete bucket from first account').delete_bucket(bucket)\
.execute_test().command_is_successful()




#******************Group Tests***********************#
#************** Group Test 1 *************
group_uri = "URI=http://acs.amazonaws.com/groups/global/AuthenticatedUsers"
AwsTest('Aws can create bucket').create_bucket("grouptestbucket").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with permission headers').put_object_with_permission_headers("grouptestbucket", "3kfile", "grant-read" , group_uri ).execute_test().command_is_successful()
AwsTest('Aws can get object acl').get_object_acl("grouptestbucket", "3kfile").execute_test().command_is_successful().command_response_should_have("http://acs.amazonaws.com/groups/global/AuthenticatedUsers")
AwsTest('Aws can delete object').delete_object("grouptestbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("grouptestbucket").execute_test().command_is_successful()
##***************Group Test Case 2 ******************
group_uri = "URI=http://acs.amazonaws.com/groups/global/AllUsers"
AwsTest('Aws can create bucket').create_bucket_with_permission_headers("grouptestbucket" , "grant-full-control", group_uri).execute_test().command_is_successful()
AwsTest('Get bucket ACL').get_bucket_acl("grouptestbucket").execute_test().command_is_successful().command_response_should_have("http://acs.amazonaws.com/groups/global/AllUsers")
AwsTest('Aws can delete bucket').delete_bucket("grouptestbucket").execute_test().command_is_successful()
##***************Group Test Case 3 ******************
group_uri = "uri=http://acs.amazonaws.com/groups/global/AuthenticatedUsers"
AwsTest('Aws can create bucket').create_bucket("grouptestbucket").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with permission headers').put_object_with_permission_headers("grouptestbucket", "3kfile", "grant-read" , group_uri ).execute_test().command_is_successful()
os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Aws can get object').get_object("grouptestbucket", "3kfile" ).execute_test().command_is_successful()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Aws can delete object').delete_object("grouptestbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("grouptestbucket").execute_test().command_is_successful()
#************** Group Test 4 *************
group_uri = "uri=http://acs.amazonaws.com/groups/global/AuthenticatedUsers"
AwsTest('Aws can create bucket').create_bucket_with_permission_headers("grouptestbucket" , "grant-write", group_uri).execute_test().command_is_successful()
os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = "invalidKey"
AwsTest('Aws can upload 3k file with permission headers').put_object("grouptestbucket", "3kfile" ).execute_test(negative_case=True).command_should_fail()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Aws can delete object').delete_object("grouptestbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("grouptestbucket").execute_test().command_is_successful()

#*************** put-object-acl Tests ********************

##**************** put-object-acl - grant read to new account and test ************

AwsTest('Aws can create bucket').create_bucket("authbucket").execute_test().command_is_successful()
cannonical_id = "id=" + testAccount_cannonicalid
AwsTest('Aws can upload 3k file with tags').put_object("authbucket", "3kfile").execute_test().command_is_successful()
AwsTest('put-object-acl').put_object_acl("authbucket", "3kfile", "grant-read" , cannonical_id ).execute_test().command_is_successful()
os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Aws can get object').get_object("authbucket", "3kfile").execute_test().command_is_successful()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Aws can delete object').delete_object("authbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("authbucket").execute_test().command_is_successful()

##************* READ/WRITE ACP always permitted for owner ********************

AwsTest('Aws can create bucket').create_bucket("authbucket").execute_test().command_is_successful()
cannonical_id = "id=" + testAccount_cannonicalid
cannonical_id2 = "id=" + testAccount2_cannonicalid
AwsTest('Aws can upload 3k file with permission headers').put_object_with_permission_headers("authbucket", "3kfile", "grant-read" , cannonical_id).execute_test().command_is_successful()
AwsTest('Aws can put acl').put_object_acl("authbucket", "3kfile", "grant-read" , cannonical_id2).execute_test().command_is_successful()
AwsTest('Aws can delete object').delete_object("authbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("authbucket").execute_test().command_is_successful()

##************* Negative test case for unauthorized user ********************

AwsTest('Aws can create bucket').create_bucket("authbucket").execute_test().command_is_successful()
cannonical_id = "id=" + testAccount_cannonicalid
AwsTest('Aws can upload 3k file with tags').put_object("authbucket", "3kfile").execute_test().command_is_successful()
os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Aws can put acl').put_object_acl("authbucket", "3kfile", "grant-read" , cannonical_id ).execute_test(negative_case=True).command_should_fail().command_error_should_have("Access Denied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Aws can delete object').delete_object("authbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("authbucket").execute_test().command_is_successful()

##**************** Negative Test - put-object-acl will fail for invalid id ************
AwsTest('Aws can create bucket').create_bucket("testbucket").execute_test().command_is_successful()
cannonical_id_assignment = "id=invalid"
AwsTest('Aws can upload 3k file with tags').put_object("testbucket", "3kfile").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with tags').put_object_acl("testbucket", "3kfile", "grant-read" , cannonical_id_assignment).execute_test(negative_case=True).command_should_fail()
AwsTest('Aws can delete object').delete_object("testbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("testbucket").execute_test().command_is_successful()

##**************** put-object-acl using emailAddress ************
email_id = "emailaddress=" + testAccount_email
AwsTest('Aws can create bucket').create_bucket("testbucket").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with tags').put_object("testbucket", "3kfile").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with tags').put_object_acl("testbucket", "3kfile", "grant-read" , email_id ).execute_test().command_is_successful()
result=AwsTest('Aws can get object acl').get_object_acl("testbucket", "3kfile").execute_test().command_is_successful().command_response_should_have("testAccount")
AwsTest('Aws can delete object').delete_object("testbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("testbucket").execute_test().command_is_successful()

##**************** put-object-acl using multiple emailAddresses ************
email_id = "emailAddress=" + testAccount_email+",EMAILaddress="+testAccount2_email
AwsTest('Aws can create bucket').create_bucket("testbucket").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with tags').put_object("testbucket", "3kfile").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with permission headers').put_object_acl("testbucket", "3kfile", "grant-read" , email_id ).execute_test().command_is_successful()
AwsTest('Aws can get object acl').get_object_acl("testbucket", "3kfile").execute_test().command_is_successful().command_response_should_have("testAccount")
AwsTest('Aws can get object acl').get_object_acl("testbucket", "3kfile").execute_test().command_is_successful().command_response_should_have("testAccount2")
os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Aws can get object').get_object("testbucket", "3kfile" ).execute_test().command_is_successful()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
os.environ["AWS_ACCESS_KEY_ID"] = testAccount2_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount2_secret_key
AwsTest('Aws can get object').get_object("testbucket", "3kfile" ).execute_test().command_is_successful()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Aws can delete object').delete_object("testbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("testbucket").execute_test().command_is_successful()
##**************** put-object-acl using id and email address combination ************

grantees = "id=" + testAccount_cannonicalid +",EMAILaddress="+testAccount2_email
AwsTest('Aws can create bucket').create_bucket("testbucket").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with tags').put_object("testbucket", "3kfile").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with permission headers').put_object_acl("testbucket", "3kfile", "grant-read" , grantees ).execute_test().command_is_successful()
AwsTest('Aws can get object acl').get_object_acl("testbucket", "3kfile").execute_test().command_is_successful().command_response_should_have("testAccount")
AwsTest('Aws can get object acl').get_object_acl("testbucket", "3kfile").execute_test().command_is_successful().command_response_should_have("testAccount2")
os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Aws can get object').get_object("testbucket", "3kfile" ).execute_test().command_is_successful()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
os.environ["AWS_ACCESS_KEY_ID"] = testAccount2_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount2_secret_key
AwsTest('Aws can get object').get_object("testbucket", "3kfile" ).execute_test().command_is_successful()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Aws can delete object').delete_object("testbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("testbucket").execute_test().command_is_successful()

##**************** put-object-acl - owner validation ************

ids = "id=" + testAccount_cannonicalid
AwsTest('Aws can create bucket').create_bucket("testbucket").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with tags').put_object("testbucket", "3kfile").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with tags').put_object_acl("testbucket", "3kfile","grant-write",ids).execute_test().command_is_successful()
result = AwsTest('Aws can get object acl').get_object_acl("testbucket", "3kfile").execute_test().command_is_successful()
AclTest('acl has valid Owner').validate_owner(result,"C12345", "s3_test")
print("Owner validation success")

#*************** put-object-acl - with more than one acl options ***********
cmd = "aws s3api put-object-acl --bucket testbucket --key 3kfile --acl bucket-owner-full-control\
 --access-control-policy " + fullACP
AwsTest('Aws can not put object acl with canned acl and aclxml').put_acl_with_multiple_options(cmd).\
execute_test(negative_case=True).command_should_fail().command_error_should_have("UnexpectedContent")
cmd = "aws s3api put-object-acl --bucket testbucket --key 3kfile --grant-read " + "emailaddress="\
 + testAccount2_email + " --access-control-policy " + fullACP
AwsTest('Aws can not put object acl with aclxml and permissionheader').\
put_acl_with_multiple_options(cmd).execute_test(negative_case=True).\
command_should_fail().command_error_should_have("UnexpectedContent")
cmd = "aws s3api put-object-acl --bucket testbucket --key 3kfile --grant-read " + \
"emailaddress=" + testAccount2_email + " --acl bucket-owner-full-control"
AwsTest('Aws can not put object acl with cannedacl and permissionheader').\
put_acl_with_multiple_options(cmd).execute_test(negative_case=True).command_should_fail().\
command_error_should_have("InvalidRequest")
AwsTest('Aws can delete object').delete_object("testbucket","3kfile").\
execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("testbucket").\
execute_test().command_is_successful()

##**************** Negative Test - put-object-acl - cross account authorization check************
grantees = "id=" + testAccount_cannonicalid
AwsTest('Aws can create bucket').create_bucket("testbucket").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with tags').put_object("testbucket", "3kfile").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with permission headers').put_object_acl("testbucket", "3kfile", "grant-read" , grantees ).execute_test().command_is_successful()
os.environ["AWS_ACCESS_KEY_ID"] = testAccount2_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount2_secret_key
AwsTest('Aws can get object').get_object("testbucket", "3kfile" ).execute_test(negative_case=True).command_should_fail().command_error_should_have("Access Denied")
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Aws can delete object').delete_object("testbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("testbucket").execute_test().command_is_successful()

#************** put-object-acl for group *************
group_uri = "URI=http://acs.amazonaws.com/groups/global/AuthenticatedUsers"
AwsTest('Aws can create bucket').create_bucket("grouptestbucket").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with tags').put_object("grouptestbucket", "3kfile").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with permission headers').put_object_acl("grouptestbucket", "3kfile", "grant-read" , group_uri ).execute_test().command_is_successful()
AwsTest('Aws can get object acl').get_object_acl("grouptestbucket", "3kfile").execute_test().command_is_successful().command_response_should_have("http://acs.amazonaws.com/groups/global/AuthenticatedUsers")
AwsTest('Aws can delete object').delete_object("grouptestbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("grouptestbucket").execute_test().command_is_successful()

##***************put-object-acl  and get object check for group  ******************
group_uri = "uri=http://acs.amazonaws.com/groups/global/AuthenticatedUsers"
AwsTest('Aws can create bucket').create_bucket("grouptestbucket").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with tags').put_object("grouptestbucket", "3kfile").execute_test().command_is_successful()
AwsTest('Aws can upload 3k file with permission headers').put_object_acl("grouptestbucket", "3kfile", "grant-read" , group_uri ).execute_test().command_is_successful()
os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Aws can get object').get_object("grouptestbucket", "3kfile" ).execute_test().command_is_successful()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Aws can delete object').delete_object("grouptestbucket","3kfile").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("grouptestbucket").execute_test().command_is_successful()


# Put object with canned acl - bucket-owner-read
cannonical_id = "id=" + testAccount_cannonicalid
AwsTest('Aws can create bucket').create_bucket("aclbucket").execute_test().command_is_successful()
AwsTest('Aws can put bucket acl').put_bucket_acl("aclbucket", "grant-write" , cannonical_id ).execute_test().command_is_successful()
os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Aws can create object').put_object("aclbucket", "testObject").execute_test().command_is_successful()
AwsTest('put-object-acl for canned acl testing').put_object_acl("aclbucket", "testObject", "acl" , "bucket-owner-read" ).execute_test().command_is_successful()
AwsTest('Validate the object acl').get_object_acl("aclbucket", "testObject").execute_test().command_is_successful().command_response_should_have("READ")
AwsTest('Validate the object acl').get_object_acl("aclbucket", "testObject").execute_test().command_is_successful().command_response_should_have("C12345")
AwsTest('Validate the object acl').get_object_acl("aclbucket", "testObject").execute_test().command_is_successful().command_response_should_have("FULL_CONTROL")
AwsTest('Validate the object acl').get_object_acl("aclbucket", "testObject").execute_test().command_is_successful().command_response_should_have(testAccount_cannonicalid)
AwsTest('Aws can get object').get_object("aclbucket", "testObject" ).execute_test().command_is_successful()
AwsTest('Aws can delete object').delete_object("aclbucket","testObject").execute_test().command_is_successful()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Aws can put bucket acl').put_bucket_acl("aclbucket", "grant-full-control" , "id=C12345" ).execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("aclbucket").execute_test().command_is_successful()

# Put object with canned acl - bucket-owner-full-control
cannonical_id = "id=" + testAccount_cannonicalid
AwsTest('Aws can create bucket').create_bucket("aclbucket").execute_test().command_is_successful()
AwsTest('Aws can put bucket acl').put_bucket_acl("aclbucket", "grant-write" , cannonical_id ).execute_test().command_is_successful()
os.environ["AWS_ACCESS_KEY_ID"] = testAccount_access_key
os.environ["AWS_SECRET_ACCESS_KEY"] = testAccount_secret_key
AwsTest('Aws can create object').put_object("aclbucket", "testObject").execute_test().command_is_successful()
AwsTest('put-object-acl for canned acl testing').put_object_acl("aclbucket", "testObject", "acl" , "bucket-owner-full-control" ).execute_test().command_is_successful()
result=AwsTest('Validate the object acl').get_object_acl("aclbucket", "testObject").execute_test().command_is_successful()
acl_json = json.loads(result.status.stdout)
grants = acl_json["Grants"]
assert len(grants) == 2
AwsTest('Aws can get object').get_object("aclbucket", "testObject" ).execute_test().command_is_successful()
AwsTest('Aws can delete object').delete_object("aclbucket","testObject").execute_test().command_is_successful()
del os.environ["AWS_ACCESS_KEY_ID"]
del os.environ["AWS_SECRET_ACCESS_KEY"]
AwsTest('Aws can put bucket acl').put_bucket_acl("aclbucket", "grant-full-control" , "id=C12345" ).execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("aclbucket").execute_test().command_is_successful()

# Put object with canned acl - bucket-owner-full-control when object and bucket owner are same
AwsTest('Aws can create bucket').create_bucket("aclbucket").execute_test().command_is_successful()
AwsTest('Aws can create object').put_object("aclbucket", "testObject").execute_test().command_is_successful()
AwsTest('put-object-acl for canned acl testing').put_object_acl("aclbucket", "testObject", "acl" , "bucket-owner-read" ).execute_test().command_is_successful()
result=AwsTest('Validate the object acl').get_object_acl("aclbucket", "testObject").execute_test().command_is_successful()
AclTest('acl has valid Grants').validate_grant(result, "C12345", "s3_test", 1, "FULL_CONTROL")
AwsTest('Aws can get object').get_object("aclbucket", "testObject" ).execute_test().command_is_successful()
AwsTest('Aws can delete object').delete_object("aclbucket","testObject").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("aclbucket").execute_test().command_is_successful()

# Put object with canned acl - bucket-owner-full-control when Bucket-ACL not present
AwsTest('Aws can create bucket').create_bucket("aclbucket").execute_test().command_is_successful()
AwsTest('put object with canned acl').put_object_with_permission_headers("aclbucket", "testObject", "acl" , "bucket-owner-read" ).execute_test().command_is_successful()
result=AwsTest('Validate the object acl').get_object_acl("aclbucket", "testObject").execute_test().command_is_successful()
AclTest('acl has valid Grants').validate_grant(result, "C12345", "s3_test", 1, "FULL_CONTROL")
AwsTest('Aws can get object').get_object("aclbucket", "testObject" ).execute_test().command_is_successful()
AwsTest('Aws can delete object').delete_object("aclbucket","testObject").execute_test().command_is_successful()
AwsTest('Aws can delete bucket').delete_bucket("aclbucket").execute_test().command_is_successful()

#************Delete Account *********************
test_msg = "Delete account testAccount"
account_args = {'AccountName': 'testAccount', 'Email': 'testAccount@seagate.com',  'force': True}
S3ClientConfig.access_key_id = testAccount_access_key
S3ClientConfig.secret_key = testAccount_secret_key
AuthTest(test_msg).delete_account(**account_args).execute_test().command_response_should_have("Account deleted successfully")
#***********Delete testAccount2*******************
test_msg = "Delete account testAccount2"
account_args = {'AccountName': 'testAccount2', 'Email': 'testAccount2@seagate.com',  'force': True}
S3ClientConfig.access_key_id = testAccount2_access_key
S3ClientConfig.secret_key = testAccount2_secret_key
AuthTest(test_msg).delete_account(**account_args).execute_test().command_response_should_have("Account deleted successfully")
#***********AllUsers Test************************
AwsTest('Aws can create bucket').create_bucket("seagate").execute_test().command_is_successful()

cmd = "curl -s -X GET -H \"Accept: application/json\" -H \"Content-Type: application/json\"  https://s3.seagate.com/seagate --cacert /etc/ssl/stx-s3-clients/s3/ca.crt"

AwsTest('AccessDenied For allusers').execute_curl(cmd).\
execute_test().command_is_successful().command_response_should_have("AccessDenied")

AwsTest("put_bucket_canned_acl_with public read-write permission").put_bucket_acl_with_canned_input("seagate", "public-read-write").execute_test().command_is_successful()

cmd = "curl -s -X GET -H \"Accept: application/json\" -H \"Content-Type: application/json\"  https://s3.seagate.com/seagate --cacert /etc/ssl/stx-s3-clients/s3/ca.crt"

AwsTest('Accessible For allusers').execute_curl(cmd).execute_test().command_is_successful()

AwsTest('Aws can create object').put_object("seagate", "testObject").execute_test().command_is_successful()

cmd = "curl -s -X GET -H \"Accept: application/json\" -H \"Content-Type: application/json\"  https://s3.seagate.com/seagate/testObject --cacert /etc/ssl/stx-s3-clients/s3/ca.crt"

AwsTest('AccessDenied For allusers').execute_curl(cmd).\
execute_test().command_is_successful().command_response_should_have("AccessDenied")

AwsTest("put_object_canned_acl_with public read-write permission").put_object_acl_with_canned_input("seagate", "testObject", "public-read-write").execute_test().command_is_successful()

cmd = "curl -s -X GET -H \"Accept: application/json\" -H \"Content-Type: application/json\"  https://s3.seagate.com/seagate/testObject --cacert /etc/ssl/stx-s3-clients/s3/ca.crt"

AwsTest('Accessible For allusers').execute_curl(cmd).execute_test().command_is_successful()

cmd = "curl -s -X DELETE -H \"Accept: application/json\" -H \"Content-Type: application/json\"  https://s3.seagate.com/seagate/testObject --cacert /etc/ssl/stx-s3-clients/s3/ca.crt"

AwsTest('Accessible For allusers').execute_curl(cmd).execute_test().command_is_successful()

group_uri = "uri=http://acs.amazonaws.com/groups/global/AllUsers"

AwsTest('Aws can upload 3k file with permission headers').put_bucket_acl("seagate", "grant-write-acp" , group_uri ).execute_test().command_is_successful()

cmd = "curl -s -X PUT -H \"Accept: application/json\" -H \"Content-Type: application/json\" http://s3.seagate.com/seagate?acl -H \"x-amz-acl: private\" --cacert /etc/ssl/stx-s3-clients/s3/ca.crt"

AwsTest('write acp For allusers').execute_curl(cmd).execute_test().command_is_successful()

result=AwsTest('Validate the bucket acl').get_bucket_acl("seagate").execute_test().command_is_successful()

AclTest('aws command has valid response').check_response_status(result)

AclTest('validate complete acl').validate_acl(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL")

AwsTest('Aws can create object').put_object("seagate", "testObject").execute_test().command_is_successful()

AwsTest('Aws can upload 3k file with permission headers').put_object_acl("seagate", "testObject", "grant-write-acp" , group_uri ).execute_test().command_is_successful()

cmd = "curl -s -X PUT -H \"Accept: application/json\" -H \"Content-Type: application/json\" http://s3.seagate.com/seagate/testObject?acl -H \"x-amz-acl: private\" --cacert /etc/ssl/stx-s3-clients/s3/ca.crt"

AwsTest('Write acp For allusers').execute_curl(cmd).execute_test().command_is_successful()

result=AwsTest('Validate the object acl').get_bucket_acl("seagate").execute_test().command_is_successful()

AclTest('aws command has valid response').check_response_status(result)

AclTest('validate complete acl').validate_acl(result, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL")

AwsTest('Aws can delete object').delete_object("seagate", "testObject").execute_test().command_is_successful()

AwsTest('Aws can delete bucket').delete_bucket("seagate").execute_test().command_is_successful()

cmd = "curl -s -X GET -H \"Accept: application/json\" -H \"Content-Type: application/json\"  https://s3.seagate.com/ --cacert /etc/ssl/stx-s3-clients/s3/ca.crt"

AwsTest('AccessDenied For allusers to listallbuckets').execute_curl(cmd).\
execute_test().command_is_successful().command_response_should_have("AccessDenied")

cmd = "curl -s -X PUT -H \"Accept: application/json\" -H \"Content-Type: application/json\"  https://s3.seagate.com/seagate --cacert /etc/ssl/stx-s3-clients/s3/ca.crt"

AwsTest('AccessDenied For allusers to create buckets').execute_curl(cmd).\
execute_test().command_is_successful().command_response_should_have("AccessDenied")
