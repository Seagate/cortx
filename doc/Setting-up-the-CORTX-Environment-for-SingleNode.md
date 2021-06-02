# Setting up the CORTX Environment for Single Node

You can deploy CORTX stack from the source code. This document provides step-by-step instructions to deploy and configure the CORTX environment for a single-node Virtual Machine (VM).
For a single node VM, you can create CORTX environment in two ways:

-   Single node deployment with one data disk configuration: For this deployment, total three disks are required: an OS system disk, a data disk, and a Metadata disk.
-   Single node deployment with multiple (N) data disk configuration: For this deployment, you can use N number of data disks and metadata disks.

The CORTX deployment and configuration is four step procedure:

-   Generate all the CORTX service packages such as Motr, S3Server, HA, HARE, Manager, Management-portal, etc.
-   Deploy the generated packages to create and run the CORTX cluster.
-   After the CORTX cluster is running, configure the CORTX GUI to communicates with different CORTX components and features.
-   Configure the S3 account to perform the IO operations.

## Prerequisites

-   Create a [CentOS 7.8.2003](http://repos-va.psychz.net/centos/7.8.2003/isos/x86_64/) VM with following minimum configuration:

  	- For single node deployment with one data disk configuration:

  		| Hardware         | Minimum Requirement                          |
  		|------------------|----------------------------------------------|
  		|  RAM             | 4GB                                          |
  		|  Processor       | 2 core                                       |
  		|  NICs            | 3                                           |
  		| HDD	             | 3                                           |
  		|  OS system HDD   | 1 disk of 20GB capacity                      |
  		|  Data HDD        | 1 disk of 20GB capacity                      |
  		|  Metadata HDD    | 1 disk with the capacity of 10% of data disk |

  	- For single node deployment with multiple (N) data disk configuration:

  		| Hardware                | Minimum Requirement                                                                              |
  		|-------------------------|--------------------------------------------------------------------------------------------------|
  		|  RAM                    | 4GB                                                                                              |
  		|  Processor              | 2 core                                                                                           |
  		|  NICs                   |  3                                                                                               |
  		| HDD	                    |  N                                                                                               |
  		|  OS system HDD          | 1 disk of 20GB capacity                                                                          |
  		|  Data HDD               | N disks. The N should be greater than one and each HDD capacity must be more than 10GB. |
  		|  Metadata HDD           | 1 disk with the capacity of 10% of data disk                                                     |

-   All Network Interface Cards (NICs) must have internet access. For this deployment the NICs are considered as eth33, eth34, and eth35.
-   The VM must have a valid hostname and are accessible using ping operations.
-   Install the Docker packages in the VM. See to [Docker Installation](https://docs.docker.com/engine/install/centos/).
-   Run the following command to install the Git:
    ```
      yum install git
    ```
-   Run the following command to update the hostname:  
    ```
      sudo hostnamectl set-hostname deploy-test.cortx.com
    ```
      **Note:**  Use this hostname to avoid issues further in the bootstrap process. Verify the hostname is updated using  `hostname -f`

-   Disable the SElinux by running:    
    ```
      sed -i 's/SELINUX=enforcing/SELINUX=disabled/' /etc/selinux/config
    ```
-   Reboot your VM using following command: `Reboot`
-   Start the Docker services:
    ```
    	sudo systemctl start docker
    ```

## Procedure

1. Generate the CORTX deployment packages using the instructions provided in [Generating the CORTX packages guide](https://github.com/Seagate/cortx/blob/main/doc/Release_Build_Creation.rst).
2. Deploy the packages generated to create CORTX cluster using the instruction provided in [Deploy Cortx Build Stack guide](https://github.com/Seagate/cortx/blob/main/doc/ProvisionReleaseBuild.md).
3. Configure the CORTX GUI using the instruction provided in [Configuring the CORTX GUI document](https://github.com/Seagate/cortx/blob/main/doc/Preboarding_and_Onboarding.rst).
4. Create an S3 account and perform the IO operations using the instruction provided in [IO operation in CORTX](https://github.com/Seagate/cortx/blob/main/doc/testing_io.rst).
