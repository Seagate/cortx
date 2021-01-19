=======================
Release Build Creation
=======================

This file consists of the procedure that must be followed to generate the release build outside the Seagate network.

***************
Procedure
***************

#. Setup a CentOS 7.8.2003 system.

   - You can use a Virtual Machine (VM) also.
   
#. Install the docker packages in the system or VM. Refer to `Docker Installation <https://docs.docker.com/engine/install/centos/>`_.

#. Login to GitHub Docker.

#. Clone the repositories of the required components on VM at /root/cortx (You can use any other directory by updating the docker run command accordingly). Clone the entire CORTX repository by running the following command.

   ::
   
    cd /root && git clone https://github.com/Seagate/cortx --recursive
   
#. Create directory to store artifacts. In this procedure, **/var/artifacts** is used. Update **docker run** command accordingly to use an alternative directory.

   ::
   
    mkdir -p /var/artifacts

#. Build CORTX artifacts using the below mentioned docker.

   ::
   
    time docker run --rm -v /var/artifacts:/var/artifacts -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.8.2003 make clean build -i
    
#. Generate the ISO by running the below mentioned command.

   ::
   
    docker run --rm -v /var/artifacts:/var/artifacts -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.8.2003 make iso_generation.
    
   You can also append the **iso_generation** target in **make build** command (step 5).
   
#. After the **docker run** execution is complete, the  release build will be available at the following location.

   ::

    [root@ssc-vm-1321 opensource-ci]# ll /var/artifacts/0/
   
    total 824368
   
    drwxr-xr-x 10 root root 4096 Dec 16 05:34 3rd_party
   
    drwxr-xr-x 3 root root 4096 Dec 16 05:23 cortx_iso
   
    drwxr-xr-x 2 root root 4096 Dec 16 05:49 iso
    
#. To list individual component targets, execute the below mentioned command.
 
   ::
    
    docker run ghcr.io/seagate/cortx-build:centos-7.8.2003 make help
    
   The output will be displayed as follows.
    
   ::
   
    [root@ssc-vm-1613 cortx-**]# time docker run ghcr.io/seagate/cortx-build:centos-7.8.2003 make help
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
