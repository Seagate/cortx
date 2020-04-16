"""This will generate error response"""


class EOSCoreErrorResponse(object):
    """Generate error message and reason."""
    _http_status = -1
    _error_message = ""
    _error_reason = ""

    def __init__(self, http_status_code, error_reason, error_message):
        """Initialise and parse error response."""
        self._http_status = http_status_code
        self._error_reason = error_reason
        self._error_message = error_message

    def get_error_status(self):
        """Return error status."""
        return self._http_status

    def get_error_message(self):
        """Return error message."""
        return self._error_message

    def get_error_reason(self):
        """Return error reason."""
        return self._error_reason
