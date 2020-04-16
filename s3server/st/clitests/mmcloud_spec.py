#!/usr/bin/python

import os
import time
import yaml
import logging
import subprocess
from socket import gethostname
from framework import Config
from framework import TCTPyCliTest
from mmcloud import MMCloudTest
from fs_helpers import FSHelper

# generate file using size and hostname: xMB_hostname.file
def _generate_filename(size):
    return "%sMB_%s.file" % (size, gethostname())

# convert MB to Bytes
def _fsize_in_bytes(size):
    return size * 1024 * 1024

# convert MB to KB
def _fsize_in_kbytes(size):
    return size * 1024

# generate list command response
def _generate_list_response(data_version, meta_version, state):
    return "Data Version : %s\nMeta Version : %s\nState        : %s" % (data_version, meta_version, state)

# generate list response with used blocks attribute
def _generate_list_response_with_ub(used_blocks, data_version, meta_version, state):
    return "Used blocks  : %s\n%s" % (used_blocks, _generate_list_response(data_version, meta_version, state))

def test_manual_tiering(filesizes_in_mb):
    logging.info("\n== Started manual tiering tests ==")

    for size in filesizes_in_mb:
        filename = _generate_filename(size)
        filesize = _fsize_in_bytes(size)

        logging.info("Filename: %s Filesize: %sMB", filename, size)
        # Create a file on filesystem
        MMCloudTest('Create file on filesystem').create_file_on_fs(filename, filesize)

        # List newly created file. Expected state::Resident
        expected_response = _generate_list_response(0, 0, 'Resident')
        MMCloudTest('MMCloudGateway LIST').list_file(filename).execute_test().command_is_successful().command_response_should_have(expected_response)

        # Migrate file from filesystem to cloud
        MMCloudTest('MMCloudGateway MIGRATE').migrate_file(filename).execute_test().command_is_successful()

        # List file after migration. Expected state::Non-resident
        expected_response = _generate_list_response_with_ub(0, 1, 1, 'Non-resident')
        MMCloudTest('MMCloudGateway LIST: after migration').list_file(filename).execute_test().command_is_successful().command_response_should_have(expected_response)

        # Recall file from cloud to filesystem
        MMCloudTest('MMCloudGateway RECALL').recall_file(filename).execute_test().command_is_successful()

        # List file
        expected_response = _generate_list_response_with_ub(_fsize_in_kbytes(size), 1, 1, 'Co-resident')
        MMCloudTest('MMCloudGateway LIST: after recall').list_file(filename).execute_test().command_is_successful().command_response_should_have(expected_response)

        # Delete local stub file on filesystem
        MMCloudTest('MMCloudGateway DELETE: remove local file').delete_local_file(filename).execute_test().command_is_successful()

        # Wait for local cloud dictionary to get updated
        logging.warn("Waiting for cloud directory to get updated")
        while MMCloudTest('MMCloudGateway RESTORE: dry-run').restore_file_with_dry_run(filename).execute_test(negative_case = True).get_exitstatus() != 0:
            logging.info("Waiting for 10 secs")
            time.sleep(10)

        # Restore file
        MMCloudTest('MMCloudGateway RESTORE').restore_file(filename).execute_test().command_is_successful()

        # List file
        expected_response = _generate_list_response_with_ub(_fsize_in_kbytes(size), 1, 1, 'Co-resident')
        MMCloudTest('MMCloudGateway LIST: after restore').list_file(filename).execute_test().command_is_successful().command_response_should_have(expected_response)

        # CloudList: List file versions present on cloud
        MMCloudTest('MMCloudGateway CLOUDLIST: file versions').cloud_list_with_file_versions(filename).execute_test().command_is_successful().command_response_should_have(filename)

        # Delete file
        MMCloudTest('MMCloudGateway DELETE: delete both local and cloud file').delete_with_delete_local_file(filename).execute_test().command_is_successful()

        # Reconcile files
        MMCloudTest('MMCloudGateway RECONCILE').reconcile_files("all").execute_test().command_is_successful()

        # CloudList: verify no stale objects present on cloud
        MMCloudTest('MMCloudGateway CLOUDLIST').cloud_list(".").execute_test().command_is_successful().command_response_should_not_have(filename)

    else:
        logging.info("== Successfully completed manual tiering tests ==\n")

