from cortx_pytorch import upload_cv_dataset, make_client
from torchvision import datasets

if __name__ == '__main__':
    # Define the connection settings for our client
    client = make_client('http://uvo1zt5bd36q8lnpovx.vm.cld.sr',
            'AKIAtEpiGWUcQIelPRlD1Pi6xQ', 'YNV6xS8lXnCTGSy1x2vGkmGnmdJbZSapNXaSaRhK')

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
                      maxcount=100000, workers=30)