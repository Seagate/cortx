"""EOSCoreListIndexResponse will list out index response"""
import json


class EOSCoreListIndexResponse(object):
    """EOSCoreListIndexResponse will list index response."""
    _index_content = ""

    def __init__(self, index_content):
        """Initialise index content."""
        self._index_content = json.loads(index_content.decode("utf-8"))

    def get_index_content(self):
        """return index content."""
        return self._index_content

    def get_json(self, key):
        """Returns the content value based on key."""
        json_value = json.loads(self._index_content[key].decode("utf-8"))
        return json_value

    def set_index_content(self, index_content):
        """Sets index content."""
        self._index_content = json.loads(index_content.decode("utf-8"))