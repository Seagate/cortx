import os
import sys
import threading

import boto3
import logging
import shutil
from botocore.client import Config
from matplotlib import pyplot as plt
from botocore.exceptions import ClientError
from boto3.s3.transfer import TransferConfig

END_POINT_URL = 'http://uvo1baooraa1xb575uc.vm.cld.sr/'
A_KEY = 'AKIAtEpiGWUcQIelPRlD1Pi6xQ'
S_KEY = 'YNV6xS8lXnCTGSy1x2vGkmGnmdJbZSapNXaSaRhK'


class ProgressPercentage(object):
    def __init__(self, filename):
        self._filename = filename
        self._size = float(os.path.getsize(filename))
        self._seen_so_far = 0
        self._lock = threading.Lock()

    def __call__(self, bytes_amount):
        # To simplify, assume this is hooked up to a single filename
        with self._lock:
            self._seen_so_far += bytes_amount
            percentage = (self._seen_so_far / self._size) * 100
            sys.stdout.write("\r%s  %s / %s  (%.2f%%)" %
                             (self._filename, self._seen_so_far,
                              self._size, percentage))
            sys.stdout.flush()


"""Functions for buckets operation"""
def create_bucket_op(bucket_name, region):
    if region is None:
        s3_client.create_bucket(Bucket=bucket_name)
    else:
        location = {'LocationConstraint': region}
        s3_client.create_bucket(Bucket=bucket_name,
                                CreateBucketConfiguration=location)


def list_bucket_op(bucket_name, region, operation):
    buckets = s3_client.list_buckets()
    if buckets['Buckets']:
        for bucket in buckets['Buckets']:
            print(bucket)
            return True
    else:
        logging.error('unknown bucket operation')
        return False


def bucket_operation(bucket_name, region=None, operation='list'):
    try:
        if operation == 'delete':
            s3_client.delete_bucket(Bucket=bucket_name)
        elif operation == 'create':
            create_bucket_op(bucket_name, region)
        elif operation == 'list':
            return list_bucket_op(bucket_name, region, operation)
        else:
            logging.error('unknown bucket operation')
            return False
    except ClientError as e:
        logging.error(e)
        return False
    return True


def upload_download_op_file(bucket_name, file_name, file_location,
                            region, operation):
    if not file_location:
        logging.error('The file location %d is missing for %s operation!'
                      % (file_location, operation))
        return False
    if operation == 'download':
        s3_resource.Bucket(bucket_name).download_file(file_name, file_location)
    elif operation == 'upload' and region is None:
        s3_resource.Bucket(bucket_name).upload_file(file_location, file_name)
    else:
        location = {'LocationConstraint': region}
        s3_resource.Bucket(bucket_name
                           ).upload_file(file_location, file_name,
                                         CreateBucketConfiguration=location)
    return True


"""Functions for files operation"""


def list_op_file(bucket_name):
    current_bucket = s3_resource.Bucket(bucket_name)
    print('The files in bucket %s:\n' % (bucket_name))
    for obj in current_bucket.objects.all():
        print(obj.meta.data)

    return True


def delete_op_file(bucket_name, file_name, operation):
    if not file_name:
        logging.error('The file name %s is missing for%s operation!'
                      % (file_name, operation))
        return False
    s3_client.delete_object(Bucket=bucket_name, Key=file_name)
    return True


def file_operation(bucket_name=None, file_name=None, file_location=None,
                   region=None, operation='list'):
    if not bucket_name:
        logging.error('The bucket name is %s missing!' % (bucket_name))
        return False
    try:
        if operation == 'list':
            return list_op_file(bucket_name)
        elif operation == 'delete':
            return delete_op_file(bucket_name, file_name, operation)
        elif operation == 'upload' or operation == 'download':
            return upload_download_op_file(bucket_name, file_name,
                                           file_location, region, operation)
        else:
            logging.error('unknown file operation')
            return False
    except ClientError as e:
        logging.error(e)
        return False
    return True


s3_resource = boto3.resource('s3', endpoint_url=END_POINT_URL,
                             aws_access_key_id=A_KEY,
                             aws_secret_access_key=S_KEY,
                             config=Config(signature_version='s3v4'),
                             region_name='US')

s3_client = boto3.client('s3', endpoint_url=END_POINT_URL,
                         aws_access_key_id=A_KEY,
                         aws_secret_access_key=S_KEY,
                         config=Config(signature_version='s3v4'),
                         region_name='US')

bucket_name = 'detection'
file_name = r'0_5.txt'
# path_file_upload = r'C:\PycharmProjects\cortxHackton\upload\0_5.txt'
# assert os.path.isfile(path_file_upload)
# with open(path_file_upload, "r") as f:
#     pass
path_file_download = r'download\0_5.txt'
path_save = ''

if bucket_operation(bucket_name, None, 'list'):
    print("Bucket creation completed successfully!")
#
# if file_operation(bucket_name, file_name, path_file_upload, None, 'upload'):
#     print("Uploading file to S3 completed successfully!")

if file_operation(bucket_name, file_name, path_file_download, None, 'download'):
    print("Downloading the file to S3 has been completed successfully!")

# if file_operation(bucket_name, file_name, path_file_download, None, 'delete'):
#     print("Downloading the file to S3 has been completed successfully!")

# zip_point = ''
# shutil.make_archive(zip_point, 'zip', path_save)

# if file_operation(bucket_name, '.json', path_save + '.json', None, 'upload'):
#     print("Uploading file to S3 completed successfully!")
