"""
cortx_dicom

Core components
"""


# pylint: disable=too-many-lines
#           I.  Docstring and code together consumes a lot of lines of code.
#          II.  We try to keep similar or related classes in the same file.
#         III.  The working code may be under the 1000 lines limit, but we
#               think, docstring with notations is useful to anybody to
#               understand our code and the way of our thinking.


# Imports form standard library
from importlib.resources import read_binary
import json
from os import devnull
from os.path import isfile
import pickle
from random import randint

# Imports from additional dependencies
from boto3 import client
from elasticsearch import Elasticsearch
import pydicom

# Import self
from . import dicomfields


class Config:
    """
    Static class to provide configuration for CortxDicom
    ====================================================
    """


    # Constants to represent config file save and load methods
    FILE_JSON = 'json'
    FILE_PICKLE = 'pickle'


    # Privae variable to hold config data in a secure way
    __config = {}


    @classmethod
    def get(cls, key : str = '') -> any:
        """
        Get configuration
        =================

        Parameters
        ----------
        key : str, optional (empty string if omitted)
            The key to get. If key is empty string, the whole configuration is
            returned.

        Returns
        -------
        str | dict
            If key is not an empty string, the concerning value is returned. If
            key is empty, a copy of the whole configuration dict is returned.

        Raises
        ------
        KeyError
            When key is given but it doesn't exist.
        """

        if key == '':
            result = dict(cls.__config)
        elif key in cls.__config.keys():
            result = cls.__config[key]
        else:
            raise KeyError('Config.get_config(): non-existing key.')
        return result


    @classmethod
    def is_set(cls, key : str) -> bool:
        """
        Get whether a key is set or not
        ===============================

        Returns
        -------
        bool
            True if the key is set, false if not.
        """

        return key in cls.__config.keys()


    @classmethod
    def load(cls, filename : str, method : str = '', **kwargs):
        """
        Load configuration from file
        ============================

        Parameters
        ----------
        filename : str
            Name of the file to load.
        method : str, optional (empty string if omitted)
            Method to handle file. It can be Config.FILE_JSON or
            Config.FILE_PICKLE. If an empty string is given, Config.FILE_PICKLE
            is considered to use as load method.
        keyword arguments
            Arguments to forward to json.load() or pickle.load() funtions.

        Raises
        ------
        ValueError
            When unsupported storage method is set.
        FileNotFoundError
            When the name if the file cannot be interpreted as existing file.
        TypeError
            When the loaded instance is not a dictionary.
        """

        if method == '':
            _method = Config.FILE_PICKLE
        else:
            _method = method
        if _method not in [Config.FILE_JSON, Config.FILE_PICKLE]:
            raise ValueError('Config.load_config(): unsupported method.')
        if not isfile(filename):
            raise FileNotFoundError('Config.load_config(): non existing file.')
        if _method == Config.FILE_JSON:
            with open(filename, 'r') as instream:
                _data = json.load(instream, **kwargs)
        elif _method == Config.FILE_PICKLE:
            with open(filename, 'rb') as instream:
                _data = pickle.load(instream, **kwargs)
        if not isinstance(_data, dict):
            raise TypeError('Config.load_config(): config data must be ' +
                            'instance of dict.')
        cls.__config = _data


    @classmethod
    def save(cls, filename : str, method : str = '', **kwargs):
        """
        Save configuration to file
        ==========================

        Parameters
        ----------
        filename : str
            Name of the file to save.
        method : str, optional (empty string if omitted)
            Method to handle file. It can be Config.FILE_JSON or
            Config.FILE_PICKLE. If an empty string is given, Config.FILE_PICKLE
            is considered to use as save method.
        keyword arguments
            Arguments to forward to json.dump() or pickle.dump() funtions.

        Raises
        ------
        ValueError
            When unsupported storage method is set.

        Notes
        -----
            If you want to use Config.FILE_JSON each value in the configuration
            dictionary must be serializable with JSON.
        """

        if method == '':
            _method = Config.FILE_PICKLE
        else:
            _method = method
        if _method not in [Config.FILE_JSON, Config.FILE_PICKLE]:
            raise ValueError('Config.save_config(): unsupported method.')
        if _method == Config.FILE_JSON:
            with open(filename, 'w') as outstream:
                json.dump(cls.__config, outstream, **kwargs)
        elif _method == Config.FILE_PICKLE:
            with open(filename, 'wb') as outstream:
                pickle.dump(cls.__config, outstream, **kwargs)


    @classmethod
    def set(cls, key : str, value : any) -> dict:
        """
        Set configuration
        =================

        Parameters
        ----------
        key : str
            The key to set.
        value : any
            The value to set.

        Notes
        -----
            If you would like to use Config.FILE_JSON as handle method for
            load() or save() functions, value must be serializable with JSON.
        """

        cls.__config[key] = value


