import os
from s3iamcli.cli_response import CLIResponse

class SAMLProvider:
    def __init__(self, iam_client, cli_args):
        self.iam_client = iam_client
        self.cli_args = cli_args

    def create(self):
        if(self.cli_args.file is None):
            message = "SAML Metadata Document is required to create role"
            CLIResponse.send_error_out(message)

        if(self.cli_args.name is None):
            message = "Provider name is required to create SAML provider."
            CLIResponse.send_error_out(message)

        file_path = os.path.abspath(self.cli_args.file)
        if(not os.path.isfile(file_path)):
            message = "File " + file_path + " not found."
            CLIResponse.send_error_out(message)

        with open (file_path, "r") as saml_metadata_file:
            saml_metadata = saml_metadata_file.read()

        saml_provider_args = {}
        saml_provider_args['SAMLMetadataDocument'] = saml_metadata
        saml_provider_args['Name'] = self.cli_args.name

        try:
            result = self.iam_client.create_saml_provider(**saml_provider_args)
        except Exception as ex:
            message = "Exception occured while creating saml provider.\n"
            message += str(ex)
            CLIResponse.send_error_out(message)

        message = "SAMLProviderArn = %s" % (result['SAMLProviderArn'])
        CLIResponse.send_success_out(message)

    def delete(self):
        if(self.cli_args.arn is None):
            message = "SAML Provider ARN is required to delete."
            CLIResponse.send_error_out(message)

        saml_provider_args = {}
        saml_provider_args['SAMLProviderArn'] = self.cli_args.arn

        try:
            self.iam_client.delete_saml_provider(**saml_provider_args)
        except Exception as ex:
            message = "Exception occured while deleting SAML provider.\n"
            message += str(ex)
            CLIResponse.send_error_out(message)

        message = "SAML provider deleted."
        CLIResponse.send_success_out(message)

    def update(self):
        if(self.cli_args.file is None):
            message = "SAML Metadata Document is required to update saml provider."
            CLIResponse.send_error_out(message)

        if(self.cli_args.arn is None):
            message = "SAML Provider ARN is required to update saml provider."
            CLIResponse.send_error_out(message)

        file_path = os.path.abspath(self.cli_args.file)
        if(not os.path.isfile(file_path)):
            message = "File " + file_path + " not found."
            CLIResponse.send_error_out(message)

        with open (file_path, "r") as saml_metadata_file:
            saml_metadata = saml_metadata_file.read()

        saml_provider_args = {}
        saml_provider_args['SAMLMetadataDocument'] = saml_metadata
        saml_provider_args['SAMLProviderArn'] = self.cli_args.arn

        try:
            self.iam_client.update_saml_provider(**saml_provider_args)
        except Exception as ex:
            message = "Exception occured while updating SAML provider.\n"
            message += str(ex)
            CLIResponse.send_error_out(message)

        message = "SAML provider Updated."
        CLIResponse.send_success_out(message)

    def list(self):
        try:
            result = self.iam_client.list_saml_providers()
        except Exception as ex:
            message = "Exception occured while listing SAML providers.\n"
            message += str(ex)
            CLIResponse.send_error_out(message)

        for provider in result['SAMLProviderList']:
            print("ARN = %s, ValidUntil = %s" % (provider['Arn'], provider['ValidUntil']))
