CentOS 7.7 dev VM
=================

ISO
---
download CentOS 7.7.1908 ISO.
http://mirrors.ukfast.co.uk/sites/ftp.centos.org/7.7.1908/isos/x86_64/
http://mirrors.ukfast.co.uk/sites/ftp.centos.org/7.7.1908/isos/x86_64/CentOS-7-x86_64-DVD-1908.iso

VM
--
Create VM.
Minimum requirements:
CPU = 4, Memory = 8GB, Storage = 64GB
Install CentOS 7.7.1908 ISO.

PRE-BUILD [MOTR]
----------------
[kernel 3.10.0-1062.12.1]
>> uname -r
3.10.0-1062.12.1.el7.x86_64

[install]
>> sudo yum install -y epel-release

>> sudo yum install -y ansible

[verify]
>> rpm -qa | grep ansible

`ansible-2.9.3-1.el7.noarch`

Ensure ansible version is atleast 2.9

Now you are ready to clone, build and/or run motr. Please refer to the [Motr](MeroQuickStart.md) quick start document for further help on this.

PRE-BUILD [S3SERVER]
--------------------
Follow the [install] and [verify] steps from motr prebuild steps

Next, s3server requires the user to be root. So, change user to root and check the path. Please ensure that the path contains: `/usr/local/bin`. This is required as s3server will install `s3iamcli` utility here, which it requires when running tests, and the path environment variable may not be updated resulting in `s3iamcli` not found error at test runtime.

At this point you are ready to build s3server. Please refer to the [s3server](S3ServerQuickStart.md) quick start document on how to do that.

## Running s3server:

Anytime you run the init.sh script to update build dependencies, you need to rebuild / recompile s3server again. Otherwise, you may see runtime errors.

Authorization errors: if you see the following error when s3server tests are run -
```ERROR: S3 error: 403 (InvalidAccessKeyId): The AWS access key Id you provided does not exist in our records.```
It means that the necessary authorization information is not present. This may happen if you are building the first time and you run basic tests before running the entire test suite. In such a case, you need to run the entire test suite once, this will ensure that the required authorization information is generated and stored for further use.

Authorization exception: if you see the following error message - ```The difference between the request time and current time is too large```, then it means that the system time is incorrect. In such a case, check the output of the `date` command and modify the system time to current and re-run the test.

yum repo errors: certain libraries are maintained in a Seagate yum repo - `EPEL-7` so if there are repo related errors you need to check the `baseurl` field in your `/etc/yum.repos.d/*.repo` files and if it specific to Seagate, you can contact the Seagate support team in this regard.

PRE-BUILD [HARE]
----------------
* Python ≥ 3.6 and the corresponding header files.
  To install them on CentOS 7.6, run
  
  `sudo yum install python3 python3-devel`
* Create an environment variable that points to the Motr source code:
  
  `export M0_SRC_DIR="/home/motr"`
* Ensure that Motr is built and its systemd services are installed.
  
  `$M0_SRC_DIR/scripts/m0 rebuild`
  
  `sudo $M0_SRC_DIR/scripts/install-mero-service --link`

## Single node setup

1. Make sure you are `root` user and then execute the following commands to fetch, build and install hare
```# git clone --recursive http://gitlab.mero.colo.seagate.com/mero/hare.git
# cd hare
# make
# sudo make devinstall
```

2. Add current user to `hare` group.
```# usermod --append --groups hare $USER```
Log out and log back in.

3. Describing the single node cluster to hare:
There's a sample file in hare source at the location `cfgen/examples/singlenode.yaml` file. Edit it to reflect the single node cluster environment:
* Ensure that the disks enumerated in the `io_disks` list exist. Create loop devices, if necessary:
```
# mkdir -p /var/mero
for i in {0..9}; do
    sudo dd if=/dev/zero of=/var/mero/disk$i.img bs=1M seek=9999 count=1
    sudo losetup /dev/loop$i /var/mero/disk$i.img
done
```
Ensure that the path of the disks under `io_disks` match the created loop devices: `/var/mero/disk*.img`

* Make sure that `data_iface` value refers to existing network interface (it should be present in the output of `ip a` command).

4. Now we're all set to start a cluster:
```
# hctl bootstrap --mkfs cfgen/examples/singlenode.yaml
```

On a successful cluster bootstrap the messages output on the terminal may look like:
```
2020-06-18 19:03:17: Generating cluster configuration... OK
2020-06-18 19:03:19: Starting Consul server agent on this node......... OK
2020-06-18 19:03:26: Importing configuration into the KV store... OK
2020-06-18 19:03:26: Starting Consul agents on other cluster nodes... OK
2020-06-18 19:03:27: Updating Consul agents configs from the KV store... OK
2020-06-18 19:03:27: Installing Mero configuration files... OK
2020-06-18 19:03:27: Waiting for the RC Leader to get elected..... OK
2020-06-18 19:03:30: Starting Mero (phase1, mkfs)... OK
2020-06-18 19:03:36: Starting Mero (phase1, m0d)... OK
2020-06-18 19:03:38: Starting Mero (phase2, mkfs)... OK
2020-06-18 19:03:48: Starting Mero (phase2, m0d)... OK
2020-06-18 19:03:51: Checking health of services... OK
```

Please refer to the `README.md` file in hare source for more comprehensive information.

## Note:
* If during the bootstrap you see an error message such as 
```Starting Mero (phase1, mkfs)...Job for mero-mkfs@0x7200000000000001:0x9.service failed because the control process exited with error code. See "systemctl status mero-mkfs@0x7200000000000001:0x9.service" and "journalctl -xe" for details.```
then it might be the case that lnet is not configured properly. In such a case follow these steps:
```
systemctl start lnet
lnetctl net add --net tcp0 --if enp0s3
```
where `enp0s3` is your interface id from the output of `ip a`.
Next check the output of `lctl list_nids`. It should be non-empty:
```
10.0.2.15@tcp
```

## Testing the cluster

* To test the newly created cluster, one can perform I/O. Please refer to section 1.6 in [this](Cluster_Setup.md) document on how to use motr utilities to do so.
