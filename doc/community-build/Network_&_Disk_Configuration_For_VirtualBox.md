## VirtualBox Network Configuration
To set up CORTX from the source for a single node, you need to configure the network interface cards. For VirtualBox users, it's advisable to configure the network while installing the ISO image.

## Creating Disk
Setting up CORTX for single node requires:

- 1 OS disk
- 8 Data disks
- 1 Meta disk

The OS disk is created while installing the CentOS ISO image. To create and add the data disks follow the guidelines below:-

1. Click on Storage:-

    .. image:: images/network_disks/storage.png

2. On the storage page, select controller: SATA, click on the + sign icon at the bottom and select hard disk.

    .. image:: images/network_disks/sata.png
    
3. Click on create disk image, then follow the steps below

    .. image:: images/network_disks/create_disk.png

    .. image:: images/network_disks/disk_type.png

    .. image:: images/network_disks/allocated.png

Select the size of the disk:

    .. image:: images/network_disks/size.png


4. Add the disks created to the VM:-

    .. image:: images/network_disks/attach.png

5. Create and add all the Data Disks using the steps above. Adding the Metadata Disk follows the same steps; only the size will change to 8GB. All the disks should be as follows:

    .. image:: images/network_disks/disks.png














