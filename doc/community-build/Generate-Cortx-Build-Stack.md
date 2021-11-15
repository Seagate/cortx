# Compile and Build Complete CORTX Stack

This document provides step-by-step instructions to build and generate the CORTX stack packages using Docker.

To know about various CORTX components, see [CORTX Components guide](https://github.com/Seagate/cortx/blob/main/doc/Components.md).

## Prerequisites

- All the prerequisites specified in the [Building the CORTX Environment for Single Node](Building-CORTX-From-Source-for-SingleNode.md) must be satisfied.
- Do not update OS or kernel package with `yum update` as the kernel version must be set to `3.10.0-1160.el7`
- Do not upgrade packages from CentOS 7.8 to CentOS 7.9
 

## Compile and Build CORTX Stack from HEAD

- Run the appropriate tag as per OS required i.e. CentOS 7.8 or CentOS 7.9. For example:

   - For CentOS 7.8.2003:
     ```
     docker pull ghcr.io/seagate/cortx-build:centos-7.8.2003
     ```
   - For CentOS 7.9.2009:
     ```
     docker pull ghcr.io/seagate/cortx-build:centos-7.9.2009
     ```

### Procedure

1. Run the following command to clone the CORTX repository:

   ```
   cd /root && git clone https://github.com/Seagate/cortx --recursive --depth=1
   ```

2. Run the following command to checkout the codebase from **2.0.0-527** branch:

   ```
   docker run --rm -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.9.2009 make checkout BRANCH=2.0.0-527 > /dev/null 2>&1
   cd ~/cortx/cortx-s3server; git checkout 2.0.0-527
   ```

   (Optional) Run the following command to checkout the codebase from **main** branch:
   
   ```
   docker run --rm -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.9.2009 make checkout BRANCH=main
   ```

3. Run the following command to create a directory to store packages:

   ```
   mkdir -p /var/artifacts/ && mkdir -p /mnt/cortx/{components,dependencies,scripts}
   ```

4. Run the following command to build the CORTX packages:

   ```
   docker run --rm -v /var/artifacts:/var/artifacts -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.9.2009 make clean build
   ```
   
   **Note:** This process takes some time to complete building the CORTX packages during `/var/artifacts/0 /` execution phase.

5. Run the following command to generate the ISO for each component:

   ```
   docker run --rm -v /var/artifacts:/var/artifacts -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.9.2009 make iso_generation
   ```

6. The CORTX build is generated in the directory created at step 3. To view the generated build, run:

    ```
    ll /var/artifacts/0
    ```
 
## (Optional) Compile and Build CORTX Stack as per Individual component

7. Run to view each component targets:
   ```
   docker run ghcr.io/seagate/cortx-build:centos-7.9.2009 make help
   ```
   
   [![cortx_stack_individual_component.png](https://github.com/Seagate/cortx/blob/main/doc/images/cortx_stack_individual_component.jpg "cortx_stack_individual_component.png")](https://github.com/Seagate/cortx/blob/main/doc/images/cortx_stack_individual_component.jpg "cortx_stack_individual_component.png") 

8. Deploy the packages generated to create CORTX cluster using the instruction provided in [Deploy Cortx Build Stack guide](ProvisionReleaseBuild.md).

9. If you encounter any issue while following the above steps, see [Troubleshooting guide](https://github.com/Seagate/cortx/blob/main/doc/community-build/Troubleshooting.md)


### Tested by:

- Oct 29 2021: Rose Wambui (rose.wambui@seagate.com) on a Mac running VMWare Fusion 12.2 Pro for CentOs 7.9.2009
- Oct 21 2021: Rose Wambui (rose.wambui@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro for CentOs 7.9.2009
- Oct 19 2021: Mukul Malhotra (mukul.malhotra@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro for CentOS 7.9.2009
- Aug 19 2021: Bo Wei (bo.b.wei@seagate.com) on a Windows laptop running VirtualBox 6.1.
- Aug 18 2021: Jalen Kan (jalen.j.kan@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro.
- July 28 2021: Daniar Kurniawan (daniar@uchicago.edu) on baremetal servers hosted by Chameleon Cloud and Emulab Cloud.
- July 25 2021: Bari Arviv (bari.arviv@seagate.com) on Lyve Labs server - CentOS 7.8.2003.
- July 05 2021: Pranav Sahasrabudhe (pranav.p.shasrabudhe@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro.
