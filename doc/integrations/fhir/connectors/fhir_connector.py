from doc.integrations.fhir.connectors.connector import Connector
from doc.integrations.fhir.connectors.supported_models import SupportedModels
from doc.integrations.fhir.connectors.mapper import get_fhir_model_type
from fhirclient import client

class FhirConnector(Connector):
    """ class for connector to FHIR based service."""

    def __init__(self, api_base: str, app_id: str = 'fhir_app_id', api_key: str = None, secret: str = None):
        self.settings = {
            'app_id': app_id,
            'api_base': api_base
        }

        if api_key:
            self.settings['api_key'] = api_key
        if secret:
            self.settings['secret'] = secret

        self.fhir_client = None
        self.server = None

    def connect(self, **kwargs):
        """ Connect to the rest api service / data storage.

            :param kwargs: key based arguments
        """
        self.fhir_client = client.FHIRClient(settings=self.settings)
        self.fhir_client.prepare()
        self.server = self.fhir_client.server

    def get(self, model_type: SupportedModels, model_id: str):
        """ Get one model according to id

            :param SupportedModels model_type: type of model
            :param str model_id: The id of the resource on the remote server
            :returns: An instance of the receiving class
        """
        fhir_model_type = get_fhir_model_type(model_type)
        return fhir_model_type.read(model_id, self.server)

    def insert(self, model_type: SupportedModels, data: dict):
        """ Insert one model

            :param SupportedModels model_type: type of model
            :param dict data: The data object to insert
            :returns: None or the response JSON on success
        """
        fhir_model_type = get_fhir_model_type(model_type)
        model = fhir_model_type(data)
        return model.create(self.server)

    def update(self, model_type: SupportedModels, data: dict):
        """ Update one model

            :param SupportedModels model_type: type of model
            :param dict data: The data object to update
            :returns: None or the response JSON on success
        """
        model_id = data.get('id')
        if not model_id:
            raise Exception("Cannot update a model that does not have an id")

        fhir_model_type = get_fhir_model_type(model_type)
        model = fhir_model_type(data)
        return model.update(self.server)

    def delete(self, model_type: SupportedModels, model_id: str):
        """ Delete one model

            :param SupportedModels model_type: type of model
            :param str model_id: The id of the resource on the remote server
            :returns: None or the response JSON on success
        """
        fhir_model_type = get_fhir_model_type(model_type)
        model = fhir_model_type({'id': model_id})
        return model.delete(self.server)

    def search(self, model_type: SupportedModels, terms: dict = None):
        """ Search models

            :param SupportedModels model_type: type of model
            :param dict terms: An optional search structure
            :returns: Search model instances
        """
        fhir_model_type = get_fhir_model_type(model_type)
        search = fhir_model_type.where(struct=terms)
        return search.perform_resources(self.server)
