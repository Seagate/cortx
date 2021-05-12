"""
cortx_dicom

Example.
"""

# Modules from the standard library which are used in this example only
from random import shuffle
from time import sleep

# Modules from the standard library which are used by the module too
from os import listdir
from os.path import isfile, join

# Import additional library which is used by the module too
import pydicom

# Importing the module
from cortx_dicom import Config, CortxDicom


DEMO_COUNT = 5
DICOM_FOLDER = 'dicoms'
TEST_BUCKET = 'dicom-test'


print('*** Preparation ***')
# Loading configuration
# PLEASE: don't forget to change settings according to your circumstances.
Config.load('config.json', Config.FILE_JSON)
# Instantiate CortxDicom
cortxdicom = CortxDicom()

# Create a bucket to test
buckets = cortxdicom.s3_buckets__()
if TEST_BUCKET not in buckets:
    cortxdicom.s3_engine.create_bucket(Bucket=TEST_BUCKET)

# Making filelist to create really random test
filelist = [f for f in listdir(DICOM_FOLDER) if isfile(join(DICOM_FOLDER, f))]
len_filelist = len(filelist)
if len_filelist < DEMO_COUNT:
    REAL_COUNT = len_filelist
else:
    REAL_COUNT = DEMO_COUNT
print('There are {} files in the dicoms folder, {} will be randomly selected.'
      .format(len_filelist, REAL_COUNT))

# Storing and basic searching
print('*** Storage demonstration ***')
shuffle(filelist)
es_ids, s3_ids = [], []
for i in range(REAL_COUNT):
    dicomfile = pydicom.dcmread(join(DICOM_FOLDER, filelist[i]))
    len_before = len(dicomfile)
    CortxDicom.apply_filter(dicomfile, CortxDicom.FILTER_HIPAA)
    len_after = len(dicomfile)
    print('File {}/{}: {} fields removed to fit HIPAA regulation ({}/{})'
          .format(i, REAL_COUNT, len_before - len_after, len_after, len_before))
    es_id = cortxdicom.store(dicomfile, bucket=TEST_BUCKET)
    if es_id is None:
        print('Something went wrong at storing.')
        continue
    _result = cortxdicom.search(es_id, just_find=False)
    if _result is None:
        print('Something went wrong at searching.')
        continue
    s3_id = _result[1]
    es_ids.append(es_id)
    s3_ids.append(s3_id)
    print('File stored as "{}" in S3, information as "{}" in elasticsearch.'
          .format(s3_id, es_id))

print('*** Pause - 15 seconds to let VM doing its jobs. ***')
sleep(15)

print('*** Search demonstration - Elasticsearch side ***')
# This query always returns data
result = cortxdicom.search({"query": {"match_all": {}}})
print('If you want to know about potential matches, the result is {}.'
      .format(result))
result = cortxdicom.search({"query": {"match_all": {}}}, just_find=False)
if result is None:
    print('No match found.')
else:
    print('Result of the search')
    for row in result:
        print(row)
print('Difference between CortxDicom.search() and CortxDicom.get().')
result = cortxdicom.get({"query": {"match_all": {}}})
if result is None:
    print('No match found.')
else:
    print('Result of the search')
    for row in result:
        print(type(row))

print('*** Search demonstration - S3 side ***')
result = cortxdicom.get(object_key=s3_ids[0], bucket=TEST_BUCKET)
if result is None:
    print('No match found.')
else:
    print('DICOM file:')
    print(result)

print('*** Cleaning up everything ***')
cortxdicom.es_engine.indices.delete(index=CortxDicom.ES_DEFAULT_INDEX,
                                    ignore=[400, 404])
buckets = cortxdicom.s3_buckets__()
if TEST_BUCKET in buckets:
    dicomfiles = cortxdicom.s3_objects__(TEST_BUCKET)
    for dicomfile in dicomfiles:
        cortxdicom.s3_engine.delete_object(Bucket=TEST_BUCKET, Key=dicomfile)
    cortxdicom.s3_engine.delete_bucket(Bucket=TEST_BUCKET)

print('*** The End - Thanks for running me. ***')
