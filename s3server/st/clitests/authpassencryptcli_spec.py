#!/usr/bin/python3.6

import os
from framework import Config
from framework import S3PyCliTest
from authpassencryptcli import EncryptCLITest
from s3client_config import S3ClientConfig


# Run AuthPassEncryptCLI tests.

#Encrypt password tests
EncryptCLITest('Encrypt given password').encrypt_passwd("seagate").execute_test().command_is_successful()

#Negative Tests
EncryptCLITest('Encrypt Invalid Passwd (only space)').encrypt_passwd("\" \"").execute_test(negative_case=True).command_should_fail().command_error_should_have('Invalid Password Value.')
EncryptCLITest('Encrypt Invalid Passwd (with space)').encrypt_passwd("\"xyz test\"").execute_test(negative_case=True).command_should_fail().command_error_should_have('Invalid Password Value.')
EncryptCLITest('Encrypt Invalid Passwd (empty)').encrypt_passwd("\"\"").execute_test(negative_case=True).command_should_fail().command_error_should_have('Invalid Password Value')
EncryptCLITest('No password value').encrypt_passwd("").execute_test(negative_case=True).command_should_fail().command_response_should_have('Usage: java -jar AuthPassEncryptCLI-1.0-0.jar [options]')

