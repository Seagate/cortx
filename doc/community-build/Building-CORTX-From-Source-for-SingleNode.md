# Setting up the CORTX Environment for Single Node

You can deploy the CORTX stack from the source code. You can built CORTX on any hypervisor, including VMware Workstation, VMware vSphere, or on AWS instance. This document provides step-by-step instructions to deploy and configure the CORTX environment for a single-node Virtual Machine (VM).

The CORTX deployment and configuration is a four-step procedure:

-   Generate all the CORTX service packages such as Motr, S3Server, HA, HARE, Manager, Management-portal, etc. To know more about CORTX components, see [CORTX Components guide](https://github.com/Seagate/cortx/blob/main/doc/Components.md).
-   Deploy the generated packages to create and run the CORTX cluster.
-   After the CORTX cluster is running, configure the CORTX GUI to communicates with different CORTX components and features.
-   Configure the S3 account to perform the IO operations.

## Prerequisites

-   Create a [CentOS 7.9.2009](http://repos-va.psychz.net/centos/7.9.2009/isos/x86_64/) VM with following minimum configuration:

    - RAM: 8GB
    - Processor: 4
    - NICs: 3
    - Total Disk:
       - 1 OS disk of 50GB
       - 2 data disks of 32GB each
    
-   All Network Interface Cards (NICs) must have internet access. Attach your network adapters accordingly as per your environment to establish internet connectivity. For this deployment, the NICs are considered as eth33, eth34, and eth35.
-   The VM must have a valid hostname and accessible using ping operation.

### Recommendations:

- If you are using multiple virtualization platforms including VMware Workstation, vSphere etc. then after rebooting ensure that all the NICs have internet connectivity.
- While deploying the CORTX single node VM, if the deployment fails during any step. It is recommended that you must clean up the steps and rerun the steps.
- It is recommended to use para-virtualized drivers in the guest VM for best performance.

## Procedure

1. Become a root user using following command:
    ```
    sudo su -
    ```
2. Run the following command:
    ```
    yum install git docker epel-release yum-utils wget -y && yum install nginx -y 
    hostnamectl set-hostname --static --transient --pretty deploy-test.cortx.com
    sed -i 's/SELINUX=enforcing/SELINUX=disabled/' /etc/selinux/config
    ```
    
    **Note:**  Use this hostname to avoid issues further in the bootstrap process. Verify the hostname is updated using  `hostname -f`

3. Start the Docker services:
    ```
    systemctl start docker
    systemctl enable docker
    ```
4. Reboot your VM using the following command: `Reboot`
5. Generate the CORTX deployment packages using the instructions provided in [Generating the CORTX packages guide](Generate-Cortx-Build-Stack.md).
6. Deploy the packages generated to create CORTX cluster using the instruction provided in [Deploy Cortx Build Stack guide](ProvisionReleaseBuild.md).
7. Configure the CORTX GUI using the instruction provided in [Configuring the CORTX GUI document](https://github.com/Seagate/cortx/blob/main/doc/Preboarding_and_Onboarding.rst).
8. Create an S3 account and perform the IO operations using the instruction provided in [IO operation in CORTX](https://github.com/Seagate/cortx/blob/main/doc/Performing_IO_Operations_Using_S3Client.rst).

### Tested by:

- Aug 31 2021: Mukul Malhotra (mukul.malhotra@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro.
