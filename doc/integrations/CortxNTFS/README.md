# What is CortxNTFS?

CortxNTFS is a Windows NTFS compatible “virtual drive” technology that unifies
traditional File with modern Object storage seamlessly, on premise, in the cloud
or a hybrid of both. CortxNTFS is a traditional kernel-mode File System driver
that is compatible with the low-level system API calls of the World’s \#1
Operating System – Windows. It is provides block-level file i/o, not to local
disk but to objects stored in a CORTX Object storage instance.

When the CortxNTFS software is installed, Windows sees a “virtual” drive backed
by one or more CORTX Object storage instances, in a similar way to the way it
sees a USB thumb drive as a connected storage drive. This drive can be up to 8.0
EBs (8 Million Terabytes) in size and can be dynamically grown as needed by
adding more CORTX instances. The CortNTFS vDrive is NTFS security compatible
which means it automatically supports the vast majority of Windows file-based
applications.

# What is CORTX?

CORTX is a distributed object storage system designed for great efficiency,
massive capacity, and high HDD-utilization. CORTX is 100% Open Source.

# Why is the CotrxNTFS Important?

Object storage has emerged as an alternative to traditional File System storage
for large quantities of unstructured data. Whereas traditional hierarchical File
System structures can get cumbersome as they grow very large, Object storage
brings a “flat” structure with equal access to all objects held, making it
eminently suitable for large volumes (i.e. billions or trillions of objects) of
unstructured data. Further, Object storage objects in general support a richer
set of custom metadata than the files and folders of a traditional File System
can. This potentially makes the object data of Object storage better-suited for
analytic use cases.

However, Object storage lacks the locking mechanism that enables concurrency
that File System-based approaches support. In addition, Object storage tends to
be the worst performing of all storage modes in part because of the heavier
metadata overheads (although implementations such as CORTX Object storage are
changing this!). This tends to make raw Object storage not well-suited to more
time-critical operations such as transactional processes.

But perhaps the biggest problem with Object storage affecting Enterprise mass
adoption is that the vast majority of existing Enterprise applications are File
System centric and cannot work with Object storage without significant
modifications. File systems are a key part of traditional on premise Enterprise
computing systems, and remain the most common way that applications and
operating systems store data. In contrast, Object storage was born in the cloud
and provides an abstraction model for data that supports a broad range of
customizable metadata. However, Object storage is not directly accessible to
conventional applications, creating a barrier to entry into the Enterprise.

CortxNTFS unifies File and Object storage by allowing the Enterprise to have all
of their applications “just work” with CORTX Object storage without the need to
either re-factor them, or use slow gateways that translate between file and
object. In this way CortxNTFS will drive adoption of CORTX within the
Windows-dominated Enterprise environment seamlessly.

# How was the CortxNTFS Built?

The technology used to build CortxNTFS came out of the larger World Computer
project I have been working on for several years (see
[WorldComputer.org](https://worldcomputer.org)) which features a fully
operational NTFS vDrive that is backed by Microsoft’s Azure Blob Storage
service. That NTFS vDrive technology code was refactored into a standalone piece
of software and then modified to support the CORTX S3 layer in addition to Azure
Blob Storage. To complete the effort I added a software installer for CortxNTFS
as well. The tech stack is C++ and C\#.

# A Demonstration of CotrxNTFS

See this [video](https://youtu.be/2bF17eW5CB8) for a comprehensive demonstration
of CortxNTFS in action.

# Installation and Usage of CortxNTFS

See this [guide](CortxNTFS%20Installation%20and%20Usage%20Guide.md) for
instructions on how to install and use CortxNTFS.
