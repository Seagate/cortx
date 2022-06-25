# Chia Harvesting & Plotting W/ CORTX

[Video link](https://vimeo.com/582086341)

## About

![](chia-logo.png)

Chia employs the Proof-of-Storage consensus algorithm, different compared to most other cryptocurrencies that employ the Proof-of-Work consensus algorithm. 

While Proof-of-Work mining requires expensive single-use hardware consuming large amounts of electricity, Chia mining requires users to use empty hard drive to store cryptographic numbers on unused disks into plots. This makes Chia one of the greenest and most eco-friendly cryptocurrencies. 

Chia platform was created by a companny called Chia Network, which saw a valuation of $500 million in a recent investment round, and claims to be aiming for an IPO before the end of 2021.

Our integration will aim at using CORTX as the storage backend for Chia plots.

## Our Setup

We used a Ubuntu 20.04 image on a c5.xlarge-sized AWS EC2 instance attached to a 512GB EBS storage.

## Installation Steps

#### 1. Mount CORTX as filesystem using Goofys

One of our members also worked on the [Goofys/Chia integration](https://github.com/Seagate/cortx/pull/1140) submitted for the hackathon. We will be reusing the convenience script to mount CORTX to a folder with Goofys:

```sh
wget https://raw.githubusercontent.com/c-l-j-y/cortx/main/doc/integrations/goofys/goofys-cortx-setup.sh
sh goofys-cortx-setup.sh <CORTX-ENDPOINT-URL> <BUCKET-NAME> <ACCESS-KEY-ID> <SECRET-ACCESS-KEY>
```

Our CORTX bucket is now mounted onto `$HOME/shared`.

#### 2. Clone the Chia repository and activate its environment

```sh
git clone https://github.com/Chia-Network/chia-blockchain.git -b latest --recurse-submodules
sh install.sh
. ./activate
```

**Note**: For CentOS users, refer to Chia's [guide](https://github.com/Chia-Network/chia-blockchain/wiki/INSTALL#centosred-hatfedora) for installation.

#### 3. Create temporary folders to store Chia plots

```sh
mkdir $HOME/chia-plots-tmp
mkdir $HOME/chia-plots-dest
```

#### 4. Install tmux and start a second terminal session (Optional)

```sh
sudo apt-get install tmux -y
tmux new -s plot # enter a new terminal session
. ./activate # activate the Chia environment again
```

Press `Ctrl+B` and `D` to detach from the terminal session. Type `tmux a -t plot` to attach into the terminal session again.

#### 5. Initialize Chia processes

```sh
chia init
chia keys generate
chia start farmer
```

Press `Ctrl+B` and `D` to detach from the terminal session and type `tmux a -t plot` to attach into the terminal session again.

#### 6. Start plotting Chia

Look at [this post](https://chiaforum.com/t/what-syntax-of-create-plots-to-make-parallel-plotting-and-staggering-in-cli/5076) to understand how to customize each of the flags to your liking.

```sh
# change the flags as you please
chia plots create -k 25 --override-k -n 2 -u 32 -b 7000 -r 4 -t $HOME/chia-plots-tmp -d $HOME/chia-plots-dest
```

This process will take some time. (The above command took us 6 minutes, but it can take up to hours or even days depending on the amount and size of plots you generate, and your amount of compute resources.)


#### 7. Copy the plots over to CORTX and delete the temporary folder; let Chia use CORTX as its primary storage backend

```sh
# copy the plot files over to the CORTX bucket
cp chia-plots-dest/plot-k25-*.plot $HOME/shared
# delete the plot files off of the temporary folders
rm -rf chia-plots-dest
# let Chia register our CORTX bucket as our primary storage for plots
chia plots add -d $HOME/shared
```

#### 7. Restart the Chia farmer to officially start harvesting on your Chia plots

```sh
chia start farmer -r
```

#### 8. You are now harvesting Chia with CORTX! :tada:

Use `chia farm summary` to track your farming progress, and use the `chia wallet` CLI commands to find out how to transfer your Chia to your digital wallet.

## Learning Points

We were pleasantly surprised how overall simple it was to kickstart both Chia and CORTX, as this was our first experience with blockchain mining as well as interfacing with S3-compatible object storages.

Though we were met with bugs causing this integration days to complete and film,and we almost gave up to move on to a different integration, it was such a rewarding experience to get the integration to finally work.

Thank you for the opportunity to integrate crypto with CORTX!

## Contributors

Claire, Sung Won and Rosminy :tada: :tada: :tada:

Tested by:

- Nov 11, 2021: Jalen Kan (<jalen.j.kan@seagate.com>) using Centos 7.9.2009 on Windows 10 running VMware Workstation 16.
- Sep 20, 2021: Harrison Seow (<harrison.seow@seagate.com>) using Centos 8 and Cortx OVA 2.0.0 on Windows 10 running VMware Workstation 16 Player.
