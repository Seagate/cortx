import itertools
import re
from tornado.httpclient import (HTTPError as HTTPClientError)
import mimetypes
import asyncio
from tornado.ioloop import IOLoop
import time
from .constants import (NB_TYPE,UNTITLED_FILE,FOLDER_SEPERATOR,UNTITLED_NB,UNTITLED_FOLDER,CHECKPOINT_NAME)

from tornado.web import HTTPError as HTTPServerError
import base64
import boto3
from tornado.locks import Lock
from types import SimpleNamespace

import threading
import datetime
from traitlets import Dict,Unicode,Instance,TraitType,Type,default
from notebook.services.contents.manager import ContentsManager
from collections import namedtuple
from tornado import gen
import nbformat
import json 

from nbformat.v4 import new_notebook
from .cortx_authenticator import CortxAuthenticator


class MultiPartUploadHelper:

    def __init__(self, seconds):
        self._seconds = seconds
        self._store = {}

    def _remove_old_keys(self, now):
        self._store = {
            key: (expires, value)
            for key, (expires, value) in self._store.items()
            if expires > now
        }

    def __getitem__(self, key):
        now = int(time.time())
        self._remove_old_keys(now)
        return self._store[key][1]

    def __setitem__(self, key, value):
        now = int(time.time())
        self._remove_old_keys(now)
        self._store[key] = (now + self._seconds, value)

    def __delitem__(self, key):
        now = int(time.time())
        self._remove_old_keys(now)
        del self._store[key]


@gen.coroutine
def _head_object(config, bucket, key):
    credentials = yield config.cortx_authenticator()
    s3_client = boto3.client('s3', aws_access_key_id=credentials.access_key_id,
                             aws_secret_access_key=credentials.secret_access_key, region_name='us-east-1', endpoint_url= config.endpoint_url)
    response = s3_client.list_objects_v2(
        Bucket=bucket,
        Prefix=key,
        )
    for obj in response.get('Contents', []):
        if obj['Key'] == key:
            return True
    return False



@gen.coroutine
def _put_object(config, bucket, body, object_name):
    credentials = yield config.cortx_authenticator()
    s3_client = boto3.client('s3', aws_access_key_id=credentials.access_key_id,
                             aws_secret_access_key=credentials.secret_access_key, region_name='us-east-1', endpoint_url= config.endpoint_url)
    return s3_client.put_object(Body=body, Bucket=bucket, Key=object_name)

@gen.coroutine
def _get_object(config, bucket, object_name):
    credentials = yield config.cortx_authenticator()
    s3_client = boto3.client('s3', aws_access_key_id=credentials.access_key_id,
                             aws_secret_access_key=credentials.secret_access_key, region_name='us-east-1', endpoint_url= config.endpoint_url)
    return s3_client.get_object(Bucket=bucket, Key=object_name)

@gen.coroutine
def _list_buckets(config):
    # Retrieve the list of existing buckets
    credentials = yield config.cortx_authenticator()
    s3_client = boto3.client('s3', aws_access_key_id=credentials.access_key_id,
                             aws_secret_access_key=credentials.secret_access_key, region_name='us-east-1', endpoint_url = config.endpoint_url)
    return s3_client.list_buckets()

@gen.coroutine
def _copy_object(config, bucket, source_file, destination_file):
    credentials = yield config.cortx_authenticator()
    s3_client = boto3.client('s3', aws_access_key_id=credentials.access_key_id,
                             aws_secret_access_key=credentials.secret_access_key, region_name='us-east-1', endpoint_url = config.endpoint_url)
    response = yield _get_object(config, config.bucket_name, source_file)
    file_bytes = response['Body'].read()
    response = _put_object(config, config.bucket_name, file_bytes, destination_file)
    last_modified = f"{datetime.datetime.now():%a, %d %b %Y %H:%M:%S GMT}"
    return _saved_model(destination_file.split('/')[-1], 'notebook', None, last_modified)

@gen.coroutine
def _delete_object(config, bucket, file_name):
    credentials = yield config.cortx_authenticator()
    s3_client = boto3.client('s3', aws_access_key_id=credentials.access_key_id,
                             aws_secret_access_key=credentials.secret_access_key, region_name='us-east-1', endpoint_url = config.endpoint_url)
    return s3_client.delete_object(Bucket=bucket, Key=file_name)

