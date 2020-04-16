import logging
from s3iamcli.authserver_response import AuthServerResponse

class ErrorResponse(AuthServerResponse):

    # Getter for Error Response
    def get_error_message(self):
        return self.error

    # Validator for Error Response
    def validate_response(self):

        super(ErrorResponse, self).validate_response()
        self.error = None
        try:
            self.error =  "An error occurred (" +  self.response_dict['ErrorResponse']['Error']['Code'] + ") : "+ self.response_dict['ErrorResponse']['Error']['Message'];
        except KeyError:
            logging.exception('Failed to obtain error response')
            self.is_valid = False
