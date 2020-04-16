import os
import sys
import yaml
from framework import Config
from framework import S3PyCliTest
from auth import AuthTest
from s3client_config import S3ClientConfig
from s3cmd import S3cmdTest
from s3fi import S3fiTest
from awss3api import AwsTest
from shutil import copyfile
import shutil

home_dir = os.path.expanduser("~")
original_config_file = os.path.join(home_dir,  '.sgs3iamcli/config.yaml')
backup_config_file = os.path.join(home_dir, '.sgs3iamcli/backup_config.yaml')

# Helps debugging
# Config.log_enabled = True
# Config.dummy_run = True

# Set time_readable_format to False if you want to display the time in milli seconds.
# Config.time_readable_format = False

# global params required for suit

def update_config_yaml(new_config_entries):
    shutil.copy2(original_config_file, backup_config_file)

    with open(original_config_file, 'r') as f:
        cur_yaml = yaml.load(f)
        cur_yaml.update(new_config_entries)

    with open(original_config_file, 'w') as f:
        yaml.dump(cur_yaml, f, default_flow_style = False)

def restore_config_yaml():
    # Restore original ~/.sgs3iamcli/config.yaml file
    shutil.copy2(backup_config_file, original_config_file)



class GlobalTestState():
    root_access_key = ""
    root_secret_key = ""

# Extract the response elements from response which has the following format
# <Key 1> = <Value 1>, <Key 2> = <Value 2> ... <Key n> = <Value n>
def get_response_elements(response):
    response_elements = {}
    key_pairs = response.split(',')

    for key_pair in key_pairs:
        tokens = key_pair.split('=')
        response_elements[tokens[0].strip()] = tokens[1].strip()

    return response_elements

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

# Set S3ClientConfig with root credentials
def _use_root_credentials():
    S3ClientConfig.access_key_id = GlobalTestState.root_access_key
    S3ClientConfig.secret_key = GlobalTestState.root_secret_key



# Test create account API
def account_tests():
    test_msg = "Create account s3test"
    account_args = {'AccountName': 's3test', 'Email': 's3test@seagate.com', 'ldapuser': S3ClientConfig.ldapuser, 'ldappasswd': S3ClientConfig.ldappasswd}
    account_response_pattern = "AccountId = [\w-]*, CanonicalId = [\w-]*, RootUserName = [\w+=,.@-]*, AccessKeyId = [\w-]*, SecretKey = [\w/+]*$"
    result = AuthTest(test_msg).create_account(**account_args).execute_test()
    result.command_should_match_pattern(account_response_pattern)
    account_response_elements = get_response_elements(result.status.stdout)

    GlobalTestState.root_access_key = account_response_elements['AccessKeyId']
    GlobalTestState.root_secret_key = account_response_elements['SecretKey']

    # Create Account again with same email ID
    test_msg = "Create account s3test1 should fail with EmailAlreadyExists"
    account_args = {'AccountName': 's3test1', 'Email': 's3test@seagate.com', 'ldapuser': S3ClientConfig.ldapuser, 'ldappasswd': S3ClientConfig.ldappasswd}
    account_response_pattern = "Account wasn't created."
    result = AuthTest(test_msg).create_account(**account_args).execute_test(negative_case=True)
    result.command_should_match_pattern(account_response_pattern)
    result.command_response_should_have("EmailAlreadyExists")

    test_msg = "List accounts"
    account_args = {'ldapuser': S3ClientConfig.ldapuser, 'ldappasswd': S3ClientConfig.ldappasswd}
    accounts_response_pattern = "AccountName = [\w-]*, AccountId = [\w-]*, CanonicalId = [\w-]*, Email = [\w.@]*"
    result = AuthTest(test_msg).list_account(**account_args).execute_test()
    result.command_should_match_pattern(accounts_response_pattern)


    test_msg = "List accounts - Take ldapuser and ldappasswd from config"

    # Put SG_LDAP_PASSWD and SG_LDAP_USER in ~/.sgs3iamcli/config.yaml file
    new_config_entries = {'SG_LDAP_PASSWD' : S3ClientConfig.ldappasswd, 'SG_LDAP_USER': S3ClientConfig.ldapuser}
    update_config_yaml(new_config_entries)

    accounts_response_pattern = "AccountName = [\w-]*, AccountId = [\w-]*, CanonicalId = [\w-]*, Email = [\w.@]*"
    result = AuthTest(test_msg).list_account().execute_test()
    result.command_should_match_pattern(accounts_response_pattern)

    restore_config_yaml()


    test_msg = "List accounts - Take ldapuser and ldappasswd from env"

    # Declare SG_LDAP_USER and SG_LDAP_PASSWD environment variables
    os.environ['SG_LDAP_USER'] = S3ClientConfig.ldapuser
    os.environ['SG_LDAP_PASSWD'] = S3ClientConfig.ldappasswd

    accounts_response_pattern = "AccountName = [\w-]*, AccountId = [\w-]*, CanonicalId = [\w-]*, Email = [\w.@]*"
    result = AuthTest(test_msg).list_account().execute_test()
    result.command_should_match_pattern(accounts_response_pattern)

    # Remove environment variables declared above
    os.environ.pop("SG_LDAP_USER")
    os.environ.pop("SG_LDAP_PASSWD")

    test_msg = "List accounts - Take invalid ldapuser and ldappasswd from config"

    new_config_entries = {'SG_LDAP_PASSWD': 'sgiamadmin#', 'SG_LDAP_USER': 'ldapadmin#'}
    update_config_yaml(new_config_entries)

    result = AuthTest(test_msg).list_account().execute_test(negative_case=True)

    result.command_should_match_pattern("Failed to list accounts")

    restore_config_yaml()

    #TODO - Need to fix this test. Currently skipping this test as it waits for password to be entered manually through prompt.
    '''
    test_msg = "List accounts - Take ldapuser and ldappasswd from prompt"

    _use_root_credentials()
    accounts_response_pattern = "Enter Ldap User Id: Enter Ldap password: AccountName = [\w-]*, AccountId = [\w-]*, CanonicalId = [\w-]*, Email = [\w.@]*"
    stdin_values =  S3ClientConfig.ldapuser + '\n' + S3ClientConfig.ldappasswd
    S3ClientConfig.ldapuser = None
    S3ClientConfig.ldappasswd =  None
    result = AuthTest(test_msg).list_account().execute_test(False, False, stdin_values)
    result.command_should_match_pattern(accounts_response_pattern)

    '''
    load_test_config()


# Test create user API
# Case 1 - Path not given (take default value).
# Case 2 - Path given
def user_tests():
    _use_root_credentials()

    date_pattern = "[0-9]{4}-(0[1-9]|1[0-2])-(0[1-9]|[1-2][0-9]|3[0-1]) (2[0-3]|[01][0-9]):[0-5][0-9]:[0-5][0-9][\+-][0-9]*:[0-9]*"


#Below account creation is for aws iam cli system testing. First delete account if already exist and then create it newly
    test_msg = "Delete account if already exist"
    account_args = {}
    test_msg = "Delete account aws_iam_test_account"
    account_args = {'AccountName': 'aws_iam_test_account'}
    AuthTest(test_msg).delete_account(**account_args).execute_test(negative_case=True)

    test_msg = "Create account aws_iam_test_account"
    account_args = {'AccountName': 'aws_iam_test_account', 'Email': 'iam@seagate.com', 'ldapuser': S3ClientConfig.ldapuser, 'ldappasswd': S3ClientConfig.ldappasswd}
    account_response_pattern = "AccountId = [\w-]*, CanonicalId = [\w-]*, RootUserName = [\w+=,.@-]*, AccessKeyId = [\w-]*, SecretKey = [\w/+]*$"
    result = AuthTest(test_msg).create_account(**account_args).execute_test()
    result.command_should_match_pattern(account_response_pattern)
    account_response_elements = get_response_elements(result.status.stdout)
#Save the details in file
    f = open("aws_iam_credential_file" , "w")
    f.write("[default]\n")
    f.write("aws_access_key_id = ")
    f.write(account_response_elements['AccessKeyId'])
    f.write("\naws_secret_access_key = ")
    f.write(account_response_elements['SecretKey'])
    f.close()

