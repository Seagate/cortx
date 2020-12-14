from flask import Flask
from flask import request
from flask import abort
import json
from doc.integrations.fhir.connectors.fhir_connector import FhirConnector
from doc.integrations.fhir.connectors.cortx_connector import CortxS3Connector
from doc.integrations.fhir.connectors.mapper import revised_mapping
from doc.integrations.fhir.connectors.supported_models import SupportedModels

with open('config.json') as json_data_file:
    config = json.load(json_data_file)

# Create FHIR connection
fhir_connector = FhirConnector(config['fhir']['host'])
fhir_connector.connect()

# Create CORTX connector
cortx_connector = CortxS3Connector(endpoint_url=config['cortx']['endpoint_url'],
                                   aws_access_key_id=config['cortx']['aws_access_key_id'],
                                   aws_secret_access_key=config['cortx']['aws_secret_access_key'])
cortx_connector.connect()

app = Flask(__name__)

@app.route('/')
def index():
    return "FHIR CORTX Server"

def __get_connector(source: str):
    """ Get the relevant connector according to source

        :param str source: source of the data - 'CORTX' or 'FHIR'
        :returns: An instance of the connector
    """
    lower_source = source.lower()
    mapping = {
        'cortx': cortx_connector,
        'fhir': fhir_connector
    }

    if lower_source not in mapping.keys():
        abort(400, 'Unknown source - only CORTX and FHIR available')

    return mapping[lower_source]

def __get_supported_model(supported_model_name: str):
    """ Get the relevant supported model according to model name given

        :param str supported_model_name: supported model name
        :returns: Supported model enum
    """
    supported_model = revised_mapping(supported_model_name)
    if supported_model == SupportedModels.UNKNOWN:
        abort(400, 'Model "{}" is not supported'.format(supported_model_name))

    return supported_model

def __parse_response(source: str, response_data):
    """ Get the relevant connector according to source

        :param str source: source of the data - 'CORTX' or 'FHIR'
        :returns: An instance of the connector
    """
    lower_source = source.lower()
    if lower_source == 'cortx':
        return json.loads(response_data['Body'].read().decode('utf-8'))
    elif lower_source == 'fhir':
        return response_data.as_json()
    else:
        abort(400, 'Unknown source - only CORTX and FHIR available')

@app.route('/<source>/<supported_model_name>/<model_id>', methods=['GET'])
def get_model(source: str, supported_model_name: str, model_id: str):
    """ Get one model according to id

        :param str source: source of the data - 'CORTX' or 'FHIR'
        :param str supported_model_name: supported model name
        :param str model_id: The id of the resource on the remote server
        :returns: An instance of the receiving class
    """
    connector = __get_connector(source)
    supported_model = __get_supported_model(supported_model_name)

    try:
        model = connector.get(supported_model, model_id)
        return __parse_response(source, model)
    except Exception as e:
        abort(500, e)

@app.route('/<source>/<supported_model_name>', methods=['POST'])
def insert_model(source: str, supported_model_name: str):
    """ Insert one model using request json

        :param str source: source of the data - 'CORTX' or 'FHIR'
        :param str supported_model_name: supported model name
        :returns: None or the response JSON on success
    """
    if not request.data:
        abort(400, 'Request is missing the model (json) data')

    connector = __get_connector(source)
    supported_model = __get_supported_model(supported_model_name)

    try:
        model_data = json.loads(request.data)
        return connector.insert(supported_model, model_data)
    except Exception as e:
        abort(500, e)

@app.route('/<source>/<supported_model_name>', methods=['PUT'])
def update_model(source: str, supported_model_name: str):
    """ Update one model using request json

        :param str source: source of the data - 'CORTX' or 'FHIR'
        :param str supported_model_name: supported model name
        :returns: None or the response JSON on success
    """
    if not request.data:
        abort(400, 'Request is missing the model (json) data')

    connector = __get_connector(source)
    supported_model = __get_supported_model(supported_model_name)

    try:
        model_data = json.loads(request.data)
        return connector.update(supported_model, model_data)
    except Exception as e:
        abort(500, e)

@app.route('/<source>/<supported_model_name>/<model_id>', methods=['DELETE'])
def delete_model(source: str, supported_model_name: str, model_id: str):
    """ Delete one model according to id

        :param str source: source of the data - 'CORTX' or 'FHIR'
        :param str supported_model_name: supported model name
        :param str model_id: The id of the resource on the remote server
        :returns: None or the response JSON on success
    """
    connector = __get_connector(source)
    supported_model = __get_supported_model(supported_model_name)

    try:
        return connector.delete(supported_model, model_id)
    except Exception as e:
        abort(500, e)

@app.route('/<source>/<supported_model_name>/search', methods=['POST'])
def search_model(source: str, supported_model_name: str):
    """ Search models according to request json that contains search terms

        :param str source: source of the data - 'CORTX' or 'FHIR'
        :param str supported_model_name: supported model name
        :returns: model instances
    """
    if not request.data:
        abort(400, 'Request is missing the search terms (json) data')

    connector = __get_connector(source)
    supported_model = __get_supported_model(supported_model_name)

    try:
        search_terms = json.loads(request.data)
        models = connector.search(supported_model, search_terms)
        json_models = []
        for model in models:
            parsed_model = __parse_response(source, model)
            json_models.append(parsed_model)

        return {'results': json_models}
    except Exception as e:
        abort(500, e)

if __name__ == '__main__':
    app.run()
