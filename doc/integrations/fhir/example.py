import json

from doc.integrations.fhir.connectors.supported_models import SupportedModels
from doc.integrations.fhir.connectors.fhir_connector import FhirConnector
from doc.integrations.fhir.connectors.cortx_connector import CortxS3Connector

# Create FHIR connection
fhir_connector = FhirConnector('http://test.fhir.org/r3')
fhir_connector.connect()

# Create CORTX connector
cortx_connector = CortxS3Connector(endpoint_url='http://uvo19iqqprct5bd9622.vm.cld.sr',
                                   aws_access_key_id='z5_sdXvsSvKKWxjJmvCA9g',
                                   aws_secret_access_key='kUIMmfQCyNzxel8vi8udIsadwuliOYpS/jdyMh8e')
cortx_connector.connect()

# Get a patient
print('Getting a patient from FHIR service')
patient11 = fhir_connector.get(SupportedModels.PATIENT, '11')
print(patient11.as_json())

# Search for observations
print('Search observations in FHIR service')
search_terms = {'code': '11449-6'}
observations = fhir_connector.search(SupportedModels.OBSERVATION, search_terms)
print(observations[0].as_json())

# Insert data into patient bucket
print('Insert patient data to CORTX')
insert_response = cortx_connector.insert(SupportedModels.PATIENT, patient11.as_json())
print(insert_response)

# Insert data into observation bucket
print('Insert observation data to CORTX')
for observation in observations:
    insert_response = cortx_connector.insert(SupportedModels.OBSERVATION, observation.as_json())
    print(insert_response)

# Update patient data on CORTX
print('Update patient data in CORTX')
print('Patient active value is:', patient11.active)
patient11.active = True
update_response = cortx_connector.update(SupportedModels.PATIENT, patient11.as_json())
print(update_response)

# Validate the change
print('Validate the change')
get_response = cortx_connector.get(SupportedModels.PATIENT, patient11.id)
new_patient11 = json.loads(get_response['Body'].read().decode('utf-8'))
print('New active new value is:', new_patient11['active'])