def test_manual_policy_based_tiering(filesizes_in_mb, fsname):
    logging.info("\n== Started manual policy based tiering tests ==")
    migration_policy_file = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'cfg/migrate_policy.rules')
    recall_policy_file = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'cfg/recall_policy.rules')
    delete_policy_file = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'cfg/delete_policy.rules')
    m_var_identifiers = {'path_name_like' : Config.tmp_wd}      # M style variable identifiers
    fs = FSHelper()

    for size in filesizes_in_mb:
        filename = _generate_filename(size)
        filesize = _fsize_in_bytes(size)
        logging.info("Filename: %s Filesize: %sMB", filename, size)
        MMCloudTest('Create file on filesystem').create_file_on_fs(filename, filesize)
        expected_response = _generate_list_response(0, 0, 'Resident')
        MMCloudTest('MMCloudGateway LIST: before migration').list_file(filename).execute_test().command_is_successful().command_response_should_have(expected_response)

    # suspend-and-resume sequence to froce recent changes to disk
    logging.warn('Suspending filesystem: %s', fsname)
    fs.suspend_fs(fsname)
    logging.warn('Resuming filesystem: %s', fsname)
    fs.resume_fs(fsname)

    # apply migration policy
    logging.info('Applying migration policy')
    fs.apply_policy(fsname, migration_policy_file, m_var_identifiers)

    for size in filesizes_in_mb:
        filename = _generate_filename(size)
        expected_response = _generate_list_response_with_ub(0, 1, 1, 'Non-resident')
        MMCloudTest('MMCloudGateway LIST: after MIGRATION').list_file(filename).execute_test().command_is_successful().command_response_should_have(expected_response)

    # apply recall policy
    logging.info('Applying recall policy')
    fs.apply_policy(fsname, recall_policy_file, m_var_identifiers)

    for size in filesizes_in_mb:
        filename = _generate_filename(size)
        filesize = _fsize_in_bytes(size)
        expected_response = _generate_list_response_with_ub(_fsize_in_kbytes(size), 1, 1, 'Co-resident')
        MMCloudTest('MMCloudGateway LIST: after recall').list_file(filename).execute_test().command_is_successful().command_response_should_have(expected_response)

    # apply delete policy
    logging.info('Applying delete policy')
    fs.apply_policy(fsname, delete_policy_file, m_var_identifiers)

    # give some time to update cloud directory journal
    logging.warn('Waiting for cloud directory to get updated')
    logging.info("Waiting for 30 secs")
    time.sleep(30)

    # cloudList: list stale objects on cloud
    for size in filesizes_in_mb:
        filename = _generate_filename(size)
        MMCloudTest('MMCloudGateway CLOUDLIST: show stale objects').cloud_list(".").execute_test().command_is_successful().command_response_should_have(filename)

    # reconcile: delete stale entries from cloud dictionary
    MMCloudTest('MMCloudGateway RECONCILE').reconcile_files("all").execute_test().command_is_successful()

    # give some time to update cloud directory journal
    logging.info("Waiting for 10 secs")
    time.sleep(10)

    # Confirm cloud objects are deleted
    for size in filesizes_in_mb:
        filename = _generate_filename(size)
        MMCloudTest('MMCloudGateway CLOUDLIST: verify stale entries are removed from cloud dictionay').cloud_list(".").execute_test().command_is_successful().command_response_should_not_have(filename)

    logging.info("== Successfully completed manual-policy based tiering tests ==\n")