#GetTempAuth Start


    #Create account
    test_msg = "Create account tempAuthTestAccount"
    account_args = {'AccountName': 'tempAuthTestAccount', 'Email': 'tempAuthTestAccount@seagate.com', \
                   'ldapuser': S3ClientConfig.ldapuser, \
                   'ldappasswd': S3ClientConfig.ldappasswd}
    account_response_pattern = "AccountId = [\w-]*, CanonicalId = [\w-]*, RootUserName = [\w+=,.@-]*, AccessKeyId = [\w-]*, SecretKey = [\w/+]*$"
    result1 = AuthTest(test_msg).create_account(**account_args).execute_test()
    result1.command_should_match_pattern(account_response_pattern)
    account_response_elements = get_response_elements(result1.status.stdout)
    access_key_args = {}
    access_key_args['AccountName'] = "tempAuthTestAccount"
    access_key_args['AccessKeyId'] = account_response_elements['AccessKeyId']
    access_key_args['SecretAccessKey'] = account_response_elements['SecretKey']
    s3test_access_key = S3ClientConfig.access_key_id
    s3test_secret_key = S3ClientConfig.secret_key
    S3ClientConfig.access_key_id = access_key_args['AccessKeyId']
    S3ClientConfig.secret_key = access_key_args['SecretAccessKey']

    #Create Account LoginProfile for tempAuthTestAccount"
    test_msg = 'create account login profile should succeed.'
    account_profile_response_pattern = "Account Login Profile: CreateDate = [\s\S]*, PasswordResetRequired = false, AccountName = [\s\S]*"
    user_args = {}
    account_name_flag = "-n"
    password_flag = "--password"
    user_args['AccountName'] ="tempAuthTestAccount"
    user_args['Password'] = "accountpassword"
    result = AuthTest(test_msg).create_account_login_profile(account_name_flag , password_flag,\
               **user_args).execute_test()
    result.command_should_match_pattern(account_profile_response_pattern)

    date_pattern_for_tempAuthCred = "[0-9]{4}-(0[1-9]|1[0-2])-(0[1-9]|[1-2][0-9]|3[0-1])T(2[0-3]|[01][0-9]):[0-5][0-9]:[0-5][0-9].[0-9]*[\+-][0-9]*"
    #Get Temp Auth Credentials for account for account
    access_key_args['Password'] = "accountpassword"
    test_msg = 'GetTempAuthCredentials success'
    account_name_flag = "-a"
    password_flag = "--password"
    response_pattern = "AccessKeyId = [\w-]*, SecretAccessKey = [\w/+]*, ExpiryTime = "+date_pattern_for_tempAuthCred+", SessionToken = [\w/+]*$"
    result = AuthTest(test_msg).get_temp_auth_credentials(account_name_flag, password_flag ,**access_key_args).execute_test()
    result.command_should_match_pattern(response_pattern)
    #Setting aws temporary credentials under environment variables
    response_elements = get_response_elements(result.status.stdout)
    os.environ["AWS_ACCESS_KEY_ID"] = response_elements['AccessKeyId']
    os.environ["AWS_SECRET_ACCESS_KEY"] = response_elements['SecretAccessKey']
    os.environ["AWS_SESSION_TOKEN"] = response_elements['SessionToken']
    AwsTest('Aws can create bucket').create_bucket("tempcredbucket").execute_test().command_is_successful()
    AwsTest('Aws can delete bucket').delete_bucket("tempcredbucket").execute_test().command_is_successful()
    del os.environ["AWS_ACCESS_KEY_ID"]
    del os.environ["AWS_SECRET_ACCESS_KEY"]
    del os.environ["AWS_SESSION_TOKEN"]
    #Create User
    access_key_args['UserName'] = "u1"
    test_msg = "Create User u1"
    user1_response_pattern = "UserId = [\w-]*, ARN = [\S]*, Path = /$"
    result = AuthTest(test_msg).create_user(**access_key_args).execute_test()
    result.command_should_match_pattern(user1_response_pattern)
    #Create user loginprofile
    access_key_args['Password'] = "userpassword"
    test_msg = 'create user login profile for u1'
    user_name_flag = "-n"
    password_flag = "--password"
    result = AuthTest(test_msg).create_login_profile(user_name_flag , password_flag,\
               **access_key_args).execute_test()
    #Get Temp Auth Credentials for account for user u1
    test_msg = 'GetTempAuthCredentials success'
    account_name_flag = "-a"
    password_flag = "--password"
    response_pattern = "AccessKeyId = [\w-]*, SecretAccessKey = [\w/+]*, ExpiryTime = "+date_pattern_for_tempAuthCred+", SessionToken = [\w/+]*$"
    result = AuthTest(test_msg).get_temp_auth_credentials(account_name_flag, password_flag ,**access_key_args).execute_test()
    result.command_should_match_pattern(response_pattern)

    #Get Temp Auth Credentials for account with duration more than max allowed
    test_msg = 'GetTempAuthCredentials failure'
    account_name_flag = "-a"
    password_flag = "--password"
    access_key_args['Duration'] = "500000"
    result = AuthTest(test_msg).get_temp_auth_credentials(account_name_flag, password_flag ,**access_key_args).execute_test(negative_case=True)
    result.command_response_should_have("MaxDurationIntervalExceeded")

    #Get Temp Auth Credentials for account with duration less than minimum required
    test_msg = 'GetTempAuthCredentials failure'
    account_name_flag = "-a"
    password_flag = "--password"
    access_key_args['Duration'] = "50"
    result = AuthTest(test_msg).get_temp_auth_credentials(account_name_flag, password_flag ,**access_key_args).execute_test(negative_case=True)
    result.command_response_should_have("MinDurationIntervalNotMaintained")

    #Update password Reset Flag and check
    #update userlogin profile
    test_msg = 'update user login profile for u1'
    access_key_args['PasswordResetRequired']=True
    result = AuthTest(test_msg).update_login_profile(user_name_flag ,**access_key_args).execute_test()
    result.command_response_should_have("UpdateUserLoginProfile is successful")

    #Get Temp Auth Credentials for account for passwordreset True
    test_msg = 'GetTempAuthCredentials failure'
    account_name_flag = "-a"
    password_flag = "--password"
    result = AuthTest(test_msg).get_temp_auth_credentials(account_name_flag, password_flag ,**access_key_args).execute_test(negative_case=True)
    result.command_response_should_have("PasswordResetRequired")

    # UpdateAccountLoginProfile and DeleteAccount with Temp Credentials -- Start

    os.environ["AWS_ACCESS_KEY_ID"] = response_elements['AccessKeyId']
    os.environ["AWS_SECRET_ACCESS_KEY"] = response_elements['SecretAccessKey']
    os.environ["AWS_SESSION_TOKEN"] = response_elements['SessionToken']

    test_msg = 'UpdateAccountLoginProfile Successfull'
    account_args = {}
    account_name_flag = "-n"
    password_flag = "--password"
    account_args['AccountName'] ="tempAuthTestAccount"
    account_args['Password'] ="newpwd1234"
    account_profile_response_pattern = "Account login profile updated."
    result = AuthTest(test_msg).update_account_login_profile(account_name_flag, password_flag, **account_args).execute_test()
    result.command_should_match_pattern(account_profile_response_pattern)

    #Delete account
    test_msg = "Delete account tempAuthTestAccount"
    account_args = {'AccountName': 'tempAuthTestAccount', 'Email': 'tempAuthTestAccount@seagate.com',  'force': True}
    AuthTest(test_msg).delete_account(**account_args).execute_test()\
            .command_response_should_have("Account deleted successfully")
    S3ClientConfig.access_key_id = s3test_access_key
    S3ClientConfig.secret_key = s3test_secret_key

    del os.environ["AWS_ACCESS_KEY_ID"]
    del os.environ["AWS_SECRET_ACCESS_KEY"]
    del os.environ["AWS_SESSION_TOKEN"]
    
    # UpdateAccountLoginProfile and DeleteAccount with Temp Credentials -- End

