import os
import sys
import yaml
from framework import Config
from framework import S3PyCliTest
from awsiam import AwsIamTest
from s3client_config import S3ClientConfig
from s3cmd import S3cmdTest
from s3fi import S3fiTest
import shutil



def user_tests():
    date_pattern = "[0-9|]+Z"
    #tests

    result = AwsIamTest('Create User').create_user("testUser").execute_test()
    result.command_response_should_have("testUser")

    result = AwsIamTest('CreateLoginProfile').create_login_profile("testUser","password").execute_test()
    login_profile_response_pattern = "LOGINPROFILE"+"[\s]*"+date_pattern+"[\s]*False[\s]*testUser"
    result.command_should_match_pattern(login_profile_response_pattern)
    result.command_response_should_have("testUser")

    result = AwsIamTest('GetLoginProfile Test').get_login_profile("testUser").execute_test()
    login_profile_response_pattern = "LOGINPROFILE"+"[\s]*"+date_pattern+"[\s]*False[\s]*testUser"
    result.command_should_match_pattern(login_profile_response_pattern)
    result.command_response_should_have("testUser")

    AwsIamTest('UpdateLoginProfile Test').update_login_profile("testUser").execute_test().command_is_successful()

    AwsIamTest('UpdateLoginProfile with optional parameter- password').update_login_profile_with_optional_arguments\
        ("testUser","NewPassword",None,None).execute_test().command_is_successful()

    AwsIamTest('UpdateLoginProfile with optional parameter - password-reset-required').update_login_profile_with_optional_arguments\
        ("testUser",None,"password-reset-required",None).execute_test().command_is_successful()

    result = AwsIamTest('GetLoginProfile Test').get_login_profile("testUser").execute_test()
    login_profile_response_pattern = "LOGINPROFILE"+"[\s]*"+date_pattern+"[\s]*True[\s]*testUser"
    result.command_should_match_pattern(login_profile_response_pattern)
    result.command_response_should_have("True")

    AwsIamTest('Delete User').delete_user("testUser").execute_test().command_is_successful()

if __name__ == '__main__':

    user_tests()
