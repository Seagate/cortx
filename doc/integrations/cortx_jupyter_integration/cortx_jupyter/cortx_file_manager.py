import json 
import boto3
from io import BytesIO
import joblib
import pandas as pd
import numpy as np

config_file_path = "credentials.json" 
from tensorflow.keras.models import model_from_json


def _config():
    cfg = {}
    with open(config_file_path) as fp:
       cfg = json.load(fp)
    return cfg

def read_data(file_name):
    config = _config()
    return BytesIO(_get_object(config, config['bucket_name'], file_name)['Body'].read())


def write_data(file_name, data):
    config = _config()
    if isinstance(data, pd.DataFrame):
        data = data.to_csv(index=False).encode()
    elif isinstance(data,(np.ndarray, np.generic)):
        data = data.tobytes()
    _put_object(config, config['bucket_name'], data, file_name)

# Supports tensorflow and keras
def write_model(file_name, model):
    config = _config()
    data = model.to_json()
    _put_object(config, config['bucket_name'], data, file_name)

# Supports tensorflow and keras
def read_model(file_name):
    config = _config()
    response = _get_object(config, config['bucket_name'], file_name)['Body'].read().decode('utf-8')
    return model_from_json(response)



def _put_object(config, bucket, body, object_name):
    print("path upload",config['prefix']+object_name)
    s3_client = boto3.client('s3', aws_access_key_id=config['cortx_authenticator']['access_key_id'], aws_secret_access_key=config['cortx_authenticator']['secret_access_key'], region_name='us-east-1', endpoint_url= config['endpoint_url']) 
    return s3_client.put_object(Body=body, Bucket=bucket, Key=config['prefix']+object_name)

def _get_object(config, bucket, object_name):
    print("path downloading",config['prefix']+object_name)
    s3_client = boto3.client('s3', aws_access_key_id=config['cortx_authenticator']['access_key_id'], aws_secret_access_key=config['cortx_authenticator']['secret_access_key'], region_name='us-east-1', endpoint_url= config['endpoint_url'])
    return s3_client.get_object(Bucket=bucket, Key=config['prefix']+object_name)