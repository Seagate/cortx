# Bari Arviv
import os
import sys
import boto3
import logging
import shutil
import threading
import tensorflow as tf
from botocore.client import Config
from matplotlib import pyplot as plt
from botocore.exceptions import ClientError
from boto3.s3.transfer import TransferConfig
from tensorflow.keras import datasets, layers, models, losses

END_POINT_URL = 'http://uvo120ezuu4wffvmfzd.vm.cld.sr'
A_KEY = 'GRiXYvvRT7u3QwCONd-6Pg'
S_KEY = 'sJskgwjcq8NiAk7FuM8ngqR3iSfAr36KAg8Xxo66'

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

def multipart_upload_with_s3(bucket_name, file_path=None, object_name=None):
    # Multipart upload (see notes)
    config = TransferConfig(multipart_threshold=1024 * 25, max_concurrency=10,
                            multipart_chunksize=1024 * 25, use_threads=True)
    key_path = 'multipart_files/{}'.format(object_name)
    print(bucket_name,file_path,object_name,key_path)
    s3_client.upload_file(file_path, bucket_name, key_path,
                          ExtraArgs={'ACL': 'public-read', 
                                     'ContentType': 'text/pdf'},
                          Config=config, Callback=ProgressPercentage(file_path))
    
def multipart_download_with_s3(bucket_name, file_path=None, object_name=None):
    config = TransferConfig(multipart_threshold=1024 * 25, max_concurrency=10,
                            multipart_chunksize=1024 * 25, use_threads=True)
    temp_file = os.path.dirname(__file__)
    s3_resource.Object(bucket_name, 
                       object_name
                       ).download_file(file_path, Config=config,
                                       Callback=ProgressPercentage(temp_file))

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

"""Functions for objects operation"""
def list_object_op(bucket_name):
     s3_objects = s3_client.list_objects_v2(Bucket=bucket_name)
     if s3_objects.get('Contents'):
         for obj in s3_objects['Contents']:
             print(obj)

def delete_object_op(bucket_name, object_name, operation):
    if not object_name:
        logging.error('object_name missing for {}'.format(operation))
        return False  
    s3_client.delete_object(Bucket=bucket_name, Key=object_name)
    return True

def upload_download_object_op(bucket_name, object_name, file_path, operation):
    if not file_path or not object_name:
        logging.error('file_path and/or object_name missing for upload')
        return False
    if operation == 'upload':
        multipart_upload_with_s3(bucket_name=bucket_name, file_path=file_path,
                                 object_name=object_name)
    else:
        multipart_download_with_s3(bucket_name=bucket_name, file_path=file_path,
                                   object_name=object_name)
    return True

def object_operation(bucket_name=None, object_name=None, file_path=None,
                     operation='list'):                                                             
    try:
        if not bucket_name:
            logging.error('The bucket name %s is missing for %s operation!'
                          % (bucket_name, operation))
            return False
        if operation == 'list':
            list_object_op(bucket_name)
        elif operation == 'delete':
            return delete_object_op(bucket_name, object_name, operation)
        elif operation == 'upload' or operation == 'download':
            return upload_download_object_op(bucket_name, object_name,
                                             file_path, operation)      
        else:
            logging.error('unknown object operation')
            return False
    except ClientError as e:
        logging.error(e)
        return False
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

# client is swiss knife , resource has all sort of data:
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

# simple MNIST database
file_name = 'mnist.npz'
bucket_name = 'mnist1234'
path_file_upload = 'C:/mnist.npz'
#path_file_download = 'C:/Users/barar/.spyder-py3/tmp/MNIST/mnist.npz'
path_file_download = 'C:/temp/MNIST/mnist.npz'

if bucket_operation(bucket_name, None, 'create'):
    print("Bucket creation completed successfully!")

if file_operation(bucket_name, file_name, path_file_upload, None, 'upload'):
    print("Uploading file to S3 completed successfully!")
    
if file_operation(bucket_name, file_name, path_file_download, None, 'download'):
    print("Downloading the file to S3 has been completed successfully!")

mnist = datasets.mnist

(x_train, y_train), (x_test, y_test) = mnist.load_data(path=path_file_download)
x_train, x_test = x_train / 255.0, x_test / 255.0

plt.figure(figsize=(5,5))
for k in range(12):
    plt.subplot(3, 4, k + 1)
    plt.imshow(x_train[k], cmap='Greys')
    plt.axis('off')
plt.tight_layout()
plt.show()

model = models.Sequential([
  layers.Flatten(input_shape=(28, 28)),
  layers.Dense(128, activation='relu'),
  layers.Dropout(0.2),
  layers.Dense(10)
])

model.summary()

predictions = model(x_train[:1]).numpy()
print('The prediction is: {}'.format(predictions))

tf.nn.softmax(predictions).numpy()

loss_fn = losses.SparseCategoricalCrossentropy(from_logits=True)

loss_fn(y_train[:1], predictions).numpy()

model.compile(optimizer='adam', loss=loss_fn, metrics=['accuracy'])

history = model.fit(x_train, y_train, epochs=5, 
                    validation_data=(x_test, y_test))

plt.plot(history.history['accuracy'], label='accuracy')
plt.plot(history.history['val_accuracy'], label = 'val_accuracy')
plt.xlabel('Epoch')
plt.ylabel('Accuracy')
plt.ylim([0.5, 1])
plt.legend(loc='lower right')

test_loss, test_acc = model.evaluate(x_test,  y_test, verbose=2)
print('Accuracy: {}'.format(test_acc))

probability_model = tf.keras.Sequential([model, layers.Softmax()])
probability_model(x_test[:5])

path_save = 'C:/temp/modelMNIST/'
model.save(path_save)

if file_operation(bucket_name, 'saved_model.pb', path_save + 'saved_model.pb',
                  None, 'upload'):
    print("Uploading file to S3 completed successfully!")
    
zip_point = 'C:/temp/model' 
shutil.make_archive(zip_point, 'zip', path_save)

if file_operation(bucket_name, 'model.zip', 'C:/temp/model.zip',
                  None, 'upload'):
    print("Uploading file to S3 completed successfully!")