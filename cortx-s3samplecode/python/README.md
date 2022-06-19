CORTX S3 Sample Code - python
==============================

Prerequisites
---------------------
* **python 3.7**
```
pip install boto3
pip install botocore
```

**Enter the details for the connection to the s3 instance:**
* region
* access key
* secret access key
* end point URL
* signature version

Full Script
---------------------
Sample script name - [cortx-s3samplecode-python.py](cortx-s3samplecode-python.py)

Sessions Setup
---------------------
```python
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
```

Create Bucket
---------------------
```python
s3_client.create_bucket(Bucket=bucket_name)
```

Delete Bucket
---------------------
```python
s3_client.delete_bucket(Bucket=bucket_name)
```

List Buckets
---------------------
```python
buckets = s3_client.list_buckets()

if buckets['Buckets']:
    for bucket in buckets['Buckets']:
        print(bucket)
```

Put/Upload Object
---------------------
```python
s3_resource.Bucket(bucket_name).upload_file(file_location, file_name)
```

Get/Download Object
---------------------
```python
s3_resource.Bucket(bucket_name).download_file(file_name, file_location) 
```

Delete Object
---------------------
```python
s3_client.delete_object(Bucket=bucket_name, Key=file_name)
```

List Objects
---------------------
```python
current_bucket = s3_resource.Bucket(bucket_name)
print('The files in bucket %s:\n' % (bucket_name))

for obj in current_bucket.objects.all():
    print(obj.meta.data) 
```

### Tested By:
* Sep 18, 2021: Bo Wei (bo.b.wei@seagate.com)
* August 2, 2021: Bari Arviv (bararviv0120@gmail.com | bari.arviv@seagate.com)