@gen.coroutine
def _list_objects(config, bucket, prefix, delimiter, max_keys, token=''):
    credentials = yield config.cortx_authenticator()
    s3_client = boto3.client('s3', aws_access_key_id=credentials.access_key_id,
                             aws_secret_access_key=credentials.secret_access_key, region_name='us-east-1', endpoint_url = config.endpoint_url)
    if token:
        return s3_client.list_objects_v2(
            Bucket=bucket,
            Delimiter=delimiter,
            MaxKeys=max_keys,
            Prefix=prefix,
            ContinuationToken=token)
    else:
        return s3_client.list_objects_v2(
            Bucket=bucket,
            Delimiter=delimiter,
            MaxKeys=max_keys,
            Prefix=prefix)



def _get_key(config, path):
    return config.prefix + path.lstrip('/')


def _get_path(config, key):
    return '/' + key[len(config.prefix):]

def _get_full_path(key_or_path):
    return key_or_path.split('/')[-1]


@gen.coroutine
def _get_type(config, path):
    type = \
        'notebook' if path.endswith(NB_TYPE) else \
        'directory' if _check_if_root(path) or (yield _check_directory_exists(config, path)) else \
        'file'
    return type


def _get_format(config, type, path):
    type = \
        'json' if type == 'notebook' else \
        'json' if type == 'directory' else \
        'text' if mimetypes.guess_type(path)[0] == 'text/plain' else \
        'base64'
    return type


def _get_type_from_key(key):
    type = \
        'notebook' if key.endswith(NB_TYPE) else \
        'file'
    return type

@gen.coroutine
def _check_directory_exists(config, path):
    return True if _check_if_root(path) else (yield _check_file_exists(config, path + FOLDER_SEPERATOR))

def _check_if_root(path):
    is_notebook_root = path == ''
    is_lab_root = path == '/'
    return is_notebook_root or is_lab_root

@gen.coroutine
def _check_file_exists(config, path):

    @gen.coroutine
    def _check_if_key_exists():
        key = _get_key(config, path)
        try:
            response = yield _head_object(config, config.bucket_name, key)
            
        except HTTPClientError as exception:
            if exception.response.code != 404:
                raise HTTPServerError(exception.response.code, 'Error checking if S3 exists')
            response = exception.response

        return response

    return False if _check_if_root(path) else (yield _check_if_key_exists())


@gen.coroutine
def _check_if_exists(config, path):
    return (yield _check_file_exists(config, path)) or (yield _check_directory_exists(config, path))


@gen.coroutine
def _get_model(config, path, content, type, format):
    type_to_get = type if type is not None else (yield _get_type(config, path))
    format_to_get = format if format is not None else _get_format(config, type_to_get, path)
    return (yield READ_HELPERS[(type_to_get, format_to_get)](config, path, content))


@gen.coroutine
def _get_base64_file(config, path, content):
    return (yield _get_all(config, path, content, 'file', 'application/octet-stream', 'base64', lambda file_bytes: base64.b64encode(file_bytes).decode('utf-8')))


@gen.coroutine
def _get_text_file(config, path, content):
    return (yield _get_all(config, path, content, 'file', 'text/plain', 'text', lambda file_bytes: file_bytes.decode('utf-8')))


@gen.coroutine
def _get_notebook(config, path, content):
    notebook_dict = yield _get_all(config, path, content, 'notebook', None, 'json', lambda file_bytes: json.loads(file_bytes.decode('utf-8')))
    return nbformat.from_dict(notebook_dict)


@gen.coroutine
def _get_folder(config, path, content):
    key = _get_key(config, path)
    key_prefix = key if (key == '' or key[-1] == '/') else (key + '/')
    keys, directories = \
        (yield _list_current_child_files_and_folders(config, key_prefix)) if content else \
        ([], [])

    all_keys = {key for (key, _) in keys}

    return {
        'name': _get_full_path(path),
        'path': path,
        'type': 'directory',
        'mimetype': None,
        'writable': True,
        'last_modified': datetime.datetime.fromtimestamp(86400),
        'created': datetime.datetime.fromtimestamp(86400),
        'format': 'json' if content else None,
        'content': ([
            {
                'type': 'directory',
                'name': _get_full_path(directory),
                'path': _get_path(config, directory),
            }
            for directory in directories
            if directory not in all_keys
        ] + [
            {
                'type': _get_type_from_key(key),
                'name': _get_full_path(key),
                'path': _get_path(config, key),
                'last_modified': last_modified,
            }
            for (key, last_modified) in keys
            if not key.endswith(FOLDER_SEPERATOR)
        ]) if content else None
    }


