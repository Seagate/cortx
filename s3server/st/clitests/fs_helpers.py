import os
import re
import sys
import time
import subprocess
import logging
from threading import Timer
from shutil import copyfile

class FSHelper:
    def __init__(self, description = ''):
        pass

    # Unmounts all GPFS filesystem and stops GPFS daemons on given node
    def shutdown_node(self, node):
        cmd = ['mmshutdown', '-N', node]
        rc = self.__execute(cmd)
        if rc == 0: return rc
        raise AssertionError("Failed to stop GPFS on %s" % node)

    # Start GPFS subsystem on given node
    def start_gpfs(self, node):
        cmd = ['mmstartup', '-N', node]
        rc = self.__execute(cmd)
        if rc == 0: return rc
        raise AssertionError("Failed to start GPFS on %s" % node)

    # Restart GPFS subsytem
    def restart_gpfs(self, node):
        self.shutdown_node(node)
        time.sleep(30)
        self.start_gpfs(node)
        time.sleep(30)

    # Suspend FS
    def suspend_fs(self, fsname):
        cmd = ['mmfsctl', fsname, 'suspend']
        rc = self.__execute(cmd)
        if rc == 0: return rc
        raise AssertionError("Failed to suspend GPFS on %s" % node)

    # Resume FS
    def resume_fs(self, fsname):
        cmd = ['mmfsctl', fsname, 'resume']
        rc = self.__execute(cmd)
        if rc == 0: return rc
        raise AssertionError("Failed to resume GPFS on %s" % node)

    # Deletes files, migrate files between storage pools as directed by policy rules
    def apply_policy(self, fsname, policyfile, m_var_identifiers):
        cmd = ['mmapplypolicy', fsname, '-P', policyfile]
        if logging.getLogger().isEnabledFor(10):
            cmd.extend(['-L', '6'])
        if len(m_var_identifiers) != 0:
            for key, value in m_var_identifiers.items():
                cmd.append('-M')
                cmd.append("%s=%s" % (key, value))
        rc = self.__execute(cmd)
        if rc == 0: return rc
        raise AssertionError("Failed to apply %s policy" % policyfile)

    # Establish policy rules for a GPFS file system using policyfile
    def change_policy(self, fsname, policyfile):
        cmd = ['mmchpolicy', fsname, policyfile]
        rc = self.__execute(cmd)
        if rc == 0: return rc
        raise AssertionError("Failed to install %s policy" % policyfile)

    # Register a user-defined command that GPFS will execute when certain events occur
    def register_callback(self, cb_identifier, command_path, event, node, parms):
        cmd = ['mmaddcallback', cb_identifier, '--command', command_path, '--event', event,
                '-N', node, '--parms', parms]
        rc = self.__execute(cmd)
        if rc == 0: return rc
        raise AssertionError("Failed to register %s callback" % cb_identifier)

    # Delete callback
    def delete_callback(self, cb_identifier):
        cmd = ['mmdelcallback', cb_identifier]
        rc = self.__execute(cmd)
        if rc == 0: return rc
        raise AssertionError("Failed to delete %s callback" % cb_identifier)

    # Install default policy
    def apply_default_policy(self, device):
        cmd = ['mmchpolicy', device, 'DEFAULT']
        rc = self.__execute(cmd)
        if rc == 0: return rc
        raise AssertionError("Failed to install default policy")

    # Get GPFS usage in percentage
    def get_gpfs_usage(self, device):
        cmd = ['df', device]
        values = subprocess.check_output(cmd).decode().strip().split('\n')[1]
        return int(values.split()[4].replace('%', ''))

    # Return pool usage in % : using mmlspool command
    def get_pool_usage(self, fsname, poolname):
        cmd = ['mmlspool', fsname]
        out_bytes = subprocess.check_output(cmd)
        out_list = out_bytes.decode().strip().split('\n')

        for row in out_list:
            if row.strip().startswith(poolname):
                ele = row.split()
                return 100 - ((int(ele[7]) / int(ele[6])) * 100)
        raise AssertionError("Invalid pool name" % poolname)

    # Return pool usage in % : using mmdf command
    def get_pool_usage_mmdf(self, fsname, poolname):
        cmd = ['mmdf', fsname, '-P', poolname]
        info  = subprocess.check_output(cmd).decode().strip().split('\n')[-1]
        return (100 - int(info.split()[5].replace('%)', '')))

    def get_file_state(self, fpath):
        cmd = ['mmcloudgateway', 'files', 'list', fpath]
        self.__log_cmd(cmd)
        out_bytes = subprocess.check_output(cmd)
        out_str = out_bytes.decode()
        logging.debug(out_str.strip())
        pattern = re.compile('Resident|Non-resident|Co-resident')
        result = pattern.search(out_str)
        if result:
            return result.group().strip()

    # Create a file on GPFS
    def create_file_on_fs(self, filename, filesize):
        with open(filename, 'wb') as fout:
            fout.write(os.urandom(filesize))
            fout.flush()

    # Modify/Update on file(final size = x + dx)
    def update_file_on_fs(self, filename, update_size):
        with open(filename, 'ab') as fout:
            fout.write(os.urandom(update_size))

    # Read file
    def read_file(self, filename):
        with open(filename, 'rb') as f:
            file_contents = f.read()
        return file_contents

    # Delete file(stub) on GPFS
    def delete_local_file(self, filename):
        os.remove(filename)

    def update_tct_policy_file(self, policy_file, substitutes):
        for k, v in substitutes.items():
            if type(v) == str:
                v = v.replace('/', '\/')
            expr = "s/%s/%s/g" % (k, v)
            cmd = ['sed', '-i', expr, policy_file]
            self.__execute(cmd)

    def create_tct_policy_file(self, src, dest):
        copyfile(src, dest)

    def __execute(self, cmd):
        # in debug mode, do not supress stdout and stderr messages
        if logging.getLogger().isEnabledFor(10):
            self.__log_cmd(cmd)
            rc = subprocess.call(cmd, stderr=subprocess.STDOUT)
        else:
            rc = subprocess.call(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        return rc

    def __log_cmd(self, cmd):
        logging.debug(' '.join(cmd))
