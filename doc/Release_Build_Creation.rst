=========================================================
Compile & Build Complete Cortx Stack Via Docker Container
=========================================================

This file consists of the procedure that should be followed to generate the release build outside the Seagate network using `cortx-build <https://github.com/orgs/Seagate/packages/container/package/cortx-build>`_ docker image. 

*****************************************
Prerequisites to setup Virtual Machine
*****************************************

- **Single-Node VM deployment:**
  
  - Setup a `CentOS 7.8.2003 <http://isoredirect.centos.org/centos/7.8.2003/isos/x86_64/>`_ system with the following configuration in Virtual Machine (VM):
  - Create VM(s) with at least 4 vCPUs and 4GB of RAM.
  - Minimum 3 NIC is required on different network as per recommendation
  - Storage Configuration:
    
      - Usecase-1 (Erasure coding with units N(data) + K (parity) + S (spare) as 4 + 2 + 2):
      
        - Min 9 Disks
        
          - Data Disks - Min 8 Disks (Capacity 10GB+)
          - Metadata Disks - Min 1 Disks (Capacity - 5GB+)
      
      - Usecase-2 (Erasure coding with units N(data) + K (parity) + S (spare) as 1 + 0 + 0):
      
        - Min 3 Disks
        
          - Data Disks - Min 1 Disks (Capacity 50GB+)
          - Metadata Disks - Min 1 Disks (Capacity - 40GB)
      
      - Usecase-3 [Future Feature Request - (Erasure coding with units N(data) + K (parity) + S (spare) as 4 + 2 + 0)]:
      
        - Min 7 Disks
        
          - Data Disks - Min 6 Disks (Capacity 10G+)
          - Metadata Disks - Min 1 Disks (Capacity - 5GB+)
      
**Note:** You also create disk partitions and present those as devices (in case you have insufficient number of virtual disks)
    
- Ensure IP’s have assigned to all NICs. For this deployment interface name is considered as eth33, eth34, and eth35.
- Ensure that the system have valid hostname and are accessible using ping.


*************************
Procedure for Build Steps
*************************
   
#. Install the docker packages in the system or VM. Refer to `Docker Installation <https://docs.docker.com/engine/install/centos/>`_.

#. Clone the repositories of the required components on VM at /root/cortx (You can use any other directory by updating the docker run command accordingly). Clone the entire CORTX repository by running the following command.

   ::
   
    cd /root && git clone https://github.com/Seagate/cortx --recursive --depth=1
    
#. Checkout codebase from **main** branch for all components. 
   
   ::
   
      docker run --rm -v /var/artifacts:/var/artifacts -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.8.2003 make checkout BRANCH=main
      
   You can checkout codebase from other branch/TAG for all components using above command. e.g. For **stable** branch replace **main** with **stable**.
   
#. Create directory to store artifacts. In this procedure, **/var/artifacts** is used. Update **docker run** command accordingly to use an alternative directory.

   ::
   
    mkdir -p /var/artifacts

#. Build CORTX artifacts using the below mentioned docker command. 
    **Note:** This step can take over an hour to run. Optionally you can prefix this command with ``time`` to show how long the build took.

   ::
   
    docker run --rm -v /var/artifacts:/var/artifacts -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.8.2003 make clean build
    
#. Generate the ISO by running the below mentioned command.

   ::
   
    docker run --rm -v /var/artifacts:/var/artifacts -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.8.2003 make iso_generation
    
   You can also append the **iso_generation** target in **make build** command (step 6).
   
#. After the **docker run** execution is complete, the  release build will be available at the following location.

   ::

    [root@ssc-vm-2699 ~]# ll /var/artifacts/0/
      total 1060876
      drwxr-xr-x  12 root root      4096 Apr  9 07:23 3rd_party
      drwxr-xr-x   3 root root      4096 Apr  9 07:23 cortx_iso
      -rw-r--r--   1 root root      4395 Apr  9 07:23 cortx-prep-2.0.0-0.sh
      drwxr-xr-x   2 root root      4096 Apr  9 07:24 iso
      drwxr-xr-x 198 root root      4096 Apr  9 07:23 python_deps
      -rw-r--r--   1 root root 240751885 Apr  9 07:23 python-deps-1.0.0-0.tar.gz
      -rw-r--r--   1 root root 845556896 Apr  9 07:23 third-party-centos-7.8.2003-1.0.0-0.tar.gz
      
================================
Compile & Build Cortx Components
================================
          
#. To list individual component targets, execute the below mentioned command.
 
   ::
    
    docker run ghcr.io/seagate/cortx-build:centos-7.8.2003 make help
    
   The output will be displayed as follows.
    
   ::
   
    [root@ssc-vm-1613 cortx-**]# docker run ghcr.io/seagate/cortx-build:centos-7.8.2003 make help
    usage: make "target"

    Please clone required component repositories in cortx-workspace directory before executing respective targets.

    targets:
    
        help: print this help message.
        
        clean: remove existing /var/artifacts/0 directory.
        
        build: generate complete CORTX build including third-party-deps at "/var/artifacts/0"
        
        control-path: generate control-path packages. cortx-provisioner, cortx-monitor, cortx-manager, cortx-management-portal and cortx-ha.
        
        io-path: generate io-path packages. cortx-motr, cortx-s3server and cortx-hare.
        
        cortx-motr: generate cortx-motr packages.
        
        cortx-s3server: generate cortx-s3server packages.
        
        cortx-hare: generate cortx-hare packages.
        
        cortx-ha: generate cortx-ha packages.
        
        cortx-management-portal: generate cortx-management-portal packages.
        
        cortx-manager: generate cortx-manager packages.
        
        cortx-monitor: generate cortx-monitor packages.
        
        cortx-posix: generate cortx-posix (NFS) packages.
        
        cortx-prvsnr: generate cortx-prvsnr packages.
        
        iso_generation: generate ISO file from release build.
        
#. Follow this `Guide <Provision Release Build.md>`_ to Deploy Cortx Build Stack.

**Tested by:**

- May 19, 2021: Justin Woo (justin.woo@seagate.com) on a Windows 10 Desktop running VMware Workstation 16 Pro.
- May 10, 2021: Christina Ku (christina.ku@seagate.com) on VM "CentOS 7.8.2003 x86_64".
- May 7, 2021: Mukul Malhotra (mukul.malhotra@seagate.com) on a Windows 10 desktop running VMWare Workstation 16 Pro.
- Apr 6, 2021: Harrison Seow (harrison.seow@seagate.com) on a Windows 10 desktop running VMWare Workstation 16 Player.
- Feb 12, 2021: Patrick Hession (patrick.hession@seagate.com) on a Windows laptop running VMWare Workstation Pro 16.
- April 06, 2021: Vaibhav Paratwar (vaibhav.paratwar@seagate.com) on VM "LDRr1 - 2x CentOS 7.8 Shared Disks-20210329-232113"
