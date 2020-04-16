import http.client, urllib.parse
import sys
import datetime
from s3iamcli.util import sign_request_v2
from s3iamcli.util import sign_request_v4
from s3iamcli.util import get_timestamp
from s3iamcli.conn_manager import ConnMan
from s3iamcli.error_response import ErrorResponse
from s3iamcli.create_accountloginprofile_response import CreateAccountLoginProfileResponse
from s3iamcli.get_accountloginprofile_response import GetAccountLoginProfileResponse
from s3iamcli.list_account_response import ListAccountResponse
from s3iamcli.reset_key_response import ResetAccountAccessKey
from s3iamcli.config import Config
from s3iamcli.cli_response import CLIResponse

class AccountLoginProfile:
    def __init__(self, iam_client, cli_args):
        self.iam_client = iam_client
        self.cli_args = cli_args

    # list all accounts
    def list(self):
        # Get host value from url https://iam.seagate.com:9443
        if(self.cli_args.name is None):
            message = "Account name is required."
            CLIResponse.send_error_out(message)

        url_parse_result  = urllib.parse.urlparse(Config.endpoint)

        epoch_t = datetime.datetime.utcnow();

        body = urllib.parse.urlencode({'Action' : 'GetAccountLoginProfile','AccountName' : self.cli_args.name})
        headers = {'content-type': 'application/x-www-form-urlencoded',
                'Accept': 'text/plain'}
        headers['Authorization'] = sign_request_v4('POST', '/', body, epoch_t,
            url_parse_result.netloc, Config.service, Config.default_region);
        headers['X-Amz-Date'] = get_timestamp(epoch_t);

        if(self.cli_args.session_token is not None):
            headers['X-Amz-Security-Token'] = self.cli_args.session_token;

        if(headers['Authorization'] is None):
            message = "Failed to generate v4 signature"
            CLIResponse.send_error_out(message)
        response = ConnMan.send_post_request(body, headers)
        if response['status'] == 200:
            accounts = GetAccountLoginProfileResponse(response)

            if accounts.is_valid_response():
                accounts.print_account_login_profile_info()
            else:
                # unlikely message corruption case in network
                message = "Failed to list login profile"
                CLIResponse.send_success_out(message)
        elif(response['status'] == 503):
            message = "Failed to get login profile\n" \
                      "An error occurred (503) when calling the GetAccountLoginProfile operation : " + response['reason']
            CLIResponse.send_error_out(message)
        else:
            message = "Failed to get login profile\n"
            error = ErrorResponse(response)
            message += error.get_error_message()
            CLIResponse.send_error_out(message)

    def create(self):
        if(self.cli_args.name is None or self.cli_args.name is ''):
            message = "Account name is required"
            CLIResponse.send_error_out(message)

        if(self.cli_args.password is None):
            message = "Account login password is required"
            CLIResponse.send_error_out(message)

        passwordResetRequired = False;
        if(self.cli_args.password_reset_required):
            passwordResetRequired = True

        # Get host value from url https://iam.seagate.com:9443
        url_parse_result  = urllib.parse.urlparse(Config.endpoint)
        epoch_t = datetime.datetime.utcnow();
        body = urllib.parse.urlencode({'Action' : 'CreateAccountLoginProfile',
            'AccountName' : self.cli_args.name, 'Password' : self.cli_args.password,
            'PasswordResetRequired' : passwordResetRequired})
        headers = {'content-type': 'application/x-www-form-urlencoded',
                'Accept': 'text/plain'}
        headers['Authorization'] = sign_request_v4('POST', '/', body, epoch_t, url_parse_result.netloc,
            Config.service, Config.default_region);
        headers['X-Amz-Date'] = get_timestamp(epoch_t);
        if(headers['Authorization'] is None):
            message = "Failed to generate v4 signature"
            CLIResponse.send_error_out(message)
        result = ConnMan.send_post_request(body, headers)
        # Validate response
        if(result['status'] == 201):
            profile = CreateAccountLoginProfileResponse(result)

            if profile.is_valid_response():
                profile.print_profile_info()
            else:
                # unlikely message corruption case in network
                message = "Account login profile was created. For details try get account login profile."
                CLIResponse.send_success_out(message)
        elif(result['status'] == 503):
            message = "Failed to create Account login profile.\n" \
                      "An error occurred (503) when calling the CreateAccountLoginProfile operation : " + result['reason']
            CLIResponse.send_error_out(message)
        else:
            message = "Failed to create Account login profile.\n"
            error = ErrorResponse(result)
            message += error.get_error_message()
            CLIResponse.send_error_out(message)

    def update(self):
        if(self.cli_args.name is None):
            message = "Account name is required for UpdateAccountLoginProfile"
            CLIResponse.send_error_out(message)

        passwordResetRequired = False
        if(self.cli_args.password_reset_required):
            passwordResetRequired = True
        if(self.cli_args.password is None) and (self.cli_args.password_reset_required is False) and (self.cli_args.no_password_reset_required is False):
            message = "Please provide password or password-reset flag"
            CLIResponse.send_error_out(message)

        # Get host value from url https://iam.seagate.com:9443
        url_parse_result  = urllib.parse.urlparse(Config.endpoint)
        epoch_t = datetime.datetime.utcnow();
        if(self.cli_args.password is None):
            body = urllib.parse.urlencode({'Action' : 'UpdateAccountLoginProfile',
                'AccountName' : self.cli_args.name, 'PasswordResetRequired' : passwordResetRequired})
        else:
            body = urllib.parse.urlencode({'Action' : 'UpdateAccountLoginProfile',
                'AccountName' : self.cli_args.name, 'Password' : self.cli_args.password,
                'PasswordResetRequired' : passwordResetRequired})
        headers = {'content-type': 'application/x-www-form-urlencoded',
                'Accept': 'text/plain'}
        headers['Authorization'] = sign_request_v4('POST', '/', body, epoch_t, url_parse_result.netloc,
            Config.service, Config.default_region);
        headers['X-Amz-Date'] = get_timestamp(epoch_t);
        if(self.cli_args.session_token is not None):
            headers['X-Amz-Security-Token'] = self.cli_args.session_token;

        if(headers['Authorization'] is None):
            message = "Failed to generate v4 signature"
            CLIResponse.send_error_out(message)
        result = ConnMan.send_post_request(body, headers)
        # Validate response
        if(result['status'] == 200):
            message = "Account login profile updated."
            CLIResponse.send_success_out(message)
        elif(result['status'] == 503):
            message = "Failed to update Account login profile.\n" \
                      "An error occurred (503) when calling the UpdateAccountLoginProfile operation : " + result['reason']
            CLIResponse.send_error_out(message)
        else:
            message = "Account login profile wasn't Updated."
            error = ErrorResponse(result)
            message += error.get_error_message()
            CLIResponse.send_error_out(message)


