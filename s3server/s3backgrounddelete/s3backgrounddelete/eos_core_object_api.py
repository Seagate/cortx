"""This class provides Object  REST API i.e. GET,PUT,DELETE and HEAD."""
import logging
import urllib

from s3backgrounddelete.eos_core_error_respose import EOSCoreErrorResponse
from s3backgrounddelete.eos_core_success_response import EOSCoreSuccessResponse
from s3backgrounddelete.eos_core_client import EOSCoreClient
from s3backgrounddelete.eos_core_util import EOSCoreUtil

# EOSCoreObjectApi supports object REST-API's Put, Get & Delete


class EOSCoreObjectApi(EOSCoreClient):
    """EOSCoreObjectApi provides object REST-API's Get, Put and Delete."""
    _logger = None

    def __init__(self, config, logger=None, connection=None):
        """Initialise logger and config."""
        if (logger is None):
            self._logger = logging.getLogger("EOSCoreObjectApi")
        else:
            self._logger = logger
        self.config = config
        if (connection is None):
            super(EOSCoreObjectApi, self).__init__(self.config, logger = self._logger)
        else:
            super(EOSCoreObjectApi, self).__init__(self.config, logger=self._logger, connection=connection)


    def put(self, oid, value):
        """Perform PUT request and generate response."""
        if oid is None:
            self._logger.error("Object Id is required.")
            return False, None

        query_params = ""
        request_body = value

        # The URL quoting functions focus on taking program data and making it safe for use as URL components by quoting special characters and appropriately encoding non-ASCII text.
        # https://docs.python.org/3/library/urllib.parse.html
        # For example if oid is 'JwZSAwAAAAA=-AgAAAAAA4Ag=' urllib.parse.quote(oid, safe='') yields 'JwZSAwAAAAA%3D-AgAAAAAA4Ag%3D'
        # And request_uri is '/objects/JwZSAwAAAAA%3D-AgAAAAAA4Ag%3D'

        request_uri = '/objects/' + urllib.parse.quote(oid, safe='')

        headers = EOSCoreUtil.prepare_signed_header('PUT', request_uri, query_params, request_body)

        if headers['Authorization'] is None:
            self._logger.error("Failed to generate v4 signature")
            return False, None

        try:
            response = super(
                EOSCoreObjectApi,
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
            self._logger.info("Object added successfully.")
            return True, EOSCoreSuccessResponse(response['body'])
        else:
            self._logger.info('Failed to add Object.')
            return False, EOSCoreErrorResponse(
                response['status'], response['reason'], response['body'])

    def get(self, oid):
        """Perform GET request and generate response."""
        if oid is None:
            self._logger.error("Object Id is required.")
            return False, None
        request_uri = '/objects/' + urllib.parse.quote(oid, safe='')

        query_params = ""
        body = ""

        # The URL quoting functions focus on taking program data and making it safe for use as URL components by quoting special characters and appropriately encoding non-ASCII text.
        # https://docs.python.org/3/library/urllib.parse.html
        # For example if oid is 'JwZSAwAAAAA=-AgAAAAAA4Ag=' urllib.parse.quote(oid, safe='') yields 'JwZSAwAAAAA%3D-AgAAAAAA4Ag%3D'
        # And request_uri is '/objects/JwZSAwAAAAA%3D-AgAAAAAA4Ag%3D'

        headers = EOSCoreUtil.prepare_signed_header('GET', request_uri, query_params, body)

        if headers['Authorization'] is None:
            self._logger.error("Failed to generate v4 signature")
            return False, None

        try:
            response = super(
                EOSCoreObjectApi,
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
            self._logger.info('Successfully fetched object details.')
            return True, EOSCoreSuccessResponse(response['body'])
        else:
            self._logger.info('Failed to fetch object details.')
            return False, EOSCoreErrorResponse(
                response['status'], response['reason'], response['body'])

    def delete(self, oid, layout_id):
        """Perform DELETE request and generate response."""
        if oid is None:
            self._logger.error("Object Id is required.")
            return False, None
        if layout_id is None:
            self._logger.error("Layout Id is required.")
            return False, None

        # The URL quoting functions focus on taking program data and making it safe for use as URL components by quoting special characters and appropriately encoding non-ASCII text.
        # urllib.parse.urlencode converts a mapping object or a sequence of two-element tuples, which may contain str or bytes objects, to a percent-encoded ASCII text string.
        # https://docs.python.org/3/library/urllib.parse.html
        # For example if oid is 'JwZSAwAAAAA=-AgAAAAAA4Ag=' urllib.parse.quote(oid, safe='') yields 'JwZSAwAAAAA%3D-AgAAAAAA4Ag%3D' and layout_id is 1
        # urllib.parse.urlencode({'layout-id': layout_id}) yields layout-id=1
        # And request_uri is '/objects/JwZSAwAAAAA%3D-AgAAAAAA4Ag%3D' ,
        # absolute_request_uri is layout-id is '/objects/JwZSAwAAAAA%3D-AgAAAAAA4Ag%3D?layout-id=1'

        query_params = urllib.parse.urlencode({'layout-id': layout_id})
        request_uri = '/objects/' + urllib.parse.quote(oid, safe='')
        absolute_request_uri = request_uri + '?' + query_params

        body = ''
        headers = EOSCoreUtil.prepare_signed_header('DELETE', request_uri, query_params, body)

        if headers['Authorization'] is None:
            self._logger.error("Failed to generate v4 signature")
            return False, None

        try:
            response = super(
                EOSCoreObjectApi,
                self).delete(
                absolute_request_uri,
                body,
                headers=headers)
        except ConnectionRefusedError as ex:
            self._logger.error(repr(ex))
            return False, EOSCoreErrorResponse(502,"","ConnectionRefused")
        except Exception as ex:
            self._logger.error(repr(ex))
            return False, EOSCoreErrorResponse(500,"","InternalServerError")

        if response['status'] == 204:
            self._logger.info('Object deleted successfully.')
            return True, EOSCoreSuccessResponse(response['body'])
        else:
            self._logger.info('Failed to delete Object.')
            return False, EOSCoreErrorResponse(
                response['status'], response['reason'], response['body'])


    def head(self, oid, layout_id):
        """Perform HEAD request and generate response."""
        if oid is None:
            self._logger.error("Object Id is required.")
            return False, None
        if layout_id is None:
            self._logger.error("Layout Id is required.")
            return False, None

        # The URL quoting functions focus on taking program data and making it safe for use as URL components by quoting special characters and appropriately encoding non-ASCII text.
        # urllib.parse.urlencode converts a mapping object or a sequence of two-element tuples, which may contain str or bytes objects, to a percent-encoded ASCII text string.
        # https://docs.python.org/3/library/urllib.parse.html
        # For example if oid is 'JwZSAwAAAAA=-AgAAAAAA4Ag=' urllib.parse.quote(oid, safe='') yields 'JwZSAwAAAAA%3D-AgAAAAAA4Ag%3D' and layout_id is 1
        # urllib.parse.urlencode({'layout-id': layout_id}) yields layout-id=1
        # And request_uri is '/objects/JwZSAwAAAAA%3D-AgAAAAAA4Ag%3D' ,
        # absolute_request_uri is layout-id is '/objects/JwZSAwAAAAA%3D-AgAAAAAA4Ag%3D?layout-id=1'

        query_params = urllib.parse.urlencode({'layout-id': layout_id})
        request_uri = '/objects/' + urllib.parse.quote(oid, safe='')
        absolute_request_uri = request_uri + '?' + query_params

        body = ''
        headers = EOSCoreUtil.prepare_signed_header('HEAD', request_uri, query_params, body)

        if headers['Authorization'] is None:
            self._logger.error("Failed to generate v4 signature")
            return False, None

        try:
            response = super(
                EOSCoreObjectApi,
                self).head(
                absolute_request_uri,
                body,
                headers=headers)
        except ConnectionRefusedError as ex:
            self._logger.error(repr(ex))
            return False, EOSCoreErrorResponse(502,"","ConnectionRefused")
        except Exception as ex:
            self._logger.error(repr(ex))
            return False, EOSCoreErrorResponse(500,"","InternalServerError")

        if response['status'] == 200:
            self._logger.info("HEAD Object called successfully with status code: "\
                 + str(response['status']) + " response body: " + str(response['body']))
            return True, EOSCoreSuccessResponse(response['body'])
        else:
            self._logger.info("Failed to do HEAD Object with status code: "\
                 + str(response['status']) + " response body: " + str(response['body']))
            return False, EOSCoreErrorResponse(
                response['status'], response['reason'], response['body'])

