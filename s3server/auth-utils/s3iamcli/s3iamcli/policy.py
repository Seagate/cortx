import os
from s3iamcli.cli_response import CLIResponse

class Policy:
    def __init__(self, iam_client, cli_args):
        self.iam_client = iam_client
        self.cli_args = cli_args

    def create(self):
        policy_args = {}
        if(self.cli_args.name is None):
            message = "Policy name is required."
            CLIResponse.send_error_out(message)

        if(self.cli_args.file is None):
            message = "Policy document is required."
            CLIResponse.send_error_out(message)

        file_path = os.path.abspath(self.cli_args.file)
        if(not os.path.isfile(file_path)):
            message = "Policy file " + file_path + " not found."
            CLIResponse.send_error_out(message)

        policy_args['PolicyName'] = self.cli_args.name

        with open (file_path, "r") as policy_file:
            policy = policy_file.read()
        policy_args['PolicyDocument'] = policy

        if(not self.cli_args.path is None):
            policy_args['Path'] = self.cli_args.path

        if(not self.cli_args.description is None):
            policy_args['Description'] = self.cli_args.description

        try:
            result = self.iam_client.create_policy(**policy_args)
        except Exception as ex:
            message = "Exception occured while creating policy.\n"
            message += str(ex)
            CLIResponse.send_error_out(message)

        print("PolicyId = %s, PolicyName = %s, Arn = %s, DefaultVersionId = %s" % (
                result['Policy']['PolicyId'],
                result['Policy']['PolicyName'],
                result['Policy']['Arn'],
                result['Policy']['DefaultVersionId']))
