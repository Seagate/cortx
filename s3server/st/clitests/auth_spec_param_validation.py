import os
from framework import Config
from framework import S3PyCliTest
from auth import AuthTest
from s3client_config import S3ClientConfig

# Helps debugging
# Config.log_enabled = True
# Config.dummy_run = True

# Set time_readable_format to False if you want to display the time in milli seconds.
# Config.time_readable_format = False

# Store the access keys created during the test.
# These keys should be deleted after
access_key_id = []

# Extract the response elements from response which has the following format
# <Key 1> = <Value 1>, <Key 2> = <Value 2> ... <Key n> = <Value n>
def get_response_elements(response):
    response_elements = {}
    key_pairs = response.split(',')

    for key_pair in key_pairs:
        tokens = key_pair.split('=')
        response_elements[tokens[0].strip()] = tokens[1].strip()

    return response_elements

# Run before all to setup the test environment.
def before_all():
    print("Configuring LDAP")
    S3PyCliTest('Before_all').before_all()

def set_valid_credentials():
    S3ClientConfig.access_key_id = "AKIAJTYX36YCKQSAJT7Q"
    S3ClientConfig.secret_key = "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"

def parameter_validation_tests():
    # Create user with wrong access key
    test_msg = "Create User s3user1 should fail if access key is invalid"
    S3ClientConfig.access_key_id = "INVALID-ACCESS-KEY-Z"
    S3ClientConfig.secret_key = "A6k2z84BqwXmee4WUUS2oWwM/tha7Wrd4Hc/8yRt"
    user_args = {}
    user_args['UserName'] = "s3user1"
    AuthTest(test_msg).create_user(**user_args).execute_test(negative_case=True)\
            .command_response_should_have("An error occurred (InvalidAccessKeyId) when"
                                          " calling the CreateUser operation: The AWS "
                                          "access key Id you provided does not exist "
                                          "in our records.")

    # Create user with wrong secret key
    test_msg = "Create User s3user1 should fail if secret key is invalid"
    S3ClientConfig.access_key_id = "AKIAJTYX36YCKQSAJT7Q"
    S3ClientConfig.secret_key = "INVALID-SECRET-KEY-2oWwM/tha7Wrd4Hc/8yRt"
    user_args = {}
    user_args['UserName'] = "s3user1"
    AuthTest(test_msg).create_user(**user_args).execute_test(negative_case=True)\
            .command_response_should_have("An error occurred (SignatureDoesNotMatch) when "
                                          "calling the CreateUser operation: The request "
                                          "signature we calculated does not match the "
                                          "signature you provided. Check your AWS secret "
                                          "access key and signing method. For more "
                                          "information, see REST Authentication andSOAP "
                                          "Authentication for details.")

    set_valid_credentials()

    test_msg = "Create user should fail if username is invalid"
    user_args = {}
    user_args['UserName'] = '#invalidusername'
    AuthTest(test_msg).create_user(**user_args).execute_test(negative_case=True)\
            .command_response_should_have("Failed to create user.")

    test_msg = "Create user should fail if pathname is invalid"
    user_args = {}
    user_args['UserName'] = 'test_user'
    user_args['Path'] = '#_-=INVALID%PATH/'
    AuthTest(test_msg).create_user(**user_args).execute_test(negative_case=True)\
            .command_response_should_have("Failed to create user.")

before_all()
parameter_validation_tests()
