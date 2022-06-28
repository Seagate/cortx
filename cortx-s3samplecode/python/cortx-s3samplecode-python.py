# Bari Arviv - s3 operation using boto3
import boto3
import logging
from botocore.client import Config
from botocore.exceptions import ClientError

## Global variables
# Parameters for connection to s3:
REGION = 'XXXXXX'
VERSION = 'XXXXXX'
ACCESS_KEY = 'XXXXXX'
SECRET_ACCESS_KEY = 'XXXXXX'
END_POINT_URL = 'XXXXXX' 

# Objects to perform actions: client is swiss knife , resource has all sort of data:
s3_resource = boto3.resource('s3', endpoint_url=END_POINT_URL,
                             aws_access_key_id=ACCESS_KEY,
                             aws_secret_access_key=SECRET_ACCESS_KEY,
                             config=Config(signature_version=VERSION),
                             region_name=REGION,
                             verify=False)

s3_client = boto3.client('s3', endpoint_url=END_POINT_URL,
                         aws_access_key_id=ACCESS_KEY,
                         aws_secret_access_key=SECRET_ACCESS_KEY,
                         config=Config(signature_version=VERSION),
                         region_name=REGION,
                         verify=False)

""" Functions for bucket operations """
# The function prints the list of existing buckets.
def list_buckets():
    buckets = s3_client.list_buckets()

    if buckets['Buckets']:
        for bucket in buckets['Buckets']:
            print(bucket)

# The function performs the operation according to the parameter obtained. In case of
# an exception, print it. You can create, delete and print the list of existing buckets.
def bucket_operations(bucket_name=None, operation='list'):
    if operation != 'list' and not bucket_name:
        logging.error('The bucket name is %s missing!' % (bucket_name))
        return False 

    try:
        if operation == 'delete':
            s3_client.delete_bucket(Bucket=bucket_name)
        elif operation == 'create':
            s3_client.create_bucket(Bucket=bucket_name)
        elif operation == 'list':
            list_buckets()
        else:
            logging.error('unknown bucket operation')
            return False
    except ClientError as err:
        logging.error(err)
        return False

    return True
    
""" Functions for files operations """
# The function prints the list of files in the bucket.
def list_files(bucket_name):
    current_bucket = s3_resource.Bucket(bucket_name)
    print('The files in bucket %s:\n' % (bucket_name))

    for obj in current_bucket.objects.all():
        print(obj.meta.data) 

# The function performs the operation according to the parameter obtained. In case of an exception,
# print it. You can upload, download and delete a file and print the list of files inside the bucket.
def file_operations(bucket_name, operation='list', file_name=None, file_location=None):
    if not bucket_name:
        logging.error('The bucket name is %s missing!' % (bucket_name))
        return False 

    try:
        if operation == 'list':
            list_files(bucket_name)
        elif operation == 'delete':
            s3_client.delete_object(Bucket=bucket_name, Key=file_name)
        elif operation == 'download':
            s3_resource.Bucket(bucket_name).download_file(file_name, file_location) 
        elif operation == 'upload':
            s3_resource.Bucket(bucket_name).upload_file(file_location, file_name)
        else:
            logging.error('unknown file operation')
            return False  
    except ClientError as err:
        logging.error(err)
        return False
    
    return True

def main():
    bucket_name = 'bari'

    # create a new bucket
    if bucket_operations(bucket_name, 'create'):
        print("Bucket creation completed successfully!")
    
    # פrint the list of existing buckets
    bucket_operations(operation='list')
    # print the files in a bucket
    file_operations(bucket_name, operation='list')

if __name__ == '__main__':
    main()