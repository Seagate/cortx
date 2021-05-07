**What is tensorflow?** tensorflow is an end-to-end open source machine learning platform. The core open source library to help you develop and train ML models. More information can be found here: https://www.tensorflow.org/

**What is CORTX?** CORTX is a distributed object storage system designed for great efficiency, massive capacity, and high HDD-utilization. CORTX is 100% Open Source.

**How do CORTX and tensorflow work together?** The most straightforward way is to use CORTX to store and retrieve your datasets. Tensorflow also supports reading and writing data to S3. S3 is an object storage API which is nearly ubiquitous, and can help in situations where data must accessed by multiple actors, such as in distributed training. 
We will use Boto3 to connect tensorflow to CORTX.

**What is Boto3?** Boto3 is a SDK for Python that enables developers to create, configure and manage S3 compatible services. Because CORTX is S3 compatible we can use Boto3 to connect tensorflwo to CORTX. 

* * *

# Configuring tensorflow to work with CORTX using boto3

## Pre-requisites:
1) Create a bucket in CORTX
2) Upload dataset in CORTX
3) Get credentials on how to access your s3 bucket including **Endpoint URL, Access Key ID, Secret Access Key.**

### Working with boto3 in a tensorflow python application

Step 1: Install boto3
```pip install boto3```

Step 2: Import boto3 into your tensorflow application

```
import boto3
from botocore.client import Config
```

Step 3: Configure boto3 resource to point to CORTX instance.
```
s3 = boto3.resource('s3',
                    endpoint_url='http://endpoint.com',
                    aws_access_key_id='<ACCESS_KEY>',
                    aws_secret_access_key='<SECRET_ACCESS_KEY>')
```

Step 4:  Download dataset into tensorflow application
```s3.Bucket('tensorflow').download_file('mnist.npz', '/tmp/CORTX_mnist.npz')```

* * *
### Example: 

Check out this jupyter notebook example of using tensorflow with CORTX using boto3.
