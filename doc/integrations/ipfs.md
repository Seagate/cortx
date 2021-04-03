
# What is IPFS?

The [InterPlanetary File System (IPFS)](https://ipfs.io/) is a protocol and peer-to-peer network for storing and sharing data in a distributed file system. IPFS uses content-addressing to uniquely identify each file in a global namespace connecting all computing devices.

IPFS allows users to not only receive but host content, in a similar manner to BitTorrent. As opposed to a centrally located server, IPFS is built around a decentralized system of user-operators who hold a portion of the overall data, creating a resilient system of file storage and sharing. Any user in the network can serve a file by its content address, and other peers in the network can find and request that content from any node who has it using a distributed hash table (DHT).

# What is CORTX?

CORTX is a distributed object storage system designed for great efficiency, massive capacity, and high HDD-utilization. CORTX is 100% Open Source.

## How do CORTX and IPFS work together?

A node or peer is the IPFS program that you run on your local computer to store/cache files and then connect to the IPFS network

Each node uses an on-disk storage system called a (datastore.)[https://docs.ipfs.io/concepts/glossary/#datastore]

We can use CORTX as the datastore for the IPFS node.

## Configuring IPFS to use CORTX as a datastore

#### Step 1: Have a CORTX system up and running

If you need instructions on how to set up your own CORTX system you can find instructions on how to setup one on a local machine [here](https://github.com/Seagate/cortx/blob/main/doc/CORTX_on_Open_Virtual_Appliance.rst) or AWS [here.](https://github.com/Seagate/cortx/blob/main/doc/integrations/AWS_EC2.md)

To connect the IFPS client to CORTX you will need these details
* IP ADDRESS
* SECRET KEY
* ACCESS KEY
* BUCKET NAME
* SUBDIRECTORY NAME within BUCKET

> *Note: You will need to write data to CORTX over __http (port 80)__ and __NOT__ https (port 443)*

Check using a tool like [Cyberduck](https://cyberduck.io/) that you can read and write data over http using the details above.

#### Step 2: Create a sperate machine running Linux where you will install the IPFS client

#### Step 3: Install Go on the Linux machine by following [these instructions.](https://golang.org/doc/install)

#### Step 4: Build the [go-ipfs](https://github.com/ipfs/go-ipfs/) client with the [go-ds-s3](https://github.com/ipfs/go-ds-s3) plugin on the Linux machine.

```bash
# We use go modules for everything.
> export GO111MODULE=on

# Clone go-ipfs.
> git clone https://github.com/ipfs/go-ipfs
> cd go-ipfs

# Pull in the datastore plugin (you can specify a version other than latest if you'd like).
> go get github.com/ipfs/go-ds-s3@latest

# Add the plugin to the preload list.
> echo "s3ds github.com/ipfs/go-ds-s3/plugin 0" >> plugin/loader/preload_list

# Rebuild go-ipfs with the plugin
> make build

# (Optionally) install go-ipfs
> make install
```
#### Step 5: Initialize the IPFS config

```
cd cmd/ipfs
./ipfs init
```

#### Step 6: Edit $IPFS_DIR/config to include s3 details 

In your home directory there should be a folder `.ipfs` and in that folder there should be a `config` file.

Include the CORTX details below to the `.config` file

```json
{
  "Datastore": {
  ...

    "Spec": {
      "mounts": [
        {
          "child": {
            "type": "s3ds",
            "region": "us-east-1",
            "bucket": "<BUCKET NAME>",
            "rootDirectory": "<BUCKET SUBDIRECTORY>",
            "regionEndpoint": "http://<IP-ADDRESS>:80",
            "accessKey": "<SECRET-KEY>",
            "secretKey": "<ACCESS-KEY>"
          },
          "mountpoint": "/blocks",
          "prefix": "s3.datastore",
          "type": "measure"
        },
```
#### Step 7: Overwrite `$IPFS_DIR/datastore_spec` as specified below

```
{"mounts":[{"bucket":"<BUCKET NAME>","mountpoint":"/blocks","region":"us-east-1","rootDirectory":"<BUCKET SUBDIRECTORY>"},{"mountpoint":"/","path":"datastore","type":"levelds"}],"type":"mount"}
```

#### Step 8: Run the IPFS node

```./ipfs daemon```

#### Step 9: check that IPFS is working and storing data in CORTX

```
echo "hello world" > hello
ipfs add hello
# This should output a hash string that looks something like:
# QmT78zSuBmuS4z925WZfrqQ1qHaJ56DQaTfyMUF7F8ff5o
ipfs cat <that hash>
```
