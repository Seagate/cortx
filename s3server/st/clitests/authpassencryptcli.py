import os
import sys
import time
from threading import Timer
import subprocess
from framework import S3PyCliTest
from framework import Config
from framework import logit

class EncryptCLITest(S3PyCliTest):
    encryptcli_cmd = "java -jar /opt/seagate/auth/AuthPassEncryptCLI-1.0-0.jar"

    def __init__(self, description):
        super(EncryptCLITest, self).__init__(description)


    def run(self):
        super(EncryptCLITest, self).run()

    def teardown(self):
        super(EncryptCLITest, self).teardown()

    def with_cli(self, cmd):
        super(EncryptCLITest, self).with_cli(cmd)

    def encrypt_passwd(self, passwd):
        cmd =  "%s -s %s" % (self.encryptcli_cmd,
                    passwd)
        self.with_cli(cmd)
        return self
