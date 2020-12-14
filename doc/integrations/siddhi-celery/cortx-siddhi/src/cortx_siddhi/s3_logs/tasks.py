import datetime
import boto3

from .. import app
from .. import config

@app.app.task
def log_test(log_line):
    client = config.get_client()
    object_name = datetime.datetime.now().strftime('log-%Y-%m-%d-%H-%M-%S')
    client.put_object(Bucket=config.LOGS_BUCKET, Key=object_name, Body=log_line)

def setup():
    client = config.get_client()
    try:
        client.create_bucket(Bucket=config.LOGS_BUCKET)
    except boto3.exceptions.botocore.errorfactory.ClientError:
        pass

setup()
