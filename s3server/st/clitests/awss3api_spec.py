import os
from framework import Config
from framework import S3PyCliTest
from awss3api import AwsTest
from s3client_config import S3ClientConfig
from aclvalidation import AclTest

# Helps debugging
# Config.log_enabled = True
# Config.dummy_run = True
# Config.client_execution_timeout = 300 * 1000
# Config.request_timeout = 300 * 1000
# Config.socket_timeout = 300 * 1000

# Extract the upload id from response which has the following format
# [bucketname    objecctname    uploadid]

def get_upload_id(response):
    key_pairs = response.split('\t')
    return key_pairs[2]

# Run before all to setup the test environment.
print("Configuring LDAP")
S3PyCliTest('Before_all').before_all()

#******** Create Bucket ********
AwsTest('Aws can create bucket').create_bucket("seagatebucket").execute_test().command_is_successful()

#******** Put Bucket Tag ********
AwsTest('Aws can create bucket tag').put_bucket_tagging("seagatebucket", [{'Key': 'organization','Value': 'marketing'}])\
    .execute_test().command_is_successful()

#******** List Bucket Tag ********
AwsTest('Aws can list bucket tag').list_bucket_tagging("seagatebucket").execute_test().command_is_successful()\
    .command_response_should_have("organization").command_response_should_have("marketing")

#******** List bucket Tag after Updating tags ********
AwsTest('Aws can create bucket tag').put_bucket_tagging("seagatebucket", [{'Key': 'seagate','Value': 'storage'}])\
    .execute_test().command_is_successful()

AwsTest('Aws can list bucket tag').list_bucket_tagging("seagatebucket").execute_test().command_is_successful()\
    .command_response_should_not_have("organization").command_response_should_not_have("marketing")\
    .command_response_should_have("seagate").command_response_should_have("storage")

#********** Negative case to check restriction on length of bucket-tagging key length ***********
invalid_long_tag_key = 'LiXrj2I0fMJNAyhl61P7a536zhZXgkAZCNfaTLy11SkNcxmMPd7wrVf6hpHYgmw0r5KiNgt5vSFcLfmgoiQ5hizpCLotjoIWj9TYdO02NTg58XGMUX8Xol2wez42UnZb1'

AwsTest('Aws can not create bucket tag with key size greater than 128 unicode characters')\
    .put_bucket_tagging("seagatebucket", [{'Key': invalid_long_tag_key,'Value': 'marketing'}])\
    .execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidTagError")

#*********** Negative case to check restriction on length of bucket-tagging value length *********
invalid_long_tag_value = 'kJHu017I0OErlyoZLfx6fKodbEWXBBJjvCt0YuliSZvfBNPlgvecAZkUjwtG3YHRMpTDRcMMcsQAfTlrq47ZRvzsVLBAkFgBLFm9ytbfV2E6Vo6NX4uEnMaVFq9z7Mh9XzRZaCdrF7plwiMgjs1hQbOOm2VLXiVf31paJLipWw2D66DExvp0e1IqufzGQ5KzvEu5vJRAFtfiXLRhsRHWV0cvli0wVSWpK6Wg3NbSz7kMrX2MqIQ5gyZNWeT4ISzJNjLS7BVljDPh4IOSn6KAjEQLFeOZzmvUwcSU8VG92mL14EEZqzu8etBYzK499BEfM67B9bkAmiwu3Lpr6DZk6a0vBwLfBijVyOfDhGTzkXxYnqjYnGZ65S4sfnS4yeBeI0KGWAIVa39I3VwAwigL3lAmgqxrYSVJN0JgHVSapE9lb80YjVoADE6jtLAeXbv3YGfbUtagpSaj16kVdueW1wGlDmNLTuZOhKW3cSOnXLhhVKbLCff4l8CA9lh70q1Vs'

AwsTest('Aws can not create bucket tag with value size greater than 2*256 unicode characters')\
    .put_bucket_tagging("seagatebucket", [{'Key': 'organization','Value': invalid_long_tag_value}])\
    .execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidTagError")

