import os
import yaml
import re
import subprocess
import logging

class CloudGatewaySetup:
    def __init__(self):
        self.cfg_dir = os.path.join(os.path.dirname(__file__), 'cfg')
        cloud_config_file = os.path.join(self.cfg_dir, 'cloud_config.yaml')
        with open(cloud_config_file, 'r') as f:
            self.cloud_config = yaml.safe_load(f)
        self.__create_secret_key_file()

    def __create_secret_key_file(self):
        pwd_file_name = self.cloud_config['pwd-file']
        secret_key = self.cloud_config['secret-key']

        if secret_key ==  None:
            raise AssertionError("Secret key is required.")
        # create temp pwd file if not provided
        if pwd_file_name == None:
            pwd_file_name = 'secret_key_file'

        self.cloud_config['pwd-file'] = os.path.join(self.cfg_dir, pwd_file_name)
        with open(self.cloud_config['pwd-file'], 'w') as f:
            f.write(secret_key)

    def service_start(self):
        cmd = ['mmcloudgateway', 'service',  'start']
        return self.__execute(cmd)

    def service_stop(self):
        cmd = ['mmcloudgateway', 'service', 'stop']
        return self.__execute(cmd)

    def service_status(self):
        cmd = ['mmcloudgateway', 'service', 'status']
        self.__log_cmd(cmd)
        out_bytes = subprocess.check_output(cmd)
        out_str = out_bytes.decode()
        logging.debug(out_str.strip())
        pattern = re.compile('Started|Suspended|Stopped')
        result = pattern.search(out_str)
        if result:
            return result.group()

    def account_create(self):
        cmd = ['mmcloudgateway', 'account', 'create',
                '--cloud-nodeclass', self.cloud_config['cloud-nodeclass'],
                '--cloud-name', self.cloud_config['cloud-name'],
                '--cloud-type', self.cloud_config['cloud-type'],
                '--username', self.cloud_config['username'],
                '--pwd-file', os.path.join(self.cfg_dir, self.cloud_config['pwd-file']),
                '--enable', str(self.cloud_config['enable']),
                '--cloud-url', self.cloud_config['cloud-url'],
                '--mpu-parts-size', str(self.cloud_config['mpu-parts-size']),
                '--enc-enable', str(self.cloud_config['enc-enable']),
                '--etag-enable', str(self.cloud_config['etag-enable'])]

        if self.cloud_config['cloud-type'].upper() == 'S3':
            cmd.extend(['--location', self.cloud_config['location']])
            if self.cloud_config['server-cert-path'] != None:
                cmd.extend(['--server-cert-path', self.cloud_config['server-cert-path']])
        rc = self.__execute(cmd)
        if rc == 0: logging.info("TCT cloud gateway account has been successfully created.")

        return rc

    def account_test(self):
        cmd = ['mmcloudgateway', 'account', 'test',
                '--cloud-nodeclass', self.cloud_config['cloud-nodeclass'],
                '--cloud-name', self.cloud_config['cloud-name']]
        return self.__execute(cmd)

    def account_delete(self):
        cmd = ['mmcloudgateway', 'account', 'delete',
                '--cloud-nodeclass', self.cloud_config['cloud-nodeclass'],
                '--cloud-name', self.cloud_config['cloud-name']]
        rc = self.__execute(cmd)
        if rc == 0: logging.warn("TCT cloud gateway account has been successfully deleted.")

        return rc

    def account_list(self):
        cmd = ['mmcloudgateway', 'account', 'list',
                '--cloud-nodeclass', self.cloud_config['cloud-nodeclass'],
                '--cloud-name', self.cloud_config['cloud-name']]
        self.__log_cmd(cmd)
        out_bytes = subprocess.check_output(cmd)
        logging.debug(out_bytes.decode().strip())

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
