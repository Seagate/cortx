import os
from s3iamcli.cli_response import CLIResponse

class FederationToken:
    def __init__(self, sts_client, cli_args):
        self.sts_client = sts_client
        self.cli_args = cli_args

    def create(self):
        if(self.cli_args.name is None):
            message = "Name is required to create federation token."
            CLIResponse.send_error_out(message)

        federation_token_args = {}
        federation_token_args['Name'] = self.cli_args.name

        if(not self.cli_args.duration is None):
            federation_token_args['DurationSeconds'] = self.cli_args.duration

        if(not self.cli_args.file is None):
            file_path = os.path.abspath(self.cli_args.file)
            if(not os.path.isfile(file_path)):
                message = "Policy file " + file_path + " not found."
                CLIResponse.send_error_out(message)

            with open (file_path, "r") as federation_token_file:
                policy = federation_token_file.read()

            federation_token_args['Policy'] = policy

        try:
            result = self.sts_client.get_federation_token(**federation_token_args)
        except Exception as ex:
            message = "Exception occured while creating federation token.\n"
            message += str(ex)
            CLIResponse.send_error_out(message)

        print("FederatedUserId = %s, AccessKeyId = %s, SecretAccessKey = %s, SessionToken = %s" % (
                result['FederatedUser']['FederatedUserId'],
                result['Credentials']['AccessKeyId'],
                result['Credentials']['SecretAccessKey'],
                result['Credentials']['SessionToken']))
