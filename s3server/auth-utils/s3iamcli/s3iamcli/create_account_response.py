import logging
from s3iamcli.authserver_response import AuthServerResponse

class CreateAccountResponse(AuthServerResponse):

    # Printing account info while creating user.
    def print_account_info(self):
        print("AccountId = %s, CanonicalId = %s, RootUserName = %s, AccessKeyId = %s, SecretKey = %s" %
              (self.get_value(self.account, 'AccountId'),
               self.get_value(self.account, 'CanonicalId'),
               self.get_value(self.account, 'RootUserName'),
               self.get_value(self.account, 'AccessKeyId'),
               self.get_value(self.account, 'RootSecretKeyId')))

    # Validator for create account operation
    def validate_response(self):

        super(CreateAccountResponse, self).validate_response()
        self.account = None
        try:
            self.account = (self.response_dict['CreateAccountResponse']['CreateAccountResult']
                            ['Account'])
        except KeyError:
            logging.exception('Failed to obtain create account key from account response')
            self.is_valid = False
