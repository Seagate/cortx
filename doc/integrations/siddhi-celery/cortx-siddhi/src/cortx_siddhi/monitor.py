import logging
import time
import boto3

from . import config

SCAN_INTERVAL = 1 * 60
SCAN_INTERVAL = 5

class S3Monitor():
    def __init__(self):
        self._cache = {}

    @staticmethod
    def compare_buckets(previous_buckets, buckets):
        return buckets.difference(previous_buckets), previous_buckets.difference(buckets)

    @staticmethod
    def get_buckets(client):
        query_result = client.list_buckets()['Buckets']
        result = set((bucket['Name'], bucket['CreationDate']) for bucket in query_result)
        return result

    def monitor_buckets(self, siddhi_input_handler):
        client = config.get_client()
        previous_buckets = S3Monitor.get_buckets(client)

        while True:
            time.sleep(SCAN_INTERVAL)
            try:
                buckets = S3Monitor.get_buckets(client)
            except boto3.exceptions.botocore.errorfactory.ClientError:
                buckets = None

            if buckets is not None:
                created_buckets, deleted_buckets = S3Monitor.compare_buckets(previous_buckets, buckets)
                for bucket in created_buckets:
                    siddhi_input_handler.send(['BUCKET_CREATED', bucket[0], ''])

                for bucket in deleted_buckets:
                    siddhi_input_handler.send(['BUCKET_DELETED', bucket[0], ''])

                previous_buckets = buckets

            self.monitor_objects_in_bucket(siddhi_input_handler, 'logs')


    def monitor_objects_in_bucket(self, siddhi_input_handler, bucket):
        client = config.get_client()
        try:
            response = client.list_objects(Bucket=bucket)
        except boto3.exceptions.botocore.errorfactory.ClientError:
            return

        if 'Contents' not in response:
            self._cache[bucket] = set()
            return

        objects_set = set(s3_object['Key'] for s3_object in response['Contents'])

        if bucket not in self._cache:
            self._cache[bucket] = objects_set
            return

        for created_object in objects_set.difference(self._cache[bucket]):
            siddhi_input_handler.send(['OBJECT_CREATED', bucket, created_object])

        for deleted_object in self._cache[bucket].difference(objects_set):
            siddhi_input_handler.send(['OBJECT_REMOVED', bucket, deleted_object])

        self._cache[bucket] = objects_set


if __name__ == '__main__':
    main()
    