@gen.coroutine
def _save_model(config, model, path):
    save_type = model['type'] if 'type' in model else (yield _get_type(config, path))
    save_format = model['format'] if 'format' in model else _get_format(
        config, save_type, path)
    return (yield PUT_HELPERS[(save_type, save_format)](
        config,
        model['chunk'] if 'chunk' in model else None,
        model['content'] if 'content' in model else None,
        path,
    ))


@gen.coroutine
def _save_base64_file(config, chunk, content, path):
    return (yield _save_all(config, chunk, base64.b64decode(content.encode('utf-8')), path, 'file', 'application/octet-stream'))

@gen.coroutine
def _save_text_file(config, chunk, content, path):
    return (yield _save_all(config, chunk, content.encode('utf-8'), path, 'file', 'text/plain'))



@gen.coroutine
def _save_folder(config, chunk, content, path):
    return (yield _save_all(config, chunk, b'', path + FOLDER_SEPERATOR, 'directory', None))


@gen.coroutine
def _save_notebook(config, chunk, content, path):
    return (yield _save_all(config, chunk, json.dumps(content).encode('utf-8'), path, 'notebook', None))


@gen.coroutine
def _save_all(config, chunk, content_bytes, path, type, mimetype):

    response = \
        (yield _save_file_bytes(config, content_bytes, path, type, mimetype)) if chunk is None else \
        (yield _save_cells(config, chunk, content_bytes, path, type, mimetype))

    return response


@gen.coroutine
def _save_cells(config, chunk, content_bytes, path, type, mimetype):

    if chunk == 1:
        config.multipart_uploads[path] = []
    config.multipart_uploads[path].append(content_bytes)

    if chunk == -1:
        combined_bytes = b''.join(config.multipart_uploads[path])
        del config.multipart_uploads[path]
        return (yield _save_file_bytes(config, combined_bytes, path, type, mimetype))
    else:
        return _saved_model(path, type, mimetype, datetime.datetime.now())

@gen.coroutine
def _save_file_bytes(config, content_bytes, path, type, mimetype):

    object_name = _get_key(config, path)
    response = _put_object(config, config.bucket_name, content_bytes, object_name)
    last_modified = f"{datetime.datetime.now():%a, %d %b %Y %H:%M:%S GMT}"

    return _saved_model(path, type, mimetype, last_modified)


def _saved_model(path, type, mimetype, last_modified):
    return {
        'name': _get_full_path(path),
        'path': path,
        'type': type,
        'mimetype': mimetype,
        'writable': True,
        'last_modified': last_modified,
        'created': last_modified,
        'format': None,
        'content': None,
    }


@gen.coroutine
def _get_all(config, path, content, type, mimetype, format, decode):
    method = 'GET' if content else 'HEAD'
    key = _get_key(config, path)
    response = yield _get_object(config, config.bucket_name, key)
    file_bytes = response['Body'].read()
    last_modified_str = response['LastModified']
    if not isinstance(last_modified_str, str):
        last_modified = f"{datetime.datetime.now():%a, %d %b %Y %H:%M:%S GMT}"
    else:
        last_modified = datetime.datetime.strptime(
        last_modified_str, "%Y-%m-%dT%H:%M:%S.%fZ")

    return {
        'name': _get_full_path(path),
        'path': path,
        'type': type,
        'mimetype': mimetype,
        'writable': True,
        'last_modified': last_modified,
        'created': last_modified,
        'format': format if content else None,
        'content': decode(file_bytes) if content else None,
    }



def _get_checkpoint_path(path, checkpoint_id):
    return path + '/' + CHECKPOINT_NAME + '/' + checkpoint_id


@gen.coroutine
def _get_next_filename(config, filename, path='', insert=''):
    basename, dot, ext = filename.partition('.')
    suffix = dot + ext

    for i in itertools.count():
        insert_i = f'{insert}{i}' if i else ''
        name = f'{basename}{insert_i}{suffix}'
        if not (yield _check_if_exists(config, f'/{path}/{name}')):
            break
    return name


