# CORTX v1.0 Virtual Clusters Setup

This is a step by step guide to get CORTX virtual cluster setup ready.
Also see https://github.com/Seagate/cortx-hare/blob/main/README.md

## 1. Single-node setup

### 1.1. Create a VM

Create a virtual machine.

### 1.2. Install the RPMs

* Build and Install RPMs.
Follow the [Hare User Guide](https://github.com/Seagate/cortx-hare#installation) to Build and Install Hare from source. This guide will also show you how to build and install Motr.

  
### 1.3. Configure LNet

Create `lnet.conf` file, if it does not exist, and restart `lnet` service.
* `cat /etc/modprobe.d/lnet.conf`
  ```
  options lnet networks=tcp(eth1) config_on_load=1
  ```
* `sudo systemctl restart lnet`
* `sudo lctl list_nids` should output IP address(es):
  ```
  192.168.1.160@tcp
  ```

### 1.4. Prepare the CDF

* Make a copy of the single-node CDF (Cluster Description File),
  provided by Hare:
  ```bash
  cp /opt/seagate/cortx/hare/share/cfgen/examples/singlenode.yaml .
  ```

* Edit the copy:

  - ensure that the disks referred to by `io_disks` values exist. To see available devices use command 'lsblk' or 'lsscsi' or 'fdisk -l'.
    (add new disks to VM, if necessary, or create loop devices);

  - make sure that `data_iface` value refers to existing network
    interface (it should be present in the output of `ip a` command
    and its IP address should be seen in `sudo lctl list_nids` output).

Sample diff:
```diff
--- cfgen/examples/singlenode.yaml	2020-05-19 11:41:18.077398588 +0000
+++ /tmp/singlenode.yaml	2020-05-19 21:51:02.704874383 +0000
@@ -10,16 +10,14 @@
       - runs_confd: true
         io_disks: []
       - io_disks:
-          - /dev/loop0
-          - /dev/loop1
-          - /dev/loop2
-          - /dev/loop3
-          - /dev/loop4
-          - /dev/loop5
-          - /dev/loop6
-          - /dev/loop7
-          - /dev/loop8
-          - /dev/loop9
+          - /dev/sdb
+          - /dev/sdc
+          - /dev/sdd
+          - /dev/sde
+          - /dev/sdf
+          - /dev/sdg
+          - /dev/sdh
+          - /dev/sdi
     m0_clients:
       s3: 0         # number of S3 servers to start
       other: 2      # max quantity of other Motr clients this host may have
     pools:
    - name: the pool
      #type: sns  # optional; supported values: "sns" (default), "dix", "md"
      data_units: 4      # N=4 Update N and K here
      parity_units: 2    # K=2, Also make sure N+2K <= P number of devices.
      allowed_failures: { site: 0, rack: 0, encl: 0, ctrl: 0, disk: 2 } 
```

### 1.5. Bootstrap the cluster

* `sudo hctl bootstrap --mkfs singlenode.yaml`
* `hctl status`
  ```
  Profile: 0x7000000000000001:0x26
  Data Pools:
      0x6f00000000000001:0x27
  Services:
      ssc-vm-0171.colo.seagate.com (RC)
      [started ]  hax        0x7200000000000001:0x6   192.168.1.160@tcp:12345:1:1
      [started ]  confd      0x7200000000000001:0x9   192.168.1.160@tcp:12345:2:1
      [started ]  ioservice  0x7200000000000001:0xc   192.168.1.160@tcp:12345:2:2
      [unknown ]  m0_client  0x7200000000000001:0x20  192.168.1.160@tcp:12345:4:1
      [unknown ]  m0_client  0x7200000000000001:0x23  192.168.1.160@tcp:12345:4:2
  ```

### 1.6. Perform the I/O

m0cp, m0cat and other motr utils argument refer `hctl status`.
For e.g,
```
 -l  local endpoint is,
     [unknown ]  m0_client  0x7200000000000001:0x20  *192.168.1.160@tcp:12345:4:1*
 -H  HA endpoint is,
     [started ]  hax        0x7200000000000001:0x6   *192.168.1.160@tcp:12345:1:1*
 -p  profile is,
     Profile: *0x7000000000000001:0x26*
 -P  process is,
     [unknown ]  m0_client  *0x7200000000000001:0x20*  192.168.1.160@tcp:12345:4:1
 ```

* Write some data to Motr.
  ```bash
  m0cp -l 192.168.1.160@tcp:12345:4:1 -H 192.168.1.160@tcp:12345:1:1 \
       -p 0x7000000000000001:0x26 -P 0x7200000000000001:0x23 -o 12:10 \
       -s 1m -c 128 /home/src/single/random.img -L 9
  ```

* Read the data from Motr.
  ```bash
  m0cat -l 192.168.1.160@tcp:12345:4:1 -H 192.168.1.160@tcp:12345:1:1 \
        -p 0x7000000000000001:0x26 -P 0x7200000000000001:0x23 -o 12:10 \
        -s 1m -c 128 /home/src/single/random_from_motr.img -L 9
  ```

* Ensure that I/O succeeded.
  ```bash
  cmp random.img random_from_motr.img
  ```

<!-- ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ -->
## 2. Dual-node setup

### 2.1. Create two VMs

Create two virtual machines.

### 2.2. Setup passwordless SSH

Enable passwordless SSH access between two nodes for `root` user.
(Substitute `node-1` and `node-2` with the host names of your VMs.)

* On 'node-1':
  ```bash
  sudo su -  # Get root. Skip this step if you are 'root' already.
  ssh-keygen
  ssh-copy-id node-2
  ```
* On 'node-2':
  ```bash
  sudo su -  # Get root. Skip this step if you are 'root' already.
  ssh-keygen
  ssh-copy-id node-1
  ```
* Verify that passwordless SSH works by executing the following command
  _from 'node-1'_:
  ```bash
  sudo su -
  ssh node-2 ssh node-1 echo it works  # use actual hostnames
  ```
  
* Disable firewall on both nodes:
  ```bash
  sudo systemctl stop firewalld
  sudo systemctl disable firewalld
  ```
* Disable SELinux:
  ```
  cat /etc/selinux/config 

  # This file controls the state of SELinux on the system.
  # SELINUX= can take one of these three values:
  #     enforcing - SELinux security policy is enforced.
  #     permissive - SELinux prints warnings instead of enforcing.
  #     disabled - No SELinux policy is loaded.
  SELINUX=disabled
  # SELINUXTYPE= can take one of three values:
  #     targeted - Targeted processes are protected,
  #     minimum - Modification of targeted policy. Only selected processes are protected. 
  #     mls - Multi Level Security protection.
  SELINUXTYPE=targeted 
  ```


### 2.3. Install the RPMs

Execute [step 1.2](#12-install-the-rpms) **on both nodes**.

### 2.4. Configure LNet

Execute [step 1.3](#13-configure-lnet) **on both nodes**.

### 2.5. Prepare the CDF

Edit a copy of `/opt/seagate/cortx/hare/share/cfgen/examples/ci-boot2.yaml`.
```bash
cp /opt/seagate/cortx/hare/share/cfgen/examples/ci-boot2.yaml motr-cluster.yaml
```

See [section 1.4](#14-prepare-the-cdf) for details.

Sample diff:
```diff
--- /opt/seagate/cortx/hare/share/cfgen/examples/ci-boot2.yaml	2020-09-22 13:14:20.000000000 -0400
+++ motr-cluster.yaml	2020-12-15 00:21:47.447643009 -0500
@@ -1,41 +1,41 @@
 nodes:
-  - hostname: ssu1
-    data_iface: eth1
+  - hostname: node-1
+    data_iface: eth0
     m0_servers:
       - runs_confd: true
         io_disks:
           data: []
       - io_disks:
           data:
-            - /dev/vdb
-            - /dev/vdc
-            - /dev/vdd
-            - /dev/vde
-            - /dev/vdf
-            - /dev/vdg
+            - /dev/sdb
+            - /dev/sdc
+            - /dev/sdd
+            - /dev/sde
+            - /dev/sdf
+            - /dev/sdg
     m0_clients:
         s3: 0
         other: 2
-  - hostname: node-2
-    data_iface: eth1
+  - hostname: ssu2
+    data_iface: eth0
     m0_servers:
       - runs_confd: true
         io_disks:
           data: []
       - io_disks:
           data:
-            - /dev/vdb
-            - /dev/vdc
-            - /dev/vdd
-            - /dev/vde
-            - /dev/vdf
-            - /dev/vdg
+            - /dev/sdb
+            - /dev/sdc
+            - /dev/sdd
+            - /dev/sde
+            - /dev/sdf
+            - /dev/sdg
     m0_clients:
         s3: 0
         other: 2
 pools:
   - name: the pool
     #type: sns  # optional; supported values: "sns" (default), "dix", "md"
-    data_units: 1
-    parity_units: 0
+    data_units: 4
+    parity_units: 2
     #allowed_failures: { site: 0, rack: 0, encl: 0, ctrl: 0, disk: 0 }
```

### 2.6. Bootstrap the cluster

* `sudo hctl bootstrap --mkfs motr-cluster.yaml`
* `hctl status`
  ```
  Profile: 0x7000000000000001:0x49
  Data Pools:
      0x6f00000000000001:0x4a
  Services:
      node-1
      [started ]  hax       0x7200000000000001:0x29  192.168.1.156@tcp:12345:1:1
      [started ]  confd     0x7200000000000001:0x2c  192.168.1.156@tcp:12345:2:1
      [offline ]  ioservice 0x7200000000000001:0x2f  192.168.1.156@tcp:12345:2:2
      [unknown ]  m0_client 0x7200000000000001:0x43  192.168.1.156@tcp:12345:4:1
      [unknown ]  m0_client 0x7200000000000001:0x46  192.168.1.156@tcp:12345:4:2

      node-2 (RC)
      [started ]  hax        0x7200000000000001:0x6   192.168.1.159@tcp:12345:1:1
      [started ]  confd      0x7200000000000001:0x9   192.168.1.159@tcp:12345:2:1
      [started ]  ioservice  0x7200000000000001:0xc   192.168.1.159@tcp:12345:2:2
      [unknown ]  m0_client  0x7200000000000001:0x20  192.168.1.159@tcp:12345:4:1
      [unknown ]  m0_client  0x7200000000000001:0x23  192.168.1.159@tcp:12345:4:2
  ```

### 2.7. Perform the I/O

* Write some data to Motr.
  ```bash
  m0cp -l 192.168.1.159@tcp:12345:4:1 -H 192.168.1.159@tcp:12345:1:1 \
       -p 0x7000000000000001:0x49 -P 0x7200000000000001:0x20 -o 21:40 \
       -s 1m -c 128 /home/src/single/random.img -L 9
  ```

* Read the data from Motr.
  ```bash
  m0cat -l 192.168.1.159@tcp:12345:4:1 -H 192.168.1.159@tcp:12345:1:1 \
        -p 0x7000000000000001:0x49 -P 0x7200000000000001:0x20 -o 21:40 \
        -s 1m -c 128 /home/src/single/random_from_motr.img -L 9
  ```

* Ensure that I/O succeeded.
  ```bash
  cmp random.img random_from_motr.img
  ```

<!-- ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ -->
## 3. Installation from sources (single-node setup)

### 3.1. Create a VM

Create a virtual machine.

### 3.2. Generate SSH keys

Follow
[these steps](CortxMotrQuickStart.md#Accessing-the-code-right-way).

### 3.3. Get Motr sources

Follow [these steps](CortxMotrQuickStart.md#Cloning-CORTX).

### 3.4. Compile and install Motr

```bash
cd cortx-motr
scripts/m0 make
sudo scripts/install-motr-service
cd -
```

### 3.5. Get Hare sources

```bash
git clone --recursive git@github.com:Seagate/cortx-hare.git -b main
```

### 3.6. Compile and install Hare

```bash
cd cortx-hare
make
sudo make devinstall
cd -
```

### 3.7. Prepare the CDF

See [step 1.4](#14-prepare-the-cdf).

### 3.8. Bootstrap the cluster

See [step 1.5](#15-bootstrap-the-cluster).

### 3.9. Perform the I/O

See [step 1.6](#16-perform-the-io).

<!-- ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ -->
## 4. KNOWN ISSUES

### 4.1. Pacemaker RPMs are not available

Pacemaker RPMs are needed for Hare.

The workaround:
```bash
curl 'https://raw.githubusercontent.com/Seagate/cortx-prvsnr/main/cli/src/cortx-prereqs.sh?token=APAGAPH5GQBM4LM54UOZJVK7B23XM' -o cortx-prereqs.sh; chmod a+x cortx-prereqs.sh

sudo ./cortx-prereqs.sh --disable-sub-mgr
```


Tested by
---------
2020.12.14 Single-node Setup and Dual-node Setup are verified by Huang Hua <hua.huang@seagate.com> in CentOS7.7.1908