#GetTempAuth End

    test_msg = "Create User s3user1 (default path)"
    user_args = {'UserName': 's3user1'}
    user1_response_pattern = "UserId = [\w-]*, ARN = [\S]*, Path = /$"
    result = AuthTest(test_msg).create_user(**user_args).execute_test()
    result.command_should_match_pattern(user1_response_pattern)

    test_msg = 'Update User s3user1 (new name = s3user1New, new path - /test/success)'
    user_args = {}
    user_args['UserName'] = "s3user1"
    user_args['NewUserName'] = "s3user1New"
    user_args['NewPath'] = "/test/success/"
    result = AuthTest(test_msg).update_user(**user_args).execute_test()
    result.command_response_should_have("User Updated.")

    test_msg = 'create user login profile should fail for exceeding max allowed password length.'
    user_args = {}
    maxPasswordLength = "abcdefghijklmnopqrstuvwxyzabcdefghijkabcdefghijklmnopqrstuvwxyzabcdefghijkabcdefghijklmnopqrstuvwxyzabcdefghijk\
abcdefghijklmnopqrstuvwxyzabcdefghijkjabcdefghijklmnopqrstuvwxyzabcdefghijkjabcdefghijklmnopqrstuvwxyzabcdefghijkjabcdefghijklmnopqrddd";
    user_name_flag = "-n"
    password_flag = "--password"
    user_args['UserName'] = "s3user1New"
    user_args['Password'] = maxPasswordLength;
    result = AuthTest(test_msg).create_login_profile(user_name_flag, password_flag,\
               **user_args).execute_test(negative_case=True)
    result.command_response_should_have("Failed to create userloginprofile.")

    test_msg = 'create user login profile should fail for invalid username.'
    user_args = {}
    user_name_flag = "-n"
    password_flag = "--password"

    user_args['UserName'] = "s3userinvalidname"
    user_args['Password'] = "abcdef"
    result = AuthTest(test_msg).create_login_profile(user_name_flag, password_flag,\
              **user_args).execute_test(negative_case=True)
    result.command_response_should_have("Failed to create userloginprofile.")

    test_msg = 'create user login profile should fail for empty username.'
    user_args = {}
    user_name_flag = "-n"
    password_flag = "--password"
    user_args['UserName'] ="\"\""
    user_args['Password'] = "abcdre"
    result = AuthTest(test_msg).create_login_profile(user_name_flag, password_flag,\
                **user_args).execute_test(negative_case=True)
    result.command_response_should_have("Failed to create userloginprofile.")

    test_msg = 'create user login profile should fail for username missing.'
    user_args = {}
    user_name_flag = ""
    password_flag = "--password"
    user_args['UserName'] =""
    user_args['Password'] = "abcdref"
    result = AuthTest(test_msg).create_login_profile(user_name_flag , password_flag,\
               **user_args).execute_test(negative_case=True)
    result.command_response_should_have("User name is required for user login-profile creation")

    test_msg = 'create user login profile should fail for password missing.'
    user_args = {}
    user_name_flag = "-n"
    password_flag = ""
    user_args['UserName'] ="abcd"
    user_args['Password'] = ""
    result = AuthTest(test_msg).create_login_profile(user_name_flag , password_flag,\
               **user_args).execute_test(negative_case=True)
    result.command_response_should_have("User password is required for user login-profile creation")

    test_msg = 'create user login profile should fail for password length less than 6 with PasswordPolicyVoilation.'
    user_args = {}
    user_name_flag = "-n"
    password_flag = "--password"
    user_args['UserName'] ="s3user1New"
    user_args['Password'] = "abcd"
    result = AuthTest(test_msg).create_login_profile(user_name_flag , password_flag,\
               **user_args).execute_test(negative_case=True)
    result.command_response_should_have("PasswordPolicyVoilation")

    test_msg = 'create user login profile should fail with username as root.'
    user_args = {}
    user_name_flag = "-n"
    password_flag = "--password"
    user_args['UserName'] ="root"
    user_args['Password'] = "pqrsef"
    result = AuthTest(test_msg).create_login_profile(user_name_flag , password_flag,\
               **user_args).execute_test(negative_case=True)
    result.command_response_should_have("Cannot create account login profile with CreateUserLoginProfile")

    test_msg = 'create user login profile should succeed.'
    user_args = {}
    user_name_flag = "-n"
    password_flag = "--password"
    user_args['UserName'] ="s3user1New"
    user_args['Password'] = "abcdefg"
    login_profile_response_pattern = "Login Profile "+date_pattern+" False "+user_args['UserName']
    result = AuthTest(test_msg).create_login_profile(user_name_flag , password_flag,\
               **user_args).execute_test()
    result.command_should_match_pattern(login_profile_response_pattern)

    test_msg = 'create user login profile failed for user with existing login profile'
    result = AuthTest(test_msg).create_login_profile(user_name_flag , password_flag,\
               **user_args).execute_test(negative_case=True)
    result.command_response_should_have("EntityAlreadyExists")

    #********* Test create user login profile with --password-reset-required *********************
    test_msg = 'Create User user01'
    user_args = {'UserName': 'user01'}
    user1_response_pattern = "UserId = [\w-]*, ARN = [\S]*, Path = /$"
    result = AuthTest(test_msg).create_user(**user_args).execute_test()
    result.command_should_match_pattern(user1_response_pattern)
    test_msg = 'create user login profile should succeed with --password-reset-required'
    user_args = {}
    user_name_flag = "-n"
    password_flag  = "--password"
    user_args['UserName'] = "user01"
    user_args['Password'] = "abcdef"
    user_args['PasswordResetRequired'] = "True"
    login_profile_response_pattern = "Login Profile "+date_pattern+" "+user_args['PasswordResetRequired']+" "+user_args['UserName']
    result = AuthTest(test_msg).create_login_profile(user_name_flag , password_flag,\
               **user_args).execute_test()
    result.command_should_match_pattern(login_profile_response_pattern)
    test_msg = 'Delete User user01'
    user_args = {}
    user_args['UserName'] = "user01"
    user_args['Password'] = "abcdef"
    result = AuthTest(test_msg).delete_user(**user_args).execute_test()
    result.command_response_should_have("User deleted.")

    #********* Test create user login profile with --no-password-reset-required *********************
    test_msg = 'Create User user02'
    user_args = {'UserName': 'user02'}
    user1_response_pattern = "UserId = [\w-]*, ARN = [\S]*, Path = /$"
    result = AuthTest(test_msg).create_user(**user_args).execute_test()
    result.command_should_match_pattern(user1_response_pattern)
    test_msg = 'create user login profile should succeed with --no-password-reset-required'
    user_args = {}
    user_name_flag = "-n"
    password_flag  = "--password"
    user_args['UserName'] = "user02"
    user_args['Password'] = "abcddt"
    user_args['PasswordResetRequired'] = "False"
    login_profile_response_pattern = "Login Profile "+date_pattern+" "+user_args['PasswordResetRequired']+" "+user_args['UserName']
    result = AuthTest(test_msg).create_login_profile(user_name_flag , password_flag,\
               **user_args).execute_test()
    result.command_should_match_pattern(login_profile_response_pattern)
    test_msg = 'Delete User user02'
    user_args = {}
    user_args['UserName'] = "user02"
    user_args['Password'] = "abcddt"
    result = AuthTest(test_msg).delete_user(**user_args).execute_test()
    result.command_response_should_have("User deleted.")


    test_msg = 'GetUserLoginProfile Successfull'
    user_args = {}
    user_name_flag = "-n"
    user_args['UserName'] ="s3user1New"
    user_profile_response_pattern = "Login Profile "+date_pattern+" False "+user_args['UserName']
    result = AuthTest(test_msg).get_login_profile(user_name_flag , **user_args).execute_test()
    result.command_should_match_pattern(user_profile_response_pattern)

    test_msg = 'GetUserLoginProfile failed for invalid user'
    user_args = {}
    user_name_flag = "-n"
    user_args['UserName'] ="abcd"
    result = AuthTest(test_msg).get_login_profile(user_name_flag , **user_args).execute_test(negative_case=True)
    result.command_response_should_have("Failed to get Login Profile")

    test_msg = 'GetUserLoginProfile should fail with username as root'
    user_args = {}
    user_name_flag = "-n"
    user_args['UserName'] ="root"
    result = AuthTest(test_msg).get_login_profile(user_name_flag , **user_args).execute_test(negative_case=True)
    result.command_response_should_have("Cannot get account login profile with GetUserLoginProfile")


    test_msg = "Create User loginProfileTestUser (default path)"
    user_args = {'UserName': 'loginProfileTestUser'}
    user1_response_pattern = "UserId = [\w-]*, ARN = [\S]*, Path = /$"
    result = AuthTest(test_msg).create_user(**user_args).execute_test()
    result.command_should_match_pattern(user1_response_pattern)
    test_msg = 'GetUserLoginProfile failed for user without LoginProfile created'
    user_args = {}
    user_name_flag = "-n"
    user_args['UserName'] ="loginProfileTestUser"
    result = AuthTest(test_msg).get_login_profile(user_name_flag , **user_args).execute_test(negative_case=True)
    result.command_response_should_have("NoSuchEntity")
    test_msg = 'Delete User loginProfileTestUser'
    user_args = {}
    user_args['UserName'] = "loginProfileTestUser"
    result = AuthTest(test_msg).delete_user(**user_args).execute_test()
    result.command_response_should_have("User deleted.")



    test_msg = "Create User updateLoginProfileTestUser (default path)"
    user_args = {'UserName': 'updateLoginProfileTestUser'}
    user1_response_pattern = "UserId = [\w-]*, ARN = [\S]*, Path = /$"
    result = AuthTest(test_msg).create_user(**user_args).execute_test()
    result.command_should_match_pattern(user1_response_pattern)
    test_msg = 'Create access key (user name is updateLoginProfileTestUser)'
    access_key_args = {}
    access_key_args['UserName'] = 'updateLoginProfileTestUser'
    accesskey_response_pattern = "AccessKeyId = [\w-]*, SecretAccessKey = [\w/+]*, Status = [\w]*$"
    result = AuthTest(test_msg).create_access_key(**access_key_args).execute_test()
    result.command_should_match_pattern(accesskey_response_pattern)
    accesskey_response_elements = get_response_elements(result.status.stdout)
    access_key_args['AccessKeyId'] = accesskey_response_elements['AccessKeyId']
    access_key_args['SecretAccessKey'] = accesskey_response_elements['SecretAccessKey']
    test_msg = 'UpdateLoginProfile should fail when tried with IAM User accessKey-secretKey'
    user_name_flag = "-n"
    access_key_args['UserName'] ="updateLoginProfileTestUser"
    access_key_args['Password'] = "newPassword"
    result = AuthTest(test_msg).update_login_profile_with_user_key(user_name_flag , **access_key_args).execute_test(negative_case=True)
    result.command_response_should_have("InvalidUser")
    test_msg = 'Delete access key'
    result = AuthTest(test_msg).delete_access_key(**access_key_args).execute_test()
    result.command_response_should_have("Access key deleted.")
    test_msg = 'Delete User UpdateLoginProfileTestUser'
    user_args = {}
    user_args['UserName'] = "updateLoginProfileTestUser"
    result = AuthTest(test_msg).delete_user(**user_args).execute_test()
    result.command_response_should_have("User deleted.")



    test_msg = 'UpdateLoginProfile is successful'
    user_args = {}
    user_name_flag = "-n"
    user_args['UserName'] ="s3user1New"
    user_args['Password'] = "newPassword"
    result = AuthTest(test_msg).update_login_profile(user_name_flag , **user_args).execute_test()
    result.command_response_should_have("UpdateUserLoginProfile is successful")

    test_msg = 'UpdateLoginProfile fails without new password ,password-reset and no-password-reset flag entered'
    user_args = {}
    user_name_flag = "-n"
    user_args['UserName'] ="s3user1New"
    result = AuthTest(test_msg).update_login_profile(user_name_flag , **user_args).execute_test(negative_case=True)
    result.command_response_should_have("Please provide password or password-reset flag")

    test_msg = 'UpdateLoginProfile should fail with username as root'
    user_args = {}
    user_name_flag = "-n"
    user_args['UserName'] ="root"
    user_args['Password'] = "newPassword"
    result = AuthTest(test_msg).update_login_profile(user_name_flag , **user_args).execute_test(negative_case=True)
    result.command_response_should_have("Cannot update account login profile with UpdateUserLoginProfile")

    test_msg = 'UpdateLoginProfile should fail for password length less than 6 with PasswordPolicyVoilation.'
    user_args = {}
    user_name_flag = "-n"
    user_args['UserName'] ="s3user1New"
    user_args['Password'] = "abcd"
    result = AuthTest(test_msg).update_login_profile(user_name_flag , **user_args).execute_test(negative_case=True)
    result.command_response_should_have("PasswordPolicyVoilation")

    test_msg = 'UpdateLoginProfile is successful with only password-reset flag entered'
    user_args = {}
    user_name_flag = "-n"
    user_args['UserName'] ="s3user1New"
    user_args['PasswordResetRequired']=True
    result = AuthTest(test_msg).update_login_profile(user_name_flag , **user_args).execute_test()
    result.command_response_should_have("UpdateUserLoginProfile is successful")
    test_msg = 'GetLoginProfile to validate password reset flag set to True'
    user_args = {}
    user_name_flag = "-n"
    user_args['UserName'] ="s3user1New"
    result = AuthTest(test_msg).get_login_profile(user_name_flag , **user_args).execute_test()
    result.command_response_should_have("True")

    test_msg = "Create User updateLoginProfileTestUser (default path)"
    user_args = {'UserName': 'updateLoginProfileTestUser'}
    user1_response_pattern = "UserId = [\w-]*, ARN = [\S]*, Path = /$"
    result = AuthTest(test_msg).create_user(**user_args).execute_test()
    result.command_should_match_pattern(user1_response_pattern)
    test_msg = 'UpdateUserLoginProfile failed for user without LoginProfile created'
    user_args = {}
    user_name_flag = "-n"
    user_args['UserName'] ="updateLoginProfileTestUser"
    user_args['Password'] = "newPassword"
    result = AuthTest(test_msg).update_login_profile(user_name_flag , **user_args).execute_test(negative_case=True)
    result.command_response_should_have("NoSuchEntity")
    test_msg = 'Delete User updateLoginProfileTestUser'
    user_args = {}
    user_args['UserName'] = "updateLoginProfileTestUser"
    result = AuthTest(test_msg).delete_user(**user_args).execute_test()
    result.command_response_should_have("User deleted.")

    test_msg = 'UpdateUserLoginProfile failed for username missing.'
    user_args = {}
    user_name_flag = ""
    user_args['UserName'] =""
    user_args['Password'] = "abcdefd"
    result = AuthTest(test_msg).update_login_profile(user_name_flag , **user_args).execute_test(negative_case=True)
    result.command_response_should_have("UserName is required for UpdateUserLoginProfile")

    test_msg = 'UpdateLoginProfile failed as user doesnt exist in ldap'
    user_args = {}
    user_name_flag = "-n"
    user_args['UserName'] ="dummyUser"
    user_args['Password'] = "password"
    result = AuthTest(test_msg).update_login_profile(user_name_flag ,  **user_args).execute_test(negative_case=True)
    result.command_response_should_have("UpdateUserLoginProfile failed")

    test_msg = 'UpdateLoginProfile failed for invalid username'
    user_args = {}
    user_name_flag = "-n"
    user_args['UserName'] ="dummyUser$"
    user_args['Password'] = "password"
    result = AuthTest(test_msg).update_login_profile(user_name_flag ,  **user_args).execute_test(negative_case=True)
    result.command_response_should_have("InvalidParameterValue")


    #*************************Test s3iamcli ChangePassword for IAM user******************
    test_msg = "Create User changePasswordUserLoginProfileTestUser "
    user_args = {'UserName': 'changePasswordUserLoginProfileTestUser'}
    user1_response_pattern = "UserId = [\w-]*, ARN = [\S]*, Path = /$"
    result = AuthTest(test_msg).create_user(**user_args).execute_test()
    result.command_should_match_pattern(user1_response_pattern)

    test_msg = 'Create access key (user name is changePasswordUserLoginProfileTestUser)'
    access_key_args = {}
    user_access_key_args = {}
    access_key_args['UserName'] = 'changePasswordUserLoginProfileTestUser'
    accesskey_response_pattern = "AccessKeyId = [\w-]*, SecretAccessKey = [\w/+]*, Status = [\w]*$"
    result = AuthTest(test_msg).create_access_key(**access_key_args).execute_test()
    result.command_should_match_pattern(accesskey_response_pattern)
    accesskey_response_elements = get_response_elements(result.status.stdout)
    user_access_key_args['AccessKeyId'] = accesskey_response_elements['AccessKeyId']
    user_access_key_args['SecretAccessKey'] = accesskey_response_elements['SecretAccessKey']

    test_msg = 'create user login profile for changePasswordUserLoginProfileTestUser.'
    user_args = {}
    user_name_flag = "-n"
    password_flag = "--password"
    user_args['UserName'] ="changePasswordUserLoginProfileTestUser"
    user_args['Password'] = "abcdfs"
    login_profile_response_pattern = "Login Profile "+date_pattern+" False "+user_args['UserName']
    result = AuthTest(test_msg).create_login_profile(user_name_flag , password_flag,\
               **user_args).execute_test()
    result.command_should_match_pattern(login_profile_response_pattern)

    test_msg = 'ChangePassword should fail with root accessKey-secretKey, user OldPassword and NewPassword.'
    account_user_access_key_args = {}
    account_user_access_key_args['AccessKeyId'] = S3ClientConfig.access_key_id
    account_user_access_key_args['SecretAccessKey'] = S3ClientConfig.secret_key
    account_user_access_key_args['OldPassword'] ="abcdfs"
    account_user_access_key_args['NewPassword'] = "pqrswq"
    result = AuthTest(test_msg).change_user_password(**account_user_access_key_args).execute_test(negative_case=True)
    result.command_response_should_have("ChangePassword failed")
    result.command_response_should_have("InvalidUserType")

    test_msg = 'ChangePassword should fail with IAM user accessKey-secretKey,NewPassword and invalid oldPassword.'
    test_access_key_args = {}
    test_access_key_args['AccessKeyId'] = user_access_key_args['AccessKeyId']
    test_access_key_args['SecretAccessKey'] = user_access_key_args['SecretAccessKey']
    test_access_key_args['NewPassword'] = "pqrswq"
    test_access_key_args['OldPassword'] = "pqrsqq"
    result = AuthTest(test_msg).change_user_password(**test_access_key_args).execute_test(negative_case=True)
    result.command_response_should_have("ChangePassword failed")
    result.command_response_should_have("InvalidPassword")

    test_msg = 'ChangePassword should fail with for password length less than 6 with PasswordPolicyVoilation.'
    test_access_key_args = {}
    test_access_key_args['AccessKeyId'] = user_access_key_args['AccessKeyId']
    test_access_key_args['SecretAccessKey'] = user_access_key_args['SecretAccessKey']
    test_access_key_args['NewPassword'] = "pqrs"
    test_access_key_args['OldPassword'] = "abcdfs"
    result = AuthTest(test_msg).change_user_password(**test_access_key_args).execute_test(negative_case=True)
    result.command_response_should_have("ChangePassword failed")
    result.command_response_should_have("PasswordPolicyVoilation")

    test_msg = 'ChangePassword with IAM User accessKey-secretKey, OldPassword and NewPassowrd should succeed.'
    user_access_key_args['OldPassword'] ="abcdfs"
    user_access_key_args['NewPassword'] = "pqrsoe"
    result = AuthTest(test_msg).change_user_password(**user_access_key_args).execute_test()
    result.command_response_should_have("ChangePassword is successful")

    test_msg = 'Two subsequent ChangePassword with valid password value should succeed - first changepassword'
    user_access_key_args['OldPassword'] ="pqrsoe"
    user_access_key_args['NewPassword'] = "vcxvsd"
    result = AuthTest(test_msg).change_user_password(**user_access_key_args).execute_test()
    result.command_response_should_have("ChangePassword is successful")
    test_msg = 'Two subsequent ChangePassword with valid password value should succeed - second changepassword'
    user_access_key_args['OldPassword'] ="vcxvsd"
    user_access_key_args['NewPassword'] = "xyzdet"
    result = AuthTest(test_msg).change_user_password(**user_access_key_args).execute_test()
    result.command_response_should_have("ChangePassword is successful")

    test_msg = 'ChangePassword with same value for oldPassword and newPassword should fail.'
    user_access_key_args['OldPassword'] ="xyzdet"
    user_access_key_args['NewPassword'] = "xyzdet"
    result = AuthTest(test_msg).change_user_password(**user_access_key_args).execute_test(negative_case=True)
    result.command_response_should_have("ChangePassword failed")
    result.command_response_should_have("InvalidPassword")

    test_msg = 'ChangePassword with empty value i.e\"\" for newPassword should fail.'
    user_access_key_args['OldPassword'] ="xyzdet"
    user_access_key_args['NewPassword'] = "\"\""
    result = AuthTest(test_msg).change_user_password(**user_access_key_args).execute_test(negative_case=True)
    result.command_response_should_have("ChangePassword failed")
    result.command_response_should_have("Invalid length for parameter NewPassword")

    test_msg = 'ChangePassword with special character i.e. pqrsdd\\t as newPassword should succeed.'
    user_access_key_args['OldPassword'] ="xyzdet"
    user_access_key_args['NewPassword'] = "pqrsdd\\t"
    result = AuthTest(test_msg).change_user_password(**user_access_key_args).execute_test()
    result.command_response_should_have("ChangePassword is successful")

    test_msg = 'ChangePassword with space i.e." avcghj " as newPassword should succeed.'
    user_access_key_args['OldPassword'] ="pqrsdd\\t"
    user_access_key_args['NewPassword'] = " avcghj "
    result = AuthTest(test_msg).change_user_password(**user_access_key_args).execute_test()
    result.command_response_should_have("ChangePassword is successful")

    test_msg = 'ChangePassword with special character e.g xvc#?*% as newPassword should succeed.'
    user_access_key_args['OldPassword'] =" avcghj "
    user_access_key_args['NewPassword'] = "xvc#?*%"
    result = AuthTest(test_msg).change_user_password(**user_access_key_args).execute_test()
    result.command_response_should_have("ChangePassword is successful")

    test_msg = "Create User TestUser "
    user_args = {'UserName': 'TestUser'}
    user1_response_pattern = "UserId = [\w-]*, ARN = [\S]*, Path = /$"
    result = AuthTest(test_msg).create_user(**user_args).execute_test()
    result.command_should_match_pattern(user1_response_pattern)

    test_msg = 'Create access key (user name is TestUser)'
    access_key_args = {}
    test_user_access_key_args = {}
    access_key_args['UserName'] = 'TestUser'
    accesskey_response_pattern = "AccessKeyId = [\w-]*, SecretAccessKey = [\w/+]*, Status = [\w]*$"
    result = AuthTest(test_msg).create_access_key(**access_key_args).execute_test()
    result.command_should_match_pattern(accesskey_response_pattern)
    accesskey_response_elements = get_response_elements(result.status.stdout)
    test_user_access_key_args['AccessKeyId'] = accesskey_response_elements['AccessKeyId']
    test_user_access_key_args['SecretAccessKey'] = accesskey_response_elements['SecretAccessKey']

    test_msg = 'ChangePassword should fail with another IAM user(i.e.TestUser) accessKey-secretKey, OldPassword and NewPassword.'
    test_user_access_key_args['OldPassword'] ="pqrsdd"
    test_user_access_key_args['NewPassword'] = "xyzadd"
    result = AuthTest(test_msg).change_user_password(**account_user_access_key_args).execute_test(negative_case=True)
    result.command_response_should_have("ChangePassword failed")
    result.command_response_should_have("InvalidUserType")

    test_msg = 'Delete access key for changePasswordUserLoginProfileTestUser'
    result = AuthTest(test_msg).delete_access_key(**user_access_key_args).execute_test()
    result.command_response_should_have("Access key deleted.")

    test_msg = 'Delete access key for TestUser'
    result = AuthTest(test_msg).delete_access_key(**test_user_access_key_args).execute_test()
    result.command_response_should_have("Access key deleted.")

    test_msg = 'Delete User changePasswordUserLoginProfileTestUser'
    user_args = {}
    user_args['UserName'] = "changePasswordUserLoginProfileTestUser"
    result = AuthTest(test_msg).delete_user(**user_args).execute_test()
    result.command_response_should_have("User deleted.")

    test_msg = 'Delete User TestUser'
    user_args = {}
    user_args['UserName'] = "TestUser"
    result = AuthTest(test_msg).delete_user(**user_args).execute_test()
    result.command_response_should_have("User deleted.")



    test_msg = 'create account login profile should succeed.'
    account_profile_response_pattern = "Account Login Profile: CreateDate = [\s\S]*, PasswordResetRequired = false, AccountName = [\s\S]*"
    user_args = {}
    account_name_flag = "-n"
    password_flag = "--password"
    user_args['AccountName'] ="s3test"
    user_args['Password'] = "abcdiu"
    result = AuthTest(test_msg).create_account_login_profile(account_name_flag , password_flag,\
               **user_args).execute_test()
    result.command_should_match_pattern(account_profile_response_pattern)

    test_msg = 'create account login profile should fail for already created profile.'
    user_args = {}
    account_name_flag = "-n"
    password_flag = "--password"
    user_args['AccountName'] ="s3test"
    user_args['Password'] = "abcdiu"
    result = AuthTest(test_msg).create_account_login_profile(account_name_flag , password_flag,\
               **user_args).execute_test(negative_case=True)
    result.command_response_should_have("The request was rejected because it attempted to create or update a resource that already exists")

    test_msg = 'create account login profile should fail for exceeding max allowed password length.'
    user_args = {}
    maxPasswordLength = "abcdefghijklmnopqrstuvwxyzabcdefghijkabcdefghijklmnopqrstuvwxyzabcdefghijkabcdefghijklmnopqrstuvwxyzabcdefghijk\
abcdefghijklmnopqrstuvwxyzabcdefghijkjabcdefghijklmnopqrstuvwxyzabcdefghijkjabcdefghijklmnopqrstuvwxyzabcdefghijkjabcdefghijklmnopqrddd";
    account_name_flag = "-n"
    password_flag = "--password"
    user_args['AccountName'] ="s3test"
    user_args['Password'] = maxPasswordLength;
    result = AuthTest(test_msg).create_account_login_profile(account_name_flag , password_flag,\
               **user_args).execute_test(negative_case=True)
    result.command_response_should_have("Failed to create Account login profile")

    test_msg = 'create account login profile should fail for empty account name.'
    user_args = {}
    account_name_flag = "-n"
    password_flag = "--password"
    user_args['AccountName'] ="\"\""
    user_args['Password'] = "abcdriu"
    result = AuthTest(test_msg).create_account_login_profile(account_name_flag , password_flag,\
               **user_args).execute_test(negative_case=True)
    result.command_response_should_have("Account name is required")

    test_msg = 'create account login profile should fail for account missing name.'
    user_args = {}
    account_name_flag = ""
    password_flag = "--password"
    user_args['AccountName'] =""
    user_args['Password'] = "abcdriu"
    result = AuthTest(test_msg).create_account_login_profile(account_name_flag , password_flag,\
               **user_args).execute_test(negative_case=True)
    result.command_response_should_have("Account name is required")

    test_msg = 'create account login profile should fail for password missing.'
    user_args = {}
    account_name_flag = "-n"
    password_flag = ""
    user_args['AccountName'] ="abcd"
    user_args['Password'] = ""
    result = AuthTest(test_msg).create_account_login_profile(account_name_flag , password_flag,\
               **user_args).execute_test(negative_case=True)
    result.command_response_should_have("Account login password is required")

    test_msg = "Create account s3test_loginprofile0"
    account_args = {'AccountName': 's3test_loginprofile', 'Email': 's3test_loginprofile@seagate.com', \
                   'ldapuser': S3ClientConfig.ldapuser, \
                   'ldappasswd': S3ClientConfig.ldappasswd}
    account_response_pattern = "AccountId = [\w-]*, CanonicalId = [\w-]*, RootUserName = [\w+=,.@-]*, AccessKeyId = [\w-]*, SecretKey = [\w/+]*$"
    result1 = AuthTest(test_msg).create_account(**account_args).execute_test()
    result1.command_should_match_pattern(account_response_pattern)
    account_response_elements1 = get_response_elements(result1.status.stdout)
    test_msg = "Create User accountLoginProfileTestUser"
    user_args = {'UserName': 'accountLoginProfileTestUser'}
    user1_response_pattern = "UserId = [\w-]*, ARN = [\S]*, Path = /$"
    result = AuthTest(test_msg).create_user(**user_args).execute_test()
    result.command_should_match_pattern(user1_response_pattern)
    test_msg = 'Create access key (user name is accountLoginProfileTestUser)'
    access_key_args = {}
    access_key_args['AccountName'] = 'accountLoginProfileTestUser'
    accesskey_response_pattern = "AccessKeyId = [\w-]*, SecretAccessKey = [\w/+]*, Status = [\w]*$"
    result = AuthTest(test_msg).create_access_key(**access_key_args).execute_test()
    result.command_should_match_pattern(accesskey_response_pattern)
    accesskey_response_elements = get_response_elements(result.status.stdout)
    access_key_args['AccessKeyId'] = accesskey_response_elements['AccessKeyId']
    access_key_args['SecretAccessKey'] = accesskey_response_elements['SecretAccessKey']
    test_msg = 'CreateAccountLoginProfile should fail when tried with IAM User accessKey-secretKey'
    user_name_flag = "-n"
    password_flag = "--password"
    access_key_args['AccountName'] ="s3test_loginprofile0"
    access_key_args['Password'] = "newPassword"
    result = AuthTest(test_msg).create_account_login_profile(user_name_flag , password_flag,\
               **access_key_args).execute_test(negative_case=True)
    result.command_response_should_have("User is not authorized to perform invoked action.")
    test_msg = 'Delete access key'
    result = AuthTest(test_msg).delete_access_key(**access_key_args).execute_test()
    result.command_response_should_have("Access key deleted.")
    test_msg = 'Delete User accountLoginProfileTestUser'
    user_args = {}
    user_args['UserName'] = "accountLoginProfileTestUser"
    result = AuthTest(test_msg).delete_user(**user_args).execute_test()
    result.command_response_should_have("User deleted.")

    test_msg = "Create account s3test_loginprofile1"
    account_args = {'AccountName': 's3test_loginprofile1', 'Email': 's3test_loginprofile1@seagate.com', \
                   'ldapuser': S3ClientConfig.ldapuser, \
                   'ldappasswd': S3ClientConfig.ldappasswd}
    account_response_pattern = "AccountId = [\w-]*, CanonicalId = [\w-]*, RootUserName = [\w+=,.@-]*, AccessKeyId = [\w-]*, SecretKey = [\w/+]*$"
    result1 = AuthTest(test_msg).create_account(**account_args).execute_test()
    result1.command_should_match_pattern(account_response_pattern)
    account_response_elements1 = get_response_elements(result1.status.stdout)
    access_key_args = {}
    access_key_args['AccountName'] = "s3test_loginprofile1"
    access_key_args['AccessKeyId'] = account_response_elements1['AccessKeyId']
    access_key_args['SecretAccessKey'] = account_response_elements1['SecretKey']
    test_msg = "Create account s3test_loginprofile2"
    account_args = {'AccountName': 's3test_loginprofile2', 'Email': 's3test_loginprofile2@seagate.com', \
                   'ldapuser': S3ClientConfig.ldapuser, \
                   'ldappasswd': S3ClientConfig.ldappasswd}
    account_response_pattern = "AccountId = [\w-]*, CanonicalId = [\w-]*, RootUserName = [\w+=,.@-]*, AccessKeyId = [\w-]*, SecretKey = [\w/+]*$"
    result2 = AuthTest(test_msg).create_account(**account_args).execute_test()
    result2.command_should_match_pattern(account_response_pattern)
    account_response_elements2 = get_response_elements(result2.status.stdout)
    test_msg = "Attempt: create account-login-profile for account name - s3test_loginprofile1 and access key of account s3test_loginprofile2 - Should fail."
    access_key_args2 = {}
    access_key_args2['AccountName'] = "s3test_loginprofile1"
    access_key_args2['AccessKeyId'] = account_response_elements2['AccessKeyId']
    access_key_args2['SecretAccessKey'] = account_response_elements2['SecretKey']
    access_key_args2['Password'] = "newPassword"
    result = AuthTest(test_msg).create_account_login_profile(user_name_flag , password_flag,\
                           **access_key_args2).execute_test(negative_case=True)
    result.command_response_should_have("User is not authorized to perform invoked action")
    account_args = {}
    test_msg = "Delete account s3test_loginprofile1"
    account_args = {'AccountName': 's3test_loginprofile1', 'Email': 's3test_loginprofile1@seagate.com',  'force': True}
    s3test_access_key = S3ClientConfig.access_key_id
    s3test_secret_key = S3ClientConfig.secret_key
    S3ClientConfig.access_key_id = access_key_args['AccessKeyId']
    S3ClientConfig.secret_key = access_key_args['SecretAccessKey']
    AuthTest(test_msg).delete_account(**account_args).execute_test()\
            .command_response_should_have("Account deleted successfully")
    account_args = {}
    test_msg = "Delete account s3test_loginprofile2"
    account_args = {'AccountName': 's3test_loginprofile2', 'Email': 's3test_loginprofile2@seagate.com',  'force': True}
    S3ClientConfig.access_key_id = access_key_args2['AccessKeyId']
    S3ClientConfig.secret_key = access_key_args2['SecretAccessKey']
    AuthTest(test_msg).delete_account(**account_args).execute_test()\
            .command_response_should_have("Account deleted successfully")
    S3ClientConfig.access_key_id = s3test_access_key
    S3ClientConfig.secret_key = s3test_secret_key

    test_msg = 'GetAccountLoginProfile Successfull'
    account_args = {}
    account_name_flag = "-n"
    account_args['AccountName'] ="s3test"
    account_profile_response_pattern =  "Account Login Profile: CreateDate = [\s\S]*, PasswordResetRequired = (true|false), AccountName = [\s\S]*"
    result = AuthTest(test_msg).get_account_login_profile(account_name_flag , **account_args).execute_test()
    result.command_should_match_pattern(account_profile_response_pattern)

    test_msg = 'UpdateAccountLoginProfile Successfull'
    account_args = {}
    account_name_flag = "-n"
    password_flag = "--password"
    account_args['AccountName'] ="s3test"
    account_args['Password'] ="s3test456"
    account_profile_response_pattern = "Account login profile updated."
    result = AuthTest(test_msg).update_account_login_profile(account_name_flag, password_flag, **account_args).execute_test()
    result.command_should_match_pattern(account_profile_response_pattern)

    test_msg = "Create account s3test_loginprofile_update"
    account_args = {'AccountName': 's3test_loginprofile_update', 'Email': 's3test_loginprofile_update@seagate.com', \
                   'ldapuser': S3ClientConfig.ldapuser, \
                   'ldappasswd': S3ClientConfig.ldappasswd}
    account_response_pattern = "AccountId = [\w-]*, CanonicalId = [\w-]*, RootUserName = [\w+=,.@-]*, AccessKeyId = [\w-]*, SecretKey = [\w/+]*$"
    result1 = AuthTest(test_msg).create_account(**account_args).execute_test()
    result1.command_should_match_pattern(account_response_pattern)
    account_response_elements1 = get_response_elements(result1.status.stdout)
    access_key_args = {}
    access_key_args['AccountName'] = "s3test_loginprofile_update"
    access_key_args['AccessKeyId'] = account_response_elements1['AccessKeyId']
    access_key_args['SecretAccessKey'] = account_response_elements1['SecretKey']
    access_key_args['Password'] = "abcdoy"
    test_msg = "create account-login-profile for account name - s3test_loginprofile_update with PasswordResetRequired - false."
    account_profile_response_pattern = "Account Login Profile: CreateDate = [\s\S]*, PasswordResetRequired = false, AccountName = [\s\S]*"
    account_name_flag = "-n"
    password_flag = "--password"
    result = AuthTest(test_msg).create_account_login_profile(account_name_flag , password_flag,\
                           **access_key_args).execute_test()
    result.command_should_match_pattern(account_profile_response_pattern)
    test_msg = 'UpdateAccountLoginProfile should succeed with PasswordResetRequired set to true'
    account_name_flag = "-n"
    password_flag = "--password"
    access_key_args['PasswordResetRequired'] ="True"
    account_profile_response_pattern = "Account login profile updated."
    result = AuthTest(test_msg).update_account_login_profile(account_name_flag, password_flag, **access_key_args).execute_test()
    result.command_should_match_pattern(account_profile_response_pattern)
    test_msg = 'GetAccountLoginProfile Successfull'
    account_name_flag = "-n"
    account_profile_response_pattern =  "Account Login Profile: CreateDate = [\s\S]*, PasswordResetRequired = true, AccountName = [\s\S]*"
    result = AuthTest(test_msg).get_account_login_profile(account_name_flag , **access_key_args).execute_test()
    result.command_should_match_pattern(account_profile_response_pattern)

    test_msg = "Create User updateAccountLoginProfileTestUser"
    user_args = {'UserName': 'updateAccountLoginProfileTestUser'}
    user1_response_pattern = "UserId = [\w-]*, ARN = [\S]*, Path = /$"
    result = AuthTest(test_msg).create_user(**user_args).execute_test()
    result.command_should_match_pattern(user1_response_pattern)
    test_msg = 'Create access key (user name is accountLoginProfileTestUser)'
    access_key_args = {}
    access_key_args['AccountName'] = 'updateAccountLoginProfileTestUser'
    accesskey_response_pattern = "AccessKeyId = [\w-]*, SecretAccessKey = [\w/+]*, Status = [\w]*$"
    result = AuthTest(test_msg).create_access_key(**access_key_args).execute_test()
    result.command_should_match_pattern(accesskey_response_pattern)
    accesskey_response_elements = get_response_elements(result.status.stdout)
    access_key_args['AccessKeyId'] = accesskey_response_elements['AccessKeyId']
    access_key_args['SecretAccessKey'] = accesskey_response_elements['SecretAccessKey']
    access_key_args['AccountName'] = 's3test_loginprofile_update'
    test_msg = 'UpdateAccountLoginProfile should fail for unauthorized user'
    access_key_args['Password'] = "abcd"
    account_name_flag = "-n"
    password_flag = "--password"
    result = AuthTest(test_msg).update_account_login_profile(account_name_flag, password_flag, **access_key_args).execute_test(negative_case=True)
    result.command_response_should_have("User is not authorized to perform invoked action.")
    test_msg = 'Delete access key'
    result = AuthTest(test_msg).delete_access_key(**access_key_args).execute_test()
    result.command_response_should_have("Access key deleted.")
    test_msg = 'Delete User updateAccountLoginProfileTestUser'
    user_args = {}
    user_args['UserName'] = "updateAccountLoginProfileTestUser"
    result = AuthTest(test_msg).delete_user(**user_args).execute_test()
    result.command_response_should_have("User deleted.")

    test_msg = "Create User getaccountloginprofiletest"
    user_args = {'UserName': 'getaccountloginprofiletest'}
    user1_response_pattern = "UserId = [\w-]*, ARN = [\S]*, Path = /$"
    result = AuthTest(test_msg).create_user(**user_args).execute_test()
    result.command_should_match_pattern(user1_response_pattern)
    test_msg = 'Create access key'
    account_args = {}
    account_args['UserName'] = 'getaccountloginprofiletest'
    accesskey_response_pattern = "AccessKeyId = [\w-]*, SecretAccessKey = [\w/+]*, Status = [\w]*$"
    result = AuthTest(test_msg).create_access_key(**account_args).execute_test()
    result.command_should_match_pattern(accesskey_response_pattern)
    accesskey_response_elements = get_response_elements(result.status.stdout)
    account_args['AccessKeyId'] = accesskey_response_elements['AccessKeyId']
    account_args['SecretAccessKey'] = accesskey_response_elements['SecretAccessKey']
    test_msg = 'GetAccountLoginProfile should fail when tried with IAM User accessKey-secretKey'
    account_name_flag = "-n"
    account_args['AccountName'] ="s3test"
    result = AuthTest(test_msg).get_account_login_profile(account_name_flag , **account_args).execute_test(negative_case=True)
    result.command_response_should_have("User is not authorized to perform invoked action.")
    test_msg = 'Delete access key'
    result = AuthTest(test_msg).delete_access_key(**account_args).execute_test()
    result.command_response_should_have("Access key deleted.")
    test_msg = 'Delete User getaccountloginprofiletest'
    user_args = {}
    user_args['UserName'] = "getaccountloginprofiletest"
    result = AuthTest(test_msg).delete_user(**user_args).execute_test()
    result.command_response_should_have("User deleted.")

    test_msg = 'List Users (path prefix = /test/)'
    user_args = {'PathPrefix': '/test/'}
    list_user_pattern = "UserId = [\w-]*, UserName = s3user1New, ARN = [\S]*, Path = /test/success/$"
    result = AuthTest(test_msg).list_users(**user_args).execute_test()
    result.command_should_match_pattern(list_user_pattern)


    test_msg = "List Users - Take access key and secret key from config"

    new_config_entries = {'SG_ACCESS_KEY' : S3ClientConfig.access_key_id, 'SG_SECRET_KEY': S3ClientConfig.secret_key}
    update_config_yaml(new_config_entries)
    list_user_pattern = "UserId = [\w-]*, UserName = s3user1New, ARN = [\S]*, Path = /test/success/$"
    result = AuthTest(test_msg).list_users(**user_args).execute_test()
    result.command_should_match_pattern(list_user_pattern)

    restore_config_yaml()


    test_msg = "List users - Take access key and secret key from env"
    _use_root_credentials()

    # Declare SG_LDAP_USER and SG_LDAP_PASSWD environment variables
    os.environ['SG_ACCESS_KEY'] = S3ClientConfig.access_key_id
    os.environ['SG_SECRET_KEY'] = S3ClientConfig.secret_key

    user_args = {'PathPrefix': '/test/'}
    list_user_pattern = "UserId = [\w-]*, UserName = s3user1New, ARN = [\S]*, Path = /test/success/$"
    result = AuthTest(test_msg).list_users(**user_args).execute_test()
    result.command_should_match_pattern(list_user_pattern)

    # Remove environment variables declared above
    os.environ.pop('SG_ACCESS_KEY')
    os.environ.pop('SG_SECRET_KEY')


    #TODO - Need to fix this test. Currently skipping this test as it waits for password to be entered manually through prompt.
    '''
    test_msg = "List users - Take access key and secret key from prompt"

    user_args = {'PathPrefix': '/test/'}
    list_user_pattern = "Enter Access Key: Enter Secret Key: UserId = [\w-]*, UserName = s3user1New, ARN = [\S]*, Path = /test/success/$"

    stdin_values = S3ClientConfig.access_key_id + '\n' + S3ClientConfig.secret_key
    S3ClientConfig.access_key_id = None
    S3ClientConfig.secret_key =  None
    result = AuthTest(test_msg).list_users(**user_args).execute_test(False, False, stdin_values)
    result.command_should_match_pattern(list_user_pattern)
    '''

    _use_root_credentials()
    test_msg = 'Reset s3user1 user attributes (path and name)'
    user_args = {}
    user_args['UserName'] = "s3user1New"
    user_args['NewUserName'] = "s3user1"
    user_args['NewPath'] = "/"
    result = AuthTest(test_msg).update_user(**user_args).execute_test()
    result.command_response_should_have("User Updated.")

    test_msg = 'Delete User s3user1'
    user_args = {}
    user_args['UserName'] = "s3user1"
    result = AuthTest(test_msg).delete_user(**user_args).execute_test()
    result.command_response_should_have("User deleted.")

    test_msg = "Create User s3user2 (path = /test/)"
    user_args['UserName'] = "s3user2"
    user_args['Path'] = "/test/"
    user2_response_pattern = "UserId = [\w-]*, ARN = [\S]*, Path = /test/$"
    result = AuthTest(test_msg).create_user(**user_args).execute_test()
    result.command_should_match_pattern(user2_response_pattern)

    test_msg = 'Delete User s3user2'
    user_args['UserName'] = "s3user2"
    result = AuthTest(test_msg).delete_user(**user_args).execute_test()
    result.command_response_should_have("User deleted.")

    test_msg = 'Update User root (new name = s3root) should fail'
    user_args = {}
    user_args['UserName'] = "root"
    user_args['NewUserName'] = "s3root"
    result = AuthTest(test_msg).update_user(**user_args).execute_test(negative_case=True)
    result.command_response_should_have("Cannot change user name of root user")

    test_msg = 'Update User root (new path - /test/success)'
    user_args = {}
    user_args['UserName'] = "root"
    user_args['NewPath'] = "/test/success/"
    result = AuthTest(test_msg).update_user(**user_args).execute_test()
    result.command_response_should_have("User Updated.")

    test_msg = 'List Users (default path)'
    user_args = {}
    result = AuthTest(test_msg).list_users(**user_args).execute_test()
    result == ""

    test_msg = 'Reset root user attributes (path and name)'
    user_args = {}
    user_args['UserName'] = "root"
    user_args['NewPath'] = "/"
    result = AuthTest(test_msg).update_user(**user_args).execute_test()
    result.command_response_should_have("User Updated.")