#*********** Negative case to check restriction on length of bucket-tagging value length *********
tag_list_with_too_many_tags = []
for x in range(52):
    key = "seagate" + str(x)
    value = "storage" + str(x)
    case = {'Key': key, 'Value': value}
    tag_list_with_too_many_tags.append(case)

AwsTest('Aws can create bucket tag for more than 50 entries').put_bucket_tagging("seagatebucket", tag_list_with_too_many_tags).execute_test(negative_case=True)\
    .command_should_fail().command_error_should_have("InvalidTagError")


#*********** Negative case to check invalid char(s) for key in bucket-tagging *********
invalid_chars = ["!", "^", "%", "<", ">", "[", "]", "~", "|", "#", "?", "@", "*"]
for char in invalid_chars:
    invalid_key = "sea"+ char + "gate"
    AwsTest('Aws can not create bucket tag with invalid char(' + char +') in Key').put_bucket_tagging("seagatebucket",  [{'Key': invalid_key, 'Value': 'storage'}]).execute_test(negative_case=True)\
    .command_should_fail().command_error_should_have("InvalidTagError")

#*********** Negative case to check invalid char(s) for value in bucket-tagging *********
#invalid_chars = ["!", "^", "%", "<", ">", "[", "]", "~", "|", "#", "?", "@", "*"]
for char in invalid_chars:
    invalid_value = "stor"+ char + "age"
    AwsTest('Aws can not create bucket tag with invalid char(' + char +') in Value').put_bucket_tagging("seagatebucket",  [{'Key': 'seagate', 'Value': invalid_value}]).execute_test(negative_case=True)\
    .command_should_fail().command_error_should_have("InvalidTagError")

#************Negative case to check creation of tag for non-existant bucket*******
AwsTest('Aws can not create bucket tag on non-existant').put_bucket_tagging("seagate-bucket", [{'Key': 'seagate','Value': 'storage'}])\
    .execute_test(negative_case=True).command_should_fail().command_error_should_have("NoSuchBucket")

#************** Delete Bucket Tag ********
AwsTest('Aws can delete bucket tag').delete_bucket_tagging("seagatebucket").execute_test().command_is_successful()

AwsTest('Aws can list bucket tag').list_bucket_tagging("seagatebucket").execute_test(negative_case=True)\
    .command_should_fail().command_error_should_have("NoSuchTagSetError")

#******* delete bucket **********
AwsTest('Aws can delete bucket').delete_bucket("seagatebucket").execute_test().command_is_successful()

#******** Create Bucket ********
AwsTest('Aws can create bucket').create_bucket("seagatebuckettag").execute_test().command_is_successful()

#******** Put Object Tag ********
AwsTest('Aws can upload 3k file with tags').put_object_with_tagging("seagatebuckettag", "3kfile", 3000, "organization=marketing" ).execute_test().command_is_successful()

#******** List Object Tag ********
AwsTest('Aws can list object tag').list_object_tagging("seagatebuckettag","3kfile").execute_test().command_is_successful()\
    .command_response_should_have("organization").command_response_should_have("marketing")

#******** List Object Tag after Updating tags ********
AwsTest('Aws can update tags for 3k file').put_object_tagging("seagatebuckettag", "3kfile", [{'Key': 'seagate','Value': 'storage'}] ).execute_test().command_is_successful()

AwsTest('Aws can list bucket tag after update').list_object_tagging("seagatebuckettag","3kfile").execute_test().command_is_successful()\
    .command_response_should_not_have("organization").command_response_should_not_have("marketing")\
    .command_response_should_have("seagate").command_response_should_have("storage")

#*********** Negative case to check restriction on length of object-tagging value length *********
tag_list_with_too_many_tags = []
for x in range(11):
    key = "seagate" + str(x)
    value = "storage" + str(x)
    case = {'Key': key, 'Value': value}
    tag_list_with_too_many_tags.append(case)

