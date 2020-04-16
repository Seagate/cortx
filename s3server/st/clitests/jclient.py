
import os
import sys
import time
from threading import Timer
import subprocess
from framework import S3PyCliTest
from framework import Config
from framework import logit
from s3client_config import S3ClientConfig
from shlex import quote

class JClientTest(S3PyCliTest):
    jclient_cmd = "java -jar ../jclient.jar"

    def __init__(self, description):
        self.s3cfg = os.path.join(os.path.dirname(os.path.realpath(__file__)), Config.config_file)
        self.chunked = ""
        super(JClientTest, self).__init__(description)

    def setup(self):
        if hasattr(self, 'filename') and hasattr(self, 'filesize'):
            file_to_create = os.path.join(self.working_dir, self.filename)
            logit("Creating file [%s] with size [%d]" % (file_to_create, self.filesize))
            with open(file_to_create, 'wb') as fout:
                fout.write(os.urandom(self.filesize))
        super(JClientTest, self).setup()

    # return the common params
    def get_test_config(self):
        if S3ClientConfig.pathstyle:
            config = "%s -x '%s' -y '%s' -p" % (self.chunked, S3ClientConfig.access_key_id,
                        S3ClientConfig.secret_key)
        else:
            config = "%s -x '%s' -y '%s'" % (self.chunked, S3ClientConfig.access_key_id,
                        S3ClientConfig.secret_key)
        return config

    def with_cli(self, cmd):
        if 'jclient.jar' in cmd:
            if Config.client_execution_timeout != None:
                cmd = cmd + " --cli-exec-timeout %s" % (Config.client_execution_timeout)
            if Config.request_timeout != None:
                cmd = cmd + " --req-timeout %s" % (Config.request_timeout)
            if Config.socket_timeout != None:
                cmd = cmd + " --sock-timeout %s" % (Config.socket_timeout)
        super(JClientTest, self).with_cli(cmd)

    def run(self):
        super(JClientTest, self).run()

    def teardown(self):
        super(JClientTest, self).teardown()

    def create_bucket(self, bucket_name, region=None):
        if region:
            cmd =  "%s mb s3://%s -l %s %s" % (self.jclient_cmd,
                    bucket_name, region, self.get_test_config())
        else:
            cmd =  "%s mb s3://%s %s" % (self.jclient_cmd,
                    bucket_name, self.get_test_config())

        self.with_cli(cmd)
        return self

    def list_buckets(self):
        cmd =  "%s ls %s" % (self.jclient_cmd, self.get_test_config())
        self.with_cli(cmd)
        return self

    def delete_bucket(self, bucket_name):
        cmd =  "%s rb s3://%s %s" % (self.jclient_cmd,
                bucket_name, self.get_test_config())
        self.with_cli(cmd)
        return self

    def list_specific_objects(self, bucket_name, object_pattern):
        s3target = "s3://" + bucket_name + "/" + object_pattern;
        cmd =  "%s ls %s %s" % (self.jclient_cmd,
                quote(s3target), self.get_test_config())

        self.with_cli(cmd)
        return self

    def list_objects(self, bucket_name):
        cmd =  "%s ls s3://%s %s" % (self.jclient_cmd, bucket_name,
                self.get_test_config())
        self.with_cli(cmd)
        return self

    def put_object(self, bucket_name, filename, filesize, prefix=None, chunked=False):
        self.filename = filename
        self.filesize = filesize
        self.prefix = prefix
        if chunked:
            self.chunked = "-C"
        else:
            self.chunked = ""
        if prefix:
            # s3://%s/%s/%s = s3://bucket/prefix/filename
            s3target = "s3://" + bucket_name + "/" + prefix + "/" + filename;
            cmd = "%s put %s %s %s" % (self.jclient_cmd, quote(filename),
                quote(s3target), self.get_test_config())
        else:
            s3target = "s3://" + bucket_name;
            cmd = "%s put %s %s %s" % (self.jclient_cmd, quote(filename),
                quote(s3target), self.get_test_config())

        self.with_cli(cmd)
        return self

    def init_mpu(self, bucket_name, filename, filesize):
        self.filename = filename
        self.filesize = filesize
        s3target = "s3://" + bucket_name;
        cmd =  "%s initmpu %s %s %s" % (self.jclient_cmd, quote(filename),
                quote(s3target), self.get_test_config())

        self.with_cli(cmd)
        return self

    def put_object_multipart(self, bucket_name, filename, filesize, size_of_part, chunked=False):
        self.filename = filename
        self.filesize = filesize
        if chunked:
            self.chunked = "-C"
        else:
            self.chunked = ""
        s3target = "s3://" + bucket_name;
        cmd =  "%s put %s %s -m %s %s" % (self.jclient_cmd, quote(filename),
                quote(s3target), str(size_of_part), self.get_test_config())

        self.with_cli(cmd)
        return self

    def get_object(self, bucket_name, filename):
        s3target = "s3://" + bucket_name + "/" + filename;
        cmd =  "%s get %s %s %s" % (self.jclient_cmd,
                quote(s3target), quote(filename), self.get_test_config())
        self.with_cli(cmd)
        return self

    def delete_object(self, bucket_name, filename):
        s3target = "s3://" + bucket_name + "/" + filename;
        cmd =  "%s del %s %s" % (self.jclient_cmd,
                quote(s3target), self.get_test_config())
        self.with_cli(cmd)
        return self

    def delete_multiple_objects(self, bucket_name, objects):
        self.bucket_name = bucket_name
        cmd =  "%s multidel s3://%s " % (self.jclient_cmd,
                bucket_name)

        for i, object in enumerate(objects):
            cmd += " " + object

        cmd += " " + self.get_test_config()
        self.with_cli(cmd)
        return self

    def partial_multipart_upload(self, bucket_name, filename, filesize,
        size_of_part, no_of_parts, with_upload_id=None, from_part=None, chunked=False):
        self.filename = filename
        self.filesize = filesize
        if chunked:
            self.chunked = "-C"
        else:
            self.chunked = ""

        s3target = "s3://" + bucket_name;
        cmd =  "%s partialput %s %s %s -m %s %s" % (self.jclient_cmd,
            quote(filename), quote(s3target), str(no_of_parts), str(size_of_part),
            self.get_test_config())

        if with_upload_id:
            cmd += " --with-upload-id %s" % with_upload_id

        if from_part:
            cmd += " --from-part %s" % from_part

        self.with_cli(cmd)
        return self

    def list_multipart(self, bucket_name, prefix=None, delimiter=None,
            next_marker=None, upload_id_marker=None, max_uploads=None,
            show_next=None):
        cmd = "%s multipart s3://%s %s" % (self.jclient_cmd, bucket_name,
            self.get_test_config())

        if prefix:
            cmd += " --prefix %s" % prefix

        if delimiter:
            cmd += " --delimiter %s" % delimiter

        if next_marker:
            cmd += " --next-marker %s" % next_marker

        if upload_id_marker:
            cmd += " --upload-id-marker %s" % upload_id_marker

        if max_uploads:
            cmd += " --max-uploads %s" % max_uploads

        if show_next:
            cmd += " --show-next"

        self.with_cli(cmd)
        return self

    def list_parts(self, bucket_name, file_name, upload_id):
        s3target = "s3://" + bucket_name + "/" + file_name;
        cmd = "%s listmp %s %s %s" % (self.jclient_cmd,
            quote(s3target), upload_id, self.get_test_config())

        self.with_cli(cmd)
        return self

    def abort_multipart(self, bucket_name, file_name, upload_id):
        s3target = "s3://" + bucket_name + "/" + file_name;
        cmd = "%s abortmp %s %s %s" % (self.jclient_cmd,
            quote(s3target), upload_id, self.get_test_config())

        self.with_cli(cmd)
        return self

    def check_bucket_exists(self, bucket_name):
        cmd = "%s exists s3://%s %s" % (self.jclient_cmd, bucket_name,
            self.get_test_config())

        self.with_cli(cmd)
        return self

    def head_object(self, bucket_name, file_name):
        s3target = "s3://" + bucket_name + "/" + file_name;
        cmd = "%s head %s %s" % (self.jclient_cmd,
            quote(s3target), self.get_test_config())

        self.with_cli(cmd)
        return self

    def get_bucket_location(self, bucket_name):
        cmd = "%s location s3://%s %s" % (self.jclient_cmd, bucket_name,
            self.get_test_config())

        self.with_cli(cmd)
        return self

    # acl_action = acl-public or acl-private
    # acl_action = acl-grant, perm = PERM:UserCanonicalID[:DisplayName]
    # acl_action = acl-revoke, perm = perm = PERM:UserCanonicalID
    def set_acl(self, bucket_name, file_name=None, action="acl-public", permission=""):
        assert action in ["acl-public", "acl-private", "acl-grant", "acl-revoke"]
        self.bucket_name = bucket_name
        self.file_name = file_name
        acl_action = action

        if action == "acl-grant" or action == "acl-revoke":
            acl_action = action + "=" + permission

        if file_name:
            s3target = "s3://" + bucket_name + "/" + file_name;
            cmd = "%s setacl %s %s %s" % (self.jclient_cmd,
                quote(s3target), acl_action, self.get_test_config())
        else:
            cmd = "%s setacl s3://%s %s %s" % (self.jclient_cmd, bucket_name, acl_action,
                self.get_test_config())

        self.with_cli(cmd)
        return self

    def get_acl(self, bucket_name, file_name=None):
        self.bucket_name = bucket_name
        self.file_name = file_name

        if file_name:
            s3target = "s3://" + bucket_name + "/" + file_name;
            cmd = "%s getacl %s %s" % (self.jclient_cmd,
                quote(s3target), self.get_test_config())
        else:
            cmd = "%s getacl s3://%s %s" % (self.jclient_cmd, bucket_name,
                self.get_test_config())

        self.with_cli(cmd)
        return self

    def set_bucket_policy(self, bucket_name, policy_file):
        cmd = "%s setpolicy s3://%s %s %s" % (self.jclient_cmd, bucket_name,
            policy_file, self.get_test_config())

        self.with_cli(cmd)
        return self

    def get_bucket_policy(self, bucket_name):
        cmd = "%s getpolicy s3://%s %s" % (self.jclient_cmd, bucket_name,
            self.get_test_config())

        self.with_cli(cmd)
        return self

    def delete_bucket_policy(self, bucket_name):
        cmd = "%s delpolicy s3://%s %s" % (self.jclient_cmd, bucket_name,
            self.get_test_config())

        self.with_cli(cmd)
        return self
