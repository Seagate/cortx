"""
cortx_dof

Example.
"""


# Modules from the standard library which are used in this example only
from time import sleep

# Import additional libraries which are used by the module too
from dof.data import DataElement
from dof.file import DofFile
from dof.information import ContainerInfo, ModelInfo

# Import additional libraries which are not used by the module too
import torch
from torchvision import datasets, models, transforms

# Importing the module
from cortx_dof import Config, CortxDof


DEMO_COUNT = 5
TEST_BUCKET = 'dof-test'


print('*** Preparation ***')
# Loading configuration
# PLEASE: don't forget to change settings according to your circumstances.
Config.load('config.json', Config.FILE_JSON)
# Instantiate cortxdof
cortxdof = CortxDof()

# Create a bucket to test
buckets = cortxdof.s3_buckets__()
if TEST_BUCKET not in buckets:
    cortxdof.s3_engine.create_bucket(Bucket=TEST_BUCKET)

# Prepare to device agnostic code
device = 'cuda' if torch.cuda.is_available() else 'cpu'

# Id containers
es_ids, s3_ids = [], []

print('PyTorch will use {}'.format(device))

print('*** Stage 1 - raw dataset ***')
# MNIST will mock NASA's Mars dataset in this example
raw_transformer = transforms.Compose([transforms.ToTensor()])
raw_dataset = datasets.MNIST('./raw_dataset', download=True,
                             transform=raw_transformer)
raw_loader = torch.utils.data.DataLoader(raw_dataset, shuffle=True)

len_raw = len(raw_loader)
if len_raw < DEMO_COUNT:
    REAL_COUNT = len_raw
else:
    REAL_COUNT = DEMO_COUNT
print('There are {} files in the MNIST dataset, {} will be randomly selected.'
      .format(len_raw, REAL_COUNT))

# Creating a separate ContainerInfo will be useful in the future
info_core = ContainerInfo()
info_core['author'] = 'Test Author'
info_core['author_contact'] = 'email@example.com'
info_core['source'] = 'https://pytorch.org/vision/stable/datasets.html#mnist'
info_core['license'] = 'Creative Commons Attribution-Share Alike 3.0'
info_core['dataset'] = 'raw dataset'

# DoF file for raw dataset
dof_raw = DofFile(info_core)

# Let's get some raw data
for i, (x, y) in enumerate(raw_loader):
    dof_x = DataElement(x, DataElement.X)
    dof_y = DataElement(y, DataElement.Y)
    id_x = dof_raw.dataset.add_element(dof_x)
    id_y = dof_raw.dataset.add_element(dof_y)
    id_x = dof_raw.dataset.linker.link(id_x, id_y)
    if i >= REAL_COUNT:
        break

# Save DoF file to cortx
es_id = cortxdof.store(dof_raw, bucket=TEST_BUCKET)
if es_id is None:
    print('Something went wrong at storing.')
else:
    _result = cortxdof.search(es_id, just_find=False)
    if _result is None:
        print('Something went wrong at searching.')
    else:
        s3_id = _result[1]
        print('Raw dataset is stored. S3: "{}", elasticsearch: "{}"'
              .format(s3_id, es_id))
        es_ids.append(es_id)
        s3_ids.append(s3_id)

# Some cleanup to spare memory
del dof_raw
del raw_loader
del raw_dataset
del raw_transformer

print('*** Stage 2 - preprocessed dataset ***')
preprocessing_transformer = transforms.Compose([transforms.ToTensor(),
                                transforms.Resize(224),
                                transforms.Normalize((0.1307,), (0.3081,))])
preprocessed_dataset = datasets.MNIST('./raw_dataset', download=True,
                             transform=preprocessing_transformer)
preprocessed_loader = torch.utils.data.DataLoader(preprocessed_dataset,
                                                  shuffle=True)

# Since we already have a ContainerInfo we just have update it a bit
info_core['dataset'] = 'preprocessed dataset'

# Dof file for preprocessed dataset
dof_preprocessed = DofFile(info_core)

# Let's create preprocessed images
for i, (x, y) in enumerate(preprocessed_loader):
    dof_x = DataElement(x, DataElement.X)
    dof_y = DataElement(y, DataElement.Y)
    id_x = dof_preprocessed.dataset.add_element(dof_x)
    id_y = dof_preprocessed.dataset.add_element(dof_y)
    id_x = dof_preprocessed.dataset.linker.link(id_x, id_y)
    if i >= REAL_COUNT:
        break