AwsTest('Aws can create object tag for more than 10 entries').put_object_tagging("seagatebuckettag","3kfile", tag_list_with_too_many_tags).execute_test(negative_case=True)\
    .command_should_fail().command_error_should_have("InvalidTagError")

#************** Delete Object Tag ********
AwsTest('Aws can delete object tag').delete_object_tagging("seagatebuckettag","3kfile").execute_test().command_is_successful()

AwsTest('Aws can list object tag').list_object_tagging("seagatebuckettag","3kfile").execute_test()\
    .command_is_successful().command_response_should_not_have("NoSuchTagSetError")

#******** Object tags with UTF-8 ********
AwsTest('Aws can create object tag with 2-bytes UTF-8 characters').put_object_tagging("seagatebuckettag", "3kfile",  [{'Key': 'utf-8_1', 'Value': '\u0410\u0411\u0412\u0413' }])\
    .execute_test().command_is_successful()

AwsTest('Aws can create object tag with 3-bytes UTF-8 characters').put_object_tagging("seagatebuckettag", "3kfile",  [{'Key': 'utf-8_2', 'Value': '\u20AC\u20AD'}])\
    .execute_test().command_is_successful()

# In the next line we will create 256-length string of 4-byte UTF-8 symbols
long_utf8_4bytes_string = b'\xF0\x90\x8D\x88'.decode('utf-8') * 256
AwsTest('Aws can create object tag with long value of 4-bytes UTF-8 characters').put_object_tagging("seagatebuckettag", "3kfile",  [{'Key': 'utf-8_3', 'Value': long_utf8_4bytes_string}])\
    .execute_test().command_is_successful()

long_utf8_4bytes_string = b'\xF0\x90\x8D\x88'.decode('utf-8') * 128
AwsTest('Aws can create object tag with long name of 4-bytes UTF-8 characters').put_object_tagging("seagatebuckettag", "3kfile",  [{'Key': long_utf8_4bytes_string, 'Value': 'utf-8_3'}])\
    .execute_test().command_is_successful()

#*********** Negative case to check invalid char(s) for key in object-tagging *********
invalid_chars = ["!", "^", "%", "<", ">", "[", "]", "~", "|", "#", "?", "*"]
for char in invalid_chars:
    invalid_key = "sea"+ char + "gate"
    AwsTest('Aws can not create object tag with invalid char(' + char +') in Key').put_object_tagging("seagatebuckettag", "3kfile",  [{'Key': invalid_key, 'Value': 'storage'}])\
    .execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidTagError")

#*********** Negative case to check invalid char(s) for value in object-tagging *********
for char in invalid_chars:
    invalid_value = "stor"+ char + "age"
    AwsTest('Aws can not create object tag with invalid char(' + char +') in Value').put_object_tagging("seagatebuckettag", "3kfile",  [{'Key': 'seagate', 'Value': invalid_value}])\
    .execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidTagError")

#************** Delete Object  ********
AwsTest('Aws can delete object').delete_object("seagatebuckettag","3kfile").execute_test().command_is_successful()

#************** Create a multipart upload ********
result=AwsTest('Aws can upload 10Mb file with tags').create_multipart_upload("seagatebuckettag", "10Mbfile", 10485760, "domain=storage" ).execute_test().command_is_successful()
upload_id = get_upload_id(result.status.stdout)
print(upload_id)

#************** Upload Individual parts ********
result=AwsTest('Aws can upload 5Mb first part').upload_part("seagatebuckettag", "firstpart", 5242880, "10Mbfile", "1" , upload_id).execute_test().command_is_successful()
e_tag_1 = result.status.stdout
print(e_tag_1)

result=AwsTest('Aws can upload 5Mb second part').upload_part("seagatebuckettag", "secondpart", 5242880, "10Mbfile", "2" , upload_id).execute_test().command_is_successful()
e_tag_2 = result.status.stdout
print(e_tag_2)

parts="Parts=[{ETag="+e_tag_1.strip('\n')+",PartNumber="+str(1)+"},{ETag="+e_tag_2.strip('\n')+",PartNumber="+str(2)+"}]"
print(parts)

