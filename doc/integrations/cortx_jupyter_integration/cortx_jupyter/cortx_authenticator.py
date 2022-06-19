from traitlets.config.configurable import Configurable
from tornado import gen
from traitlets import Unicode
from collections import namedtuple


CredentialConfig = namedtuple('CredentialConfig', [
    'access_key_id', 'secret_access_key'
])

class CortxAuthenticator(Configurable):
    access_key_id = Unicode(config=True)
    secret_access_key = Unicode(config=True)

    @gen.coroutine
    def get_credentials(self):
        return CredentialConfig(
            access_key_id=self.access_key_id,
            secret_access_key=self.secret_access_key,
        )
