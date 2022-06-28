# CORTX - IPFS - FILECOIN Integration

![alt text](https://i.imgur.com/SfcbktD.jpg)

# Introduction

**What is CORTX**

CORTX is a distributed object storage system designed for great efficiency, massive capacity, and high HDD-utilization. The ideal big data storage system would allow storage of a virtually unlimited amount of data, cope both with high rates of random write and read access, flexibly and efficiently deal with a range of different data models, support both structured and unstructured data, and for privacy reasons, only work on encrypted data. Obviously, all these needs cannot be fully satisfied.

**What is IPFS**

The InterPlanetary File System (IPFS) is a protocol and peer-to-peer network for storing and sharing data in a distributed file system. IPFS uses content-addressing to uniquely identify each file in a global namespace connecting all computing devices.

IPFS allows users to host and receive content in a manner similar to BitTorrent. As opposed to a centrally located server, IPFS is built around a decentralized system of user-operators who hold a portion of the overall data, creating a resilient system of file storage and sharing. Any user in the network can serve a file by its content address, and other peers in the network can find and request that content from any node who has it.

**What is Filecoin**

Filecoin is a peer-to-peer network that stores files, with built-in economic incentives to ensure files are stored reliably over time.

In Filecoin, users pay to store their files on storage providers. Storage providers are computers responsible for storing files and proving they have stored the files correctly over time. Anyone who wants to store their files or get paid for storing other users’ files can join Filecoin. Available storage, and the price of that storage, is not controlled by any single company. Instead, Filecoin facilitates open markets for storing and retrieving files that anyone can participate in.

Filecoin includes a blockchain and native cryptocurrency (FIL). Storage providers earn units of FIL for storing files. Filecoin’s blockchain records transactions to send and receive FIL, along with proofs from storage providers that they are storing their files correctly.

**Why this integration important?**
- Currently there is no incentive for a user to run an IPFS node, with filecoin, the node user can be given filecoin as an incentive to keep an IPFS node runninga
and contribute to the effort of a decentralized data store.

## Integration Walkthrough
- See below steps

## System Specifications
### CORTX System
CPU: Intel Core i3-1157G
OS: Windows 11 Professional
Hypervisor: Hyper-V with VMWare Workstation
Net: Realtek Gigabit Ethernet running in Bridged Mode

### Main System
CPU: AMD Ryzen 7 4750G
GPU: Radeon RX 560
OS: Windows 11 Professional with WSL2 running Ubuntu 22.04
Net: Realtek Gigabit Ethernet

# Step 1 - Let's prepare the CORTX system
1. Follow the procedure outlined at https://github.com/Seagate/cortx/blob/main/doc/ova/1.0.4/CORTX_on_Open_Virtual_Appliance.rst
2. Note down the addresses for ens32 (management ip), ens33 (data ip) and ens34
3. In my case ens32 is 192.168.8.156, ens33 is 192.168.8.158
4. At Step 14 in the above, go to here https://github.com/Seagate/cortx/blob/main/doc/Preboarding_and_Onboarding.rst
5. Since we are using the 1.0.4 OVA use the following address instead https://192.168.8.156:28100/#/preboarding/welcome and complete the process. You will be prompted to create an Admin User, do it and note the password. Once back at the dashboard, create and S3 user and download the credentials file at the end (this will be needed later)
6. The CORTX system should be ready now. It can be left alone for the moment.

# Step 2 - Let's prepare the main system
1. Start with a fresh install of Ubuntu 22.04 from the Microsoft Store so that we can use WSL2.
2. Open Ubuntu and create a user, once done lets run the following
    sudo apt update
    sudo apt full-upgrade
    sudo apt install mesa-opencl-icd ocl-icd-opencl-dev gcc git bzr jq pkg-config curl clang build-essential hwloc libhwloc-dev wget

### Step 2.1 - Install GOLang
1. Change to home directory
    - cd ~
2. Download and extract Go
    - wget -c https://go.dev/dl/go1.17.10.linux-amd64.tar.gz
    - sudo tar -C /usr/local -xzf go1.17.10.linux-amd64.tar.gz
3. Add the following to ~/.profile using nano
    - export PATH=$PATH:/usr/local/go/bin
    - export PATH=$PATH:$(go env GOPATH)/bin
    - export GOPATH=$(go env GOPATH)
4. Source ~/.profile
    - source ~/.profile
5. Install Go 1.15.10 for later
    - go install golang.org/dl/go1.15.10@latest
    - go1.15.10 download
6. Check if Go is installed
    - go version (should output 1.17.10)
    - go1.15 version (should output 1.15.10)

### Step 2.2 - Install IPFS with s3 plugin
Due to the way plugins are so finicky in Go, we will bundle it into the IPFS binary
1. Clone and cd into the git repo
    - git clone https://github.com/ipfs/go-ipfs
    - cd go-ipfs
2. Run the following
    - export GO111MODULE=on
    - go get github.com/ipfs/go-ds-s3/plugin@v0.8.0
    - echo -e "\ns3ds github.com/ipfs/go-ds-s3/plugin 0" >> plugin/loader/preload_list
    - make build
    - go mod tidy
    - make build
    - make install (add the ipfs binary to GOPATH)
3. Run the IPFS binary
    - ipfs init
4. Replace the following files in ~/.ipfs with files from repo
    - cd ~/.ipfs
    - rm config
    - rm datastore_spec
5. While still in ~/.ipfs, make changes to the config and datastore_spec files
    --config--
    - replace "bucketName" with the name of your bucket
    - replace "rootDirectoryName" with the name of the subdirectory of the bucket above
    - replace "accessKeyCSV" with your access key from prior
    - replace "secretkeyCSV" with your secret key from prior
    - replace "regionEndpointIP" with the Data IP from prior (ens33)

    --datastore_spec--
    - replace "bucketName" with the name of your bucket
    - replace "rootDirectoryName" with the name of the subdirectory of the bucket above
    
6. In a seperate terminal run
    - ipfs daemon
7. Which should say at the end that the daemon is running (Yay!!)

### Step 2.3 - Build Lotus-Devnet (for local development use)
Since we are just testing for now, we will use the devnet version

1. Let's clone the repo
    - git clone https://github.com/textileio/lotus-devnet.git
2. Lets build it
    - cd lotus-devnet
    - make clean
    - CGO_CFLAGS="-D__BLST_PORTABLE__" make
    - go1.15.10 build -o lotus-devnet main.go
3. Let us run it (in a seperate terminal)
    - ./lotus-devnet --ipfsaddr "/ip4/127.0.0.1/tcp/5001"
The --ipfsaddr arguement is so it can connect to the IPFS daemon running earlier. The lotus devnet runs on port 7777 instead of 1234 unlike the regular lotus client

### Step 2.4 - Build/Install Powergate
To make interfacing easier we will use powergate to link IPFS and Filecoin

1. Let's clone the powergate repo
    - git clone https://github.com/textileio/powergate.git
    - cd powergate
2. Lets compile the powergate daemon
    - make build-powd
3. Lets compile powergate cli
    - make build-pow
4. Download Geolite DB
	- wget -c https://github.com/textileio/powergate/raw/master/iplocation/maxmind/GeoLite2-City.mmdb
5. Open a terminal and cd into powergate directory and run daemon
    - ./powd --devnet --lotushost "/ip4/127.0.0.1/tcp/7777" --ipfsapiaddr "/ip4/127.0.0.1/tcp/5001"

### Step 3 - Lets interface Filecoin and IPFS
Now that the IPFS daemon, lotus-devnet and the Powergate daemon are running, lets test

1. Create a dummy file in ~ for storing (8M)
    - dd if=/dev/random of=hello.world bs=1M  count=8
2. Lets create a user and export the token (in powergate dir)
    - ./pow admin user create
2.1. The above returns a token which we can copy, then lets export so we don't type it all the time
    - export POW_TOKEN=90f517db-dc00-45eb-94f5-550f7b405bb9
3. Let's stage the data which at the end returns a CID which we need for later
    - ./pow data stage ~/hello.world
4. Let's carry out the transaction now
    - ./pow config apply --watch CID
5. This may take a while but eventually it will show JOB_STATUS_SUCCESS and StorageDealActive
6. While we are here lets do an md5 sum of hello.world
    - md5sum ~/hello.world
7. Let's retrieve the file we stored now
    - ./pow data get CID ~/hello.world.new
8. Compute md5sum of the hello.world.new
    - md5sum ~/hello.world.new
9. Compare the md5sums and they should be equal so the data is the same


### Optional (Step 2.3 can be replaced  by Step 4, and 4.2 for a live system. 4.1 Can be for a test net instead of live)

### Step 4 - Use a live lotus client
Replace the lotus-devnet with a full lotus node is possible. 

To run a Lotus node your computer must have:
 - macOS or Linux installed. Windows is not yet supported.
 - 8-core CPU and 32 GiB RAM. Models with support for Intel SHA Extensions (AMD since Zen microarchitecture or Intel since Ice Lake) will significantly speed things up.
 - Enough space to store the current Lotus chain (preferably on an SSD storage medium). The chain grows at approximately 38 GiB per day. The chain can be synced from trusted state snapshots and compacted or pruned to a minimum size of around 33Gib. The full history was around 10TiB in June of 2021.

1. Install rustup
    - curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
    - source $HOME/.cargo/env
2. Clone lotus repo
    - git clone https://github.com/filecoin-project/lotus.git
    - cd lotus
    - git checkout master
3. Export the following
    - export RUSTFLAGS="-C target-cpu=native -g"
    - export FFI_BUILD_FROM_SOURCE=1
    - export CGO_CFLAGS_ALLOW="-D__BLST_PORTABLE__"
    - export CGO_CFLAGS="-D__BLST_PORTABLE__"
4. Build lotus
    - make clean all
    - sudo make install
5. Run lotus daemon
    - lotus daemon
6. To interact with it, you need a new terminal

### Step 4.1 - Create a lotus local-net 
1. Export the following flags or put into ~/.profile
    - export LOTUS_PATH=~/.lotus-local-net
    - export LOTUS_MINER_PATH=~/.lotus-miner-local-net
    - export LOTUS_SKIP_GENESIS_CHECK=_yes_
    - export CGO_CFLAGS_ALLOW="-D__BLST_PORTABLE__"
    - export CGO_CFLAGS="-D__BLST_PORTABLE__"
2. Clone the repo and checkout
    - git clone https://github.com/filecoin-project/lotus lotus-local-net
    - cd lotus-local-net
    - git checkout v1.15.3
3. Remove existing repo
    - rm -rf ~/.genesis-sectors
4. Build it and run some commands
    - make 2k
    - ./lotus fetch-params 2048
    - ./lotus-seed pre-seal --sector-size 2KiB --num-sectors 2
    - ./lotus-seed genesis new localnet.json
    - ./lotus-seed genesis add-miner localnet.json ~/.genesis-sectors/pre-seal-t01000.json
    - ./lotus daemon --lotus-make-genesis=devgen.car --genesis-template=localnet.json --bootstrap=false
5. In a new terminal
    - export LOTUS_PATH=~/.lotus-local-net
    - export LOTUS_MINER_PATH=~/.lotus-miner-local-net
    - export LOTUS_SKIP_GENESIS_CHECK=_yes_
    - export CGO_CFLAGS_ALLOW="-D__BLST_PORTABLE__"
    - export CGO_CFLAGS="-D__BLST_PORTABLE__"
    
    - ./lotus wallet import --as-default ~/.genesis-sectors/pre-seal-t01000.key
    - ./lotus-miner init --genesis-miner --actor=t01000 --sector-size=2KiB --pre-sealed-sectors=~/.genesis-sectors --pre-sealed-metadata=~/.genesis-sectors/pre-seal-t01000.json --nosync
    - ./lotus-miner run --nosync

6. Stop the lotus daemon and miner
7. Change directory to ~/.lotus-local-net
	- cd ~/.lotus-local-net
8. Open config
	- nano config.toml
9. Change UseIpfs
	- UseIpfs = true
10. Restart daemon and miner in their respective terminals
	- ./lotus daemon --lotus-make-genesis=devgen.car --genesis-template=localnet.json --bootstrap=false
	- ./lotus-miner run --nosync
	
### Step 4.2 - Lets make a deal
1. Create a dummy file
	- dd if=/dev/random of=hello.world bs=1k  count=1
2. from lotus-local-net directory
	- ipfs add -r ~/hello.world (returns a Qm....)
	- ./lotus client deal Qm.... t01000 0.0000000001 518400 (returns a deal CID bafy....)
	

## Demo
- An install video can be found [here](https://youtu.be/6NqOxSdcRXw)
- A demo video which describe the entire code can be found [here](https://youtu.be/yA_qGKGFHCQ)

## Sidenote
- You can use CyberDuck to connect to the Cortx S3 instance to check the files are stored. The live lotus client that connects to the mainnet
needs a lot of initial data bandwidth which was not available due to high system requirements

## What's next?
- Deploy a live lotus client and join the mainnet

## Contributors
- **Pratish "kyroninja" Neerputh**
- **Shraddha Neerputh**
