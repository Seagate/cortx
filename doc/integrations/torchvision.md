# Pytorch datasets

Motivation/demo video: https://youtu.be/L0UH-fRhyDU 

## Motivation

Deep learning datasets are most of the time constituted of many small files and their associated label. Models are then repeatedly trained on random small subset of the datasets (for example `BS`=256 samples). The usual workflow to generate a batch of data is the following:

1. Sample `BS` file names from the dataset
2. Open each individual file from the disc
3. Process files (decode/resize/apply transformations)
4. Concatenate all images
5. train/evaluate the model on the image

This approached worked great for a while, however, recently, it has become clear that large datasets are superior to small ones. As a result, ML practitionners have been using larger and lager datasets and they became so big that storing them on a local machine became too expensive. One way to solve this cost problem is to store the dataset on a dedicated storage node filled with cheap HDDs and have machines doing the model training collect the data they need from the storage node.

While this aproach is radically cheaper and more scalabe, it yields to two major performance issues:

- For a single training iteration we need to collect many images (eg. 256). Since each image is loaded individually and has to be queried through the network and since images are small most the time is spent in network overhead and not actually transferring the data.
- Because we read small images in random order they end up in random locations in the platter of the HDDs. It means that most of the time is spent moving the head of the HDD to the next location of interest and not actually reading.

These two issue combined tank the performance so much that it becomes impossible for the machines to obtain data at a rate that is sufficient to train deep learning models.

## Solution

The key ideas behind the solution is the following: since files are too small we need to group them together in shards. It solves the two issues:

- Because files are groupped, a single network call retrieves a lot of data. Hence, the time lost doing networking is lost once per shard instead of once per image.
- Similarly, the time lost moving between different locations on the HDD platter is lost once per shard instead of once per image.

The workflow becomes:

1. Pre-process the dataset (done a single time)
    1. Iterate through the dataset in a random order
    2. Group the images in shards of given size
    3. Compress and concatenate the content in the shard
    4. Upload the shard on CORTX
2. Read the dataset
    1. Iterate through the shards in random order
    2. Unpack/uncompress each shard
    3. Group a couple of shards together and shuffle them to obtain a different random order every time

## Usage

This workflow has been implemented for computer vision dataset in pytorch in the following repo: https://github.com/GuillaumeLeclerc/cortx_pytorch


### 1: Install

For this submission we created a `pip` package so that it is as straightforward to use as possible:

```
pip install cortx_pytorch
```

### 2: Convert and upload your dataset

The example is here for locally sourced ImageNet but the library supports any Pytorch computer vision dataset

```python
from cortx_pytorch import upload_cv_dataset, make_client
from torchvision import datasets

if __name__ == '__main__':
    # Define the connection settings for our client
    client = make_client(URL, ACCESS_KEY, SECRET_KEY)


    bucket = 'testbucket'  # Bucket where to read/write our ML dataset
    folder = 'imagenet-val'  # Folder where this particular dataset will be

    # We use a pytorch dataset as a source to prime the content of CORTX
    # Once we have encoded and uploaded it we don't need it anymore
    # Here we use a locally available Imagenet dataset
    ds = ds = datasets.ImageFolder('/scratch/datasets/imagenet-pytorch/val')

    # Packs and upload any computer vision dataset on cortx
    #
    # It only needs to be done once !
    # Image are groupped in objects of size at most `masize` and at most
    # `maxcount` images. We use `workers` processes to prepare the data
    # in parallel
    upload_cv_dataset(ds, client=client, bucket=bucket,
                      base_folder=folder, maxsize=1e8,
                      maxcount=100000, workers=30
```

### 3: Use the dataset like any pytorch dataset

```python
import torch as ch
from tqdm import tqdm

from cortx_pytorch import RemoteDataset, make_client
from torchvision import transforms

preproc = transforms.Compose([
    transforms.RandomResizedCrop(224),
    transforms.RandomHorizontalFlip(),
    transforms.ToTensor(),
])
        
if __name__ == '__main__':

    # Define the connection settings for our client
    client = make_client(URL, ACCESS_KEY, SECRET_KEY)

    bucket = 'testbucket'  # Bucket where to read/write our ML dataset
    folder = 'imagenet-val'  # Folder where this particular dataset will be
    
    # Now that we have created and upload the dataset on CORTX we can use
    # it in Pytorch
    dataset = (RemoteDataset(client, bucket, folder)
        .decode("pil") # Decode the data as PIL images
        .to_tuple("jpg;png", "cls") # Extract images and labels from the dataset
        .map_tuple(preproc, lambda x: x) # Apply data augmentations
        .batched(64)  # Make batches of 64 images
    )
    # We create a regular pytorch data loader as we would do for regular data sets
    dataloader = ch.utils.data.DataLoader(dataset, num_workers=3, batch_size=None)
    for image, label in tqdm((x for x in dataloader), total = 100000 / 60):
        # Train / evaluate ML models on this batch of data
        pass

```
