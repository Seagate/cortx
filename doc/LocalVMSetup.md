THIS FILE IS OLD AND SHOULD BE REMOVED AFTER VERIFYING THAT NO LINKS EXIST TO IT ANYWHERE ELSEWHERE


# Create a new VM from scratch
For CORTX development, you can create a new VM from scatch and then install a fresh CentOS 7.7.1908.

## 1. Download ISO file
You can download CentOS 7.7.1908 ISO file from http://www.centos.org, or from any mirror site you like.

## 2. Create a new VM in your VM provider (VMWare Fusion, or Oracle VirtualBox)
* At least two CPUs or cores.
* At least 4GB of memories.
* At least 40GB of disk.
* Two network adapters. One is local only, and another is NAT.

## 3. Install a fresh CentOS 7.7.1908
Automatic install or manual install. Basic installation is OK. When you build CORTX from source in next steps, dependant packages will be resolved and installed.

## 4. You may need to add EPEL repo.
Please refer to: https://fedoraproject.org/wiki/EPEL

## 5. Now you are ready to build and test CORTX. 
Please refer to the [Contribution Guide](../CONTRIBUTING.md) to get started.

