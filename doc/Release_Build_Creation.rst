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

#. Clone the CORTX repository by running the following command.

   ::
   
    cd /root && git clone https://github.com/Seagate/cortx --recursive
    
   The CORTX repository gets cloned at the **/root** location. If you want to clone the repository in another location, replace  **/root/cortx** with the appropriate location.
   
#. Build CORTX artifacts using the below mentioned docker.

   ::
   
    time docker run --rm -v /var/artifacts:/var/artifacts -v /root/cortx:/cortx-workspace -e GITHUB_TOKEN=$GITHUB_TOKEN ghcr.io/seagate/cortx-build:centos-7.8.2003 make clean build
    
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
