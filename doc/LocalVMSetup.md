THIS FILE IS OLD AND SHOULD BE REMOVED AFTER VERIFYING THAT NO LINKS EXIST TO IT ANYWHERE ELSEWHERE


# Create a new VM from scratch
For CORTX development, you can create a new VM from scatch and then install a fresh CentOS 7.8.2003.
This setup should take you roughly 20 to 30 minutes.

## 1. Download ISO file
You can download CentOS 7.8.2003 ISO file from http://www.centos.org, or from any mirror site you like.

## 2. Create a new VM in your VM provider (VMWare Fusion, or Oracle VirtualBox)
* At least two CPUs or cores.
* At least 4GB of memories.
* At least 40GB of disk.
* Two network adapters. One is local only, and another is NAT.

## 3. Install a fresh CentOS 7.8.2003
Automatic install or manual install. Basic installation is OK. When you build CORTX from source in next steps, dependant packages will be resolved and installed.

## 4. You may need to configure your network settings
run a test to check if you are connected to the network 

     sudo nmcli d
   
if you have a network connection then skip on to step 4, otherwise follow the below commands:

     sudo nmtui
 
 -> Edit a connection, select your network interface and choose "Automatically connect" option (by pressing the space key) and then select OK.
 
    sudo reboot now

## 5. You may need to add EPEL repo.
Please refer to: https://fedoraproject.org/wiki/EPEL

## 6. Now you are ready to build and test CORTX. 
Please refer to the [Contribution Guide](../CONTRIBUTING.md) to get started.