# Test create user API
# Each user can have only 2 access keys. Hence test all the APIs in the same function.
def accesskey_tests():
    access_key_args = {}

    test_msg = 'Create access key (user name not provided)'
    accesskey_response_pattern = "AccessKeyId = [\w-]*, SecretAccessKey = [\w/+]*, Status = [\w]*$"
    result = AuthTest(test_msg).create_access_key(**access_key_args).execute_test()
    result.command_should_match_pattern(accesskey_response_pattern)

    accesskey_response_elements = get_response_elements(result.status.stdout)
    access_key_args['AccessKeyId'] = accesskey_response_elements['AccessKeyId']

    test_msg = 'Delete access key'
    result = AuthTest(test_msg).delete_access_key(**access_key_args).execute_test()
    result.command_response_should_have("Access key deleted.")

    test_msg = 'Create access key (user doest not exist.)'
    access_key_args = {}
    access_key_args['UserName'] = 'userDoesNotExist'
    result = AuthTest(test_msg).create_access_key(**access_key_args).execute_test()
    result.command_response_should_have("Failed to create access key.")

    test_msg = 'Create access key (user name is root)'
    access_key_args['UserName'] = 'root'
    result = AuthTest(test_msg).create_access_key(**access_key_args).execute_test()
    result.command_should_match_pattern(accesskey_response_pattern)

    accesskey_response_elements = get_response_elements(result.status.stdout)
    access_key_args['AccessKeyId'] = accesskey_response_elements['AccessKeyId']

    test_msg = 'Create access key (Allow only 2 credentials per user.)'
    access_key_args['UserName'] = 'root'
    result = AuthTest(test_msg).create_access_key(**access_key_args).execute_test(negative_case=True)
    result.command_response_should_have("Failed to create access key.")

    test_msg = 'Delete access key (user name and access key id combination is incorrect)'
    access_key_args['UserName'] = 'root3'
    result = AuthTest(test_msg).delete_access_key(**access_key_args).execute_test(negative_case=True)
    result.command_response_should_have("Failed to delete access key.")

    test_msg = 'Update access key for root user should fail(Change status from Active to Inactive)'
    access_key_args['Status'] = "Inactive"
    access_key_args['UserName'] = 'root'
    result = AuthTest(test_msg).update_access_key(**access_key_args).execute_test(negative_case=True)
    result.command_response_should_have("Access key status for root user can not be changed")

    test_msg = 'Delete access key'
    access_key_args['UserName'] = 'root'
    result = AuthTest(test_msg).delete_access_key(**access_key_args).execute_test()
    result.command_response_should_have("Access key deleted.")

    # List the acess keys to check for status change
    test_msg = 'List access keys'
    access_key_args['UserName'] = 'root'
    accesskey_response_pattern = "UserName = root, AccessKeyId = [\w-]*, Status = Active$"
    result = AuthTest(test_msg).list_access_keys(**access_key_args).execute_test()
    result.command_should_match_pattern(accesskey_response_pattern)

    user_args = {}
    user_args['UserName'] = "s3user1"
    test_msg = "Create User s3user1 (default path)"
    user1_response_pattern = "UserId = [\w-]*, ARN = [\S]*, Path = /$"
    result = AuthTest(test_msg).create_user(**user_args).execute_test()
    result.command_should_match_pattern(user1_response_pattern)

    test_msg = 'Create access key (user name is s3user1)'
    access_key_args = {}
    access_key_args['UserName'] = 's3user1'
    accesskey_response_pattern = "AccessKeyId = [\w-]*, SecretAccessKey = [\w/+]*, Status = [\w]*$"
    result = AuthTest(test_msg).create_access_key(**access_key_args).execute_test()
    result.command_should_match_pattern(accesskey_response_pattern)
    accesskey_response_elements = get_response_elements(result.status.stdout)
    access_key_args['AccessKeyId'] = accesskey_response_elements['AccessKeyId']

    test_msg = 'Update access key (Change status from Active to Inactive)'
    access_key_args['Status'] = "Inactive"
    access_key_args['UserName'] = 's3user1'
    result = AuthTest(test_msg).update_access_key(**access_key_args).execute_test()
    result.command_response_should_have("Access key Updated.")

    test_msg = 'List access keys (Check if status is inactive.)'
    access_key_args['UserName'] = 's3user1'
    result = AuthTest(test_msg).list_access_keys(**access_key_args).execute_test()
    result.command_response_should_have("Inactive")

    test_msg = 'Delete access key'
    access_key_args['UserName'] = 's3user1'
    result = AuthTest(test_msg).delete_access_key(**access_key_args).execute_test()
    result.command_response_should_have("Access key deleted.")

    test_msg = 'Delete User s3user1'
    user_args = {}
    user_args['UserName'] = "s3user1"
    result = AuthTest(test_msg).delete_user(**user_args).execute_test()
    result.command_response_should_have("User deleted.")


    # Check if non root users are not allowed to use own access key
    # and secret key on other users for creating and deleting access keys

    '''
    Setup for tests:
    '''
    _use_root_credentials()
    user_args = {}
    test_msg = "Create User s3user_1 using root access key and secret key " \
               + "(path = /test/)"
    user_args['UserName'] = "s3user_1"
    user_args['Path'] = "/test/"
    user2_response_pattern = "UserId = [\w-]*, ARN = [\S]*, Path = /test/$"
    result = AuthTest(test_msg).create_user(**user_args).execute_test()
    result.command_should_match_pattern(user2_response_pattern)


    test_msg = "Create access key using root access key and secret key " \
               + "(user name is s3user_1)"
    access_key_args['UserName'] = 's3user_1'
    result = AuthTest(test_msg).create_access_key(**access_key_args).execute_test()
    result.command_should_match_pattern(accesskey_response_pattern)

    response_elements = get_response_elements(result.status.stdout)
    # Saving access key and secret key for s3user_1 for later use.
    access_key_id_of_s3user1 = response_elements['AccessKeyId']
    secret_key_of_s3user1 = response_elements['SecretAccessKey']

    # Overwriting values of access key and secret key given by
    # _use_root_credentials() with s3user_1's access key and secret key.
    S3ClientConfig.access_key_id = access_key_id_of_s3user1
    S3ClientConfig.secret_key = secret_key_of_s3user1


    '''
    runTest:
    '''
    test_msg = "Create User s3user_2 using s3user_1's access key and secret key " \
               + "(path = /test/)"
    user_args['UserName'] = "s3user_2"
    user_args['Path'] = "/test/"
    user2_response_pattern = "UserId = [\w-]*, ARN = [\S]*, Path = /test/$"
    result = AuthTest(test_msg).create_user(**user_args).execute_test(negative_case=True)
    result.command_response_should_have("User is not authorized to perform invoked action")

    '''
    Setup for tests:
    '''
    _use_root_credentials()
    test_msg = "Create User s3user_2 using root's access key and secret key " \
               + "(path = /test/)"
    user_args['UserName'] = "s3user_2"
    user_args['Path'] = "/test/"
    user2_response_pattern = "UserId = [\w-]*, ARN = [\S]*, Path = /test/$"
    result = AuthTest(test_msg).create_user(**user_args).execute_test()
    result.command_should_match_pattern(user2_response_pattern)

    '''
    runTest:
    '''

    test_msg = "Create access key using s3user_1's access key and secret key " \
               +  "(user name is s3user_2)"
    S3ClientConfig.access_key_id = access_key_id_of_s3user1
    S3ClientConfig.secret_key = secret_key_of_s3user1
    access_key_args['UserName'] = 's3user_2'
    result = AuthTest(test_msg).create_access_key(**access_key_args).execute_test(negative_case=True)
    result.command_response_should_have("User is not authorized to perform invoked action")


    '''
    Setup for tests:
    '''
    _use_root_credentials()
    test_msg = "Create access key using root access key and secret key " \
               + "(user name is s3user_2)"
    access_key_args['UserName'] = 's3user_2'
    result = AuthTest(test_msg).create_access_key(**access_key_args).execute_test()
    result.command_should_match_pattern(accesskey_response_pattern)
    response_elements = get_response_elements(result.status.stdout)

    # Saving access key and secret key for s3user_1 for later use.
    access_key_id_of_s3user2 = response_elements['AccessKeyId']
    secret_key_of_s3user2 = response_elements['SecretAccessKey']

    # Overwriting values of access key and secret key given by
    # _use_root_credentials() with s3user_2's access key and secret key.
    S3ClientConfig.access_key_id = access_key_id_of_s3user2
    S3ClientConfig.secret_key = secret_key_of_s3user2

    '''
    runTest:
    '''
    test_msg = 'Delete access key of s3user_1 using s3user_2\'s access key' \
               + ' and secret key'
    access_key_args['UserName'] = 's3user_1'
    result = AuthTest(test_msg).delete_access_key(**access_key_args).execute_test(negative_case=True)
    result.command_response_should_have("User is not authorized to perform invoked action")

    '''
    Setup for tests:
    '''
    _use_root_credentials()
    test_msg = 'Delete access key of s3user_1 using root credentials'
    access_key_args['UserName'] = 's3user_1'
    access_key_args['AccessKeyId']  = access_key_id_of_s3user1
    result = AuthTest(test_msg).delete_access_key(**access_key_args).execute_test()
    result.command_response_should_have("Access key deleted.")


    S3ClientConfig.access_key_id = access_key_id_of_s3user2
    S3ClientConfig.secret_key = secret_key_of_s3user2

    '''
    runTest:
    '''
    test_msg = "Delete User s3user_1 using s3user_2's access key and secret key"
    user_args['UserName'] = "s3user_1"
    result = AuthTest(test_msg).delete_user(**user_args).execute_test(negative_case=True)
    result.command_response_should_have("User is not authorized to perform invoked action")

    '''
    Teardown:
    '''
    _use_root_credentials()
    test_msg = 'Delete access key of s3user_2 using root access key and secret key'
    access_key_args['UserName'] = 's3user_2'
    access_key_args['AccessKeyId']  = access_key_id_of_s3user2
    result = AuthTest(test_msg).delete_access_key(**access_key_args).execute_test()
    result.command_response_should_have("Access key deleted.")

    test_msg = 'Delete User s3user_1 using root access key and secret key'
    user_args = {}
    user_args['UserName'] = "s3user_1"
    result = AuthTest(test_msg).delete_user(**user_args).execute_test()
    result.command_response_should_have("User deleted.")

    test_msg = 'Delete User s3user_2 using root access key and secret key'
    user_args = {}
    user_args['UserName'] = "s3user_2"
    result = AuthTest(test_msg).delete_user(**user_args).execute_test()
    result.command_response_should_have("User deleted.")


    '''
    Setup for tests for scenario when one account's access key
    and secret key are used for creating access key for user in aanother account :
    '''

    test_msg = "Create account s3test_1"
    account_args = {'AccountName': 's3test_1', 'Email': 's3test_1@seagate.com', \
                   'ldapuser': S3ClientConfig.ldapuser, \
                   'ldappasswd': S3ClientConfig.ldappasswd}
    account_response_pattern = "AccountId = [\w-]*, CanonicalId = [\w-]*, RootUserName = [\w+=,.@-]*, AccessKeyId = [\w-]*, SecretKey = [\w/+]*$"
    result = AuthTest(test_msg).create_account(**account_args).execute_test()
    result.command_should_match_pattern(account_response_pattern)
    account_response_elements = get_response_elements(result.status.stdout)

    # Overwriting values of access key and secret key given by
    # _use_root_credentials() with new account 's3test_1's access key and secret key.
    S3ClientConfig.access_key_id = account_response_elements['AccessKeyId']
    S3ClientConfig.secret_key = account_response_elements['SecretKey']

    '''
    runTest:
    '''
    test_msg = "Create access key using another account's access key and secret key " \
               +  "(user name is s3user_2)"
    access_key_args = {}
    access_key_args['UserName'] = 's3user_2'
    result = AuthTest(test_msg).create_access_key(**access_key_args).execute_test(negative_case=True)
    #import pdb; pdb.set_trace()
    result.command_response_should_have("The request was rejected because it " \
        + "referenced a user that does not exist.")

    '''
    Teardown:
    '''
    account_args = {}
    test_msg = "Delete account s3test_1"
    account_args = {'AccountName': 's3test_1'}
    AuthTest(test_msg).delete_account(**account_args).execute_test()\
            .command_response_should_have("Account deleted successfully")

    # restoring previous values for further tests
    _use_root_credentials()

