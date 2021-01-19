### BUILD AND TEST ENVIRONMENT FOR CORTX

Building and testing CORTX can be done using a single system.  Obviously CORTX is designed to be a scale-out distributed object storage system but these instructions currently only cover setting up a single node environment.

Currently CORTX requires the CentOS 7.8.2003 distribution running the 3.10.0-1127 kernel. Building it and testing it can be done on either a physical or virtual machine matching these requirements.  We have provided instructions to [install dependencies](InstallingDependencies.md).

For your convenience, we offer a [pre-built VM image (OVA)](https://github.com/Seagate/cortx/releases/tag/VA) which can be used for development and testing. 

A CORTX development environment requires RoCE—RDMA over Converged Ethernet and TCP connectivity.
   - Note: The CORTX stack currently does not work on Intel's OmniPath cards.
   - Note: Seagate employees may have access to resources to help with this.  Please refer to [this page](DEV_VM.md) for more information.
   
For information on release build generation, refer to [Release_Build_Generation](Release_Build_Creation.rst)
