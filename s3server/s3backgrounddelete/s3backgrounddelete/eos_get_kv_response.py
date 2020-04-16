"""This will generate GET Key-value response."""


class EOSCoreGetKVResponse(object):
    """Generate Get Key-Value reponse."""
    _key = ""
    _value = ""

    def __init__(self, key, value):
        """Initialise key and value."""
        self._key = key
        self._value = value.decode("utf-8")

    def get_key(self):
        """Return key."""
        return self._key

    def get_value(self):
        """Return value."""
        return self._value
