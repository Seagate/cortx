import json
import uuid
from doc.integrations.fhir.connectors.connector import Connector
from doc.integrations.fhir.connectors.supported_models import SupportedModels
from doc.integrations.fhir.connectors.mapper import get_cortx_s3_bucket
import boto3

class CortxS3Connector(Connector):
    """ class for connector to CORTX S3 service."""

    def __init__(self, endpoint_url: str, aws_access_key_id: str,
                 aws_secret_access_key: str, content_type: str = 'application/json'):
        self.settings = {
            'endpoint_url': endpoint_url,
            'aws_access_key_id': aws_access_key_id,
            'aws_secret_access_key': aws_secret_access_key
        }

        self.client = None
        self.content_type = content_type

    def connect(self, **kwargs):
        """ Connect to the rest api service / data storage.

            :param kwargs: key based arguments
        """
        self.client = boto3.client('s3', **self.settings)

    def get(self, model_type: SupportedModels, model_id: str):
        """ Get one model according to id

            :param SupportedModels model_type: type of model
            :param str model_id: The id of the resource on the remote server
            :returns: An instance of the receiving class
        """
        bucket_name = get_cortx_s3_bucket(model_type)
        response = self.client.get_object(Bucket=bucket_name, Key=model_id)
        return response

    def insert(self, model_type: SupportedModels, data: dict):
        """ Insert one model

            :param SupportedModels model_type: type of model
            :param dict data: The data object to insert
            :returns: None or the response JSON on success
        """
        model_id = data.get('id')
        if not model_id:
            data.update({'id': str(uuid.uuid4())})

        return self.update(model_type, data)

    def update(self, model_type: SupportedModels, data: dict):
        """ Update one model

            :param SupportedModels model_type: type of model
            :param dict data: The data object to update
            :returns: None or the response JSON on success
        """
        model_id = data.get('id')
        if not model_id:
            raise Exception("Cannot update a model that does not have an id")

        bucket_name = get_cortx_s3_bucket(model_type)
        file_data = bytes(json.dumps(data).encode('UTF-8'))
        response = self.client.put_object(Bucket=bucket_name, Key=model_id,
                                          Body=file_data, ContentType=self.content_type)
        return response

    def delete(self, model_type: SupportedModels, model_id: str):
        """ Delete one model

            :param SupportedModels model_type: type of model
            :param str model_id: The id of the resource on the remote server
            :returns: None or the response JSON on success
        """
        bucket_name = get_cortx_s3_bucket(model_type)
        response = self.client.delete_object(Bucket=bucket_name, Key=model_id)
        return response

    def search(self, model_type: SupportedModels, terms: dict = None):
        """ Search models using search terms

            :param SupportedModels model_type: type of model
            :param dict terms: An optional search structure
            :returns: Search model instances
        """
        raise Exception("No support for searching S3 bucket")
