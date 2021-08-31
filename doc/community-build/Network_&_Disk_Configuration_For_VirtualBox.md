## VirtualBox Network Configuration
To set up CORTX from the source for a single node, you need to configure the network interface cards. For VirtualBox users, it's advisable to configure the network while installing the ISO image.

1. Select Network

    <img src="../images/network_disks/network.png">
    <br><br>
2. Enable Adapters 1, 2 and 3. Change Attached to from NAT to Bridged Adapter. On the promiscuous mode,select Allow All; this allows incoming traffic to pass the physical network adapter of the local machine and reach the virtual network adapter of the VM.

    <img src="../images/network_disks/enable.png">
    <br><br>

- The network configurations should be as follows:-

    <img src="../images/network_disks/adapters.png">
    <br><br>

3. While installing the CentOS ISO image, connect the NIC cards:-

    <img src="../images/network_disks/connect.png">
    <br><br>

4. Click on configure:

    <img src="../images/network_disks/ethernet.png">
    <br><br>

5. We want to automatically connect to the network when available.

    <img src="../images/network_disks/ethernet_1.png">
    <br><br>
- This will connect the NIC cards to DHCP IP address, and the DNS configured on your local machine.

    <img src="../images/network_disks/ethernet_2.png">
    <br><br>
6. Start your VM, check the network settings using **ip a l**;-

    <img src="../images/network_disks/config.png">
    <br><br>
**Note** For this setup, you should consider your **enp0s3**, **enp0s8** and **enp0s9** as your **management IP**, **public data IP** and **private data IP**, respectively.
## Creating Disks
Setting up CORTX for single node requires:

- 1 OS disk
- 8 Data disks
- 1 Meta disk

The OS disk is created while installing the CentOS ISO image. To create and add the data disks follow the guidelines below:-

1. Click on Storage:-

    <img src="../images/network_disks/storage.png">
    <br><br>
2. On the storage page, select controller: SATA, click on the + sign icon at the bottom and select hard disk.

    <img src="../images/network_disks/sata.png">
    <br><br>
3. Click on create disk image, then follow the steps below

    <img src="../images/network_disks/create_disk.png">

    <img src="../images/network_disks/disk_type.png">

    <img src="../images/network_disks/allocated.png">
    <br><br>
- Select the size of the disk:
    <img src="../images/network_disks/size.png">
    <br><br>
4. Add the disks created to the VM:-
    <img src="../images/network_disks/attach.png">
    <br><br>
5. Create and add all the Data Disks using the steps above. Adding the Metadata Disk follows the same steps; only the size will change to 8GB. All the disks should be as follows:

    <img src="../images/network_disks/disks.png">
