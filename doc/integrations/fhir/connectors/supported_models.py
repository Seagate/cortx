from enum import Enum

class SupportedModels(Enum):
    UNKNOWN = 0
    PATIENT = 1
    OBSERVATION = 2
    PROCEDURE = 3
    APPOINTMENT = 4

    # When changing this don't forget to change mapper.py as well