def role_tests():
    policy_doc = os.path.join(os.path.dirname(__file__), 'resources', 'policy')
    policy_doc_full_path = os.path.abspath(policy_doc)

    test_msg = 'Create role (Path not specified)'
    role_args = {}
    role_args['RoleName'] = 'S3Test'
    role_args['AssumeRolePolicyDocument'] = policy_doc_full_path
    role_response_pattern = "RoleId = [\w-]*, RoleName = S3Test, ARN = [\S]*, Path = /$"
    result = AuthTest(test_msg).create_role(**role_args).execute_test()
    result.command_should_match_pattern(role_response_pattern)

    test_msg = 'Delete role'
    result = AuthTest(test_msg).delete_role(**role_args).execute_test()
    result.command_response_should_have("Role deleted.")

    test_msg = 'Create role (Path is /test/)'
    role_args['Path'] = '/test/'
    role_response_pattern = "RoleId = [\w-]*, RoleName = S3Test, ARN = [\S]*, Path = /test/$"
    result = AuthTest(test_msg).create_role(**role_args).execute_test()
    result.command_should_match_pattern(role_response_pattern)

    test_msg = 'List role (Path is not given)'
    role_args = {}
    role_response_pattern = "RoleId = [\w-]*, RoleName = S3Test, ARN = [\S]*, Path = /test/$"
    result = AuthTest(test_msg).list_roles(**role_args).execute_test()
    result.command_should_match_pattern(role_response_pattern)

    test_msg = 'List role (Path is /test)'
    role_response_pattern = "RoleId = S3Test, RoleName = S3Test, ARN = [\S]*, Path = /test/$"
    result = AuthTest(test_msg).list_roles(**role_args).execute_test()
    result.command_should_match_pattern(role_response_pattern)

    test_msg = 'Delete role'
    role_args['RoleName'] = 'S3Test'
    result = AuthTest(test_msg).delete_role(**role_args).execute_test()
    result.command_response_should_have("Role deleted.")

