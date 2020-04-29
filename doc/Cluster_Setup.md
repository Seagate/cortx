# CORTXv1.0 VIRTUAL CLUSTERS SETUP
This is a step by step guide to get CORTX virtual clustur setup ready.

## Create single node cluster by using last successful build

### 1. Create one VM by using cloud
To create cloud VM visit [here](https://ssc-cloud.colo.seagate.com/ui/service/login).

### 2. Install the lustre RPM’s from last successfull build
  * `$ yum install http://ci-storage.mero.colo.seagate.com/releases/eos/integration/centos7.7.1908/last_successful/kmod-lustre-client-2.12.3-1.el7.x86_64.rpm`
  * `$ yum install http://ci-storage.mero.colo.seagate.com/releases/eos/integration/centos-7.7.1908/last_successful/lustre-client-2.12.3-1.el7.x86_64.rpm`
  
### 3. Install the eos-core rpm
  * `$ yum install http://ci-storage.mero.colo.seagate.com/releases/eos/integration/centos-7.7.1908/last_successful/eos-core-1.0.0-264_gited8d450eb_3.10.0_1062.el7`
  
### 4. Install the eos-hare rpm
Currently pacemaker rpm is not available in VM, until then refer Known issues section and install it.
  * `$ yum install http://ci-storage.mero.colo.seagate.com/releases/eos/integration/centos-7.7.1908/last_successful/eos-hare-1.0.0-452_gitd0ba14d.el7.x86_64.rpm`
  
### 5. Verification of RPM’s
  * `$ rpm -qa | grep eos`

    ```
    eos-hare-1.0.0-464_git76d5d31.el7.x86_64
    eos-core-1.0.0-275_git15bae3359_3.10.0_1062.el7.x86_64
    ```
  * `$ rpm -qa | grep lustre`
  
    ```
    kmod-lustre-client-2.12.3-1.el7.x86_64
    lustre-client-2.12.3-1.el7.x86_64
    ```

### 6. Configure Lnet
Create lnet.conf file, if not exist and restart the service
  * `$  cat /etc/modprobe.d/lnet.conf`
  
    ```
    options lnet networks=tcp(eth1) config_on_load=1
    ```

  * `$ systemctl restart lnet`
  * `$ lctl list_nids`, Make sure it display lnet nid
  
    ```
    192.168.1.159@tcp
    ```
    
### 7. Configure Hare CDF (cluster definition file)
  * Copy single node conf yaml file.
    * `$ cp /opt/seagate/eos/hare/share/cfgen/examples/singlenode.yaml .`
  * Change the hostname and io_disks in singlenode.yaml, below is the diff of against original file.
    * `$ diff /opt/seagate/eos/hare/share/cfgen/examples/singlenode.yaml singlenode.yam`
    
      ```
      5c5
      - hostname: localhost 
      ---
      - hostname: ssc-vm-0171.colo.seagate.com
      11c11
      - io_disks: { path_glob: "/dev/loop[0-9]*" }
      ---
      - io_disks: { path_glob: "/dev/sd[b-i]" }
      ```
  
### 8. Start the single node cluster and check the cluster status
  * `$ # hctl bootstrap --mkfs singlenode.yaml`
  * `$ hctl status`
  
    ```
    Profile: 0x7000000000000001:0x26
    Data Pools:
    0x6f00000000000001:0x27
    Services:
    ssc-vm-0171.colo.seagate.com (RC)
    [started ] hax 0x7200000000000001:0x6 192.168.1.160@tcp:12345:1:1
    [started ] confd 0x7200000000000001:0x9 192.168.1.160@tcp:12345:2:1
    [started ] ioservice 0x7200000000000001:0xc 192.168.1.160@tcp:12345:2:2 
    [unknown ] m0_client 0x7200000000000001:0x20 192.168.1.160@tcp:12345:4:1 
    [unknown ] m0_client 0x7200000000000001:0x23 192.168.1.160@tcp:12345:4:2 
    ```

### 9. Write data in to the mero
  * `$ c0cp -l 192.168.1.160@tcp:12345:4:1 -H 192.168.1.160@tcp:12345:1:1 -p 0x7000000000000001:0x26 -P 0x7200000000000001:0x23 -o 12:10 -s 1m -c 128 /home/src/single/random.img -L 9`

### 10. Read data from the mero
  * `$ c0cat -l 192.168.1.160@tcp:12345:4:1 -H 192.168.1.160@tcp:12345:1:1 -p 0x7000000000000001:0x26 -P 0x7200000000000001:0x23 -o 12:10 -s 1m -c 128 /home/src/single/random_from_mero.img -L 9`
  
### 11. Verify the data by using md5
  * $ md5sum random.img random_from_mero.img`
  
    ```
    cc314a3dc4d9fbbfd8c8a1859818b51c random.img
    cc314a3dc4d9fbbfd8c8a1859818b51c random_from_mero.img
    ```

## Create Dual node cluster by using last successful build

### 1. Create two VM’s by using cloud
  * To create cloud VM visit [here](https://ssc-cloud.colo.seagate.com/ui/service/login).
  
### 2. Execute in both nodes. (Node-1 & Node-2)
  * Install the luster RPM’s from last successfull build
    * `$ yum install http://ci-storage.mero.colo.seagate.com/releases/eos/integration/centos- 7.7.1908/last_successful/kmod-lustre-client-2.12.3-1.el7.x86_64.rpm`
    * `$ yum install http://ci-storage.mero.colo.seagate.com/releases/eos/integration/centos-7.7.1908/last_successful/lustre-client-2.12.3-1.el7.x86_64.rpm`

  * Install the eos-core rpm
    * `$ yum install http://ci-storage.mero.colo.seagate.com/releases/eos/integration/centos-7.7.1908/last_successful/eos-core-1.0.0-264_gited8d450eb_3.10.0_1062.el7`
    
  * Install the eos-hare rpm
    * `$ yum install http://ci-storage.mero.colo.seagate.com/releases/eos/integration/centos-7.7.1908/last_successful/eos-hare-1.0.0-452_gitd0ba14d.el7.x86_64.rpm`

  * Verification of RPM’s
    * `$ rpm -qa | grep eos`
    
      ```
      eos-hare-1.0.0-464_git76d5d31.el7.x86_64
      eos-core-1.0.0-275_git15bae3359_3.10.0_1062.el7.x86_64
      ```
    
    * `$ rpm -qa | grep lustre`
    
      ```
      kmod-lustre-client-2.12.3-1.el7.x86_64
      lustre-client-2.12.3-1.el7.x86_64
      ```
  * Configure Lnet
  Create lnet.conf file, if not exist and restart the service.
    * `$ cat /etc/modprobe.d/lnet.con`
    
      ```
      options lnet networks=tcp(eth1) config_on_load=1
      ```
      
    * `$ systemctl restart lnet`
    * `$ lctl list_nids`
    
      ```
      192.168.1.159@tcp
      ```
      
  * Set passwordless between two nodes for root user
    * `$ ssh-keygen`, Copy from Node-1 /root/.ssh/id_rsa.pub keys in to the Node-2 /root/.ssh/authorized_keys
    
    * From Node-1  
      `$ ssh root@<Node-2 hostname>`, (Login will success without password.
    
    * From Node-2  
      `$ ssh root@<Node-1 hostname>`, Login will success without password.
      
### 3. Execute in NODE-1
  * Configure Hare CDF
    * Copy ees-cluster.yaml file and change the hostname, io_disks, data_iface and data_iface_type.
    
      * `$ cp /opt/seagate/eos/hare/share/cfgen/examples/ees-cluster.yaml .`
      * `$ diff /opt/seagate/eos/hare/share/cfgen/examples/ees-cluster.yaml ees-cluster.yaml`
      
        ```
        5,7c5,7
        < - hostname: pod-c1
        < data_iface: eth1_c1 # name of data network interface
        < data_iface_type: o2ib # LNet type of network interface (optional);
        ---
        - hostname: ssc-vm-c-441.colo.seagate.com 
        > data_iface: eth1 # name of data network interface
        > data_iface_type: tcp # LNet type of network interface (optional);
        11c11
        < - io_disks: { path_glob: "/dev/sd[d-g]" }
        ---
        - io_disks: { path_glob: "/dev/sd[b-g]" }
        15,17c15,17
        < - hostname: pod-c2
        < data_iface: eth1_c2
        < data_iface_type: o2ib
        ---
        - hostname: ssc-vm-c-442.colo.seagate.com
        data_iface: eth1
        data_iface_type: tcp
        20c20
        < - io_disks: { path_glob: "/dev/sd[h-k]" }
        ---
        - io_disks: { path_glob: "/dev/sd[b-g]" }
        ```
  * Start the dual-node cluster
    * `$ hctl bootstrap --mkfs ees-cluster.yaml`
    * `$ hctl status`
    
      ```
      Profile: 0x7000000000000001:0x49
      Data Pools:
      0x6f00000000000001:0x4a
      Services:
      ssc-vm-c-442.colo.seagate.com
      [started ] hax 0x7200000000000001:0x29 192.168.1.156@tcp:12345:1:1
      [started ] confd 0x7200000000000001:0x2c 192.168.1.156@tcp:12345:2:1 
      [offline ] ioservice 0x7200000000000001:0x2f 192.168.1.156@tcp:12345:2:2
      [unknown ] m0_client 0x7200000000000001:0x43 192.168.1.156@tcp:12345:4:1 
      [unknown ] m0_client 0x7200000000000001:0x46 192.168.1.156@tcp:12345:4:2
      
      ssc-vm-c-441.colo.seagate.com (RC)
      [started ] hax 0x7200000000000001:0x6 192.168.1.159@tcp:12345:1:1
      [started ] confd 0x7200000000000001:0x9 192.168.1.159@tcp:12345:2:1
      [started ] ioservice 0x7200000000000001:0xc 192.168.1.159@tcp:12345:2:2
      [unknown ] m0_client 0x7200000000000001:0x20 192.168.1.159@tcp:12345:4:1
      [unknown ] m0_client 0x7200000000000001:0x23 192.168.1.159@tcp:12345:4:2
      ```
      
  * Write data in to the mero
    * `$ c0cp -l 192.168.1.159@tcp:12345:4:1 -H 192.168.1.159@tcp:12345:1:1 -p 0x7000000000000001:0x49 -P 0x7200000000000001:0x23 -o 21:40 -s 1m -c 128 /home/src/single/random.img -L 9`
    
  * Read data from mero
    * `$ c0cat -l 192.168.1.159@tcp:12345:4:1 -H 192.168.1.159@tcp:12345:1:1 -p 0x7000000000000001:0x49 -P 0x7200000000000001:0x23 -o 21:40 -s 1m -c 128 /home/src/single/random_from_mero.img -L 9`
    
  * Verify the data by using md5
    * `$ md5sum random.img random_from_mero.img`
    
      ```
      cc314a3dc4d9fbbfd8c8a1859818b51c random.img
      cc314a3dc4d9fbbfd8c8a1859818b51c random_from_mero.img
      ```
  
## Create single node cluster by using source code

### 1. Create two VM’s by using cloud
  * To create cloud VM visit [here](https://ssc-cloud.colo.seagate.com/ui/service/login).
  
### 2. Generate ssh-keys
  * To generate ssh keys, follow the steps given [here](MeroQuickStart.md#Create-SSH-Public-Key).
  
### 3. Clone the mero code, compile and install
  * To Clone the mero code follow the steps given[here](MeroQuickStart.md#Cloning-CORTX).
  * To compile mero code `$ sudo ./scripts/m0 make`. (It is assumed you are in main source directory of mero code )
  * To install mero services `$ sudo ./scripts/install-mero-service`.
  * `$ cd ..`
  
### 4. Clone the hare code, compile and instal
  * `$ git clone --recursive ssh://git@gitlab.mero.colo.seagate.com:6022/mero/hare.git`
  * `$ cd hare`
  * `$ sudo make`
  * `$ sudo make devinstall`
  * `$ cd ..`
  
### 5. Configure Hare CDF
  *
  