# Save DoF file to cortx
es_id = cortxdof.store(dof_preprocessed, bucket=TEST_BUCKET)
if es_id is None:
    print('Something went wrong at storing.')
else:
    _result = cortxdof.search(es_id, just_find=False)
    if _result is None:
        print('Something went wrong at searching.')
    else:
        s3_id = _result[1]
        print('Preprocessed dataset is stored. S3: "{}", elasticsearch: "{}"'
              .format(s3_id, es_id))
        es_ids.append(es_id)
        s3_ids.append(s3_id)

# Some cleanup to spare memory
del dof_preprocessed

print('*** Stage 2 - preprocessed dataset ***')
# Get a pretrained model
vgg16 = models.vgg16(pretrained=True)

# Get to know the model better
print('VGG-16 is a well trained model and it looks like this:')
print(vgg16)

# Remove the whole classifier - removing just the last layer is also common
vgg16.classifier = torch.nn.Sequential()
print('VGG-16 without full classifier layer looks like this:')
print(vgg16)

# ContainerInfo is environment-friendly, it can be re-used again and again
info_core['dataset'] = 'headles model outputs'

# Dof file for headless model outputs
dof_headless = DofFile(info_core)

# DoF file's core purpose is spare resources, therefore it's time set-up some extra
# features
dof_headless.dataset.is_dof = True
dof_headless.model_info.add(ModelInfo.MODEL, 'model_name', 'VGG-16 headless')
dof_headless.model_info.add(ModelInfo.MODEL, 'original_framework', 'PyTorch')
dof_headless.model_info.add(ModelInfo.MODEL, 'trainable_parameters', 'None')
dof_headless.model_info.add(ModelInfo.MODEL, 'intended_use_statement',
                            'NASA Mars challenge')

# Let's create some headless output
for i, (x, y) in enumerate(preprocessed_loader):
    x = x.repeat(1, 3, 1, 1)
    output = vgg16(x)
    dof_x = DataElement(output, DataElement.X)
    dof_y = DataElement(y, DataElement.Y)
    id_x = dof_headless.dataset.add_element(dof_x)
    id_y = dof_headless.dataset.add_element(dof_y)
    id_x = dof_headless.dataset.linker.link(id_x, id_y)
    if i >= REAL_COUNT:
        break

# Save DoF file to cortx
es_id = cortxdof.store(dof_headless, bucket=TEST_BUCKET)
if es_id is None:
    print('Something went wrong at storing.')
else:
    _result = cortxdof.search(es_id, just_find=False)
    if _result is None:
        print('Something went wrong at searching.')
    else:
        s3_id = _result[1]
        print('Headless output dataset is stored. S3: "{}", elasticsearch: "{}"'
              .format(s3_id, es_id))
        es_ids.append(es_id)
        s3_ids.append(s3_id)

print('*** Pause - 15 seconds to let VM doing its jobs. ***')
sleep(15)

print('*** Search demonstration - Elasticsearch side ***')
# This query always returns data
result = cortxdof.search({"query": {"match_all": {}}})
print('If you want to know about potential matches, the result is {}.'
      .format(result))
result = cortxdof.search({"query": {"match_all": {}}}, just_find=False)
if result is None:
    print('No match found.')
else:
    print('Result of the search')
    for row in result:
        print(row)
print('Difference between CortxDicom.search() and CortxDicom.get().')
result = cortxdof.get({"query": {"match_all": {}}})
if result is None:
    print('No match found.')
else:
    print('Result of the search')
    for row in result:
        print(type(row))

print('*** Search demonstration - S3 side ***')
result = cortxdof.get(object_key=s3_ids[0], bucket=TEST_BUCKET)
if result is None:
    print('No match found.')
else:
    print('DoF file:')
    print(result)

print('*** Cleaning up everything ***')
cortxdof.es_engine.indices.delete(index=CortxDof.ES_DEFAULT_INDEX,
                                    ignore=[400, 404])
buckets = cortxdof.s3_buckets__()
if TEST_BUCKET in buckets:
    dicomfiles = cortxdof.s3_objects__(TEST_BUCKET)
    for dicomfile in dicomfiles:
        cortxdof.s3_engine.delete_object(Bucket=TEST_BUCKET, Key=dicomfile)
    cortxdof.s3_engine.delete_bucket(Bucket=TEST_BUCKET)

print('*** The End - Thanks for running me. ***')
