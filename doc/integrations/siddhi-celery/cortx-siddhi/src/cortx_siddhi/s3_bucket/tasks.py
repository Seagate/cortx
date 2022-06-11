import gzip

import boto3

from .. import app
from .. import config

@app.app.task
def delete_idle_bucket(bucket_name):
    client = config.get_client()
    client.delete_bucket(Bucket=bucket_name)

@app.app.task
def compress_log(bucket_name, object_name):
    client = config.get_client()
    file_content = client.get_object(Bucket=bucket_name, Key=object_name)['Body']
    zipped_content = gzip.compress(file_content.read())
    client.put_object(Bucket=bucket_name, Key='%s.zip' % (object_name, ), Body=zipped_content)
    client.delete_object(Bucket=bucket_name, Key=object_name)
