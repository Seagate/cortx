
import os
import sys
import time
from threading import Timer
import subprocess
from framework import S3PyCliTest
from framework import Config
from framework import logit
from shlex import quote

class S3cmdTest(S3PyCliTest):
    def __init__(self, description):
        self.s3cfg = os.path.join(os.path.dirname(os.path.realpath(__file__)), Config.config_file)
        self._send_retries = " --max-retries=" + str(Config.s3cmd_max_retries) + " "
        self.credentials = ""
        super(S3cmdTest, self).__init__(description)

    def setup(self):
        if hasattr(self, 'filename') and hasattr(self, 'filesize'):
            file_to_create = os.path.join(self.working_dir, self.filename)
            logit("Creating file [%s] with size [%d]" % (file_to_create, self.filesize))
            with open(file_to_create, 'wb') as fout:
                fout.write(os.urandom(self.filesize))
        super(S3cmdTest, self).setup()

    def run(self):
        super(S3cmdTest, self).run()

    def teardown(self):
        super(S3cmdTest, self).teardown()

    def with_credentials(self, access_key, secret_key):
        self.credentials = " --access_key=" + access_key +\
                           " --secret_key=" + secret_key
        return self

    def with_cli(self, cmd):
        if Config.no_ssl:
            cmd = cmd + " --no-ssl"
        super(S3PyCliTest, self).with_cli(cmd)

    def create_bucket(self, bucket_name, region=None, host=None, no_check_hostname=None):
        self.bucket_name = bucket_name
        if host:
            if region:
                self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + "--host=" +
                               host + " --host-bucket=" + bucket_name + "." + host + " mb " +
                               " s3://" + self.bucket_name + " --bucket-location=" + region)
            else:
                self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries +
                              "--host=" + host + " --host-bucket=" + bucket_name
                              + "." + host +" mb " + " s3://" + self.bucket_name)
        else:
            if region:
                self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " mb " + " s3://" + self.bucket_name + " --bucket-location=" + region)
            else:
                self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " mb " + " s3://" + self.bucket_name)

        if no_check_hostname:
            self.command = self.command + " --no-check-hostname"
        self.command = self.command + self.credentials
        return self

    def list_buckets(self, host=None, no_check_hostname=None):
        if host:
            self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries +
            "--host=" + host + " --host-bucket=" + host + " ls ")
        else:
            self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " ls ")

        if no_check_hostname:
            self.command = self.command + " --no-check-hostname"
        self.command = self.command + self.credentials
        return self

    def info_bucket(self, bucket_name, host=None, no_check_hostname=None):
        self.bucket_name = bucket_name
        if host:
            self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries +
             "--host=" + host + " --host-bucket=" + bucket_name + "." + host +
             " info " + " s3://" + self.bucket_name)
        else:
            self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " info " + " s3://" + self.bucket_name)

        if no_check_hostname:
            self.command = self.command + " --no-check-hostname"
        self.command = self.command + self.credentials
        return self

    def info_object(self, bucket_name, filename, host=None, no_check_hostname=None):
        self.bucket_name = bucket_name
        self.filename = filename
        if host:
            self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries +
            "--host=" + host + " --host-bucket=" + bucket_name + "." + host +
            " info " + quote("s3://" + self.bucket_name + "/" +  self.filename ))
        else:
            self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " info " + quote("s3://" + self.bucket_name + "/" + self.filename))
        if no_check_hostname:
            self.command = self.command + " --no-check-hostname"
        return self

    def delete_bucket(self, bucket_name, host=None, no_check_hostname=None):
        self.bucket_name = bucket_name
        if host:
            self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries +
            "--host=" + host + " --host-bucket=" + bucket_name + "." + host +
            " rb " + " s3://" + self.bucket_name)
        else:
            self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " rb " + " s3://" + self.bucket_name)
        if no_check_hostname:
            self.command = self.command + " --no-check-hostname"
        self.command = self.command + self.credentials
        return self

    def list_objects(self, bucket_name, host=None, no_check_hostname=None):
        self.bucket_name = bucket_name
        if host:
            self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries +
            "--host=" + host + " --host-bucket=" + bucket_name + "." + host +
            " ls " + " s3://" + self.bucket_name)
        else:
            self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " ls " + " s3://" + self.bucket_name)

        if no_check_hostname:
            self.command = self.command + " --no-check-hostname"
        self.command = self.command + self.credentials
        return self

    def list_all_objects(self):
        self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " la ")
        return self

    def list_specific_objects(self, bucket_name, object_pattern):
        self.bucket_name = bucket_name
        self.object_pattern = object_pattern
        self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " ls " + quote("s3://" + self.bucket_name + "/" + self.object_pattern))
        return self

    def disk_usage_bucket(self, bucket_name):
        self.bucket_name = bucket_name
        self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " du " + " s3://" + self.bucket_name)
        return self

    def upload_test(self, bucket_name, filename, filesize, host=None, no_check_hostname=None):
        self.filename = filename
        self.filesize = filesize
        self.bucket_name = bucket_name
        s3target = "s3://" + bucket_name;
        if host:
            self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries +
            "--host=" + host + " --host-bucket=" + bucket_name + "." + host +
            " put " + self.filename + " s3://" + self.bucket_name)
        else:
            self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " put " + quote(self.filename) + " " +