def saml_provider_tests():
    metadata_doc = os.path.join(os.path.dirname(__file__), 'resources', 'saml_metadata')
    metadata_doc_full_path = os.path.abspath(metadata_doc)

    test_msg = 'Create SAML provider'
    saml_provider_args = {}
    saml_provider_args['Name'] = 'S3IDP'
    saml_provider_args['SAMLMetadataDocument'] = metadata_doc_full_path
    saml_provider_response_pattern = "SAMLProviderArn = [\S]*$"
    result = AuthTest(test_msg).create_saml_provider(**saml_provider_args).execute_test()
    result.command_should_match_pattern(saml_provider_response_pattern)

    response_elements = get_response_elements(result.status.stdout)
    saml_provider_args['SAMLProviderArn'] = response_elements['SAMLProviderArn']

    test_msg = 'Update SAML provider'
    saml_provider_args = {}
    saml_provider_args['SAMLProviderArn'] = "arn:seagate:iam:::S3IDP"
    saml_provider_args['SAMLMetadataDocument'] = metadata_doc_full_path
    result = AuthTest(test_msg).update_saml_provider(**saml_provider_args).execute_test()
    result.command_response_should_have("SAML provider Updated.")

    test_msg = 'List SAML providers'
    saml_provider_response_pattern = "ARN = arn:seagate:iam:::S3IDP, ValidUntil = [\S\s]*$"
    result = AuthTest(test_msg).list_saml_providers(**saml_provider_args).execute_test()
    result.command_should_match_pattern(saml_provider_response_pattern)

    test_msg = 'Delete SAML provider'
    result = AuthTest(test_msg).delete_saml_provider(**saml_provider_args).execute_test()
    result.command_response_should_have("SAML provider deleted.")

    test_msg = 'List SAML providers'
    result = AuthTest(test_msg).list_saml_providers(**saml_provider_args).execute_test()
    result.command_should_match_pattern("")