class CortxDicom:
    """
    Provides easy-to-use way to store and retrieve DICOM files in cortx
    ===================================================================

    Attributes:
    es_engine : Elasticsearch (access-only)
        Provide direct access to the Elasticsearch engine.
    s3_engine : S3 (S3.botocore.client) (access-only)
        Provide direct access to the boto3 S3 engine.
    """


    # Constant to provide the default content type
    CONTENT_TYPE = 'application/dicom'

    # Constant to provide the default index to store DICOM metadata
    ES_DEFAULT_INDEX = 'cortx_dicom'

    # Constants to be applied as switches at label filtering
    FILTER_NO_FILTER = 0
    FILTER_PRIVATE = 1
    FILTER_HIPAA = 2
    FILTER_GDPR = 4

    # Keys of labels
    KEY_SCHEME_HUMAN_READABLE = 1
    KEY_SCHEME_NUMERIC = 0
    KEY_SCHEME_SPACELESS = 2


    # Dictionary to hold labeling information
    __labels = {}


    def __init__(self, es_engine : any = None, s3_engine : any = None):
        """
        Initialize an instance of the object
        ====================================

        Parameters
        ----------
        es_engine : Elasticsearch, optional (None if omitted)
            An existing Elasticsearch instance. If None is given, a new
            Elasticsearch instance is created.
        s3_engine : S3 (S3.botocore.clinet), optional (None if omitted)
            An existing S3 instance. If None is given, a new S3 instance is
            created.

        Raises
        ------
        RuntimeError
            When creating es_engine from Config is not possible due to lack of
            needed keys.
        TypeError
            When a not supported type of instance is given as es_engine.
        RuntimeError
            When creating s3_engine from Config is not possible due to lack of
            needed keys.
        TypeError
            When a not supported type of instance is given as s3_engine.
        """

        if es_engine is None:
            if not Config.is_set('es.url'):
                raise RuntimeError('CortxDicom.init(): cennot create  ' +
                                   'elasticsearch engine from configuration.')
            _es_dict = {}
            if Config.is_set('es.http_compress'):
                _es_dict['http_compress'] = Config.get('es.http_compress')
            if Config.is_set('es.port'):
                _es_dict['port'] = Config.get('es.port')
            if Config.is_set('es.scheme'):
                _es_dict['scheme'] = Config.get('es.scheme')
            self.__es_engine = Elasticsearch(Config.get('es.url'), **_es_dict)
        elif isinstance(es_engine, Elasticsearch):
            self.__es_engine = es_engine
        else:
            raise TypeError('CortxDicom.init(): es_engine must be NoneType or' +
                            ' Elasticsearch instance.')
        if s3_engine is None:
            if not all([Config.is_set('s3.acces_key_id'),
                        Config.is_set('s3.sercret_access_key'),
                        Config.is_set('s3.url')]):
                raise RuntimeError('CortxDicom.init(): cennot create S3 ' +
                                   'engine from configuration.')
            self.__s3_engine = client('s3', endpoint_url=Config.get('s3.url'),
                                      aws_access_key_id=
                                      Config.get('s3.acces_key_id'),
                                      aws_secret_access_key=
                                      Config.get('s3.sercret_access_key'))
        elif getattr(getattr(s3_engine, '__class__'), '__name__') == 'S3' and \
             getattr(getattr(s3_engine, '__class__'), '__module__') == \
             'botocore.client':
            self.__s3_engine = s3_engine
        else:
            raise TypeError('CortxDicom.init(): s3_engine must be NoneType or' +
                            ' boto3 S3 client instance.')


    @classmethod
    def apply_filter(cls, dicom_object : any,
               filter_type : int = 0) -> pydicom.dataset.FileDataset:
        """
        Filter a DICOM file
        ===================

        Parameters
        ----------
        dicom_object : str | FileDataset (pydicom.dataset)
            Path of a DICOM file, or the DICOM file's instance.
        filter_type : int, optional (0 if omitted)
            Filters to apply.

        Returns
        -------
        FileDataset
            The filtered instance.

        Raises
        ------
        TypeError
            When object is neither a FileDataset nor a string instance.

        See Also
        --------
            Available filters : FILTER_* constants of class CortxDicom()

        Notes
        -----
            Deleting from a pydicom instance usually mean inplace operation.
            This means it is not necessarily to use the returned variable.
        """

        if isinstance(dicom_object, pydicom.dataset.FileDataset):
            result = dicom_object
        elif isinstance(dicom_object, str):
            result = pydicom.dcmread(dicom_object)
        else:
            raise TypeError('CortxDicom.apply_filter(): object must be a ' +
                            'DICOM instance or string, that is path to a ' +
                            'local DICOM file.')
        if filter_type // CortxDicom.FILTER_PRIVATE % 2 == 1:
            result.remove_private_tags()
        if filter_type // CortxDicom.FILTER_HIPAA % 2 == 1:
            cls.__load_labels(CortxDicom.FILTER_HIPAA)
            result.walk(cls.__remove_hipaa)
        if filter_type // CortxDicom.FILTER_GDPR % 2 == 1:
            cls.__load_labels(CortxDicom.FILTER_GDPR)
            result.walk(cls.__remove_gdpr)
        return result


    @classmethod
    def describe(cls, dicom_object : any, key_scheme : int = -1) -> list:
        """
        Describe DICOM file
        ===================

        Parameters
        ----------
        dicom_object : str | FileDataset (pydicom.dataset)
            Path of a DICOM file, or the DICOM file's instance.
        key_scheme : int, optional (-1 if omitted)
            Scheme of DICOM keys to add. It must be
            CortxDicom.KEY_SCHEME_HUMAN_READABLE, CortxDicom.KEY_SCHEME_NUMERIC
            or CortxDicom.KEY_SCHEME_SPACELESS. If -1 is given, default value
            is used. At the moment it is CortxDicom.KEY_SCHEME_HUMAN_READABLE.

        Returns
        -------
        list[dict('key' : key, 'value' : value)]
            List of key value pairs in dictionaries.

        Raises
        ------
        TypeError
            When object is neither a FileDataset nor a string instance.
        ValueError
            When the given key scheme is not supported.

        See Also
        --------
            Available schemes : KEY_SCHEME_* constants of class CortxDicom()
        """

        if isinstance(dicom_object, pydicom.dataset.FileDataset):
            _object = dicom_object
        elif isinstance(dicom_object, str):
            _object = pydicom.dcmread(dicom_object)
        else:
            raise TypeError('CortxDicom.describe(): object must be a DICOM ' +
                            'instance or string, that is path to a local ' +
                            'DICOM file.')
        if key_scheme == -1:
            _key_scheme = CortxDicom.KEY_SCHEME_HUMAN_READABLE
        else:
            _key_scheme = key_scheme
        if _key_scheme not in [CortxDicom.KEY_SCHEME_HUMAN_READABLE,
                               CortxDicom.KEY_SCHEME_NUMERIC,
                               CortxDicom.KEY_SCHEME_SPACELESS]:
            raise ValueError('CortxDicom.describe(): unsupported key scheme.')
        cls.__load_labels(0)
        # Solve dicom bug to avoid UnicodeDecodeError
        with open(devnull, 'w') as nullstream:
            print(_object, file=nullstream)
        result = []
        for key, value in _object.items():
            str_key = str(key)
            if str_key in cls.__labels[0].keys():
                if value.value is not None:
                    _key = cls.__labels[0][str_key][_key_scheme]
                    if isinstance(value.value, bytes):
                        _value = value.value.decode('utf-8', 'strict')
                    else:
                        _value = str(value.value)
                    result.append({'dicom.key' : _key, 'dicom.value' : _value})
        return result


    @property
    def es_engine(self) -> any:
        """
        Provide direct access to the Elasticsearch engine
        =================================================

        Returns
        -------
        Elasticsearch
            The engine itself.
        """

        return self.__es_engine


    def get(self, search_expression : any = None, object_key : str = '',
            bucket : str = '', index : str = '') -> any:
        """
        Get a DICOM file
        ================

        Parameters
        ----------
        search_expression : str | tuple | dict, optional (None if omitted)
            If string is given, it is treated as an Id to get. If a tuple is
            given it is treated as a simple key = value search. If dict is
            given it is directly forwarded to elasticsearch as a query. If None
            is given S3 is used.
        index : str, optional (empty string if omitted)
            Name of the index to get. If empty string is given, index name is
            retrieved from the configuration or default index is used if there
            is no index name in the configuration.
        object_key : str, optional (empty string if omitted)
            The name (key) to get the object in s3 with.
        bucket : str, optional (empty string if omitted)
            Name of the bucket to get.

        Returns
        -------
        FileDataset | list[FileDataset] | None
            DICOM file, if bucket and object_key or a string as
            search_expression is given. List of FileDataset if tuple or dict
            as search_expression is given. None if no result presents.

        Raises
        ------
        ValueError
            When neither search_expression nor object_key is given.

        Notes
        -----
        I.
            Parameter precedence: search_results precedes bucket and object_key
            parameters.
        III.
            Bucket precedence: bucket name in parameter always precedes bucket
            name retrived from configuration.
        IV.
            Index precedence: index name in parameter always precedes index name
            retrieved from configuration. Index name from configuration always
            precedes the default index name.
        """

        if search_expression is not None:
            _data = self.search(search_expression, index=index, just_find=False)
            if _data is None:
                result = None
            else:
                result = []
                for row in _data:
                    _element = self.s3_get_instance__(row[1], row[0])
                    if _element is not None:
                        result.append(_element)
                if len(result) == 0:
                    result = 0
        else:
            if object_key == '':
                raise ValueError('CortxDicom.get(): either search_expression ' +
                                 'or object_key must be given.')
            _bucket = self.__validate_bucket(bucket)
            if _bucket is None:
                raise AssertionError('CortxDicom.get(): bucket must set as '+
                                     'parameter or as s3.bucket key in Config.')
            result = self.s3_get_instance__(object_key, _bucket)
        return result


    @property
    def s3_engine(self) -> any:
        """
        Provide direct access to the boto3 S3 engine
        ============================================

        Returns
        -------
        Elasticsearch
            The engine itself.
        """

        return self.__s3_engine


    def search(self, search_expression : any, index : str = '',
               just_find : bool = True) -> any:
        """
        Search for a DICOM file
        =======================

        Parameters
        ----------
        search_expression : str | tuple | dict
            If string is given, it is treated as an Id to get. If a tuple is
            given it is treated as a simple key = value search. If dict is
            given it is directly forwarded to elasticsearch as a query.
        index : str, optional (empty string if omitted)
            Name of the index to get. If empty string is given, index name is
            retrieved from the configuration or default index is used if there
            is no index name in the configuration.
        just_find : bool, optional (True if omitted)
            Whether to return a bool with the result of the search or return
            information about the found object(s).

        Returns
        -------
        bool | tuple(str, str) | list[tuple(str, str)] | None
            If just_find is set to True, it returns a boll with value True,
            if the search search_expression had good results, False if not.
            If just_find is set to False, it returns a tuple of bucket,
            object_key pair, if the search_expression was a string, it returns
            a list of tuples with bucket and object_key pairs if the
            search_expression is was a dict or tuple, it returns None, if
            nothing is found.

        Raises
        ------
        ValueError
            When the tuple type search_expression has more or less than 2
            elements.
        TypeError
            When search_expression has other type than str, tuple or dict.

        Notes
        -----
            Index precedence: index name in parameter always precedes index name
            retrieved from configuration. Index name from configuration always
            precedes the default index name.
        """

        _index = self.__get_index(index)
        if isinstance(search_expression, str):
            _status = self.es_engine.get(index=_index, id=search_expression,
                                     ignore=[400, 404])
            if _status.get('_source') is not None:
                result = self.__get_s3_loaction(_status['_source'])
            else:
                result = None
        else:
            if isinstance(search_expression, tuple):
                if len(search_expression) != 2:
                    raise ValueError('CortxDicom.search(): tuple must have ' +
                                     'exactly 2 elements.')
                _query = {'query': {'simple_query_string' :
                         {'query' : '"{}" + "{}"'.format(
                         search_expression[0], search_expression[1]),
                         'fields' : 'info', 'default_operator' : 'and'}}}
            elif isinstance(search_expression, dict):
                _query = search_expression
            else:
                raise TypeError('CortxDicom.search(): supported ' +
                                'search_expression types ar str, tuple, dict.')
            _status = self.es_engine.search(index=_index, body=_query,
                                            ignore=[400, 404])
            result = self.__interpret_search_results(_status)
        if just_find:
            result = result is not None
        return result


    def store(self, dicom_object : any, object_key : str = '',
              bucket : str = '', index : str = '',
              allow_overwrite : bool = False) -> any:
        """
        Store DICOM file
        ================

        Parameters
        ----------
        dicom_object : str | FileDataset (pydicom.dataset)
            Path of a DICOM file, or the DICOM file's instance.
        object_key : str, optional (empty string if omitted)
            The name (key) to store the object in s3 with. If empty string is
            given, a new random name is generated.
        bucket : str, optional (empty string if omitted)
            Name of the bucket to store. If empty string is given, bucket name
            is retrieved from the configuration.
        index : str, optional (empty string if omitted)
            Name of the index to store. If empty string is given, index name is
            retrieved from the configuration or default index is used if there
            is no index name in the configuration.
        allow_overwrite : bool, optional (False if omitted)
            Whether or not to allow to overwrite existing object.

        Returns
        -------
        str | None
            The key created in Elasticsearch engine in case of success, None
            in case of failure.

        Raises
        ------
        TypeError
            When object is neither a FileDataset nor a string instance.
        AssertionError
            When neither bucket parameter nor s3.bucket key in Config is set.

        Notes
        -----
        I.
            Bucket precedence: bucket name in parameter always precedes bucket
            name retrived from configuration.
        II.
            Index precedence: index name in parameter always precedes index name
            retrieved from configuration. Index name from configuration always
            precedes the default index name.
        III.
            Content type precedence: content type from configuration always
            precedes the default content type.
        """

        # pylint: disable=too-many-arguments
        #         We consider a better practice having long list of named
        #         arguments then having **kwargs only.
        # pylint: disable=too-many-locals
        #         The amount of local variables is needed because of the
        #         readability of the code.

        _object = self.__validate_object(dicom_object)
        if _object is None:
            raise TypeError('CortxDicom.store(): object must be a DICOM ' +
                            'instance or string, that is path to a local ' +
                            'DICOM file.')
        _bucket = self.__validate_bucket(bucket)
        if _bucket is None:
            raise AssertionError('CortxDicom.store(): bucket must set as '+
                                 'parameter or as s3.bucket key in Config.')
        _index = self.__get_index(index)
        _content_tpye = self.__get_content_type()
        _keys = self.s3_objects__(_bucket)
        _need_overwrite = False
        if object_key != '':
            _object_key = object_key
            _need_overwrite = _object_key in _keys
        else:
            _object_key = ''.join([chr(randint(97, 122)) for i in range(64)])
            while _object_key in _keys:
                _object_key = ''.join([chr(randint(97, 122))
                                       for i in range(64)])
        if _need_overwrite and not allow_overwrite:
            result = None
        else:
            _status = self.s3_engine.put_object(Bucket=_bucket, Key=_object_key,
                                                Body=pickle.dumps(_object),
                                                ContentType=_content_tpye)
            result = s3_success(_status)
            if result is True:
                _searchable = {}
                _searchable['bucket'] = _bucket
                _searchable['object_key'] = _object_key
                _searchable['info'] = self.describe(_object)
                _status = self.es_engine.index(index=self.ES_DEFAULT_INDEX,
                                               body=_searchable)
                if es_success(_status):
                    if '_id' in _status.keys():
                        result = _status['_id']
                    else:
                        result = None
                else:
                    result = None
        return result


    def s3_buckets__(self) -> list:
        """
        Get buckets
        ===========

        Returns
        -------
        list[str]
            List of bucket names.
        """

        result = []
        data = self.__s3_engine.list_buckets()
        if s3_success(data):
            if data.get('Buckets') is not None:
                result = [e['Name'] for e in data['Buckets']]
        return result


    def s3_get_instance__(self, object_key : str, bucket : str) -> any:
        """
        Get object from S3 as a python instance
        =======================================

        Parameters
        ----------
        object_key : str
            Object key to get.
        bucket : str
            Bucket to get from.

        Returns
        -------
        any
            Python instance.

        Notes
        -----
            Caution! This function doesn't perform additional checks due to
            performance improvement. preferably use CortxDicom.get().
        """

        _keys = self.s3_objects__(bucket)
        if object_key not in _keys:
            result = None
        else:
            _data = self.s3_engine.get_object(Bucket=bucket, Key=object_key)
            _bytes = _data.get('Body')
            if bytes is None:
                result = None
            else:
                result = pickle.loads(_bytes.read())
        return result


    def s3_objects__(self, bucket : str = '') -> list:
        """
        Get objects in the given bucket
        ===============================

        Parameters
        ----------
        bucket : str, optional (empty string if omitted)
            Name of the bucket to store. If empty string is given, bucket name
            is retrieved from the configuration.

        Returns
        -------
        list[str]
            List of object names.

        Raises
        ------
        AssertionError
            When neither bucket parameter nor s3.bucket key in Config is set.

        Notes
        -----
            Bucket precedence: bucket name in parameter always precedes bucket
            name retrived from configuration.
        """

        _bucket = self.__validate_bucket(bucket)
        if _bucket is None:
            raise AssertionError('CortxDicom.s3_objects__(): bucket must ' +
                                 'set as parameter or as s3.bucket key in' +
                                 'Config.')
        result = []
        data = self.__s3_engine.list_objects(Bucket=_bucket)
        if s3_success(data):
            if data.get('Contents') is not None:
                result = [e['Key'] for e in data['Contents']]
        return result


    def __get_content_type(self) -> str:
        """
        Get valid content type
        ======================

        Returns
        -------
        str
            Content type that conforms the precedence.
        """

        # pylint: disable=no-self-use
        #         However self is not used, tha nature of the method deeply
        #         belongs to the object.

        if Config.is_set('s3.content_type'):
            result = Config.get('s3.content_type')
        else:
            result = CortxDicom.CONTENT_TYPE
        return result


    def __get_index(self, index : str) -> str:
        """
        Get valid index
        ===============

        Parameters
        ----------
        index : str
            Name of the index to store. If empty string is given, index name is
            retrieved from the configuration or default index is used if there
            is no index name in the configuration.

        Returns
        -------
        str
            Index that conforms the precedence.

        Notes
        -----
            Index precedence: index name in parameter always precedes index name
            retrieved from configuration. Index name from configuration always
            precedes the default index name.
        """

        # pylint: disable=no-self-use
        #         However self is not used, tha nature of the method deeply
        #         belongs to the object.

        if index != '':
            result = index
        elif Config.is_set('s3.index'):
            result = Config.get('s3.index')
        else:
            result = CortxDicom.ES_DEFAULT_INDEX
        return result


    def __get_s3_loaction(self, source : dict) -> tuple:
        """
        Get S3 location from elasticsearch source dict
        ==============================================

        Parameters
        ----------
        source : dict
            Dictionary from elasticsearch.

        Returns
        -------
        tuple(str, str) | None
            Tuple of bucket, object_key pair if source contains valid data,
            None if not.
        """

        # pylint: disable=no-self-use
        #         However self is not used, tha nature of the method deeply
        #         belongs to the object.

        _bucket = source.get('bucket')
        _object_key = source.get('object_key')
        if _bucket is None or _object_key is None:
            result = None
        else:
            result = (_bucket, _object_key)
        return result


    def __interpret_search_results(self, search_results : dict) -> list:
        """
        Interpreted elasticsearch search results
        ========================================

        Parameters
        ----------
        dict
            The result of the search.

        Returns
        -------
        list[tuple(str, str)] | None
            List of tuples with bucket and object_key pairs if search_results
            contains that kind of data, None, if nothing is found.
        """

        if 'hits' not in search_results.keys():
            result = None
        else:
            if 'hits' not in search_results['hits'].keys():
                result = None
            else:
                result = []
                for element in search_results['hits']['hits']:
                    _source = element.get('_source')
                    if _source is None:
                        continue
                    _location = self.__get_s3_loaction(_source)
                    if _location is None:
                        continue
                    result.append((_location))
                if len(result) == 0:
                    result = None
        return result


    @classmethod
    def __load_labels(cls, label_id : int):
        """
        Load labels if needed
        =====================

        Parameters
        ----------
        label_id : int
            Label storage to load.
            -   0                       to load all supported DICOM tags.
            -   CortxDicom.FILTER_HIPAA to load HIPAA protected tags
            -   CortxDicom.FILTER_GDPR  to load GDPR protected tags

        Raises
        ------
        ValueError
            When a not supported label_id is given.
        """

        if label_id == 0:
            if 0 not in cls.__labels.keys():
                cls.__labels[0] = pickle.loads(read_binary(dicomfields,
                                                           'dicom_tags.dict'))
        elif label_id == CortxDicom.FILTER_HIPAA:
            if CortxDicom.FILTER_HIPAA not in cls.__labels.keys():
                cls.__labels[CortxDicom.FILTER_HIPAA] = pickle.loads(
                                    read_binary(dicomfields, 'phi_hipaa.list'))
        elif label_id == CortxDicom.FILTER_GDPR:
            if CortxDicom.FILTER_GDPR not in cls.__labels.keys():
                cls.__labels[CortxDicom.FILTER_GDPR] = pickle.loads(
                                    read_binary(dicomfields, 'phi_gdpr.list'))
        else:
            raise ValueError('CortxDicom.__load_labels(): unsupported label ' +
                             'source.')


    @classmethod
    def __remove_gdpr(cls, dataset : pydicom.dataset.FileDataset,
                      element : pydicom.dataelem.DataElement):
        """
        Remove tag according to regulations of GDPR
        ===========================================

        Parameters
        ----------
        dataset : pydicom.dataset.FileDataset
            Dataset to perform removal.
        element : pydicom.dataelem.DataElement
            Data element to examine.

        Notes
        -----
            This function meets the specifications of pydicom.dataset.walk()
            function.
        """

        if str(element.tag) in cls.__labels[CortxDicom.FILTER_GDPR]:
            del dataset[element.tag]


    @classmethod
    def __remove_hipaa(cls, dataset : pydicom.dataset.FileDataset,
                      element : pydicom.dataelem.DataElement):
        """
        Remove tag according to regulations of HIPAA
        ============================================

        Parameters
        ----------
        dataset : pydicom.dataset.FileDataset
            Dataset to perform removal.
        element : pydicom.dataelem.DataElement
            Data element to examine.

        Notes
        -----
            This function meets the specifications of pydicom.dataset.walk()
            function.
        """

        if str(element.tag) in cls.__labels[CortxDicom.FILTER_HIPAA]:
            del dataset[element.tag]


    def __validate_bucket(self, bucket) -> str:
        """
        Validate bucket
        ===============

        Parameters
        ----------
        bucket : str
            Name of the bucket to store. If empty string is given, bucket name
            is retrieved from the configuration.

        Returns
        -------
        str | None
            The name of the bucket if validation is successful, None if not.

        Notes
        -----
            Bucket precedence: bucket name in parameter always precedes bucket
            name retrived from configuration.
        """

        # pylint: disable=no-self-use
        #         However self is not used, tha nature of the method deeply
        #         belongs to the object.

        result = None
        if bucket != '':
            result = bucket
        else:
            if Config.is_set('s3.bucket'):
                result = Config.get('s3.bucket')
        return result


    def __validate_object(self,
                          dicom_object : any) -> pydicom.dataset.FileDataset:
        """
        Validate object
        ===============

        Parameters
        ----------
        dicom_object : str | FileDataset (pydicom.dataset)
            Path of a DICOM file, or the DICOM file's instance.

        Returns
        -------
        FileDataset (pydicom.dataset) | None
            The DICOM object if validation is successful, None if not.
        """

        # pylint: disable=no-self-use
        #         However self is not used, tha nature of the method deeply
        #         belongs to the object.

        if isinstance(dicom_object, pydicom.dataset.FileDataset):
            result = dicom_object
        elif isinstance(dicom_object, str):
            result = pydicom.dcmread(dicom_object)
        else:
            result = None
        return result


def es_success(query_result : dict) -> bool:
    """
    Check whether elasticsearch query was successful or not
    =======================================================

    Parameters
    ----------
    query_result : dcit
        The result of the query.

    Returns
    -------
    bool
        True if the query was successful, False if not.
    """

    result = False
    if query_result.get('_shards') is not None:
        if query_result['_shards'].get('failed') is not None:
            result = query_result['_shards']['failed'] == 0
    return result


def s3_success(query_result : dict) -> bool:
    """
    Check whether S3 query was successful or not
    ============================================

    Parameters
    ----------
    query_result : dcit
        The result of the query.

    Returns
    -------
    bool
        True if the query was successful, False if not.
    """

    result = False
    if query_result.get('ResponseMetadata') is not None:
        if query_result['ResponseMetadata'].get('HTTPStatusCode') is not None:
            result = query_result['ResponseMetadata']['HTTPStatusCode'] == 200
    return result