@gen.coroutine
def _create_new_checkpoint(config, path):
    model = yield _get_model(config, path, content=True, type=None, format=None)
    type = model['type']
    content = model['content']
    format = model['format']

    checkpoint_id = str(int(time.time() * 1000000))
    checkpoint_path = _get_checkpoint_path(path, checkpoint_id)
    yield PUT_HELPERS[(type, format)](config, None, content, checkpoint_path)
    # This is a new object, so shouldn't be any eventual consistency issues
    checkpoint = yield READ_HELPERS[(type, format)](config, checkpoint_path, False)
    return {
        'id': checkpoint_id,
        'last_modified': checkpoint['last_modified'],
    }

@gen.coroutine
def _list_all_checkpoints(config, path):
    key_prefix = _get_key(config, path + '/' + CHECKPOINT_NAME + '/')
    keys, _ = yield _list_current_child_files_and_folders(config, key_prefix)
    return [
        {
            'id': key[(key.rfind('/' + CHECKPOINT_NAME + '/') + len('/' + CHECKPOINT_NAME + '/')):],
            'last_modified': last_modified,
        }
        for key, last_modified in keys
    ]

@gen.coroutine
def _rename_notebook(config, old_path, new_path):
    if (not (yield _check_if_exists(config, old_path))):
        raise HTTPServerError(400, "Source does not exist")

    if (yield _check_if_exists(config, new_path)):
        raise HTTPServerError(400, "Target already exists")

    type = yield _get_type(config, old_path)
    old_key = _get_key(config, old_path)
    new_key = _get_key(config, new_path)

    def replace_key_prefix(string):
        return new_key + string[len(old_key):]

    object_key = \
        [] if type == 'directory' else \
        [(old_key, new_key)]

    renames = object_key + [
        (key, replace_key_prefix(key))
        for (key, _) in (yield _list_all_successor_keys(config, old_key + '/'))
    ]

    for (old_key, new_key) in object_key + sorted(renames, key=lambda k: _get_copy_order_key(k[0])):
        yield _copy_key_object(config, old_key, new_key)

    for (old_key, _) in sorted(renames, key=lambda k: _delete_order_key(k[0])) + object_key:
        yield _delete_key_object(config, old_key)

    return (yield _get_model(config, new_path, content=False, type=None, format=None))


@gen.coroutine
def _new_untitled_notebook(config, path, type, ext):
    if not (yield _check_directory_exists(config, path)):
        raise HTTPServerError(404, 'No such directory: %s' % path)

    model_type = \
        type if type else \
        'notebook' if ext == '.ipynb' else \
        'file'

    untitled = \
        UNTITLED_FOLDER if model_type == 'directory' else \
        UNTITLED_NB if model_type == 'notebook' else \
        UNTITLED_FILE
    insert = \
        ' ' if model_type == 'directory' else \
        ''
    ext = \
        '.ipynb' if model_type == 'notebook' else \
        ext

    name = yield _get_next_filename(config, untitled + ext, path, insert=insert)
    path = u'{0}/{1}'.format(path, name)

    model = {
        'type': model_type,
    }
    return (yield _get_new_notebook(config, model, path))



def _run_sync(func):
    result = None
    exception = None

    def _func():
        nonlocal result
        nonlocal exception
        asyncio.set_event_loop(asyncio.new_event_loop())
        try:
            result = IOLoop.current().run_sync(func)
        except BaseException as _exception:
            exception = _exception

    thread = threading.Thread(target=_func)
    thread.start()
    thread.join()

    if exception is not None:
        raise exception
    else:
        return result

@gen.coroutine
def _delete_notebook(config, path):
    if not path:
        raise HTTPServerError(400, "Can't delete root")

    type = _get_type(config, path)
    root_key = _get_key(config, path)

    object_key = \
        [] if type == 'directory' else \
        [root_key]

    descendant_keys = [
        key
        for (key, _) in (yield _list_all_successor_keys(config, root_key + '/'))
    ]

    for key in sorted(descendant_keys, key=_delete_order_key) + object_key:
        yield _delete_key_object(config, key)


@gen.coroutine
def _get_checkpoint_model(config, type, checkpoint_id, path):
    format = _get_format(config, type, path)
    checkpoint_path = _get_checkpoint_path(path, checkpoint_id)
    return (yield READ_HELPERS[(type, format)](config, checkpoint_path, True))


@gen.coroutine
def _delete_notebook_checkpoint(config, checkpoint_id, path):
    checkpoint_path = _get_checkpoint_path(path, checkpoint_id)
    yield _delete_notebook(config, checkpoint_path)