#************** Get object acl should fail before complete multipart upload ******
AwsTest('Aws can get object acl').get_object_acl("seagatebuckettag", "10Mbfile").execute_test(negative_case=True)\
.command_should_fail().command_error_should_have("NoSuchKey")

#************** Complete multipart upload ********
result=AwsTest('Aws can complete multipart upload 10Mb file with tags').complete_multipart_upload("seagatebuckettag", "10Mbfile", parts, upload_id).execute_test().command_is_successful().command_response_should_have("seagatebuckettag/10Mbfile")

#************* Get object ACL for multipart Upload **********

aclresult=AwsTest('Aws can get object acl').get_object_acl("seagatebuckettag", "10Mbfile").execute_test().command_is_successful()

print("ACL validation started..")
AclTest('validate complete acl').validate_acl(aclresult, "C12345", "s3_test", "C12345", "s3_test", "FULL_CONTROL")
AclTest('acl has valid Owner').validate_owner(aclresult, "C12345", "s3_test")
AclTest('acl has valid Grants').validate_grant(aclresult, "C12345", "s3_test", 1, "FULL_CONTROL")
print("ACL validation Completed..")

#******** List Object Tags for Multipart Upload********
AwsTest('Aws can list object tags for multipart upload').list_object_tagging("seagatebuckettag","10Mbfile").execute_test().command_is_successful()\
    .command_response_should_have("domain").command_response_should_have("storage")

#************** Delete Object  ********
AwsTest('Aws can delete object').delete_object("seagatebuckettag","10Mbfile").execute_test().command_is_successful()

#******* Multipart upload should fail if non-last part is less then 5 Mb ******

result=AwsTest('Multipart upload with wrong-sized parts').create_multipart_upload("seagatebuckettag", "10Mbfile", 10485760, "domain=storage" ).execute_test().command_is_successful()
upload_id = get_upload_id(result.status.stdout)
print(upload_id)

result=AwsTest('Aws can upload 4Mb first part').upload_part("seagatebuckettag", "firstpart", 4194304, "10Mbfile", "1" , upload_id).execute_test().command_is_successful()
e_tag_1 = result.status.stdout
print(e_tag_1)

result=AwsTest('Aws can upload 4Mb second part').upload_part("seagatebuckettag", "secondpart", 4194304, "10Mbfile", "2" , upload_id).execute_test().command_is_successful()
e_tag_2 = result.status.stdout
print(e_tag_2)

result=AwsTest('Aws can upload 2Mb third part').upload_part("seagatebuckettag", "thirdpart", 2097152, "10Mbfile", "3" , upload_id).execute_test().command_is_successful()
e_tag_3 = result.status.stdout
print(e_tag_3)

parts="Parts=[{ETag="+e_tag_1.strip('\n')+",PartNumber=1},{ETag="+e_tag_2.strip('\n')+",PartNumber=2},{ETag="+e_tag_3.strip('\n')+",PartNumber=3}]"
print(parts)

result=AwsTest('Aws cannot complete multipart upload when non-last part is less than 5MB').complete_multipart_upload("seagatebuckettag", "10Mbfile", parts, upload_id).execute_test(negative_case=True).command_should_fail().command_error_should_have("EntityTooSmall")

#******* Multipart upload should fail if user completes multipart-upload with wrong ETag ******

result=AwsTest('Multipart upload with wrong ETag').create_multipart_upload("seagatebuckettag", "10Mbfile", 10485760, "domain=storage" ).execute_test().command_is_successful()
upload_id = get_upload_id(result.status.stdout)
print(upload_id)

parts="Parts=[{ETag=00000000000000000000000000000000,PartNumber=1}]"

result=AwsTest('Aws cannot complete multipart upload with wrong ETag').complete_multipart_upload("seagatebuckettag", "10Mbfile", parts, upload_id).execute_test(negative_case=True).command_should_fail().command_error_should_have("InvalidPart")

#******* Delete bucket **********
AwsTest('Aws can delete bucket').delete_bucket("seagatebuckettag").execute_test().command_is_successful()
