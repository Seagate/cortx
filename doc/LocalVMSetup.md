# Setup a CentOS Local VM
This is a guide which lists the steps required to get a local VM and use it for  S3 and/or Motr development.

## 1. Download the VM
Download both .ovf or .vmdk files from the following link into a directory on your computer: https://seagatetechnology-my.sharepoint.com/:f:/g/personal/basavaraj_kirunge_seagate_com/EgkMhqmUbIdFs8Tr9N-bHecBtMDwm02QbG0T8vS7TLOdUg?e=aiRHBr

## 2. Add the VM in VMWare Fusion or Oracle VirtualBox
You need to have either VMWare Fusion or Oracle VirtualBox to use the VM. In case you have VirtualBox installed, select the "Import Appliance" option and select the .ovf file downloaded in the previous step. 
For VMware Fusion: Select File -> New -> Create Custom Virtual Machine -> Linux centos7 64-bit -> Legacy Bios -> Use Existing Virtual Machine -> (browse to file S3dev_CentOS_7-disk1.vmdk & select continue) -> Finish.

Note: if you are a Seagate employee and do not already have access to either VMWare Fusion or Oracle VirtualBox, IT requires you to submit an Exception request since these are not officially supported applications.

## 3. Using the VM
Start the VM, you will be presented with a login prompt after sometime. You need to enter the following credentials to login:
Username: root  
Password: seagate

## 4. Configuring the networking interface
At the command prompt type `# ip a`. You will see some output like this:
<p align="center"><img src="../../assets/images/ip_a_op.png?raw=true"></p>
Note down the interface name (other than the loopback interface "lo"). In this case it is *ens33*. This will be required in Step 6. If the interface is down use the following command to bring it up:<br/>

`# ifup ens33`

## 5. Build and Test CORTX
Now you are to build and test CORTX. 
Please refer to the [CORTX Quickstart Guide](../QUICK_START.md) to get started.

## 6. Check for LNet
Type the following command:
`# lctl list_nids`
you should get the following output:
<p align="center"><img src="../../assets/images/lctl_list_nids_op.png?raw=true"></p>

If you don't get this output, LNet is not up. Perform the following steps:  
`# systemctl start lnet`  
`# lnetctl net add --net tcp0 --if ens33`  

And check again. Now you should be able to see a similar output as shown above.

## 7. Verify system time
Verify the system time is current, if not set it to the current time using the `timedatectl` utility.

## You are now all set to use this VM for either Motr or S3 development.

## Notes:
* Port Forwarding  
  The VM does not have a window manager, so if you find it restrictive to use, enable port forwarding. Here's a link which explains how to do it in Virtual Box: https://www.techrepublic.com/article/how-to-use-port-forwarding-in-virtualbox/ .


---------------------------------------------------------
# (Option two) Create a new VM from scratch
You can also create a new VM from scatch and then install a fresh CentOS 7.7.1908.

## 1. Download ISO file
You can download CentOS 7.7.1908 ISO file from http://www.centos.org, or from any mirror site you like.

## 2. Create a new VM in your VM provider (VMWare Fusion, or Oracle VirtualBox)
* At least two CPUs or cores.
* At least 4GB of memories.
* At least 40GB of disk.
* Two network adapters. One is local only, and another is NAT.

## 3. Install a fresh CentOS 7.7.1908
Automatic install or manual install. Basic installation is OK. When you build Motr from source in next steps, dependant packages will be resolved and installed.

## 4. You may need to add EPEL repo.
Please refer to: https://fedoraproject.org/wiki/EPEL

* RHEL/CentOS 7:
   `# yum install https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm`
   
