from dotenv import load_dotenv
from pathlib import Path
import boto3
import os

env_path = Path('.')/'.env'
load_dotenv(dotenv_path=env_path)

if __name__ == "__main__":
    resource = boto3.resource('s3', endpoint_url=str(os.environ.get('ENDPOINT_URL')),
                              aws_access_key_id=str(os.environ.get('AWS_ACCESS_KEY_ID')), aws_secret_access_key=str(os.environ.get('AWS_SECRET_ACCESS_KEY'))
                              )
    bucket = resource.Bucket('testbucket')
    print(bucket)
    for my_bucket_object in bucket.objects.all():
        print(my_bucket_object)
