import os
import yaml
from subprocess import call

class LdapSetup:
    def __init__(self):
        self.test_data_dir = os.path.join(os.path.dirname(__file__), 'test_data')

        ldap_config_file = os.path.join(self.test_data_dir, 'ldap_config.yaml')
        with open(ldap_config_file, 'r') as f:
            self.ldap_config = yaml.safe_load(f)

    def ldap_init(self):
        ldap_init_file = os.path.join(self.test_data_dir, 'create_test_data.ldif')
        cmd = "ldapadd -h %s -p %s -w %s -x -D %s -f %s" % (self.ldap_config['host'],
                self.ldap_config['port'], self.ldap_config['password'],
                self.ldap_config['login_dn'], ldap_init_file)
        obj = call(cmd, shell=True)

    def ldap_delete_all(self):
        ldap_delete_file = os.path.join(self.test_data_dir, 'clean_test_data.ldif')
        cmd = "ldapdelete -r -h %s -p %s -w %s -x -D %s -f %s" % (self.ldap_config['host'],
                self.ldap_config['port'], self.ldap_config['password'],
                self.ldap_config['login_dn'], ldap_delete_file)
        obj = call(cmd, shell=True)
