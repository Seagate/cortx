CentOS 7.8 dev VM
=================

:page_with_curl: **Notes:** 
 - CentOS 7.8.2003 is deprecated, you will not longer get any updates or security fix's for this version. 
  - You can continue using CentOS version 7.8.2003, however, you will have to access the OS packages from the vault repo. Please refer http://mirror.centos.org/centos-7/7.8.2003/readme for more information.
 - CentOS has moved to http://mirror.centos.org/centos-7/7.9.2009/ with kernel-3.10.0-1160.el7.x86_64.rpm

ISO
---
Download CentOS version 7.8.2003 ISO image for installation. You can find the image here:
https://vault.centos.org/7.8.2003/isos/x86_64/CentOS-7-x86_64-DVD-2003.iso

VM
--
Create VM with the following recommended configuration:
* CPUs = 4
* Memory = 8GB
* Storage = 128GB

Install CentOS version 7.8.2003 in this VM from the previously downloaded ISO image. The default install of CentOS may turn off the network interface at boot for security. To turn on the network interface edit the network-scripts for the interface by changing ONBOOT veriable to 'yes' & reboot the VM. 

```
# ip addr show
# sudo vi /etc/sysconfig/network-script/<name of interface>
```

PRE-BUILD [MOTR]
----------------
It is recommended to change user to `root` as `root` user privileges are required for most steps.

* Ensure kernel version is `3.10.0-1062.12.1`
```
# uname -r
3.10.0-1062.12.1.el7.x86_64
```

* [install] The epel-release is needed to be able to install ansible. 
```
# yum install -y epel-release
# yum install -y ansible
```

* [verify]
```
# rpm -qa | grep ansible
ansible-2.9.3-1.el7.noarch
```
Ensure that the ansible version is `2.9` or greater. 

Now you are ready to clone, build and/or run motr. Please refer to the [Motr](https://github.com/Seagate/cortx-motr/blob/main/doc/Quick-Start-Guide.rst) quick start document for further help on this.

PRE-BUILD [S3SERVER]
--------------------
* Follow the [install] and [verify] steps from motr prebuild steps.

* Next, s3server requires the user to be root. So, change user to root and check the path. Please ensure that the path contains: `/usr/local/bin`. This is required as s3server will install `s3iamcli` utility here, which it requires when running tests, and the path environment variable may not be updated resulting in `s3iamcli` not found error at test runtime.

* At this point you are ready to build s3server. Please refer to the [s3server](https://github.com/Seagate/cortx-s3server/blob/main/docs/CORTX-S3%20Server%20Quick%20Start%20Guide.md) quick start document on how to do that.

## Running s3server:

* Anytime you run the init.sh script to update build dependencies, you need to rebuild / recompile s3server again. Otherwise, you may see runtime errors.

* Authorization errors: if you see the following error when s3server tests are run -
```ERROR: S3 error: 403 (InvalidAccessKeyId): The AWS access key Id you provided does not exist in our records.```
It means that the necessary authorization information is not present. This may happen if you are building the first time and you run basic tests before running the entire test suite. In such a case, you need to run the entire test suite once, this will ensure that the required authorization information is generated and stored for further use.

* Authorization exception: if you see the following error message - ```The difference between the request time and current time is too large```, then it means that the system time is incorrect. In such a case, check the output of the `date` command and modify the system time to current and re-run the test.

* yum repo errors: certain libraries are maintained in a Seagate yum repo - `EPEL-7` so if there are repo related errors you need to check the `baseurl` field in your `/etc/yum.repos.d/*.repo` files and if it specific to Seagate, you can contact the Seagate support team in this regard.

PRE-BUILD [HARE]
----------------
* Python ≥ 3.6 and the corresponding header files. To install them on CentOS 7.6, run
 ``` 
# yum install python3 python3-devel
```

* Create an environment variable that points to the Motr source code:
```  
# export M0_SRC_DIR="/home/motr"`
```

* Ensure that Motr is built and its systemd services are installed.
```
# $M0_SRC_DIR/scripts/m0 rebuild
# $M0_SRC_DIR/scripts/install-motr-service --link
```

## Single node setup

1. To fetch hare sources refer to section 3.5 in [this](Cluster_Setup.md) document. 

2. Make sure you are `root` user, `cd` into the hare source directory and then execute the following commands to build and install hare 
```
# make
# make devinstall
```

3. Add current user to `hare` group.
```
# usermod --append --groups hare $USER
```
Log out and log back in.

4. Describing the single node cluster to hare:
There's a sample file in hare source at the location `cfgen/examples/singlenode.yaml` file. Edit it to reflect the single node cluster environment:
* Ensure that the disks enumerated in the `io_disks` list exist. Create loop devices, if necessary:
```
# mkdir -p /var/motr
# for i in {0..9}; do
    sudo dd if=/dev/zero of=/var/motr/disk$i.img bs=1M seek=9999 count=1
    sudo losetup /dev/loop$i /var/motr/disk$i.img
  done
```
Ensure that the path of the disks under `io_disks` match the created loop devices: `/var/motr/disk*.img`

* Make sure that `data_iface` value refers to existing network interface (it should be present in the output of `ip a` command).

5. Now we're all set to start a cluster:
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
2020-06-18 19:03:27: Installing Motr configuration files... OK
2020-06-18 19:03:27: Waiting for the RC Leader to get elected..... OK
2020-06-18 19:03:30: Starting Motr (phase1, mkfs)... OK
2020-06-18 19:03:36: Starting Motr (phase1, m0d)... OK
2020-06-18 19:03:38: Starting Motr (phase2, mkfs)... OK
2020-06-18 19:03:48: Starting Motr (phase2, m0d)... OK
2020-06-18 19:03:51: Checking health of services... OK
```

Please refer to the `README.md` file in hare source for more comprehensive information.

## Note:
* If during the bootstrap you see an error message such as 
```Starting Motr (phase1, mkfs)...Job for motr-mkfs@0x7200000000000001:0x9.service failed because the control process exited with error code. See "systemctl status motr-mkfs@0x7200000000000001:0x9.service" and "journalctl -xe" for details.```
then it might be the case that lnet is not configured properly. In such a case follow these steps:
```
# systemctl start lnet
# lnetctl net add --net tcp0 --if enp0s3
```
where `enp0s3` is your interface id from the output of `ip a`.
Next check the output of `lctl list_nids`. It should be non-empty:
```
10.0.2.15@tcp
```

* If during bootstrap you see an error message such as
```
The RC leader can only be elected if there is a Consul server node
on which both hare-hax and confd m0d@ service are in "non-failed" state.
```
then it might be because of an earlier aborted I/O, try resetting confd as follows
```
systemctl reset-failed m0d@0x7200000000000001:0x9
```
where `0x7200000000000001:0x9` is the confd ID from `hctl status`.

## Testing the cluster

* To test the newly created cluster, one can perform I/O. Please refer to section 1.6 in [this](Cluster_Setup.md) document on how to use motr utilities to do so.

## Stopping a cluster

* To stop a cluster, execute the following command:
```
# hctl shutdown
Stopping m0d@0x7200000000000001:0xc (ios) at localhost... 
Stopped m0d@0x7200000000000001:0xc (ios) at localhost
Stopping m0d@0x7200000000000001:0x9 (confd) at localhost... 
Stopped m0d@0x7200000000000001:0x9 (confd) at localhost
Stopping hare-hax at localhost... 
Stopped hare-hax at localhost
Stopping hare-consul-agent at localhost... 
Stopped hare-consul-agent at localhost
Killing RC Leader at localhost... done
``` 
