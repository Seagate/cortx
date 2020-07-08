# CORTX v1.0 Virtual Clusters Setup

This is a step by step guide to get CORTX virtual clustur setup ready.

## 1. Single-node setup

### 1.1. Create a VM

Create a virtual machine using
[Red Hat CloudForms](https://ssc-cloud.colo.seagate.com/ui/service/login).

### 1.2. Install the RPMs

* Add 'last_successful' yum repository.
  ```bash
  REPO=ci-storage.mero.colo.seagate.com/releases/eos
  REPO+=/github/master/rhel-7.7.1908/last_successful

  sudo yum-config-manager --add-repo="http://$REPO"
  sudo tee -a /etc/yum.repos.d/${REPO//\//_}.repo <<< 'gpgcheck=0'
  ```

* Install the RPMs.

  :warning: Currently the Pacemaker RPMs, required by `cortx-hare`, are not
  available in the VM.  See the workaround in
  [KNOWN ISSUES](#41-pacemaker-rpms-are-not-available) section.

  ```bash
  sudo yum install -y cortx-hare
  ```

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

  - ensure that the disks referred to by `io_disks` values exist
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

<!-- XXX TODO:
  - Describe the options of `c0cp` and `c0cat` commands.
  - Where do values come from?
  -->

* Write some data to Motr.
  ```bash
  c0cp -l 192.168.1.160@tcp:12345:4:1 -H 192.168.1.160@tcp:12345:1:1 \
       -p 0x7000000000000001:0x26 -P 0x7200000000000001:0x23 -o 12:10 \
       -s 1m -c 128 /home/src/single/random.img -L 9
  ```

* Read the data from Motr.
  ```bash
  c0cat -l 192.168.1.160@tcp:12345:4:1 -H 192.168.1.160@tcp:12345:1:1 \
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

Create two virtual machines using
[Red Hat CloudForms](https://ssc-cloud.colo.seagate.com/ui/service/login).

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

### 2.3. Install the RPMs

Execute [step 1.2](#12-install-the-rpms) **on both nodes**.

### 2.4. Configure LNet

Execute [step 1.3](#13-configure-lnet) **on both nodes**.

### 2.5. Prepare the CDF

Edit a copy of `/opt/seagate/eos/hare/share/cfgen/examples/ees-cluster.yaml`.
See [section 1.4](#14-prepare-the-cdf) for details.

Sample diff:
```diff
--- /opt/seagate/cortx/hare/share/cfgen/examples/ees-cluster.yaml	2020-05-19 11:41:18.077166724 +0000
+++ ees-cluster.yaml	2020-05-19 21:32:32.258714734 +0000
@@ -2,10 +2,8 @@
 # See `cfgen --help-schema` for the format description.

 nodes:
-  - hostname: pod-c1        # [user@]hostname
-    data_iface: eth1_c1     # name of data network interface
-    data_iface_type: o2ib   # LNet type of network interface (optional);
-                            # supported values: "tcp" (default), "o2ib"
+  - hostname: node-1        # [user@]hostname
+    data_iface: eth1     # name of data network interface
     m0_servers:
       - runs_confd: true
         io_disks: []
@@ -17,9 +15,8 @@
     m0_clients:
         s3: 0           # number of S3 servers to start
         other: 2        # max quantity of other Motr clients this node may have
-  - hostname: pod-c2
-    data_iface: eth1_c2
-    data_iface_type: o2ib
+  - hostname: node-2
+    data_iface: eth1
     m0_servers:
       - runs_confd: true
         io_disks: []
```

### 2.6. Bootstrap the cluster

* `sudo hctl bootstrap --mkfs ees-cluster.yaml`
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

<!-- XXX TODO:
  - Describe the options of `c0cp` and `c0cat` commands.
  - Where do values come from?
  -->

* Write some data to Motr.
  ```bash
  c0cp -l 192.168.1.159@tcp:12345:4:1 -H 192.168.1.159@tcp:12345:1:1 \
       -p 0x7000000000000001:0x49 -P 0x7200000000000001:0x23 -o 21:40 \
       -s 1m -c 128 /home/src/single/random.img -L 9
  ```

* Read the data from Motr.
  ```bash
  c0cat -l 192.168.1.159@tcp:12345:4:1 -H 192.168.1.159@tcp:12345:1:1 \
        -p 0x7000000000000001:0x49 -P 0x7200000000000001:0x23 -o 21:40 \
        -s 1m -c 128 /home/src/single/random_from_motr.img -L 9
  ```

* Ensure that I/O succeeded.
  ```bash
  cmp random.img random_from_motr.img
  ```

<!-- ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ -->
## 3. Installation from sources (single-node setup)

### 3.1. Create a VM

Create a virtual machine using
[Red Hat CloudForms](https://ssc-cloud.colo.seagate.com/ui/service/login).

### 3.2. Generate SSH keys

Follow
[these steps](CortxMotrQuickStart.md#Accessing-the-code-right-way).

### 3.3. Get Motr sources

Follow [these steps](CortxMotrQuickStart.md#Cloning-CORTX).

### 3.4. Compile and install Motr

```bash
cd cortx-motr
scripts/m0 make
sudo scripts/install-mero-service
cd -
```

### 3.5. Get Hare sources

```bash
git clone --recursive ssh://git@gitlab.mero.colo.seagate.com:6022/mero/hare.git
```
NOTE: Hare component is not migrated to Github yet, doc will be updated once Hare repo is migrated.

### 3.6. Compile and install Hare

```bash
cd hare
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
curl 'https://raw.githubusercontent.com/Seagate/cortx-prvsnr/master/cli/src/cortx-prereqs.sh?token=APAGAPH5GQBM4LM54UOZJVK7B23XM' -o cortx-prereqs.sh; chmod a+x cortx-prereqs.sh

sudo ./cortx-prereqs.sh --disable-sub-mgr
```
