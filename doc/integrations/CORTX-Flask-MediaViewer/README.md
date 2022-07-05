# CORTX-flask-app
CORTX is very useful as a mass storage tool, and when I considered what I personally found to be most valuable about cloud storage, I kept coming back to media: my personal images and videos. Being able to back up these memories and have the peace of mind that I won’t lose them is a huge benefit of cloud storage. So I decided to build a Flask project which allowed me to not only upload these images to CORTX, and replicate them to IPFS, but also access them through a static web page, connected to the database. This means that not only are my images backed up, but they are accessible remotely, meaning I can show them to friends and family on the go.

My project allows images to be uploaded to CORTX and IPFS, and also viewed via a static web page, all powered by a Flask app.

Link to my video - https://youtu.be/iwQ2beyMFtg

### 1. Create CORTX instance:
 - To create a CORTX instance you can follow the steps outlined [here](https://github.com/Seagate/cortx/blob/main/doc/ova/2.0.0/PI-6/CORTX_on_Open_Virtual_Appliance_PI-6.rst)

### 2. Connect CORTX to IPFS
 - You can see how to do this in this [guide](https://github.com/Seagate/cortx/tree/main/doc/integrations/ipfs)
 - Alternatively:
    1. Start with a fresh install of Ubuntu 22.04
    2. Update `apt` and install necessary libraries
      ```bash
      sudo apt update
      sudo apt full-upgrade
      sudo apt install   ocl-icd-opencl-dev gcc wget git pkg-config  curl hwloc  libhwloc-dev clang mesa-opencl-icd
      ```
    3. Install golang
    ```bash
    wget -c https://go.dev/dl/go1.17.10.linux-amd64.tar.gz
    rm -rf /usr/local/go && tar -C /usr/local -xzf go1.17.10.linux-amd64.tar.gz
    export PATH=$PATH:/usr/local/go/bin
    export PATH=$PATH:$(go env GOPATH)/bin
    export GOPATH=$(go env GOPATH)
    ```
    Check golang is installed:
    ```bash
    go version
    ```
    4. Install IPFS with s3 plugin
      ```bash
      git clone https://github.com/ipfs/go-ipfs
      cd go-ipfs
      export GO111MODULE=on
      go get github.com/ipfs/go-ds-s3/plugin@v0.8.0
      echo -e "\ns3ds github.com/ipfs/go-ds-s3/plugin 0" >> plugin/loader/preload_list
      make build
      go mod tidy
      make build
      make install
      ```
    5. Run the IPFS binary
    ```bash
    ipfs init
    ```
    6. Update the ~/.ipfs config and datastore_spec files
    - vi config
      ```bash
      "Datastore": {
      "StorageMax": "10GB",
      "StorageGCWatermark": 90,
      "GCPeriod": "1h",
      "Spec": {
        "mounts": [
          {
            "child": {
              "type": "s3ds",
              "region": "us-east-1",
              "bucket": "picture",
              "rootDirectory": "uploads",
              "regionEndpoint": "http://uvo15mtkooygylat1ef.vm.cld.sr:31949",
              "accessKey": "sgiamadmin",
              "secretKey": "ldapadmin"
            },
            "mountpoint": "/blocks",
            "prefix": "s3.datastore",
            "type": "measure"
          },
          {
            "child": {
              "compression": "none",
              "path": "datastore",
              "type": "levelds"
            },
            "mountpoint": "/",
            "prefix": "leveldb.datastore",
            "type": "measure"
          }
        ],
        "type": "mount"
      },
      "HashOnRead": false,
      "BloomFilterSize": 0
      },
      ```
    - datastore_spec
    ```bash
    {"mounts":[{"bucket":"picture","mountpoint":"/blocks","region":"us-east-1","rootDirectory":"uploads"},    {"mountpoint":"/","path":"datastore","type":"levelds"}],"type":"mount"}
    ```
  
### 3. Start IPFS
 - Once ipfs is installed you should be able to start the daemon process 
 - Run the below command in a new terminal:
   ```bash
   cd ~/go-ipfs/cmd/ipfs
   ./ipfs daemon
   ```

### 4. Start flask-app
 - Once your storage servers running you can start your flask app
   ```bash
   python3 app.py
   ```
  
### 5. All done!
 - Now you can enjoy your photos from anywhere in the world with the comfort of knowing they are secure thanks to CORTX!
 - Here is what it looks like
   ![flask-app-demo](https://user-images.githubusercontent.com/23244853/177158710-a2c9e722-9d15-4cb1-9bce-9a6a4e4e6752.PNG)

