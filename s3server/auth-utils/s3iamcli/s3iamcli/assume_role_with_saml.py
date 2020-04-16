import os
from s3iamcli.cli_response import CLIResponse

class AssumeRoleWithSAML:
    def __init__(self, sts_client, cli_args):
        self.sts_client = sts_client
        self.cli_args = cli_args

    def create(self):
        if(self.cli_args.saml_principal_arn is None):
            message = "SAML principal ARN is required."
            CLIResponse.send_error_out(message)

        if(self.cli_args.saml_role_arn is None):
            message = "SAML role ARN is required."
            CLIResponse.send_error_out(message)

        if(self.cli_args.saml_assertion is None):
            message = "SAML assertion is required."
            CLIResponse.send_error_out(message)

        assertion_file_path = os.path.abspath(self.cli_args.saml_assertion)
        if(not os.path.isfile(assertion_file_path)):
            message = "Saml assertion file not found."
            CLIResponse.send_error_out(message)

        with open (assertion_file_path, "r") as saml_assertion_file:
            assertion = saml_assertion_file.read()

        assume_role_with_saml_args = {}
        assume_role_with_saml_args['SAMLAssertion'] = assertion
        assume_role_with_saml_args['RoleArn'] = self.cli_args.saml_role_arn
        assume_role_with_saml_args['PrincipalArn'] = self.cli_args.saml_principal_arn

        if(not self.cli_args.duration is None):
            assume_role_with_saml_args['DurationSeconds'] = self.cli_args.duration

        if(not self.cli_args.file is None):
            file_path = os.path.abspath(self.cli_args.file)
            if(not os.path.isfile(file_path)):
                message = "Policy file not found. " + file_path
                CLIResponse.send_error_out(message)

            with open (file_path, "r") as federation_token_file:
                policy = federation_token_file.read()

            assume_role_with_saml_args['Policy'] = policy

        try:
            result = self.sts_client.assume_role_with_saml(**assume_role_with_saml_args)
        except Exception as ex:
            message = "Exception occured while creating credentials with saml.\n"
            message += str(ex)
            CLIResponse.send_error_out(message)

        print("AssumedRoleId- %s, AccessKeyId- %s, SecretAccessKey- %s, SessionToken- %s" % (
                result['AssumedRoleUser']['AssumedRoleId'],
                result['Credentials']['AccessKeyId'],
                result['Credentials']['SecretAccessKey'],
                result['Credentials']['SessionToken'],))