@gen.coroutine
def _list_current_child_files_and_folders(config, key_prefix):
    return (yield _get_all_keys(config, key_prefix, '/'))

@gen.coroutine
def _list_all_successor_keys(config, key_prefix):
    return (yield _get_all_keys(config, key_prefix, ''))[0]

@gen.coroutine
def _restore_notebook_checkpoint(config, checkpoint_id, path):
    type = (yield _get_model(config, path, content=False, type=None, format=None))['type']
    model = yield _get_checkpoint_model(config, type, checkpoint_id, path)
    yield _save_model(config, model, path)


@gen.coroutine
def _get_new_notebook(config, model, path):
    if model is None:
        model = {}

    model.setdefault('type', 'notebook' if path.endswith('.ipynb') else 'file')

    if 'content' not in model and model['type'] == 'notebook':
        model['content'] = new_notebook()
        model['format'] = 'json'
    elif 'content' not in model and model['type'] == 'file':
        model['content'] = ''
        model['format'] = 'text'

    return (yield _save_model(config, model, path))



@gen.coroutine
def _copy_notebook(config, from_path, to_path):
    model = yield _get_model(config, from_path, content=False, type=None, format=None)
    if model['type'] == 'directory':
        raise HTTPServerError(400, "Can't copy directories")

    from_dir, from_name = \
        from_path.rsplit('/', 1) if '/' in from_path else \
        ('', from_path)

    to_path = \
        to_path if to_path is not None else \
        from_dir

    if (yield _check_directory_exists(config, to_path)):
        copy_pat = re.compile(r'\-Copy\d*\.')
        name = copy_pat.sub(u'.', from_name)
        to_name = yield _get_next_filename(config, name, to_path, insert='-Copy')
        to_path = u'{0}/{1}'.format(to_path, to_name)

    from_key = _get_key(config, from_path)
    to_key = _get_key(config, to_path)

    yield _copy_key_object(config, from_key, to_key)
    return {
        **model,
        'name': to_name,
        'path': to_path,
    }


@gen.coroutine
def _get_all_keys(config, key_prefix, delimiter):

    max_keys = 1000

    @gen.coroutine
    def _list_first_page():
        response = yield _list_objects(config, config.bucket_name, key_prefix, delimiter, max_keys)
        return _parse_list_response(response)

    @gen.coroutine
    def _list_later_page(token):
        response = yield _list_objects(config, config.bucket_name, key_prefix, delimiter, max_keys, token)

        return _parse_list_response(response)

    def _parse_list_response(response):
        next_token = ''
        keys = []
        directories = []
        if 'Contents' in response:
            for item in response['Contents']:
                key = item['Key']
                last_modified_str = item['LastModified']
                if not isinstance(last_modified_str, str):
                    last_modified = f"{datetime.datetime.now():%a, %d %b %Y %H:%M:%S GMT}"
                else:
                    last_modified = datetime.datetime.strptime(
                    last_modified_str, "%Y-%m-%dT%H:%M:%S.%fZ")
                keys.append((key, last_modified))
        if 'CommonPrefixes' in response:
            # Prefixes end in '/', which we strip off
            for item in response['CommonPrefixes']:
                directories.append(item['Prefix'][:-1])
        if 'NextContinuationToken' in response:
            next_token = response['NextContinuationToken']
        return (next_token, keys, directories)

    token, keys, directories = yield _list_first_page()

    while token:
        token, keys_page, directories_page = yield _list_later_page(token)
        keys.extend(keys_page)
        directories.extend(directories_page)

    return keys, directories

READ_HELPERS = {
('directory', 'json'): _get_folder,
('notebook', 'json'): _get_notebook,
('file', 'text'): _get_text_file,
('file', 'base64'): _get_base64_file
}

PUT_HELPERS = {
   ('file', 'base64'):  _save_base64_file,
    ('notebook', 'json'): _save_notebook,
    ('directory', 'json'): _save_folder,
    ('file', 'text'): _save_text_file
}

@gen.coroutine
def _delete_key_object(config, key):
    yield _delete_object(config, config.bucket_name, key)


@gen.coroutine
def _copy_key_object(config, old_key, new_key):
    yield _copy_object(config, config.bucket_name, old_key, new_key)


def _get_copy_order_key(key):
    return (key.count('/'), 0 if key.endswith(FOLDER_SEPERATOR) else 1)

def _delete_order_key(key):
    return tuple(-1 * key_i for key_i in _get_copy_order_key(key))