def get_federation_token_test():
    federation_token_args = {}
    federation_token_args['Name'] = 's3root'

    test_msg = 'Get Federation Token'
    response_pattern = "FederatedUserId = [\S]*, AccessKeyId = [\w-]*, SecretAccessKey = [\w/+]*, SessionToken = [\w/+]*$"
    result = AuthTest(test_msg).get_federation_token(**federation_token_args).execute_test()
    result.command_should_match_pattern(response_pattern)

    response_elements = get_response_elements(result.status.stdout)
    S3ClientConfig.access_key_id = response_elements['AccessKeyId']
    S3ClientConfig.secret_key = response_elements['SecretAccessKey']
    S3ClientConfig.token = response_elements['SessionToken']


    _use_root_credentials()

    test_msg = 'Delete access key'
    access_key_args = {}
    access_key_args['AccessKeyId'] = response_elements['AccessKeyId']
    result = AuthTest(test_msg).delete_access_key(**access_key_args).execute_test()
    result.command_response_should_have("Access key deleted.")

    test_msg = 'Delete User s3root'
    user_args = {}
    user_args['UserName'] = "s3root"
    result = AuthTest(test_msg).delete_user(**user_args).execute_test()
    result.command_response_should_have("User deleted.")

def delete_account_tests():
    _use_root_credentials()

    test_msg = "Create User s3user1 (default path)"
    user_args = {'UserName': 's3user1'}
    user1_response_pattern = "UserId = [\w-]*, ARN = [\S]*, Path = /$"
    AuthTest(test_msg).create_user(**user_args).execute_test()\
            .command_should_match_pattern(user1_response_pattern)

    account_args = {'AccountName': 's3test'}
    test_msg = "Delete account s3test should fail"
    AuthTest(test_msg).delete_account(**account_args).execute_test(negative_case=True)\
            .command_response_should_have("attempted to delete a resource that has attached subordinate entities")

    # Test: create a account s3test1 and try to delete account s3test1 using access
    # key and secret key of account s3test. Account delete operation should fail.
    test_msg = "Create account s3test1"

    account_args = {'AccountName': 's3test1', 'Email': 's3test1@seagate.com', 'ldapuser': S3ClientConfig.ldapuser, 'ldappasswd': S3ClientConfig.ldappasswd}
    account_response_pattern = "AccountId = [\w-]*, CanonicalId = [\w-]*, RootUserName = [\w+=,.@-]*, AccessKeyId = [\w-]*, SecretKey = [\w/+]*$"
    result = AuthTest(test_msg).create_account(**account_args).execute_test()
    result.command_should_match_pattern(account_response_pattern)
    account_response_elements = get_response_elements(result.status.stdout)
    s3test1_root_access_key = account_response_elements['AccessKeyId']
    s3test1_root_secret_key = account_response_elements['SecretKey']

    test_msg = "Delete account s3test1 using credentials of account s3test should fail."
    account_args = {'AccountName': 's3test1'}
    AuthTest(test_msg).delete_account(**account_args).execute_test(negative_case=True)\
            .command_response_should_have("You are not authorized to perform this operation.")

    # Test: delete account s3test with force option [recursively/forcefully]
    test_msg = "Delete account s3test"
    account_args = {'AccountName': 's3test', 'force': True}
    AuthTest(test_msg).delete_account(**account_args).execute_test()\
            .command_response_should_have("Account deleted successfully")

    # TODO  This test will be fixed as part of COSTOR-706
    # Use invalid access key and secret key of account s3test1
    GlobalTestState.root_access_key = "xRZ807dxQEqakueNTBpyNQ#"
    GlobalTestState.root_secret_key = "caEE2plJfA1BrhthYsh9H9siEQZtCMF4etvj1o9B"
    _use_root_credentials()

    # Test: delete account with invalid access key and secret key format
    test_msg = "Delete account s3test1 with invalid access key format"
    account_args = {'AccountName': 's3test1'}
    AuthTest(test_msg).delete_account(**account_args).execute_test(negative_case=True) \
       .command_response_should_have("The AWS access key Id you provided does not exist in our records.")

    # Use access key and secret key of account s3test1
    GlobalTestState.root_access_key = s3test1_root_access_key
    GlobalTestState.root_secret_key = s3test1_root_secret_key
    _use_root_credentials()

    # Test: delete account without force option
    test_msg = "Delete account s3test1"
    account_args = {'AccountName': 's3test1'}
    AuthTest(test_msg).delete_account(**account_args).execute_test()\
            .command_response_should_have("Account deleted successfully")

    # Test: Account cannot be deleted if it contains some buckets
    test_msg = "Create account s3test1"

    account_args = {'AccountName': 's3test1', 'Email': 's3test1@seagate.com', 'ldapuser': S3ClientConfig.ldapuser, 'ldappasswd': S3ClientConfig.ldappasswd}
    account_response_pattern = "AccountId = [\w-]*, CanonicalId = [\w-]*, RootUserName = [\w+=,.@-]*, AccessKeyId = [\w-]*, SecretKey = [\w/+]*$"
    result = AuthTest(test_msg).create_account(**account_args).execute_test()
    result.command_should_match_pattern(account_response_pattern)
    account_response_elements = get_response_elements(result.status.stdout)

    GlobalTestState.root_access_key = account_response_elements['AccessKeyId']
    GlobalTestState.root_secret_key = account_response_elements['SecretKey']
    _use_root_credentials()

    S3ClientConfig.pathstyle = False

    S3cmdTest('s3cmd can create bucket').with_credentials(GlobalTestState.root_access_key, GlobalTestState.root_secret_key)\
    .create_bucket("seagatebucket").execute_test().command_is_successful()

    test_msg = "Delete account s3test1 containing buckets"
    account_args = {'AccountName': 's3test1'}
    AuthTest(test_msg).delete_account(**account_args).execute_test(negative_case=True)\
            .command_response_should_have("Account cannot be deleted as it owns some resources.")

    S3cmdTest('s3cmd can delete bucket').with_credentials(GlobalTestState.root_access_key, GlobalTestState.root_secret_key)\
    .delete_bucket("seagatebucket").execute_test().command_is_successful()

    # Test: Account cannot be deleted on clovis_idx_op fail
    test_msg = "Cannot delete account s3test1 on clovis_idx_op fail"
    S3fiTest('s3cmd can enable FI clovis_idx_op_fail').\
        enable_fi("enable", "always", "clovis_idx_op_fail").\
        execute_test().command_is_successful()
    account_args = {'AccountName': 's3test1'}
    AuthTest(test_msg).delete_account(**account_args).execute_test(negative_case=True)\
            .command_response_should_have("Account cannot be deleted")
    S3fiTest('s3cmd disable Fault injection').\
        disable_fi("clovis_idx_op_fail").\
        execute_test().command_is_successful()

    test_msg = "Delete account s3test1 contains no buckets"
    account_args = {'AccountName': 's3test1'}
    AuthTest(test_msg).delete_account(**account_args).execute_test()\
            .command_response_should_have("Account deleted successfully")