def test_auto_threshold_based_tiering(fsname):
    logging.info("\n== Started threshold based tiering tests ==")
    poolname = 'system'
    cb_identifier = 'thresholdCallback'
    policy_file = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'cfg/threshold_based_migration.rules')
    template_file = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'cfg/threshold_based_migration.rules.template')
    command_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'cfg/threshold_cmd.sh')

    fs = FSHelper()

    # get system pool usage x1 : low threshold
    low_threshold = fs.get_pool_usage(fsname, poolname) + 1
    # x2 : high threshold
    high_threshold = low_threshold + 3

    # REFACTOR: create TCT policy file
    fs.create_tct_policy_file(template_file, policy_file)

    # update TCT policy file contents
    substitutes = {'HIGH_PH' : high_threshold, 'LOW_PH': low_threshold, 'PATH_PH': Config.tmp_wd}
    fs.update_tct_policy_file(policy_file, substitutes)

    # change policy to TCT policy
    logging.info("Installing '%s' policy", policy_file)
    fs.change_policy(fsname, policy_file)

    # register callback to invoke mmapplypolicy when pool reaches high threshold value
    logging.info("Registering '%s' callback", cb_identifier)
    fs.register_callback(cb_identifier, command_path, 'lowDiskSpace', gethostname(), '%fsName')

    # restart GPFS
    logging.warn('Restarting GPFS for callback and policy to take effect')
    fs.restart_gpfs(gethostname())

    # TODO: Start TCT, if not started

    size = 0
    files = []
    while fs.get_pool_usage(fsname, poolname) <= high_threshold:
        size += 10
        filename = _generate_filename(size)
        files.append(filename)
        filesize = _fsize_in_bytes(size)
        MMCloudTest('Create file on filesystem').create_file_on_fs(filename, filesize)
        expected_response = _generate_list_response(0, 0, 'Resident')
        MMCloudTest('MMCloudGateway LIST: before migration').list_file(filename).execute_test().command_is_successful().command_response_should_have(expected_response)
    else:
        logging.warn('%s pool reached high threshold value(%s)', poolname, high_threshold)

    # wait till pool hits low_threshold value or gets timeout
    dx = high_threshold
    timeout = time.time() + 10 * 60
    while dx > low_threshold:
        if dx < high_threshold:
            logging.info('Migration is under progress!')
        elif dx == high_threshold and time.time() > timeout:
            logging.warn('Migration did not started. Aborting!')
            break
        logging.info("Waiting for 20 secs")
        time.sleep(20)
        dx = fs.get_pool_usage(fsname, poolname)
    else:
        logging.info('Migration has successfully completed')
        logging.warn('%s pool reached low threshold value(%s)', poolname, high_threshold)

    counter = 0
    for filename in reversed(files):
        fpath = os.path.join(Config.tmp_wd, filename)
        if fs.get_file_state(fpath) == 'Non-resident':
            fs.read_file(fpath)
            expected_response = _generate_list_response(1, 1, 'Co-resident')
            MMCloudTest('MMCloudGateway LIST: after recall').list_file(fpath).execute_test().command_is_successful().command_response_should_have(expected_response)
            # TODO: REPLACE WITH TRASPARENT DELETE
            MMCloudTest('MMCloudGateway DELETE: delete both local and cloud file').delete_with_delete_local_file(filename).execute_test().command_is_successful()
            counter += 1
        else:
            MMCloudTest('MMCloudGateway DELETE: remove file').delete_local_file(fpath).execute_test().command_is_successful()
    else:
        # reconcile: delete stale entries from cloud dictionary
        MMCloudTest('MMCloudGateway RECONCILE').reconcile_files("all").execute_test().command_is_successful()
        logging.info('Total %d files were migrated to cloud', counter)

    # revert back to default policy
    logging.warn("Installing 'DEFAULT' policy")
    fs.apply_default_policy(fsname)

    # delete callback
    logging.warn("Deleting '%s' callback", cb_identifier)
    fs.delete_callback(cb_identifier)

    logging.info("== Successfully completed threshold based tiering tests ==\n")

# suspend-and-resume sequence to froce recent changes to disk
def fs_flush(fsname):
    fs = FSHelper()
    logging.warn('Suspending filesystem: %s', fsname)
    fs.suspend_fs(fsname)
    logging.warn('Resuming filesystem: %s', fsname)
    fs.resume_fs(fsname)

def cleanup():
    logging.warn('Deleting temp directory and cloud gatway account created for testing')
    TCTPyCliTest('After all: teardown').teardown()

def setup_logger(log_level):
    logging.basicConfig(format = '%(levelname)s: %(message)s', level = log_level.upper())
    logging.addLevelName(50, 'CRIT') # critical
    logging.addLevelName(30, 'WARN') # warning

def read_configs(conf_file):
    with open(conf_file, 'r') as f:
        return yaml.safe_load(f)

if __name__ == '__main__':
    Config.config_file = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'cfg/cloud_config.yaml')
    cloud_config = read_configs(Config.config_file)
    setup_logger(cloud_config['log-level'])
    Config.tmp_wd = os.path.join(cloud_config['fs-root'], cloud_config['test-dir'])
    fsname = cloud_config['fs-name']
    filesizes_in_mb = cloud_config['filesizes-in-mb']

    logging.info("Configuring Cloud Gateway")
    TCTPyCliTest('Before_all').before_all()

    test_manual_tiering(filesizes_in_mb)
    fs_flush(fsname)

    test_manual_policy_based_tiering(filesizes_in_mb, fsname)
    fs_flush(fsname)

    test_auto_threshold_based_tiering(fsname)
    fs_flush(fsname)

    cleanup()
    logging.info("Successfully completed all tests!")
