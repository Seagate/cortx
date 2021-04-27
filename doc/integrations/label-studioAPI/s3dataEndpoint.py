import os
import boto3
import logging
from botocore.exceptions import ClientError
from boto3.s3.transfer import TransferConfig
from botocore.client import Config
import progressPercent

class S3DataEndpoint:
    def __init__(self, end_url, accessKey, secretKey):
        self.end_url = end_url
        self.accessKey = accessKey
        self.secretKey = secretKey

        self.s3_resource = boto3.resource('s3', endpoint_url=self.end_url,
                                          aws_access_key_id=self.accessKey,
                                          aws_secret_access_key=self.secretKey,
                                          config=Config(signature_version='s3v4'),
                                          region_name='US')

        # command to access data from default session
        self.s3_client = boto3.client('s3', endpoint_url=self.end_url,
                                      aws_access_key_id=self.accessKey,
                                      aws_secret_access_key=self.secretKey,
                                      config=Config(signature_version='s3v4'),
                                      region_name='US')

    def multipart_upload_with_s3(self, bucket_name, file_path=None, object_name=None):
        # Multipart upload (see notes)
        config = TransferConfig(multipart_threshold=1024 * 25, max_concurrency=10,
                                multipart_chunksize=1024 * 25, use_threads=True)
        key_path = 'multipart_files/{}'.format(object_name)
        print(bucket_name, file_path, object_name, key_path)
        self.s3_client.upload_file(file_path, bucket_name, key_path,
                                   ExtraArgs={'ACL': 'public-read',
                                              'ContentType': 'text/pdf'},
                                   Config=config, Callback=progressPercent.ProgressPercentage(file_path))

    def multipart_download_with_s3(self, bucket_name, file_path=None, object_name=None):
        config = TransferConfig(multipart_threshold=1024 * 25, max_concurrency=10,
                                multipart_chunksize=1024 * 25, use_threads=True)
        temp_file = os.path.dirname(__file__)
        self.s3_resource.Object(bucket_name,
                                object_name
                                ).download_file(file_path, Config=config,
                                                Callback=progressPercent.ProgressPercentage(temp_file))

    # Functions for buckets operation
    def create_bucket_op(self, bucket_name, region):
        if region is None:
            self.s3_client.create_bucket(Bucket=bucket_name)
        else:
            location = {'LocationConstraint': region}
            self.s3_client.create_bucket(Bucket=bucket_name,
                                         CreateBucketConfiguration=location)

    def list_bucket_op(self, bucket_name, region, operation):
        buckets = self.s3_client.list_buckets()
        if buckets['Buckets']:
            for bucket in buckets['Buckets']:
                print(bucket)
                return True
        else:
            logging.error('unknown bucket operation')
            return False

    def bucket_operation(self, bucket_name, region=None, operation='list'):
        try:
            if operation == 'delete':
                self.s3_client.delete_bucket(Bucket=bucket_name)
            elif operation == 'create':
                self.create_bucket_op(bucket_name, region)
            elif operation == 'list':
                return self.list_bucket_op(bucket_name, region, operation)
            else:
                logging.error('unknown bucket operation')
                return False
        except ClientError as e:
            logging.error(e)
            return False
        return True

    # Functions for objects operation
    def list_object_op(self, bucket_name):
        self.s3_objects = self.s3_client.list_objects_v2(Bucket=bucket_name)
        if self.s3_objects.get('Contents'):
            for obj in self.s3_objects[ 'Contents' ]:
                print(obj)

    def delete_object_op(self, bucket_name, object_name, operation):
        if not object_name:
            logging.error('object_name missing for {}'.format(operation))
            return False
        self.s3_client.delete_object(Bucket=bucket_name, Key=object_name)
        return True

    def upload_download_object_op(self, bucket_name, object_name, file_path, operation):
        if not file_path or not object_name:
            logging.error('file_path and/or object_name missing for upload')
            return False
        if operation == 'upload':
            self.multipart_upload_with_s3(bucket_name=bucket_name, file_path=file_path,
                                          object_name=object_name)
        else:
            self.multipart_download_with_s3(bucket_name=bucket_name, file_path=file_path,
                                            object_name=object_name)
        return True

    def object_operation(self, bucket_name=None, object_name=None, file_path=None,
                         operation='list'):
        try:
            if not bucket_name:
                logging.error('The bucket name %s is missing for %s operation!'
                              % (bucket_name, operation))
                return False
            if operation == 'list':
                self.list_object_op(bucket_name)
            elif operation == 'delete':
                return self.delete_object_op(bucket_name, object_name, operation)
            elif operation == 'upload' or operation == 'download':
                return self.upload_download_object_op(bucket_name, object_name,
                                                      file_path, operation)
            else:
                logging.error('unknown object operation')
                return False
        except ClientError as e:
            logging.error(e)
            return False
        return True

    # Functions for files operation

    def list_op_file(self, bucket_name):
        current_bucket = self.s3_resource.Bucket(bucket_name)
        print('The files in bucket %s:\n' % bucket_name)
        for obj in current_bucket.objects.all():
            print(obj.meta.data)

        return True

    def delete_op_file(self, bucket_name, file_name, operation):
        if not file_name:
            logging.error('The file name %s is missing for%s operation!'
                          % (file_name, operation))
            return False
        self.s3_client.delete_object(Bucket=bucket_name, Key=file_name)
        return True

    def upload_download_op_file(self, bucket_name, file_name, file_location,
                                region, operation):
        if not file_location:
            logging.error('The file location %d is missing for %s operation!'
                          % (file_location, operation))
            return False
        if operation == 'download':
            self.s3_resource.Bucket(bucket_name).download_file(file_name, file_location)
        elif operation == 'upload' and region is None:
            self.s3_resource.Bucket(bucket_name).upload_file(file_location, file_name)
        else:
            location = {'LocationConstraint': region}
            self.s3_resource.Bucket(bucket_name).upload_file(file_location, file_name,
                                                             CreateBucketConfiguration=location)
        return True

    def file_operation(self, bucket_name=None, file_name=None, file_location=None,
                       region=None, operation='list'):
        if not bucket_name:
            logging.error('The bucket name is %s missing!' % bucket_name)
            return False
        try:
            if operation == 'list':
                return self.list_op_file(bucket_name)
            elif operation == 'delete':
                return self.delete_op_file(bucket_name, file_name, operation)
            elif operation == 'upload' or operation == 'download':
                return self.upload_download_op_file(bucket_name, file_name,
                                                    file_location, region, operation)
            else:
                logging.error('unknown file operation')
                return False
        except ClientError as e:
            logging.error(e)
            return False
        return True

# dummy calls
def main():
    END_POINT_URL = 'http://uvo100ebn7cuuq50c0t.vm.cld.sr'
    A_KEY = 'AKIAtEpiGWUcQIelPRlD1Pi6xQ'
    S_KEY = 'YNV6xS8lXnCTGSy1x2vGkmGnmdJbZSapNXaSaRhK'

    s3 = S3DataEndpoint(end_url=END_POINT_URL, accessKey=A_KEY, secretKey=S_KEY)

    # bucket_name2 = 'otodata'
    # bucket_name = 'testbucket'
    # file_name = 'bee39.wav'
    # file_name2 = 'aom (1).png'
    #
    # path_file_upload2 = '/home/sumit/Documents/Oto-DataSet/aom (1).png'
    # path_file_download = 'bee39.wav'

    # if s3.file_operation(bucket_name2, file_name2, path_file_upload2, None, 'upload'):
    #     print("Uploading file to S3 completed successfully!")
    #

    # if s3.file_operation(bucket_name, file_name, path_file_download, None, 'download'):
    #     print("Downloading the file to S3 has been completed successfully!")


if __name__ == "__main__":
    main()
