import os
import boto3
import settings

s3 = boto3.resource(
    "s3",
    endpoint_url=os.environ.get('endpoint_url'),
    aws_access_key_id=os.environ.get('aws_access_key_id'),
    aws_secret_access_key=os.environ.get('aws_secret_access_key'),
)

bucket = s3.Bucket(os.environ.get('bucket_name'))


def download(filename):
    try:
        output_path = './.tmp/{}'.format(filename)
        bucket.download_file(filename, output_path)

        return output_path
    except Exception as e:
        return None


def upload(input_path,filename):
    try:
        bucket.upload_file(input_path, filename)

        return input_path
    except Exception as e:
        return None


def list_files():
    try:
        files = []
        for my_bucket_object in bucket.objects.all():
            files.append(my_bucket_object.key)

        return files
    except Exception as e:
        return []
