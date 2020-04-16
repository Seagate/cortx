import logging
from s3iamcli.authserver_response import AuthServerResponse

class ResetAccountAccessKey(AuthServerResponse):

    # Printing account info while resetting account access key.
    def print_account_info(self):
        print("AccountId = %s, CanonicalId = %s, RootUserName = %s, AccessKeyId = %s, SecretKey = %s" %
              (self.get_value(self.account, 'AccountId'),
               self.get_value(self.account, 'CanonicalId'),
               self.get_value(self.account, 'RootUserName'),
               self.get_value(self.account, 'AccessKeyId'),
               self.get_value(self.account, 'RootSecretKeyId')))

    # Validator for reset account access key operation
    def validate_response(self):

        super(ResetAccountAccessKey, self).validate_response()
        self.account = None
        try:
            self.account = (self.response_dict['ResetAccountAccessKeyResponse']
                            ['ResetAccountAccessKeyResult']['Account'])
        except KeyError:
            logging.exception('Failed to obtain reset account access key from account response')
            self.is_valid = False

