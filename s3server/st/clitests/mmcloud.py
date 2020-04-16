import os
import sys
import time
import subprocess
import logging
from threading import Timer
from framework import TCTPyCliTest
from framework import Config
from framework import logit

class MMCloudTest(TCTPyCliTest):

    mmcloud_cmd = "mmcloudgateway"

    def __init__(self, description):
        # start_clear_bd = False --> don't clear test base directory
        super(MMCloudTest, self).__init__(description, clear_base_dir = False)

    def setup(self):
        if hasattr(self, 'filename') and hasattr(self, 'filesize'):
            self.create_file_on_fs(self.filename, self.filesize)
        super(MMCloudTest, self).setup()

    def run(self):
        super(MMCloudTest, self).run()

    def teardown(self):
        # No, thanks!
        pass

    # Create a file on GPFS
    def create_file_on_fs(self, filename, filesize):
        file_to_create = os.path.join(self.working_dir, filename)
        logging.info("Creating file %s with size %dB", file_to_create, filesize)
        with open(file_to_create, 'wb') as fout:
            fout.write(os.urandom(filesize))
            fout.flush()

    # Modify/Update on file
    def update_file_on_fs(self, filename, update_size):
        file_to_update = os.path.join(self.working_dir, filename)
        with open(file_to_update, 'ab') as fout:
            fout.write(os.urandom(update_size))
            fout.flush()

    # Delete file(stub) on GPFS
    def delete_local_file(self, filename):
        cmd = "rm -f %s" % (filename)
        self.with_cli(cmd)

        return self

    """
    Manual Tiering Operations
    """

    # Migrate file to cloud
    def migrate_file(self, filename):
        cmd = "%s files migrate %s" % (self.mmcloud_cmd, filename)
        self.with_cli(cmd)

        return self

    # List file details
    def list_file(self, filename):
        cmd = "%s files list %s" % (self.mmcloud_cmd, filename)
        self.with_cli(cmd)

        return self

    # Recall file from cloud
    def recall_file(self, filename):
        cmd = "%s files recall %s" % (self.mmcloud_cmd, filename)
        self.with_cli(cmd)

        return self

    # Restore file from cloud (when original stub file on GPFS was deleted)
    def restore_file_with_dry_run(self, filename):
        cmd = "%s files restore -v --dry-run %s" % (self.mmcloud_cmd, filename)
        self.with_cli(cmd)

        return self

    # Restore file from cloud (when original stub file on GPFS was deleted)
    def restore_file(self, filename):
        cmd = "%s files restore %s" % (self.mmcloud_cmd, filename)
        self.with_cli(cmd)

        return self

    # If a migrated file is removed from the file system, reconcile removes the
    # corresponding cloud objects and references contained in cloud directory
    def reconcile_files(self, option):
        cmd = "%s files reconcile %s" % (self.mmcloud_cmd, option)
        self.with_cli(cmd)

        return self

    # Delete local file and cloud data
    def delete_with_delete_local_file(self, filename, keep_last_cloud_file = 'No'):
        cmd = "%s files delete --delete-local-file  %s" % (self.mmcloud_cmd, filename)
        if keep_last_cloud_file != 'No':
            cmd = "%s files delete --delete-local-file --keep-last-cloud-file %s" % (self.mmcloud_cmd, filename)

        self.with_cli(cmd)

        return self

    # Recall cloud data back to local file and delete cloud data
    def delete_with_recall_cloud_file(self, filename, keep_last_cloud_file = 'No'):
        cmd = "%s files delete --recall-cloud-file  %s" % (self.mmcloud_cmd, filename)
        if keep_last_cloud_file != 'No':
            cmd = "%s files delete --recall-cloud-file --keep-last-cloud-file %s" % (self.mmcloud_cmd, filename)

        self.with_cli(cmd)

        return self

    # Only delete the extended attribute (requires the file data to be present locally.
    # If the file is in the Non-Resident state, this operation will fail.)
    def delete_with_require_local_file(self, filename, keep_last_cloud_file = 'No'):
        cmd = "%s files delete --require-local-file  %s" % (self.mmcloud_cmd, filename)
        if keep_last_cloud_file != 'No':
            cmd = "%s files delete --require-local-file --keep-last-cloud-file %s" % (self.mmcloud_cmd, filename)

        self.with_cli(cmd)

        return self

    # List files present on cloud
    def cloud_list(self, path):
        cmd = "%s files cloudList --path %s --recursive" % (self.mmcloud_cmd, path)
        self.with_cli(cmd)

        return self

    # List versions of file migrated to cloud
    def cloud_list_with_file_versions(self, filename):
        cmd = "%s files cloudList --file-versions %s" % (self.mmcloud_cmd, filename)
        self.with_cli(cmd)

        return self