def reset_account_accesskey_tests():

    test_msg = "Create account s3test1"

    account_args = {'AccountName': 's3test1', 'Email': 's3test1@seagate.com', 'ldapuser': S3ClientConfig.ldapuser, 'ldappasswd': S3ClientConfig.ldappasswd}
    account_response_pattern = "AccountId = [\w-]*, CanonicalId = [\w-]*, RootUserName = [\w+=,.@-]*, AccessKeyId = [\w-]*, SecretKey = [\w/+]*$"
    result = AuthTest(test_msg).create_account(**account_args).execute_test()
    result.command_should_match_pattern(account_response_pattern)
    account_response_elements = get_response_elements(result.status.stdout)
    s3test1_root_access_key = account_response_elements['AccessKeyId']
    s3test1_root_secret_key = account_response_elements['SecretKey']

    # Use access key and secret key of account s3test1
    GlobalTestState.root_access_key = s3test1_root_access_key
    GlobalTestState.root_secret_key = s3test1_root_secret_key
    _use_root_credentials()

    S3ClientConfig.pathstyle = False
    # Create a bucket with just now created Account credentials
    S3cmdTest('s3cmd can create bucket').with_credentials(GlobalTestState.root_access_key, GlobalTestState.root_secret_key)\
    .create_bucket("seagatebucket").execute_test().command_is_successful()

    test_msg = "Reset account access key"

    account_args = {'AccountName': 's3test1', 'ldapuser': S3ClientConfig.ldapuser, 'ldappasswd': S3ClientConfig.ldappasswd}
    account_response_pattern = "AccountId = [\w-]*, CanonicalId = [\w-]*, RootUserName = [\w+=,.@-]*, AccessKeyId = [\w-]*, SecretKey = [\w/+]*$"
    result = AuthTest(test_msg).reset_account_accesskey(**account_args).execute_test()
    result.command_should_match_pattern(account_response_pattern)
    account_response_elements = get_response_elements(result.status.stdout)
    s3test1_root_access_key = account_response_elements['AccessKeyId']
    s3test1_root_secret_key = account_response_elements['SecretKey']

    test_msg = "Reset account access key with invalid credentials"

    account_args = {'AccountName': 's3test1', 'ldapuser': 'sgiamadmin*',
                    'ldappasswd': 'ldapadmin@'}
    result = AuthTest(test_msg).reset_account_accesskey(**account_args).execute_test(negative_case=True)
    result.command_should_match_pattern("Account access key wasn't reset.")

    #Using old access key should fail now
    S3cmdTest('s3cmd can delete bucket').with_credentials(GlobalTestState.root_access_key, GlobalTestState.root_secret_key)\
    .delete_bucket("seagatebucket").execute_test(negative_case=True).command_should_fail().command_error_should_have("")

    # Use new access key and secret key of account s3test1
    GlobalTestState.root_access_key = s3test1_root_access_key
    GlobalTestState.root_secret_key = s3test1_root_secret_key
    _use_root_credentials()

    # Using new access key should pass now
    S3cmdTest('s3cmd can delete bucket').with_credentials(GlobalTestState.root_access_key,
                                                          GlobalTestState.root_secret_key) \
    .delete_bucket("seagatebucket").execute_test().command_is_successful()

    test_msg = "Delete account s3test1"
    account_args = {'AccountName': 's3test1'}
    AuthTest(test_msg).delete_account(**account_args).execute_test()\
            .command_response_should_have("Account deleted successfully")

def auth_health_check_tests():
    # e.g curl -s -I -X HEAD https://iam.seagate.com:9443/auth/health
    health_check_uri = "/auth/health"
    result = AuthTest('Auth server health check').get_auth_health(health_check_uri).\
    execute_test().command_is_successful().command_response_should_have("200 OK")

def execute_all_system_tests():
    if Config.no_ssl :
        print('Executing auth system tests over HTTP connection')
    else:
        print('Executing auth system tests over HTTPS connection')

    # Do not change the order.
    before_all()
    account_tests()
    user_tests()
    accesskey_tests()
    role_tests()
    saml_provider_tests()
    get_federation_token_test()
    delete_account_tests()
    reset_account_accesskey_tests()
    auth_health_check_tests()

if __name__ == '__main__':

    execute_all_system_tests()
