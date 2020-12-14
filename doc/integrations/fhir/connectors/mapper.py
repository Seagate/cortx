from doc.integrations.fhir.connectors.supported_models import SupportedModels
from fhirclient.models.patient import Patient
from fhirclient.models.observation import Observation
from fhirclient.models.procedure import Procedure
from fhirclient.models.appointment import Appointment
from typing import Union, Type

def get_fhir_model_type(model_type: SupportedModels) -> Type[Union[Patient, Observation, Procedure, Appointment]]:
    """ Mapper between model type enum to FHIR server resource model

        :param model_type: type of model
        :returns: fhir resource model
    """
    mapping = {
        str(SupportedModels.PATIENT): Patient,
        str(SupportedModels.OBSERVATION): Observation,
        str(SupportedModels.PROCEDURE): Procedure,
        str(SupportedModels.APPOINTMENT): Appointment
    }

    return mapping[str(model_type)]

def get_cortx_s3_bucket(model_type: SupportedModels) -> str:
    """ Mapper between model type enum to s3 bucket in CORTX

        :param model_type: type of model
        :returns: s3 bucket in CORTX
    """
    mapping = {
        str(SupportedModels.PATIENT): 'patient',
        str(SupportedModels.OBSERVATION): 'observation',
        str(SupportedModels.PROCEDURE): 'procedure',
        str(SupportedModels.APPOINTMENT): 'appointment'
    }

    return mapping[str(model_type)]

def revised_mapping(server_model_name: str) -> SupportedModels:
    """ Mapper between server model name to the relevant supported model

        :param server_model_name: name of model (string)
        :returns: Supported model enum
    """
    lower_model_name = server_model_name.lower()
    mapping = {
        'patient': SupportedModels.PATIENT,
        'observation': SupportedModels.OBSERVATION,
        'procedure': SupportedModels.PROCEDURE,
        'appointment': SupportedModels.APPOINTMENT
    }

    if lower_model_name not in mapping.keys():
        return SupportedModels.UNKNOWN

    return mapping[lower_model_name]
