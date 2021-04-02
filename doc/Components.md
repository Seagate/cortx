# Components of CORTX

## A list of the Components of CORTX

1. [Motr](https://github.com/seagate/cortx-motr)
1. [Provisioner](https://github.com/Seagate/cortx-prvsnr)
1. [Experiments](https://github.com/Seagate/cortx-experiments)
1. [HA](https://github.com/Seagate/cortx-ha/)
1. [HARE](https://github.com/Seagate/cortx-hare/)
1. [Management Portal](https://github.com/Seagate/cortx-management-portal)
1. [Manager](https://github.com/Seagate/cortx-manager)
1. [Monitor](https://github.com/Seagate/cortx-monitor)
1. [POSIX](https://github.com/Seagate/cortx-posix)
1. [S3-Server](https://github.com/Seagate/cortx-s3server)
1. [Utils](https://github.com/Seagate/cortx-utils)

## Definitions and Links to each of the components

### [Motr](https://github.com/seagate/cortx-motr)

At the core of CORTX lies Motr. Motr is a distributed object storage system, targeting mass capacity storage configurations. 
To ensure the most efficient storage utilization, Motr interacts directly with block devices (i.e., it does not _layer_ on a local file system).
The Motr design was heavily influenced by the Lustre file system, NFSv4 and database technology. It must be noted that traditional file system properties 
(hierarchical directory namespace, strong POSIX consistency guarantees, &c.) are no longer desirable or achievable at mass capacity. Instead, Motr is a more
general storage system that provides an optional file system interface. This allows wider range of deployments, including cloud.

You can find the Motr repo here [Motr](https://github.com/Seagate/cortx-motr).

### [Provisioner](https://github.com/Seagate/cortx-prvsnr)

The process of setting up your IT Infrastructure is called Provisioning. Provisioning involves managing access to data and resources 
to make these available to users and systems. Resource deployment is a two-step process where you provision and then configure the resource.
To set up the CORTX project, you'll need to provision your: 

   - Server 

   - Network 

   - Service 

The main feature of the CORTX-Provisioner is to: 

   - Let you provision your environments such as Dev, QA, Lab, and Production with minimal user intervention. 

   - Allow you to maintain the modularity or granularity of during various stages of Provisioning process to help enhancements, maintainability, and training with minimal 
   collateral impacts. 

Please also read and contribute to our [Wiki](https://github.com/Seagate/cortx-prvsnr/wiki) and [docs folder](https://github.com/Seagate/cortx-prvsnr/tree/pre-cortx-1.0/docs)
where we maintain a lot more documentation about this repository. 

You can find the Provisioner repo here [Provisioner](https://github.com/Seagate/cortx-prvsnr).

### [Experiments](https://github.com/Seagate/cortx-experiments)

The cortx-experiments repo is used for recording all the pocs and experiments executed by architecture team. 

**Guideline for adding a new poc -**

1. Add a new directory in repo. 

1. Inside the new directory, create three directories objective, docs and src. 

1. The objective directory contains a document explaining the objective of poc. 

1. The docs document contains all the documents generated for as a result of poc. 

1. The src directory should contain any code which was written for executing the poc. 

You can find the Experiments repo here [Experiments](https://github.com/Seagate/cortx-experiments).
 
### [HA](https://github.com/Seagate/cortx-ha/)

CORTX-HA or High-Availability always ensures the availability of CORTX and prevents hardware component or software service failures. If any of your hardware components 
or software services are affected, CORTX-HA takes over the failover or failback control flow and stabilizes them across the CORTX cluster. 

You can find the HA repo here [HA](https://github.com/Seagate/cortx-ha).

### [HARE](https://github.com/Seagate/cortx-hare/)

What HARE does: 

1. Configure Motr object store. 

1. Start/stops Motr services. 

1. Notifies Motr of service and device faults. 

Hare implementation uses [Consul](https://www.consul.io/) key-value store and health-checking mechanisms.

You can find the HARE repo here [HARE](https://github.com/Seagate/cortx-hare).

### [Management Portal](https://github.com/Seagate/cortx-management-portal)

CORTX Management Portal provides user interface (UI) to facilitate the easy utility of different CORTX components and features.
It communicates with CORTX manager through middleware application. The CORTX Manager communicates with different CORTX components 
and features and gives proper response to CORTX Management Portal. 

You can find the Management-Portal repo here [Management-Portal](https://github.com/Seagate/cortx-management-portal).

### [Manager](https://github.com/Seagate/cortx-manager)

CORTX Manager provides APIs which communicate with different CORTX components and features. These APIs are consumed by CORTX Management Portal and CLI.

You can find the Manager repo here [Manager](https://github.com/Seagate/cortx-manager).

### [Monitor](https://github.com/Seagate/cortx-monitor)

CORTX Monitor tracks platform health and raises alerts on sensing any unintended state. It can detect hardware faults, removal or replacement by continuously
sensing sub-systems like Storage Enclosure, Node Servers and Components, and Network Interfaces.

You can find the Monitor repo here [Monitor](https://github.com/Seagate/cortx-monitor).

### [POSIX](https://github.com/Seagate/cortx-posix)

CORTX-POSIX is top level code repository, which helps in building various sub-components (like CORTXFS, NSAL, DSAL etc.) to support different file access protocols 
(like SAMBA, NFS etc.) to Seagate CORTX. This code base consists of scripts which will facilitate in fetching the sub-components repos, and build the code. Note that 
currently only NFS protocol is supported. The supported NFS server is user-space implementation (a.k.a. NFS Ganesha). 

**Disclaimer** 
The code for file access protocol (like NFS) for CORTX is distributed across multiple repos. The code is no longer under active development. We welcome anyone and 
everyone who is interested to please attempt to revive the development of this code. Just please be advised that this code is not ready for production usage and is 
only provided as a starting point for anyone interested in an NFS layer to CORTX and who is willing to drive its development. 

You can find the POSIX repo here [POSIX](https://github.com/Seagate/cortx-posix).

### [S3-Server](https://github.com/Seagate/cortx-s3server)

CORTX Simple Storage Service or CORTX-S3 Server is an object storage service with high data availability, durability, scalability, performance, and security. 
You can use CORTX-S3 Server to store any amount of data for varying business needs and implement it across industries of varying sizes. 

You can easily manage data and access controls using CORTX-S3 Server data management features. 

You can find the S3-Server repo here [S3-Server](https://github.com/Seagate/cortx-s3server).

### [Utils](https://github.com/Seagate/cortx-utils)

The cortx-utils repository contains utility tools and code for various CORTX submodules. For example, S3 and NFS submodules use code from the cortx-utils repos. 

You can find the Utils repo here [Utils](https://github.com/Seagate/cortx-utils).


