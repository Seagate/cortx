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
    client = make_client('http://uvo1zt5bd36q8lnpovx.vm.cld.sr',
            'AKIAtEpiGWUcQIelPRlD1Pi6xQ', 'YNV6xS8lXnCTGSy1x2vGkmGnmdJbZSapNXaSaRhK')

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
