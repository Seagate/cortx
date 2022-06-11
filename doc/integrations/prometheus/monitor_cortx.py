#!/usr/bin/env python3

import boto3
from collections import defaultdict
import datetime

def run_monitoring(name):
    endpoint_url = 'http://127.0.0.1'
    s3 = boto3.resource('s3',
        aws_access_key_id='nADhGwhbSwKOCmEwvdnncA',
        aws_secret_access_key='klBIOoTa6tfzeo2q3f4nWmvBYhlvKTlvAJs7EKpz',
        endpoint_url=endpoint_url)
    bucket = s3.Bucket('gtbucket')
    now = datetime.datetime.utcnow()
    seconds_since_epoch = int(now.timestamp())
    counts = defaultdict(lambda: 0)
    for obj in bucket.objects.all():
        key = obj.key
        if key.endswith('/'):
            continue
        key = obj.key
        parts = key.split('/')
        folder = parts[0]
        counts[folder] = counts[folder] + 1
    print_counts(dict(counts), seconds_since_epoch )


def print_counts(counts: dict, seconds_since_epoch: int):
    for folder, items_count in counts.items():
        print_stat(folder, items_count, seconds_since_epoch)


def print_stat(folder, items_count, seconds_since_epoch: int):
    print(f'objects_count,folder={folder} count={items_count} {seconds_since_epoch}')


if __name__ == '__main__':
    run_monitoring('PyCharm')

