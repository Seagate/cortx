import http.client, urllib.parse
import sys
import datetime
from s3iamcli.util import sign_request_v2
from s3iamcli.util import sign_request_v4
from s3iamcli.util import get_timestamp
from s3iamcli.conn_manager import ConnMan
from s3iamcli.error_response import ErrorResponse
from s3iamcli.create_account_response import CreateAccountResponse
from s3iamcli.list_account_response import ListAccountResponse
from s3iamcli.reset_key_response import ResetAccountAccessKey
from s3iamcli.config import Config
from s3iamcli.cli_response import CLIResponse

class Account:
    def __init__(self, iam_client, cli_args):
        self.iam_client = iam_client
        self.cli_args = cli_args

    def create(self):
        if self.cli_args.name is None:
            message = "Account name is required."
            CLIResponse.send_error_out(message)

        if self.cli_args.email is None:
            message = "Email Id of the user is required to create an Account"
            CLIResponse.send_error_out(message)
        # Get host value from url https://iam.seagate.com:9443
        url_parse_result  = urllib.parse.urlparse(Config.endpoint)
        epoch_t = datetime.datetime.utcnow();
        body = urllib.parse.urlencode({'Action' : 'CreateAccount',
            'AccountName' : self.cli_args.name, 'Email' : self.cli_args.email})
        headers = {'content-type': 'application/x-www-form-urlencoded',
                'Accept': 'text/plain'}
        headers['Authorization'] = sign_request_v4('POST', '/', body, epoch_t, url_parse_result.netloc,
            Config.service, Config.default_region);
        headers['X-Amz-Date'] = get_timestamp(epoch_t);
        if(headers['Authorization'] is None):
            print("Failed to generate v4 signature")
            sys.exit(1)
        response = ConnMan.send_post_request(body, headers)
        if(response['status'] == 201):
            account = CreateAccountResponse(response)

            if account.is_valid_response():
                account.print_account_info()
            else:
                # unlikely message corruption case in network
                message = "Account was created. For account details try account listing."
                CLIResponse.send_success_out(message)
        elif(response['status'] == 503):
            message = "Account wasn't created.\n" \
                      "An error occurred (503) when calling the CreateAccount operation : " + response['reason']
            CLIResponse.send_error_out(message)
        else:
            message = "Account wasn't created.\n"
            error = ErrorResponse(response)
            message += error.get_error_message()
            CLIResponse.send_error_out(message)

    # list all accounts
    def list(self):
        # Get host value from url https://iam.seagate.com:9443
        url_parse_result  = urllib.parse.urlparse(Config.endpoint)

        epoch_t = datetime.datetime.utcnow();

        body = urllib.parse.urlencode({'Action' : 'ListAccounts'})
        headers = {'content-type': 'application/x-www-form-urlencoded',
                'Accept': 'text/plain'}
        headers['Authorization'] = sign_request_v4('POST', '/', body, epoch_t,
            url_parse_result.netloc, Config.service, Config.default_region);
        headers['X-Amz-Date'] = get_timestamp(epoch_t);

        if(headers['Authorization'] is None):
            print("Failed to generate v4 signature")
            CLIResponse.send_error_out(message)
        response = ConnMan.send_post_request(body, headers)

        if response['status'] == 200:
            accounts = ListAccountResponse(response)

            if accounts.is_valid_response():
                accounts.print_account_listing()
            else:
                # unlikely message corruption case in network
                print("Failed to list accounts.")
                sys.exit(0)
        elif(response['status'] == 503):
            message = "Failed to list accounts!\n" \
                      "An error occurred (503) when calling the ListAccounts operation : " + response['reason']
            CLIResponse.send_error_out(message)
        else:
            message = "Failed to list accounts!\n"
            error = ErrorResponse(response)
            message += error.get_error_message()
            CLIResponse.send_error_out(message)

    # delete account
    def delete(self):
        if self.cli_args.name is None:
            print('Account name is required.')
            CLIResponse.send_error_out(message)
        # Get host value from url https://iam.seagate.com:9443
        url_parse_result  = urllib.parse.urlparse(Config.endpoint)

        epoch_t = datetime.datetime.utcnow();

        account_params = {'Action': 'DeleteAccount', 'AccountName': self.cli_args.name}
        if self.cli_args.force:
            account_params['force'] = True

        epoch_t = datetime.datetime.utcnow()
        body = urllib.parse.urlencode(account_params)

        headers = {'content-type': 'application/x-www-form-urlencoded',
                'Accept': 'text/plain'}
        headers['Authorization'] = sign_request_v4('POST', '/', body, epoch_t,
            url_parse_result.netloc, Config.service, Config.default_region);
        headers['X-Amz-Date'] = get_timestamp(epoch_t);

        if(self.cli_args.session_token is not None):
            headers['X-Amz-Security-Token'] = self.cli_args.session_token;
        if(headers['Authorization'] is None):
            print("Failed to generate v4 signature")
            CLIResponse.send_error_out(message)

        response = ConnMan.send_post_request(body, headers)

        if response['status'] == 200:
            print('Account deleted successfully.')
        elif(response['status'] == 503):
            message = "Account cannot be deleted.\n" \
                      "An error occurred (503) when calling the DeleteAccount operation : " + response['reason']
            CLIResponse.send_error_out(message)
        else:
            message = 'Account cannot be deleted.\n'
            error = ErrorResponse(response)
            message += error.get_error_message()
            CLIResponse.send_error_out(message)

    # Reset Account Access Key
    def reset_access_key(self):
        if self.cli_args.name is None:
            print("Account name is required.")
            return
        # Get host value from url https://iam.seagate.com:9443
        url_parse_result  = urllib.parse.urlparse(Config.endpoint)
        epoch_t = datetime.datetime.utcnow();
        body = urllib.parse.urlencode({'Action' : 'ResetAccountAccessKey',
            'AccountName' : self.cli_args.name, 'Email' : self.cli_args.email})
        headers = {'content-type': 'application/x-www-form-urlencoded',
                'Accept': 'text/plain'}
        headers['Authorization'] = sign_request_v4('POST', '/', body, epoch_t,
            url_parse_result.netloc, Config.service, Config.default_region);
        headers['X-Amz-Date'] = get_timestamp(epoch_t);

        if(headers['Authorization'] is None):
            message = "Failed to generate v4 signature"
            CLIResponse.send_error_out(message)

        response = ConnMan.send_post_request(body, headers)
        if response['status'] == 201:
            account = ResetAccountAccessKey(response)

            if account.is_valid_response():
                account.print_account_info()
            else:
                # unlikely message corruption case in network
                message = "Account access key was reset successfully."
                CLIResponse.send_success_out(message)
        elif(response['status'] == 503):
            message = "Account access key wasn't reset.\n" \
                      "An error occurred (503) when calling the ResetAccountAccessKey operation : " + response['reason']
            CLIResponse.send_error_out(message)
        else:
            message = "Account access key wasn't reset.\n"
            error = ErrorResponse(response)
            message += error.get_error_message()
            CLIResponse.send_error_out(message)

