"""This class provides Key-value REST API i.e. GET,PUT and DELETE."""
import logging
import urllib

from s3backgrounddelete.eos_core_client import EOSCoreClient
from s3backgrounddelete.eos_get_kv_response import EOSCoreGetKVResponse
from s3backgrounddelete.eos_core_error_respose import EOSCoreErrorResponse
from s3backgrounddelete.eos_core_success_response import EOSCoreSuccessResponse
from s3backgrounddelete.eos_core_util import EOSCoreUtil

# EOSCoreKVApi supports key-value REST-API's Put, Get & Delete


class EOSCoreKVApi(EOSCoreClient):
    """EOSCoreKVApi provides key-value REST-API's Put, Get & Delete."""
    _logger = None

    def __init__(self, config, logger=None, connection=None):
        """Initialise logger and config."""
        if (logger is None):
            self._logger = logging.getLogger("EOSCoreKVApi")
        else:
            self._logger = logger
        self._logger = logging.getLogger()
        self.config = config
        if (connection is None):
            super(EOSCoreKVApi, self).__init__(self.config, logger=self._logger)
        else:
            super(EOSCoreKVApi, self).__init__(self.config, logger=self._logger, connection=connection)


    def put(self, index_id=None, object_key_name=None, value=""):
        """Perform PUT request and generate response."""
        if index_id is None:
            self._logger.error("Index Id is required.")
            return False, None
        if object_key_name is None:
            self._logger.error("Key is required")
            return False, None

        query_params = ""
        request_body = value

        # The URL quoting functions focus on taking program data and making it safe for use as URL components by quoting special characters and appropriately encoding non-ASCII text.
        # https://docs.python.org/3/library/urllib.parse.html
        # For example if index_id is 'AAAAAAAAAHg=-AwAQAAAAAAA=' and object_key_name is "testobject+"
        # urllib.parse.quote(index_id, safe='') and urllib.parse.quote(object_key_name) yields 'testobject%2B' respectively
        # And request_uri is
        # '/indexes/AAAAAAAAAHg%3D-AwAQAAAAAAA%3D/testobject%2B'

        request_uri = '/indexes/' + \
            urllib.parse.quote(index_id, safe='') + '/' + \
            urllib.parse.quote(object_key_name)
        headers = EOSCoreUtil.prepare_signed_header('PUT', request_uri, query_params, request_body)

        if(headers['Authorization'] is None):
            self._logger.error("Failed to generate v4 signature")
            return False, None

        try:
            response = super(
                EOSCoreKVApi,
                self).put(
                request_uri,
                request_body,
                headers=headers)
        except ConnectionRefusedError as ex:
            self._logger.error(repr(ex))
            return False, EOSCoreErrorResponse(502,"","ConnectionRefused")
        except Exception as ex:
            self._logger.error(repr(ex))
            return False, EOSCoreErrorResponse(500,"","InternalServerError")



        if response['status'] == 201:
            self._logger.info("Key value details added successfully.")
            return True, EOSCoreSuccessResponse(response['body'])
        else:
            self._logger.info('Failed to add key value details.')
            return False, EOSCoreErrorResponse(
                response['status'], response['reason'], response['body'])

    def get(self, index_id=None, object_key_name=None):
        """Perform GET request and generate response."""
        if index_id is None:
            self._logger.error("Index Id is required.")
            return False, None
        if object_key_name is None:
            self._logger.error("Key is required")
            return False, None

        # The URL quoting functions focus on taking program data and making it safe for use as URL components by quoting special characters and appropriately encoding non-ASCII text.
        # https://docs.python.org/3/library/urllib.parse.html
        # For example if index_id is 'AAAAAAAAAHg=-AwAQAAAAAAA=' and object_key_name is "testobject+"
        # urllib.parse.quote(index_id, safe='') and urllib.parse.quote(object_key_name) yields 'testobject%2B' respectively
        # And request_uri is
        # '/indexes/AAAAAAAAAHg%3D-AwAQAAAAAAA%3D/testobject%2B'

        request_uri = '/indexes/' + \
            urllib.parse.quote(index_id, safe='') + '/' + \
            urllib.parse.quote(object_key_name)

        query_params = ""
        body = ""
        headers = EOSCoreUtil.prepare_signed_header('GET', request_uri, query_params, body)

        if(headers['Authorization'] is None):
            self._logger.error("Failed to generate v4 signature")
            return False, None

        try:
            response = super(
                EOSCoreKVApi,
                self).get(
                request_uri,
                headers=headers)
        except ConnectionRefusedError as ex:
            self._logger.error(repr(ex))
            return False, EOSCoreErrorResponse(502,"","ConnectionRefused")
        except Exception as ex:
            self._logger.error(repr(ex))
            return False, EOSCoreErrorResponse(500,"","InternalServerError")

        if response['status'] == 200:
            self._logger.info("Get kv operation successfully.")
            return True, EOSCoreGetKVResponse(
                object_key_name, response['body'])
        else:
            self._logger.info('Failed to get kv details.')
            return False, EOSCoreErrorResponse(
                response['status'], response['reason'], response['body'])

    def delete(self, index_id=None, object_key_name=None):
        """Perform DELETE request and generate response."""
        if index_id is None:
            self._logger.error("Index Id is required.")
            return False, None
        if object_key_name is None:
            self._logger.error("Key is required")
            return False, None

        # The URL quoting functions focus on taking program data and making it safe for use as URL components by quoting special characters and appropriately encoding non-ASCII text.
        # https://docs.python.org/3/library/urllib.parse.html
        # For example if index_id is 'AAAAAAAAAHg=-AwAQAAAAAAA=' and object_key_name is "testobject+"
        # urllib.parse.quote(index_id, safe='') and urllib.parse.quote(object_key_name) yields 'testobject%2B' respectively
        # And request_uri is
        # '/indexes/AAAAAAAAAHg%3D-AwAQAAAAAAA%3D/testobject%2B'

        request_uri = '/indexes/' + \
            urllib.parse.quote(index_id, safe='') + '/' + \
            urllib.parse.quote(object_key_name)

        body = ""
        query_params = ""
        headers = EOSCoreUtil.prepare_signed_header('DELETE', request_uri, query_params, body)

        if(headers['Authorization'] is None):
            self._logger.error("Failed to generate v4 signature")
            return False, None

        try:
            response = super(
                EOSCoreKVApi,
                self).delete(
                request_uri,
                headers=headers)
        except ConnectionRefusedError as ex:
            self._logger.error(repr(ex))
            return False, EOSCoreErrorResponse(502,"","ConnectionRefused")
        except Exception as ex:
            self._logger.error(repr(ex))
            return False, EOSCoreErrorResponse(500,"","InternalServerError")

        if response['status'] == 204:
            self._logger.info('Key value deleted.')
            return True, EOSCoreSuccessResponse(response['body'])
        else:
            self._logger.info('Failed to delete key value.')
            return False, EOSCoreErrorResponse(
                response['status'], response['reason'], response['body'])
