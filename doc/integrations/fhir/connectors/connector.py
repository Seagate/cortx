from abc import ABC, abstractmethod
from doc.integrations.fhir.connectors.supported_models import SupportedModels

class Connector(ABC):
    """ Base class for connector FHIR / CORTEX service."""

    @abstractmethod
    def connect(self, **kwargs):
        """ Connect to the rest api service / data storage.

            :param kwargs: key based arguments
        """
        pass

    @abstractmethod
    def get(self, model_type: SupportedModels, model_id: str):
        """ Get one model according to id

            :param SupportedModels model_type: type of model
            :param str model_id: The id of the resource on the remote server
            :returns: An instance of the receiving class
        """
        pass

    @abstractmethod
    def insert(self, model_type: SupportedModels, data: dict):
        """ Insert one model

            :param SupportedModels model_type: type of model
            :param dict data: The data object to insert
            :returns: None or the response JSON on success
        """
        pass

    @abstractmethod
    def update(self, model_type: SupportedModels, data: dict):
        """ Update one model

            :param SupportedModels model_type: type of model
            :param dict data: The data object to update
            :returns: None or the response JSON on success
        """
        pass

    @abstractmethod
    def delete(self, model_type: SupportedModels, model_id: str):
        """ Delete one model

            :param SupportedModels model_type: type of model
            :param str model_id: The id of the resource on the remote server
            :returns: None or the response JSON on success
        """
        pass

    @abstractmethod
    def search(self, model_type: SupportedModels, terms: dict = None):
        """ Search models

            :param model_type: type of model
            :param dict terms: An optional search structure
            :returns: Search model instances
        """
        pass