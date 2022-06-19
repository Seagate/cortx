import tempfile
from functools import partial
import io
from os import path

import torch as ch
from tqdm import tqdm
import webdataset as wds
import boto3
from botocore.client import Config


def make_client(end_point, access_key, secure_key):
    client = boto3.client('s3', endpoint_url=end_point,
                             aws_access_key_id=access_key,
                             aws_secret_access_key=secure_key,
                             config=Config(signature_version='s3v4'),
                             region_name='US')
    return client

class Packer(ch.utils.data.Dataset):
    def __init__(self, ds):
        super().__init__()
        self.ds = ds

    def __len__(self):
        return len(self.ds)
    
    def __getitem__(self, ix):
        im, lab = self.ds[ix]
        with io.BytesIO() as output:
            im.save(output, format="JPEG")
            return ix, output.getvalue(), lab

def upload_shard(fname, bucket, base_folder, client):
    print("uploading", fname, "on CORTX")
    obj_name = path.join(base_folder, path.basename(fname))
    client.upload_file(fname, bucket, obj_name)

def upload_cv_dataset(ds, client, bucket, base_folder, maxsize, maxcount, workers=0, batch_size=256):
    loader = ch.utils.data.DataLoader(Packer(ds), batch_size=batch_size, num_workers=workers, shuffle=True,
    collate_fn=lambda x: x)

    with tempfile.TemporaryDirectory() as tempfolder:
        pattern = path.join(tempfolder, f"shard-%06d.tar")
        writer = partial(upload_shard, client=client, bucket=bucket, base_folder=base_folder)
        with wds.ShardWriter(pattern, maxsize=int(maxsize), maxcount=int(maxcount), post=writer) as sink:
            for r in tqdm(loader):
                for ix, im, label in r:
                    sample = {"__key__": f"im-{ix}", "jpg": im, "cls": label}
                    sink.write(sample)

def readdir(client, bucket, folder):
    ob = client.list_objects(Bucket=bucket, Prefix=folder)
    return [x['Key'] for x in ob['Contents']]

def shard_downloader(it, client, bucket):
    for desc in it:
        with io.BytesIO() as output:
            client.download_fileobj(bucket, desc['url'], output)
            content = output.getvalue()
            print(len(content))
            yield {
                'stream': io.BytesIO(content)
            }

def RemoteDataset(client, bucket, folder, shuffle=True):
    shards = readdir(client, bucket, folder)
    downloader = partial(shard_downloader, client=client, bucket=bucket)
    dataset = wds.ShardList(shards, shuffle=shuffle)
    dataset = wds.Processor(dataset, downloader)
    dataset = wds.Processor(dataset, wds.tar_file_expander)
    dataset = wds.Processor(dataset, wds.group_by_keys)
    return dataset