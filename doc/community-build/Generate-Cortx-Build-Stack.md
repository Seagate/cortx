
# Compile and Build Complete CORTX Stack using Docker

This document provides step-by-step instructions to build and generate the CORTX stack packages using Docker.

To know about various CORTX components, see [CORTX Components guide](https://github.com/Seagate/cortx/blob/main/doc/Components.md).

## Prerequisites

- All the prerequisites specified in the [Building the CORTX Environment for Single Node](Building-CORTX-From-Source-for-SingleNode.md) must be satisfied.

## Procedure

1. Run the following command to clone the CORTX repository:

   ```
   cd /root && git clone https://github.com/Seagate/cortx --recursive --depth=1
   ```

2. Run the following command to check out the codebase from the **main** branch for all components:

   ```
   docker run --rm -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.8.2003 make checkout BRANCH=main
   ```

3. Run the following command to create a directory to store packages:

   ```
   mkdir -p /var/artifacts
   ```

4. Run the following command to build the CORTX packages:

   ```
   docker run --rm -v /var/artifacts:/var/artifacts -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.8.2003 make clean build
   ```

   **Note:** It may take more than an hour to generate all the CORTX packages.

5. Run the following command to generate the ISO for each component:

   ```
   docker run --rm -v /var/artifacts:/var/artifacts -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.8.2003 make iso_generation
   ```

6. The CORTX build is generated in the directory created at step 3. To view the generated build, run:

   ```
   ll /var/artifacts/0/
   ```

   The system output displays as follows:

   ```
   [root@ssc-vm-2699 ~]# ll /var/artifacts/0/
   total 1060876
   drwxr-xr-x  12 root root      4096 Apr  9 07:23 3rd_party
   drwxr-xr-x   3 root root      4096 Apr  9 07:23 cortx_iso
   -rw-r--r--   1 root root      4395 Apr  9 07:23 cortx-prep-2.0.0-0.sh
   drwxr-xr-x   2 root root      4096 Apr  9 07:24 iso
   drwxr-xr-x 198 root root      4096 Apr  9 07:23 python_deps
   -rw-r--r--   1 root root 240751885 Apr  9 07:23 python-deps-1.0.0-0.tar.gz
   -rw-r--r--   1 root root 845556896 Apr  9 07:23 third-party-centos-7.8.2003-1.0.0-0.tar.gz
   ```

7. To view each component targets, run:

   ```
   docker run ghcr.io/seagate/cortx-build:centos-7.8.2003 make help
   ```

   The system output displays as follows:

   ```
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
   ```

8. Deploy the packages generated to create CORTX cluster using the instruction provided in [Deploy Cortx Build Stack guide](ProvisionReleaseBuild.md).


## Tested by:

- July 28 2021: Daniar Kurniawan (daniar@uchicago.edu) on baremetal servers hosted by Chameleon Cloud and Emulab Cloud.
- July 25 2021: Bari Arviv (bari.arviv@seagate.com) on Lyve Labs server - CentOS 7.8.2003.
- July 05 2021: Pranav Sahasrabudhe (pranav.p.shasrabudhe@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro.
