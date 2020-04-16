import logging
from s3iamcli.authserver_response import AuthServerResponse

class GetTempAuthCredentialsResponse(AuthServerResponse):

    # Printing credentials
    def print_credentials(self):
        print("AccessKeyId = %s, SecretAccessKey = %s, ExpiryTime = %s, SessionToken = %s" %
              (self.get_value(self.credentials, 'AccessKeyId'),
               self.get_value(self.credentials, 'SecretAccessKey'),
               self.get_value(self.credentials, 'ExpiryTime'),
               self.get_value(self.credentials, 'SessionToken')
               ))

    # Validator for create account operation
    def validate_response(self):
        super(GetTempAuthCredentialsResponse, self).validate_response()
        self.credentials = None
        try:
            self.credentials = (self.response_dict['GetTempAuthCredentialsResponse']['GetTempAuthCredentialsResult']['AccessKey'])
        except KeyError:
            logging.exception('Failed to obtain credentials from GetTempAuthCredentialsResponse')
            self.is_valid = False

