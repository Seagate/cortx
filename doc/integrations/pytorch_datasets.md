# Pytorch datasets

## Motivation

Deep learning datasets are most of the time constituted of many small files and their associated label. Models are then repeatedly trained on random small subset of the datasets (for example `BS`=256 samples). The usual workflow to generate a batch of data is the following:

1. Sample `BS` file names from the dataset
2. Open each individual file from the disc
3. Process files (decode/resize/apply transformations)
4. Concatenate all images
5. train/evaluate the model on the image

This approached worked great for a while, however, recently, it has become clear that large datasets are superior to small ones. As a result, ML practitionners have been using larger and lager datasets and they became so big that storing them on a local machine became too expensive. One way to solve this cost problem is to store the dataset on a dedicated storage node filled with cheap HDDs have machines doing the model training collect the data they need from the storage node.

Which this aproach is radically cheaper and more scalabe it yields to two major performance issues:

- For a single training iteration we need to collect many images (eg. 256). Since each image is loaded individually and has to be queried through the network and since images are small most the time is spent in network overhead and not actually transferring the data.
- Because we read small images in random order they end up in random locations in the platter of the HDDs. It means that most of the time is spent moving the head of the HDD to the next location of interest and not actually reading.

These two issue combined tank the performance so much that it becomes impossible for the machines to obtain data at a rate that is sufficient to train deep learning models.
