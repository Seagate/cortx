
from tornado.locks import Lock
import datetime
from traitlets import Unicode,Instance,TraitType,Type,default
from notebook.services.contents.manager import ContentsManager
from collections import namedtuple
from tornado import gen
from .cortx_authenticator import CortxAuthenticator
from .utils import (
    _run_sync,MultiPartUploadHelper,_check_directory_exists,_check_file_exists,_get_model,_save_model,_delete_notebook,_rename_notebook,_new_untitled_notebook,_get_new_notebook,_copy_notebook,_create_new_checkpoint,_restore_notebook_checkpoint,_list_all_checkpoints,_delete_notebook_checkpoint
    )
from .utils import *
import json 

import nbformat

Config = namedtuple('Config', [
    
 'prefix', 'region', 'bucket_name', 'host_name', 'cortx_authenticator',
    'multipart_uploads', 'endpoint_url'
])

class CortxJupyter(ContentsManager):

    bucket_name = Unicode(config=True)
    host_name = Unicode(config=True)
    region_name = Unicode(config=True)
    prefix = Unicode(config=True)
    endpoint_url = Unicode(config=True)

    authentication_class = Type(CortxAuthenticator, config=True)
    authentication = Instance(CortxAuthenticator)

    @default('authentication')
    def _default_authentication(self):
        return self.authentication_class(parent=self)

    checkpoints_class = None
    write_lock = Instance(Lock)

    @default('write_lock')
    def _write_lock_default(self):
        return Lock()

    multipart_uploads = Instance(MultiPartUploadHelper)

    @default('multipart_uploads')
    def _multipart_uploads_default(self):
        return MultiPartUploadHelper(60 * 60 * 1)

    def is_hidden(self, path):
        return False

    def dir_exists(self, path):

        @gen.coroutine
        def dir_exists_async():
            c = yield self._config()
            return (yield _check_directory_exists(c, path))

        return _run_sync(dir_exists_async)

    def file_exists(self, path):

        @gen.coroutine
        def file_exists_async():
            c = yield self._config()
            return (yield _check_file_exists(c, path))

        return _run_sync(file_exists_async)

    def get(self, path, content=True, type=None, format=None):

        @gen.coroutine
        def get_async():
            c = yield self._config()
            return (yield _get_model(c, path, content, type, format))

        return _run_sync(get_async)

    @gen.coroutine
    def save(self, model, path):
        with (yield self.write_lock.acquire()):
            c = yield self._config()
            return (yield _save_model(c, model, path))

    @gen.coroutine
    def delete(self, path):
        with (yield self.write_lock.acquire()):
            c = yield self._config()
            yield _delete_notebook(c, path)

    @gen.coroutine
    def update(self, model, path):
        with (yield self.write_lock.acquire()):
            c = yield self._config()
            return (yield _rename_notebook(c, path, model['path']))

    @gen.coroutine
    def new_untitled(self, path='', type='', ext=''):
        with (yield self.write_lock.acquire()):
            c = yield self._config()
            return (yield _new_untitled_notebook(c, path, type, ext))

    @gen.coroutine
    def new(self, model, path):
        with (yield self.write_lock.acquire()):
            c = yield self._config()
            return (yield _get_new_notebook(c, model, path))

    @gen.coroutine
    def copy(self, from_path, to_path):
        with (yield self.write_lock.acquire()):
            c = yield self._config()
            return (yield _copy_notebook(c, from_path, to_path))

    @gen.coroutine
    def create_checkpoint(self, path):
        with (yield self.write_lock.acquire()):
            c = yield self._config()
            return (yield _create_new_checkpoint(c, path))

    @gen.coroutine
    def restore_checkpoint(self, checkpoint_id, path):
        with (yield self.write_lock.acquire()):
            c = yield self._config()
            return (yield _restore_notebook_checkpoint(c, checkpoint_id, path))

    @gen.coroutine
    def list_checkpoints(self, path):
        c = yield self._config()
        return (yield _list_all_checkpoints(c, path))

    @gen.coroutine
    def delete_checkpoint(self, checkpoint_id, path):
        with (yield self.write_lock.acquire()):
            c = yield self._config()
            return (yield _delete_notebook_checkpoint(c, checkpoint_id, path))

    @gen.coroutine
    def _config(self):
        authenticator = yield self.authentication.get_credentials()
        creds = {
            "region_name":self.region_name,
            "bucket_name":self.bucket_name,
            "host_name":self.host_name,
            "cortx_authenticator":authenticator._asdict(),
            "prefix":self.prefix,
            "endpoint_url":self.endpoint_url
        }

        with open('credentials.json',"w") as fp:
            json.dump(creds,fp)

        return Config(
            region=self.region_name,
            bucket_name=self.bucket_name,
            host_name=self.host_name,
            cortx_authenticator=self.authentication.get_credentials,
            prefix=self.prefix,
            multipart_uploads=self.multipart_uploads,
            endpoint_url=self.endpoint_url
        )

