"""
Unit Test for EOSCoreClient API.
"""
from http.client import HTTPConnection
from http.client import HTTPResponse
from unittest.mock import Mock
import pytest

from s3backgrounddelete.eos_core_client import EOSCoreClient
from s3backgrounddelete.eos_core_config import EOSCoreConfig

def test_get_connection_success():
    """Test if HTTPConnection object is returned"""
    config = EOSCoreConfig()
    response = EOSCoreClient(config)._get_connection()
    assert isinstance(response, HTTPConnection)


def test_get_connection_as_none():
    """
    Test if get_connection does not has endpoint configured then
    it should return "None"
    """
    config = Mock(spec=EOSCoreConfig)
    config.get_eos_core_endpoint = Mock(side_effect=KeyError())
    assert EOSCoreClient(config)._get_connection() is None


def test_get_failure():
    """
    Test if connection object is "None" then GET method should throw TypeError.
    """
    with pytest.raises(TypeError):
        config = Mock(spec=EOSCoreConfig)
        config.get_eos_core_endpoint = Mock(side_effect=KeyError())
        assert EOSCoreClient(config).get('/indexes/test_index1')


def test_get_success():
    """Test GET request should return success response."""
    result = b'{"Key": "test_key1", "Value": "testValue1"}'
    httpconnection = Mock(spec=HTTPConnection)
    httpresponse = Mock(spec=HTTPResponse)
    httpresponse.status = 200
    httpresponse.getheaders.return_value = \
        'Content-Type:text/html;Content-Length:14'
    httpresponse.read.return_value = result
    httpresponse.reason = 'OK'
    httpconnection.getresponse.return_value = httpresponse

    config = EOSCoreConfig()
    response = EOSCoreClient(config, connection=httpconnection).get(
        '/indexes/test_index1')
    assert response['status'] == 200


def test_put_failure():
    """
    Test if connection object is "None" then PUT method should throw TypeError.
    """
    with pytest.raises(TypeError):
        config = Mock(spec=EOSCoreConfig)
        config.get_eos_core_endpoint = Mock(side_effect=KeyError())
        assert EOSCoreClient(config).put('/indexes/test_index1')


def test_put_success():
    """Test PUT request should return success response."""
    httpconnection = Mock(spec=HTTPConnection)
    httpresponse = Mock(spec=HTTPResponse)
    httpresponse.status = 201
    httpresponse.getheaders.return_value = \
        'Content-Type:text/html;Content-Length:14'
    httpresponse.read.return_value = b'{}'
    httpresponse.reason = 'CREATED'
    httpconnection.getresponse.return_value = httpresponse

    config = EOSCoreConfig()
    request_uri = '/indexes/test_index1'
    response = EOSCoreClient(config, connection=httpconnection).put(request_uri)
    assert response['status'] == 201


def test_delete_failure():
    """
    Test if connection object is "None" then DELETE should throw TypeError.
    """
    with pytest.raises(TypeError):
        config = Mock(spec=EOSCoreConfig)
        config.get_eos_core_endpoint = Mock(side_effect=KeyError())
        assert EOSCoreClient(config).delete('/indexes/test_index1')


def test_delete_success():
    """Test DELETE request should return success response."""
    httpconnection = Mock(spec=HTTPConnection)
    httpresponse = Mock(spec=HTTPResponse)
    httpresponse.status = 204
    httpresponse.getheaders.return_value = \
        'Content-Type:text/html;Content-Length:14'
    httpresponse.read.return_value = b'test body'
    httpresponse.reason = 'OK'
    httpconnection.getresponse.return_value = httpresponse

    config = EOSCoreConfig()
    response = EOSCoreClient(config, connection=httpconnection).delete(
        '/indexes/test_index1')
    assert response['status'] == 204

def test_head_failure():
    """
    Test if connection object is "None" then HEAD should throw TypeError.
    """
    with pytest.raises(TypeError):
        config = Mock(spec=EOSCoreConfig)
        config.get_eos_core_endpoint = Mock(side_effect=KeyError())
        assert EOSCoreClient(config).head('/indexes/test_index1')


def test_head_success():
    """Test HEAD request should return success response."""
    httpconnection = Mock(spec=HTTPConnection)
    httpresponse = Mock(spec=HTTPResponse)
    httpresponse.status = 200
    httpresponse.getheaders.return_value = \
        'Content-Type:text/html;Content-Length:14'
    httpresponse.read.return_value = b'test body'
    httpresponse.reason = 'OK'
    httpconnection.getresponse.return_value = httpresponse

    config = EOSCoreConfig()
    response = EOSCoreClient(config, connection=httpconnection).head(
        '/indexes/test_index1')
    assert response['status'] == 200

