import logging
from s3iamcli.authserver_response import AuthServerResponse
import datetime
from datetime import timezone

class CreateAccountLoginProfileResponse(AuthServerResponse):

    # Printing account info while creating user.
    def print_profile_info(self):
        datestr = self.get_value(self.profile, 'CreateDate')
        date_time_obj = datetime.datetime.strptime(datestr, '%Y%m%d%H%M%SZ')
        createdate = date_time_obj.replace(tzinfo=timezone.utc).isoformat()
        print("Account Login Profile: CreateDate = %s, PasswordResetRequired = %s, AccountName = %s" %
              (createdate,
               self.get_value(self.profile, 'PasswordResetRequired'),
               self.get_value(self.profile, 'AccountName')))

    # Validator for create account login profile operation
    def validate_response(self):

        super(CreateAccountLoginProfileResponse, self).validate_response()
        self.profile = None
        try:
            self.profile = (self.response_dict['CreateAccountLoginProfileResponse']['CreateAccountLoginProfileResult']
                            ['LoginProfile'])
        except KeyError:
            logging.exception('Failed to obtain ... from account login profile response')

            self.is_valid = False