quote(s3target))
        if no_check_hostname:
            self.command = self.command + " --no-check-hostname"
        self.command = self.command + self.credentials
        return self

    def upload_copy_test(self, bucket_name, srcfile, destfile):
        self.srcfile = srcfile
        self.destfile = destfile
        self.bucket_name = bucket_name
        self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " cp " + quote("s3://" + self.bucket_name + "/" + self.srcfile) + " " + quote("s3://" + self.bucket_name + "/" + self.destfile))
        return self

    def upload_move_test(self, src_bucket, srcfile, dest_bucket, destfile):
        self.srcfile = srcfile
        self.destfile = destfile
        self.src_bucket = src_bucket
        self.dest_bucket = dest_bucket
        self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " cp " + quote("s3://" + self.src_bucket + "/" + self.srcfile) + " " + quote("s3://" + self.dest_bucket + "/" + self.destfile))
        return self

    def list_multipart_uploads(self, bucket_name):
        self.bucket_name = bucket_name
        self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " multipart s3://" + self.bucket_name)
        return self

    def abort_multipart(self, bucket_name, filename, upload_id):
        self.filename = filename
        self.bucket_name = bucket_name
        self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " abortmp " + quote("s3://" + self.bucket_name + "/" + self.filename) + " " + upload_id)
        return self

    def list_parts(self, bucket_name, filename, upload_id):
        self.filename = filename
        self.bucket_name = bucket_name
        s3target = "s3://" + bucket_name + "/" + filename;
        cmd = "s3cmd --no-mime-magic -c %s %s listmp %s %s" % (self.s3cfg, self._send_retries,
               quote(s3target), upload_id)

        self.with_cli(cmd)
        return self

    def download_test(self, bucket_name, filename, host=None, no_check_hostname=None):
        self.filename = filename
        self.bucket_name = bucket_name
        if host:
            self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries +
            "--host=" + host + " --host-bucket=" + bucket_name + "." + host +
            " get " + quote("s3://" + self.bucket_name + "/" + self.filename))
        else:
            self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " get " + quote("s3://" + self.bucket_name + "/"  + self.filename))
        if no_check_hostname:
            self.command = self.command + " --no-check-hostname"
        return self

    def setacl_bucket(self, bucket_name, acl_perm):
        self.bucket_name = bucket_name
        self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " setacl " + "s3://" + self.bucket_name + " --acl-grant=" + acl_perm)
        return self

    def setpolicy_bucket(self, bucket_name, policyfile):
        self.bucket_name = bucket_name
        self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " setpolicy " + self.working_dir + "/../" + policyfile + " s3://" + self.bucket_name)
        return self

    def delpolicy_bucket(self, bucket_name):
        self.bucket_name = bucket_name
        self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " delpolicy " + " s3://" + self.bucket_name)
        return self

    def accesslog_bucket(self, bucket_name):
        self.bucket_name = bucket_name
        self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " accesslog " + " s3://" + self.bucket_name)
        return self

    def fixbucket(self, bucket_name):
        self.bucket_name = bucket_name
        self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " fixbucket " + " s3://" + self.bucket_name)
        return self

    def setacl_object(self, bucket_name, filename, acl_perm):
        self.filename = filename
        self.bucket_name = bucket_name
        self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " setacl " + "s3://" + self.bucket_name + "/" + self.filename + " --acl-grant=" + acl_perm)
        return self

    def revoke_acl_bucket(self, bucket_name, acl_perm):
        self.bucket_name = bucket_name
        self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " setacl " + "s3://" + self.bucket_name + " --acl-revoke=" + acl_perm)
        return self

    def revoke_acl_object(self, bucket_name, filename, acl_perm):
        self.filename = filename
        self.bucket_name = bucket_name
        self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " setacl " + "s3://" + self.bucket_name + "/" + self.filename + " --acl-revoke=" + acl_perm)
        return self

    def stop_s3authserver_test(self):
        cmd = "sudo systemctl stop s3authserver.service";
        self.with_cli(cmd)
        return self

    def start_s3authserver_test(self):
        cmd = "sudo systemctl start s3authserver.service";
        self.with_cli(cmd)
        return self

    def delete_test(self, bucket_name, filename, host=None, no_check_hostname=None):
        self.filename = filename
        self.bucket_name = bucket_name
        if host:
            self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries +
            "--host=" + host + " --host-bucket=" + bucket_name + "." + host +
            " del " + quote("s3://" + self.bucket_name + "/" + self.filename))
        else:
            self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " del " + quote("s3://" + self.bucket_name + "/" + self.filename))
        if no_check_hostname:
            self.command = self.command + " --no-check-hostname"
        return self

    def multi_delete_test(self, bucket_name):
        self.bucket_name = bucket_name
        self.with_cli("s3cmd --no-mime-magic -c " + self.s3cfg + self._send_retries + " del " + "s3://" + self.bucket_name + "/ --recursive --force")
        self.command = self.command + self.credentials
        return self
