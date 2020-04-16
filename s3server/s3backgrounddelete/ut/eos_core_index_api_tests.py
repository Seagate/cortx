"""
Unit Test for EOSCoreIndex API.
"""
from http.client import HTTPConnection
from http.client import HTTPResponse
from unittest.mock import Mock

from s3backgrounddelete.eos_core_index_api import EOSCoreIndexApi
from s3backgrounddelete.eos_core_config import EOSCoreConfig

def test_list_no_index_id():
    """Test List api without index_id should return response as "None"."""
    config = EOSCoreConfig()
    response = EOSCoreIndexApi(config).list(None)
    if (response is not None):
        assert response[0] is False
        assert response[1] is None


def test_list_success():
    """Test List api, it should return success response."""
    result = b'{"index_id":"test_index1","object_layout_id":1,"object_metadata_path":"test_index1"}'

    httpconnection = Mock(spec=HTTPConnection)
    httpresponse = Mock(spec=HTTPResponse)
    httpresponse.status = 200
    httpresponse.getheaders.return_value = \
        'Content-Type:text/html;Content-Length:14'
    httpresponse.read.return_value = result
    httpresponse.reason = 'OK'
    httpconnection.getresponse.return_value = httpresponse

    config = EOSCoreConfig()
    response = EOSCoreIndexApi(config, connection=httpconnection).list("test_index1")
    if (response is not None):
        assert response[0] is True
        assert response[1] is not None


def test_list_failure():
    """Test if index is not present then List should return failure response."""
    httpconnection = Mock(spec=HTTPConnection)
    httpresponse = Mock(spec=HTTPResponse)
    httpresponse.status = 404
    httpresponse.getheaders.return_value = \
        'Content-Type:text/html;Content-Length:14'
    httpresponse.read.return_value = b'{}'
    httpresponse.reason = 'NOT FOUND'
    httpconnection.getresponse.return_value = httpresponse

    config = EOSCoreConfig()
    response = EOSCoreIndexApi(config, connection=httpconnection).list("test_index2")
    if (response is not None):
        assert response[0] is False
        assert response[1] is not None


def test_put_success():
    """Test PUT api, it should return success response."""
    httpconnection = Mock(spec=HTTPConnection)
    httpresponse = Mock(spec=HTTPResponse)
    httpresponse.status = 201
    httpresponse.getheaders.return_value = \
        'Content-Type:text/html;Content-Length:14'
    httpresponse.read.return_value = b'{}'
    httpresponse.reason = 'CREATED'
    httpconnection.getresponse.return_value = httpresponse

    config = EOSCoreConfig()
    response = EOSCoreIndexApi(config, connection=httpconnection).put("test_index1")
    if (response is not None):
        assert response[0] is True


def test_put_failure():
    """Test if index is already present then PUT should return failure response."""
    httpconnection = Mock(spec=HTTPConnection)
    httpresponse = Mock(spec=HTTPResponse)
    httpresponse.status = 409
    httpresponse.getheaders.return_value = \
        'Content-Type:text/html;Content-Length:14'
    httpresponse.read.return_value = b'{}'
    httpresponse.reason = 'CONFLICT'
    httpconnection.getresponse.return_value = httpresponse

    config = EOSCoreConfig()
    response = EOSCoreIndexApi(config, connection=httpconnection).put("test_index1")
    if (response is not None):
        assert response[0] is False


def test_put_no_index_id():
    """Test PUT request without index_id, it should return response as "None"."""
    config = EOSCoreConfig()
    response = EOSCoreIndexApi(config).put(None)
    if (response is not None):
        assert response[0] is False
