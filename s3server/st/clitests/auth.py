import os
from framework import PyCliTest
from framework import Config
from framework import logit
from s3client_config import S3ClientConfig

class AuthTest(PyCliTest):
    def __init__(self, description):
        self.test_data_dir = os.path.join(os.path.dirname(__file__), "test_data")
        super(AuthTest, self).__init__(description)

    def setup(self):
        super(AuthTest, self).setup()

    def run(self, cmd_args = None):
        super(AuthTest, self).run(cmd_args)

    def teardown(self):
        super(AuthTest, self).teardown()

    def with_cli(self, cmd):
        if Config.no_ssl and 's3iamcli' in cmd:
            cmd = cmd + ' --no-ssl'
        # This case is for fault injection curl commands only in SSL mode.
        elif not Config.no_ssl and 's3iamcli' not in cmd:
            cmd = cmd + ' --cacert ' + S3ClientConfig.auth_ca_file

        super(AuthTest, self).with_cli(cmd)

    def create_account(self, **account_args):
        cmd = "s3iamcli createaccount -n %s -e %s --ldapuser %s --ldappasswd %s" % (
                 account_args['AccountName'], account_args['Email'], account_args['ldapuser'], account_args['ldappasswd'])
        self.with_cli(cmd)
        return self

    def list_account(self, **account_args):
        if ('ldapuser' in account_args) and 'ldappasswd' in account_args:
            if (account_args['ldapuser'] != None) and (account_args['ldappasswd'] != None):
                cmd = "s3iamcli listaccounts --ldapuser %s --ldappasswd %s" % (account_args['ldapuser'], account_args['ldappasswd'])
        else:
            cmd = "s3iamcli listaccounts"

        self.with_cli(cmd)
        return self

    def delete_account(self, **account_args):
        cmd = "s3iamcli deleteaccount -n %s --access_key '%s' --secret_key '%s'" % (
                 account_args['AccountName'], S3ClientConfig.access_key_id,
                 S3ClientConfig.secret_key)

        if ('force' in account_args.keys() and account_args['force']):
            cmd += " --force"

        self.with_cli(cmd)
        return self

    def reset_account_accesskey(self, **account_args):
        cmd = "s3iamcli resetaccountaccesskey -n %s --ldapuser %s --ldappasswd %s" % (
                 account_args['AccountName'], account_args['ldapuser'], account_args['ldappasswd'])

        self.with_cli(cmd)
        return self

    def create_user(self, **user_args):
        cmd = "s3iamcli createuser --access_key '%s' --secret_key '%s' -n %s" % (
                 S3ClientConfig.access_key_id,
                 S3ClientConfig.secret_key, user_args['UserName'])

        if(not S3ClientConfig.token is ""):
            cmd += " --session_token '%s'" % S3ClientConfig.token

        if('Path' in user_args.keys()):
            cmd += " -p %s" % user_args['Path']

        self.with_cli(cmd)
        return self

    def update_user(self, **user_args):
        cmd = "s3iamcli updateuser --access_key '%s' --secret_key '%s' -n %s" % (
                 S3ClientConfig.access_key_id,
                 S3ClientConfig.secret_key, user_args['UserName'])

        if(not S3ClientConfig.token is ""):
            cmd += " --session_token '%s'" % S3ClientConfig.token

        if('NewUserName' in user_args.keys()):
            cmd += " --new_user %s" % user_args['NewUserName']

        if('NewPath' in user_args.keys()):
            cmd += " -p %s" % user_args['NewPath']

        self.with_cli(cmd)
        return self

    def create_login_profile(self, usernameflag = None, passwordflag = None, **login_profile_args):
        cmd = "s3iamcli createuserloginprofile --access_key '%s' --secret_key\
               '%s' %s %s %s %s" % (
               S3ClientConfig.access_key_id,
               S3ClientConfig.secret_key, usernameflag, login_profile_args\
               ['UserName'], passwordflag,login_profile_args['Password'])

        if('PasswordResetRequired' in login_profile_args.keys()):
           if(login_profile_args['PasswordResetRequired'] is "True"):
              cmd += " --password-reset-required"
           else:
              cmd += " --no-password-reset-required"
        self.with_cli(cmd)
        return self

    def get_login_profile(self, usernameflag = None, **login_profile_args):
        cmd = "s3iamcli getuserloginprofile --access_key '%s' --secret_key\
                   '%s' %s %s " % (
                 S3ClientConfig.access_key_id,
                 S3ClientConfig.secret_key, usernameflag, login_profile_args\
                     ['UserName'])
        self.with_cli(cmd)
        return self

    def change_user_password(self, **login_profile_args):
        cmd = "s3iamcli changepassword --access_key '%s' --secret_key\
                   '%s' --old_password %s --new_password %s " % (
                 login_profile_args['AccessKeyId'],
                 login_profile_args['SecretAccessKey'], login_profile_args\
                     ['OldPassword'], login_profile_args['NewPassword'])
        self.with_cli(cmd)
        return self

    def create_account_login_profile(self, accountnameflag = None, passwordflag = None, **login_profile_args):
        if 'AccessKeyId' in login_profile_args:
            cmd = "s3iamcli createaccountloginprofile --access_key '%s' --secret_key\
                   '%s' %s %s %s %s" % (
                   login_profile_args['AccessKeyId'],
                   login_profile_args['SecretAccessKey'], accountnameflag, login_profile_args\
                   ['AccountName'], passwordflag,login_profile_args['Password'])
        else:
            cmd = "s3iamcli createaccountloginprofile --access_key '%s' --secret_key\
                   '%s' %s %s %s %s" % (
                   S3ClientConfig.access_key_id,
                   S3ClientConfig.secret_key, accountnameflag, login_profile_args\
                   ['AccountName'], passwordflag,login_profile_args['Password'])

        if('PasswordResetRequired' in login_profile_args.keys()):
           if(login_profile_args['PasswordResetRequired'] is "True"):
              cmd += " --password-reset-required"
           else:
              cmd += " --no-password-reset-required"
        self.with_cli(cmd)
        return self

    def update_account_login_profile(self, accountnameflag = None, passwordflag = None, **login_profile_args):
        if 'AccessKeyId' in login_profile_args:
            cmd = "s3iamcli updateaccountloginprofile --access_key '%s' --secret_key\
                   '%s' %s %s %s %s" % (
                   login_profile_args['AccessKeyId'],
                   login_profile_args['SecretAccessKey'], accountnameflag, login_profile_args\
                   ['AccountName'], passwordflag,login_profile_args['Password'])
        else:
            cmd = "s3iamcli updateaccountloginprofile --access_key '%s' --secret_key\
                   '%s' %s %s %s %s" % (
                   S3ClientConfig.access_key_id,
                   S3ClientConfig.secret_key, accountnameflag, login_profile_args\
                   ['AccountName'], passwordflag,login_profile_args['Password'])

        if('PasswordResetRequired' in login_profile_args.keys()):
           if(login_profile_args['PasswordResetRequired'] is "True"):
              cmd += " --password-reset-required"
           else:
              cmd += " --no-password-reset-required"
        self.with_cli(cmd)
        return self

    def get_account_login_profile(self, accountnameflag = None, **login_profile_args):

        if 'AccessKeyId' in login_profile_args:
            cmd = "s3iamcli getaccountloginprofile --access_key '%s' --secret_key\
                       '%s' %s %s " % (
                     login_profile_args['AccessKeyId'],
                     login_profile_args['SecretAccessKey'], accountnameflag, login_profile_args\
                         ['AccountName'])
        else:
           cmd = "s3iamcli getaccountloginprofile --access_key '%s' --secret_key\
                      '%s' %s %s " % (
                    S3ClientConfig.access_key_id,
                    S3ClientConfig.secret_key, accountnameflag, login_profile_args\
                        ['AccountName'])
        self.with_cli(cmd)
        return self


    def delete_user(self, **user_args):
        cmd = "s3iamcli deleteuser --access_key '%s' --secret_key '%s' -n %s" % (
                 S3ClientConfig.access_key_id,
                 S3ClientConfig.secret_key, user_args['UserName'])

        if(not S3ClientConfig.token is ""):
            cmd += " --session_token '%s'" % S3ClientConfig.token

        self.with_cli(cmd)
        return self

    def list_users(self, **user_args):
        if (S3ClientConfig.access_key_id != None) and \
           (S3ClientConfig.secret_key != None):
            cmd = "s3iamcli listusers --access_key '%s' --secret_key '%s'" % (
                     S3ClientConfig.access_key_id,
                     S3ClientConfig.secret_key)
        else:
            cmd = "s3iamcli listusers"

        if(not S3ClientConfig.token is ""):
            cmd += " --session_token '%s'" % S3ClientConfig.token

        if('PathPrefix' in user_args.keys()):
            cmd += " -p %s" % user_args['PathPrefix']

        self.with_cli(cmd)
        return self

    def create_access_key(self, **access_key_args):
        cmd = "s3iamcli createaccesskey --access_key '%s' --secret_key '%s' " % (
                 S3ClientConfig.access_key_id,
                 S3ClientConfig.secret_key)

        if(not S3ClientConfig.token is ""):
            cmd += " --session_token '%s'" % S3ClientConfig.token

        if('UserName' in access_key_args.keys()):
            cmd += " -n %s" % access_key_args['UserName']

        self.with_cli(cmd)
        return self

    def delete_access_key(self, **access_key_args):
        cmd = "s3iamcli deleteaccesskey --access_key '%s' --secret_key '%s' -k %s" % (
                 S3ClientConfig.access_key_id,
                 S3ClientConfig.secret_key, access_key_args['AccessKeyId'])

        if(not S3ClientConfig.token is ""):
            cmd += " --session_token '%s'" % S3ClientConfig.token

        if('UserName' in access_key_args.keys()):
            cmd += " -n %s" % access_key_args['UserName']

        self.with_cli(cmd)
        return self

    def update_access_key(self, **access_key_args):
        cmd = "s3iamcli updateaccesskey --access_key '%s' --secret_key '%s' -k %s -s %s" % (
                 S3ClientConfig.access_key_id,
                 S3ClientConfig.secret_key, access_key_args['AccessKeyId'],
                access_key_args['Status'])

        if(not S3ClientConfig.token is ""):
            cmd += " --session_token '%s'" % S3ClientConfig.token

        if('UserName' in access_key_args.keys()):
            cmd += " -n %s" % access_key_args['UserName']

        self.with_cli(cmd)
        return self

    def list_access_keys(self, **access_key_args):
        cmd = "s3iamcli listaccesskeys --access_key '%s' --secret_key '%s'" % (
                 S3ClientConfig.access_key_id,
                 S3ClientConfig.secret_key)

        if(not S3ClientConfig.token is ""):
            cmd += " --session_token '%s'" % S3ClientConfig.token

        if('UserName' in access_key_args.keys()):
            cmd += " -n %s" % access_key_args['UserName']

        self.with_cli(cmd)
        return self

    def create_role(self, **role_args):
        cmd = "s3iamcli createrole --access_key '%s' --secret_key '%s' -n %s -f %s" % (
                 S3ClientConfig.access_key_id,
                 S3ClientConfig.secret_key, role_args['RoleName'],
                 role_args['AssumeRolePolicyDocument'])

        if(not S3ClientConfig.token is ""):
            cmd += " --session_token '%s'" % S3ClientConfig.token

        if('Path' in role_args.keys()):
            cmd += " -p %s" % role_args['Path']

        self.with_cli(cmd)
        return self

    def delete_role(self, **role_args):
        cmd = "s3iamcli deleterole --access_key '%s' --secret_key '%s' -n %s" % (
                 S3ClientConfig.access_key_id,
                 S3ClientConfig.secret_key, role_args['RoleName'])

        if(not S3ClientConfig.token is ""):
            cmd += " --session_token '%s'" % S3ClientConfig.token

        self.with_cli(cmd)
        return self

    def list_roles(self, **role_args):
        cmd = "s3iamcli listroles --access_key '%s' --secret_key '%s'" % (
                 S3ClientConfig.access_key_id,
                 S3ClientConfig.secret_key)

        if(not S3ClientConfig.token is ""):
            cmd += " --session_token '%s'" % S3ClientConfig.token

        if('Path' in role_args.keys()):
            cmd += " -p %s" % role_args['Path']

        self.with_cli(cmd)
        return self

    def create_saml_provider(self, **saml_provider_args):
        cmd = "s3iamcli createsamlprovider --access_key '%s' --secret_key '%s' -n %s -f %s" % (
                 S3ClientConfig.access_key_id,
                 S3ClientConfig.secret_key, saml_provider_args['Name'],
                 saml_provider_args['SAMLMetadataDocument'])

        if(not S3ClientConfig.token is ""):
            cmd += " --session_token '%s'" % S3ClientConfig.token

        self.with_cli(cmd)
        return self

    def update_saml_provider(self, **saml_provider_args):
        cmd = "s3iamcli updatesamlprovider --access_key '%s' --secret_key '%s' \
                --arn '%s' -f %s" % (
                 S3ClientConfig.access_key_id,
                 S3ClientConfig.secret_key, saml_provider_args['SAMLProviderArn'],
                 saml_provider_args['SAMLMetadataDocument'])

        if(not S3ClientConfig.token is ""):
            cmd += " --session_token '%s'" % S3ClientConfig.token

        self.with_cli(cmd)
        return self

    def list_saml_providers(self, **saml_provider_args):
        cmd = "s3iamcli listsamlproviders --access_key '%s' --secret_key '%s'" % (
                 S3ClientConfig.access_key_id,
                 S3ClientConfig.secret_key)

        if(not S3ClientConfig.token is ""):
            cmd += " --session_token '%s'" % S3ClientConfig.token

        self.with_cli(cmd)
        return self

    def delete_saml_provider(self, **saml_provider_args):
        cmd = "s3iamcli deletesamlprovider --access_key '%s' --secret_key '%s' \
                --arn '%s'" % (
                 S3ClientConfig.access_key_id,
                 S3ClientConfig.secret_key, saml_provider_args['SAMLProviderArn'])

        if(not S3ClientConfig.token is ""):
            cmd += " --session_token '%s'" % S3ClientConfig.token

        self.with_cli(cmd)
        return self

    def get_federation_token(self, **federation_token_args):
        cmd = "s3iamcli getfederationtoken --access_key '%s' --secret_key '%s' \
                -n '%s'" % (
                 S3ClientConfig.access_key_id,
                 S3ClientConfig.secret_key, federation_token_args['Name'])

        if(not S3ClientConfig.token is ""):
            cmd += " --session_token '%s'" % S3ClientConfig.token

        if('Policy' in federation_token_args.keys()):
            cmd += " -f %s" % federation_token_args['Policy']

        if('DurationSeconds' in federation_token_args.keys()):
            cmd += " --duration %s" % federation_token_args['DurationSeconds']

        self.with_cli(cmd)
        return self

    def inject_fault(self, fault_point, mode, value):
        cmd = "curl -s --data 'Action=InjectFault&FaultPoint=%s&Mode=%s&Value=%s' "\
                % (fault_point, mode, value)
        if Config.no_ssl:
            cmd = cmd + " " + S3ClientConfig.iam_uri_http
        else:
            cmd = cmd + " " + S3ClientConfig.iam_uri_https

        self.with_cli(cmd)
        return self

    def reset_fault(self, fault_point):
        cmd = "curl -s --data 'Action=ResetFault&FaultPoint=%s' "\
                % (fault_point)
        if Config.no_ssl:
            cmd = cmd + " " + S3ClientConfig.iam_uri_http
        else:
            cmd = cmd + " " + S3ClientConfig.iam_uri_https

        self.with_cli(cmd)
        return self

    def update_login_profile(self, usernameflag = None, **login_profile_args):
        cmd = "s3iamcli updateuserloginprofile --access_key '%s' --secret_key\
                   '%s' %s %s" % (
                 S3ClientConfig.access_key_id,
                 S3ClientConfig.secret_key, usernameflag, login_profile_args['UserName'])
        if('Password' in login_profile_args.keys()):
            cmd += " --password %s" % login_profile_args['Password']
        if('PasswordResetRequired' in login_profile_args.keys()):
            cmd += " --password-reset-required"

        self.with_cli(cmd)
        return self

    def update_login_profile_with_user_key(self, usernameflag = None, **login_profile_args):
        cmd = "s3iamcli updateuserloginprofile --access_key '%s' --secret_key\
                   '%s' %s %s" % (
                 login_profile_args['AccessKeyId'],
                 login_profile_args['SecretAccessKey'], usernameflag, login_profile_args['UserName'])
        if('Password' in login_profile_args.keys()):
            cmd += " --password %s" % login_profile_args['Password']
        if('PasswordResetRequired' in login_profile_args.keys()):
            cmd += " --password-reset-required"
        self.with_cli(cmd)
        return self

    def get_temp_auth_credentials(self,accountnameflag = None , passwordflag=None , **login_args):
        cmd = "s3iamcli gettempauthcredentials %s %s %s %s" % (
            accountnameflag,login_args['AccountName'], passwordflag, login_args['Password'])
        if('UserName' in login_args.keys()):
            cmd += " -n %s" % login_args['UserName']
        if('Duration' in login_args.keys()):
            cmd += " -d %s" % login_args['Duration']
        print(cmd)
        self.with_cli(cmd)
        return self

    @staticmethod
    def get_response_elements(response):
        response_elements = {}
        key_pairs = response.split(',')
        for key_pair in key_pairs:
            tokens = key_pair.split('=')
            response_elements[tokens[0].strip()] = tokens[1].strip()
        return response_elements

    def get_auth_health(self, request_uri):
        # e.g curl -s -I -X HEAD https://iam.seagate.com:9443/auth/health
        cmd = "curl -s -I -X HEAD"
        if Config.no_ssl:
            cmd = cmd + " " + S3ClientConfig.iam_uri_http
        else:
            cmd = cmd + " " + S3ClientConfig.iam_uri_https
        cmd = cmd + request_uri
        self.with_cli(cmd)
        return self
