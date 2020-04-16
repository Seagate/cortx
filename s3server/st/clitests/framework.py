import os
import sys
import shutil
import time
import re
import yaml
from scripttest import TestFileEnvironment
from ldap_setup import LdapSetup
from cloud_setup import CloudGatewaySetup

class Config:
    log_enabled = False
    dummy_run = False
    config_file = 'pathstyle.s3cfg'
    time_readable_format = True
    tmp_wd = ''
    s3cmd_max_retries = 5
    no_ssl = False
    use_ipv6 = False
    client_execution_timeout = None
    request_timeout = None
    socket_timeout = None
    aws_shared_credential_file = "aws_credential_file"
    aws_iam_shared_credential_file = "aws_iam_credential_file"
    aws_config_file = "aws_config_file"
    aws_iam_config_file = "aws_iam_config_file"
    python = 'python3.6'

class CloudConfig:
    pass

def logit(str):
    if Config.log_enabled:
        print(str)

class PyCliTest(object):
    'Base class for all test cases.'

    def __init__(self, description, tmp_wd = 'tests-out', clear_base_dir = True):
        self.description = description
        self.command = ''
        self.negative_case = False
        self.ignore_err = False
        self.tmp_wd = tmp_wd
        self._create_temp_working_dir()
        self.env = TestFileEnvironment(base_path = self.working_dir, start_clear = clear_base_dir)

    def before_all(self):
        raise NotImplementedError("before_all not implemented.")

    def _create_temp_working_dir(self):
        self.working_dir = os.path.join(os.path.dirname(os.path.realpath(__file__)), self.tmp_wd)

    def with_cli(self, command):
        self.command = command
        return self

    def setup(self):
        # Do some initializations required to run the tests
        logit("Setting up the test [%s]" % (self.description))
        return self

    def run(self, stdin_values = None):
        # Execute the test.
        logit("Running command: [%s]" % (self.command))

        stdin_param= None
        expect_error_value = False
        expect_stderr_value = False

        start_time = time.time()
        if Config.dummy_run:
            self.status = self.env.run("echo [%s]" % (self.command))
        else:

            if stdin_values != None:
                stdin_param = bytearray(stdin_values, 'utf-8')
            if(self.negative_case):
                expect_error_value = True
                expect_stderr_value = True
            elif(self.ignore_err):
                expect_error_value = False
                expect_stderr_value = True

            self.status = self.env.run(self.command, expect_error=expect_error_value, stdin=stdin_param, expect_stderr=expect_stderr_value)

        end_time = time.time()
        self.print_time(round(end_time - start_time, 3))
        logit("returncode: [%d]" % (self.status.returncode))
        logit("stdout: [%s]" % (self.status.stdout))
        logit("stderr: [%s]" % (self.status.stderr))
        return self

    def print_time(self, time):
        time_str = "Execution time - "
        if(Config.time_readable_format):
            if(time < 1):
                time_str += str(round(time * 1000)) + " ms"
            elif(time > 60):
                m, s = divmod(time, 60)
                time_str += str(round(m)) + " min " + str(round(s,3)) + " seconds"
            else:
                time_str += str(round(time,3)) + " seconds"
        else:
            time_str += str(round(time * 1000)) + " ms"

        print(time_str)

    def teardown(self):
        # Do some cleanup
        logit("Running teardown [%s]" % (self.description) + "    ")
        shutil.rmtree(self.working_dir, ignore_errors=True)
        return self

    def execute_test(self, negative_case = False, ignore_err = False, stdin_values = None):
        print("\nTest case [%s] - " % (self.description), end="")
        self.negative_case = negative_case
        self.ignore_err = ignore_err
        self.setup()
        if stdin_values != None:
            self.run(stdin_values)
        else:
            self.run()
        self.teardown()
        return self

    def command_is_successful(self):
        if not Config.dummy_run:
           assert self.status.returncode == 0, 'Test Failed'
        print("Command was successful.")
        return self

    def command_should_fail(self):
        if not Config.dummy_run:
          assert self.status.returncode != 0, 'Test Failed'
        print("Command has failed as expected.")
        return self

    def command_response_should_have(self, msg):
        if not Config.dummy_run:
            assert msg in self.status.stdout
        print("Response has [%s]." % (msg))
        return self

    def command_response_should_not_have(self, msg):
        if not Config.dummy_run:
            assert msg not in self.status.stdout
        print("Response does not have [%s]." % (msg))
        return self

    def command_error_should_have(self, msg):
        if not Config.dummy_run:
            assert msg in self.status.stderr
        print("Error message has [%s]." % (msg))
        return self

    def command_error_should_not_have(self, msg):
        if not Config.dummy_run:
            assert msg in self.status.stderr
        print("Error message does not have [%s]." % (msg))
        return self

    def command_created_file(self, file_name):
        logit("Checking for file [%s] in files created [%s]" % (file_name, ', '.join(self.status.files_created)))
        if not Config.dummy_run:
            assert file_name in self.status.files_created, file_name + ' NOT created'
        print("File [%s] was created." % (file_name))
        return self

    def command_deleted_file(self, file_name):
        logit("Checking for file [%s] in files deleted [%s]" % (file_name, ', '.join(self.status.files_deleted)))
        if not Config.dummy_run:
            assert file_name in self.status.files_deleted, file_name + ' NOT deleted'
        print("File [%s] was deleted." % (file_name))
        return self

    def command_updated_file(self, file_name):
        logit("Checking for file [%s] in files updated [%s]" % (file_name, ', '.join(self.status.files_updated)))
        if not Config.dummy_run:
            assert file_name in self.status.files_updated, file_name + ' NOT updated'
        print("File [%s] was updated." % (file_name))
        return self

    # Match the output with the given pattern.
    def command_should_match_pattern(self, pattern):
        regex = re.compile(pattern)
        if not Config.dummy_run:
            assert bool(regex.match(self.status.stdout)), ("Expected output %s.\nActual output %s" % (pattern, self.status.stdout))
        print("Command was successful.")
        return self

    # Match the std error with the given pattern.
    def command_error_should_match_pattern(self, pattern):
        regex = re.compile(pattern)
        if not Config.dummy_run:
            assert bool(regex.match(self.status.stderr)), ("Expected output %s.\nActual output %s" % (pattern, self.status.stdout))
        print("Command was successful.")
        return self

    # Get exit status for command
    def get_exitstatus(self):
        return self.status.returncode

class S3PyCliTest(PyCliTest):

    def before_all(self):
        ldap_setup = LdapSetup()
        ldap_setup.ldap_delete_all()
        ldap_setup.ldap_init()


class TCTPyCliTest(PyCliTest):

    def __init__(self, description, clear_base_dir = True):
        super(TCTPyCliTest, self).__init__(description, tmp_wd = Config.tmp_wd, clear_base_dir = clear_base_dir)

    def before_all(self):
        cloud = CloudGatewaySetup()
        if cloud.service_status() != 'Started':
            rc = cloud.service_start()
            if rc != 0:
                raise AssertionError("Failed to start cloud gateway service.")
            # wait for mmcloudgatway service to start properly
            time.sleep(15)
        rc = cloud.account_create()
        if rc == 61:
            raise AssertionError("Pre-configured cloud gateway account exists.")
        elif rc != 0:
            raise AssertionError("Failed to create cloud gateway account.")

        # list configured cloud details
        cloud.account_list()

    def teardown(self):
        CloudGatewaySetup().account_delete()
        super(TCTPyCliTest, self).teardown()
