import argparse
import re
import yaml
import sys
import os
import imp
import botocore
import logging
import shutil
import getpass

from boto3.session import Session
from s3iamcli.config import Credentials
from s3iamcli.config import Config
from s3iamcli.account import Account
from s3iamcli.cli_response import CLIResponse

class S3IamCli:
    def iam_usage(self):
        return '''
        CreateAccount -n <Account Name> -e <Email Id>
        CreateUserLoginProfile -n <User Name>
            --password <User Password>
            [--password-reset-required | --no-password-reset-required]
        CreateAccountLoginProfile -n <Account Name>
            --password <Account Password>
            [--password-reset-required | --no-password-reset-required]
        UpdateAccountLoginProfile -n <Account Name>
            [--password <Account Password>]
            [--password-reset-required | --no-password-reset-required]
        GetUserLoginProfile -n <User Name>
        GetAccountLoginProfile -n <Account Name>
        UpdateUserLoginProfile -n <User Name>
            [--password <User Password>]
            [--password-reset-required | --no-password-reset-required]
        ResetAccountAccessKey -n <Account Name>
        ListAccounts
        CreateAccessKey
            -n <User Name>
        CreateUser -n <User Name>
            [-p <Path>]
        DeleteAccount -n <Account Name>
            [--force]
        DeleteAccesskey -k <Access Key Id>
            -n <User Name>
        DeleteUser -n <User Name>
        ListAccessKeys
            -n <User Name>
        ListUsers
            [-p <Path Prefix>]
        UpdateUser -n <Old User Name>
            --new_user <New User Name> [-p <New Path>]
        ChangePassword --old_password <Old User Password>
            --new_password <New User Password>
        UpdateAccessKey -k <Access Key Id> -s <Active/Inactive>
            -n <User Name>
        GetTempAuthCredentials -a <Account Name>
            --password <Account Password>
            [-d <Duration in seconds>]
            [-n <User Name>]
        '''
    def iam_usage_hidden(self):
        return '''

        -----------------------------------
        *** hidden_help options ****
        -----------------------------------

        AssumeRoleWithSAML --saml_principal_arn <SAML IDP ARN> --saml_role_arn <Role ARN>
        --saml_assertion <File containing SAML Assertion>
            -f <Policy Document> -d <Duration in seconds>
        CreateGroup -n <Group Name>
            -p <Path>
        CreatePolicy -n <Policy Name> -f <Path of the policy document>
            -p <Path> --description <Description of the policy>
        CreateRole -n <Role Name> -f <Path of role policy document>
            -p <Path>
        CreateSAMLProvider -n <Name of saml provider> -f <Path of metadata file>
        Delete Role -n <Role Name>
        DeleteSamlProvider --arn <Saml Provider ARN>
        GetFederationToken -n <User Name>
            -d <Duration in seconds> -f <Policy Document File>
        ListRoles -p <Path Prefix>
        ListSamlProviders
        UpdateSAMLProvider --arn <SAML Provider ARN> -f <Path of metadata file>
        '''

    def get_conf_dir(self):
        return os.path.join(os.path.dirname(__file__),'config')

    def load_config(self, cli_args):
        conf_home_dir = os.path.join(os.path.expanduser('~'), '.sgs3iamcli')
        conf_file = os.path.join(conf_home_dir,'config.yaml')
        if not os.path.isfile(conf_file):
            try:
               os.stat(conf_home_dir)
            except:
               os.mkdir(conf_home_dir)
            shutil.copy(os.path.join(self.get_conf_dir(),'s3iamclicfg.yaml'), conf_file)

        if not os.access(conf_file, os.R_OK):
            message = "Failed to read " + conf_file + " it doesn't have read access"
            CLIResponse.send_error_out(message)
        with open(conf_file, 'r') as f:
            config = yaml.safe_load(f)

        service = Config.service.upper()
        if not cli_args.no_ssl:
            Config.use_ssl = True
            service = service + '_HTTPS'
            Config.ca_cert_file = config['SSL']['CA_CERT_FILE']
            if('~' == Config.ca_cert_file[0]):
               Config.ca_cert_file = os.path.expanduser('~') + Config.ca_cert_file[1:len(Config.ca_cert_file)]

            Config.check_ssl_hostname = config['SSL']['CHECK_SSL_HOSTNAME']
            Config.verify_ssl_cert = config['SSL']['VERIFY_SSL_CERT']

        if Config.endpoint != None:
            Config.endpoint = config['ENDPOINTS'][service]

        """

        SG_ACCESS_KEY, SG_SECRET_KEY, SG_LDAP_USER, SG_LDAP_PASSWD credentials are taken from one of the
        following sources in descending order of priority.
        1. CLI options
        2. Declared environment variables
        3. '~/.sgs3iamcli/config.yaml' file
        4. Asking user to enter i.e prompt

        """
        # Take credentials from '~/.sgs3iamcli/config.yaml' file
        if (cli_args.access_key == None and cli_args.secret_key == None):
            if 'SG_ACCESS_KEY' in config and config['SG_ACCESS_KEY'] and \
               'SG_SECRET_KEY' in config and config['SG_SECRET_KEY']:
                cli_args.access_key = config['SG_ACCESS_KEY']
                cli_args.secret_key = config['SG_SECRET_KEY']

        if (cli_args.ldapuser == None and cli_args.ldappasswd == None):
            if ('SG_LDAP_USER' in config and config['SG_LDAP_USER']) and \
               ('SG_LDAP_PASSWD' in config and config['SG_LDAP_PASSWD']):
                cli_args.ldapuser = config['SG_LDAP_USER']
                cli_args.ldappasswd = config['SG_LDAP_PASSWD']


        if config['BOTO']['ENABLE_LOGGER']:
            logging.basicConfig(filename=config['BOTO']['LOG_FILE_PATH'],
                level=config['BOTO']['LOG_LEVEL'])

        if 'DEFAULT_REGION' in config and config['DEFAULT_REGION']:
            Config.default_region = config['DEFAULT_REGION']
        else:
            Config.default_region = 'us-west2'

    def load_controller_action(self):
        controller_action_file = os.path.join(self.get_conf_dir(), 'controller_action.yaml')
        with open(controller_action_file, 'r') as f:
            return yaml.safe_load(f)

     # Import module
    def import_module(self, module_name):
        fp, pathname, description = imp.find_module(module_name, [os.path.dirname(__file__)]) #, sys.path)

        try:
            return imp.load_module(module_name, fp, pathname, description)
        finally:
            if fp:
                fp.close()

    # Convert the string to Class Object.
    def str_to_class(self, module, class_name):
        return getattr(module, class_name)

    # Create a new IAM serssion.
    def get_session(self, access_key, secret_key, session_token = None):
        return Session(aws_access_key_id=access_key,
                      aws_secret_access_key=secret_key,
                      aws_session_token=session_token)

    # Create an IAM client.
    def get_client(self, session):
        if Config.use_ssl:
            if Config.verify_ssl_cert:
                return session.client(Config.service, endpoint_url=Config.endpoint, verify=Config.ca_cert_file, region_name=Config.default_region)
            return session.client(Config.service, endpoint_url=Config.endpoint, verify=False, region_name=Config.default_region)
        else:
            return session.client(Config.service, endpoint_url=Config.endpoint, use_ssl=False, region_name=Config.default_region)

    # run method
    def run(self):
        show_hidden_args = '--hidden_help' in sys.argv
        parser = argparse.ArgumentParser(usage = self.iam_usage())
        parser.add_argument("action", help="Action to be performed.")
        parser.add_argument("-n", "--name", help="User Name.")
        parser.add_argument("-a", "--account_name", help="Account Name.")
        parser.add_argument("-e", "--email", help="Email id.")
        parser.add_argument("-p", "--path", help="Path or Path Prefix.")
        parser.add_argument("-f", "--file", help="File Path.")
        parser.add_argument("-d", "--duration", help="Access Key Duration.", type = int)
        parser.add_argument("-k", "--access_key_update", help="Access Key to be updated or deleted.")
        parser.add_argument("-s", "--status", help="Active/Inactive")
        parser.add_argument("--force", help="Delete account forcefully.", action='store_true')
        parser.add_argument("--access_key", help="Access Key Id.")
        parser.add_argument("--secret_key", help="Secret Key.")
        parser.add_argument("--password", help="Password.")
        parser.add_argument("--old_password", help="Old Password.")
        parser.add_argument("--new_password", help="New Password.")
        parser.add_argument("--password-reset-required", help="Password reset required on next login.", action ='store_true')
        parser.add_argument("--no-password-reset-required", help="No password reset required on next login.", action ='store_true')
        parser.add_argument("--ldapuser", help="Ldap User Id.")
        parser.add_argument("--ldappasswd", help="Ldap Password.")
        parser.add_argument("--session_token", help="Session Token.")
        parser.add_argument("--arn", help="ARN.")
        parser.add_argument("--description", help="Description of the entity.")
        parser.add_argument("--saml_principal_arn", help="SAML Principal ARN." if show_hidden_args else argparse.SUPPRESS)
        parser.add_argument("--saml_role_arn", help="SAML Role ARN." if show_hidden_args else argparse.SUPPRESS)
        parser.add_argument("--saml_assertion", help="File containing SAML assertion." if show_hidden_args else argparse.SUPPRESS)
        parser.add_argument("--new_user", help="New user name.")
        parser.add_argument("--no-ssl", help="Use HTTP protocol.", action ='store_true')
        parser.add_argument("--hidden_help",dest = 'hidden_help', action ='store_true', help=argparse.SUPPRESS)
        cli_args = parser.parse_args()

        if cli_args.hidden_help is True and cli_args.action == 'show':
            parser.print_help()
            print(self.iam_usage_hidden())
            sys.exit()

        controller_action = self.load_controller_action()
        """
        Check if the action is valid.
        Note - class name and module name are the same
        """
        try:
            class_name = controller_action[cli_args.action.lower()]['controller']
        except Exception as ex:
            message = "Action not found.\n"
            message += str(ex)
            CLIResponse.send_error_out(message)

         # Get service for the action
        if(not 'service' in controller_action[cli_args.action.lower()].keys()):
            print("Set the service(iam/s3/sts) for the action in the controller_action.yml.")
            sys.exit()
        Config.service = controller_action[cli_args.action.lower()]['service']

        # Load configurations
        self.load_config(cli_args)

        if(cli_args.action.lower() in ["createaccount","listaccounts", "resetaccountaccesskey"] ):

            # Take credentials from declared environment variables
            if ('SG_LDAP_USER' in os.environ) and ('SG_LDAP_PASSWD' in os.environ):
                cli_args.ldapuser = os.environ['SG_LDAP_USER']
                cli_args.ldappasswd = os.environ['SG_LDAP_PASSWD']

            # Take credentials by asking user to enter i.e prompt
            if(cli_args.ldapuser is None) and (cli_args.ldappasswd is None):

                cli_args.ldapuser = input("Enter Ldap User Id: ")
                if not cli_args.ldapuser:
                    message = "Provide Ldap User Id."
                    CLIResponse.send_error_out(message)

                cli_args.ldappasswd = getpass.getpass("Enter Ldap password: ")
                if not cli_args.ldappasswd:
                    message = "Provide Ldap password."
                    CLIResponse.send_error_out(message)

            cli_args.access_key = cli_args.ldapuser
            cli_args.secret_key = cli_args.ldappasswd

        elif(cli_args.action.lower() in ["gettempauthcredentials"]):
            cli_args.access_key = ""
            cli_args.secret_key = ""
        else:

            # Take credentials from declared environment variables
            if ('SG_ACCESS_KEY' in os.environ) and ('SG_SECRET_KEY' in os.environ):
                cli_args.access_key = os.environ['SG_ACCESS_KEY']
                cli_args.secret_key = os.environ['SG_SECRET_KEY']

            # Take credentials by asking user to enter i.e prompt
            if (cli_args.access_key is None) and (cli_args.secret_key is None):

                cli_args.access_key = input("Enter Access Key: ")
                if not cli_args.access_key:
                    message = "Provide access key."
                    CLIResponse.send_error_out(message)

                cli_args.secret_key = getpass.getpass("Enter Secret Key: ")
                if not cli_args.secret_key:
                    message = "Provide secret key."
                    CLIResponse.send_error_out(message)

        Credentials.access_key = cli_args.access_key
        Credentials.secret_key = cli_args.secret_key


        # Create boto3.session object using the access key id and the secret key
        session = self.get_session(cli_args.access_key, cli_args.secret_key, cli_args.session_token)

        # Create boto3.client object.
        client = self.get_client(session)

        # If module is not specified in the controller_action.yaml, then assume
        # class name as the module name.
        if('module' in controller_action[cli_args.action.lower()].keys()):
            module_name = controller_action[cli_args.action.lower()]['module']
        else:
            module_name = class_name.lower()

        try:
            module = self.import_module(module_name)
        except Exception as ex:
            message = "Internal error. Module %s not found\n" % class_name
            message += str(ex)
            CLIResponse.send_error_out(message)

        # Create an object of the controller (user, role etc)
        try:
            controller_obj = self.str_to_class(module, class_name)(client, cli_args)
        except Exception as ex:
            message = "Internal error. Class %s not found\n" % class_name
            message += str(ex)
            CLIResponse.send_error_out(message)

        action = controller_action[cli_args.action.lower()]['action']

        # Call the method of the controller i.e Create, Delete, Update, List or ChangePassword
        try:
            getattr(controller_obj, action)()
        except Exception as ex:
            message = str(ex)
            CLIResponse.send_error_out(message)

