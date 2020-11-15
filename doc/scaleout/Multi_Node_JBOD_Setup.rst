*********************************
3 Node JBOD Setup (Prerequisites)
*********************************

Perform the below mentioned procedure to complete the process of 3 node JBOD Setup.

1. Prepare three servers and three JBODs as per the following guidelines.

   - **Server Reference Configuration**

     - Minimal Configuration

       - 1x Intel Xeon CPU, 6 cores per CPU (2x Intel Xeon CPU, 10 cores per CPU for optimal performance)

       - 64 GB RAM ( 192 GB RAM for optimal performance)

       - 2x 1 TB internal HDD

       - One dual-port or single-port high-speed NICs (for the data networks). We recommend using Mellanox HCAs.

       - At least one 1 GbE network port (for the Management network)

       - SCSI HBA with external ports (to connect to JBOD)

**Notes (Server Reference Configuration)**

- Enabled EPEL and SCL repositories

::

  yum -y install epel-release centos-release-scl

- The minimum number of network ports per server is 3.

- Usage of Mellanox HCAs is recommended but not mandatory. For optimal performance you need two high-speed network ports (10 GbE minimum; 50 GbE or 100 GbE recommended). All the three servers must have Mellanox HCA or none of the servers must have it.
    
- Infiniband and OmniPath adapters are not supported.
  
  
  
  - **JBOD Reference Configuration**

    - The minimum number of disks per JBOD is 7. One JBOD must be connected to one server. The minimum size of the JBOD disk is 2TB.

  - **Network Configuration Requirements**

    - The CORTX software requires 3 separate networks. The networks could be physically separate (connected to different switches) or separate VLANs. We recommend you to physically separate the management and data networks.

      +--------------------------+---------------------------------------------+
      | **Network name/purpose** | **Corresponding NIC**                       |
      +--------------------------+---------------------------------------------+
      | Management network       | connected to the 1 GbE NIC                  |
      +--------------------------+---------------------------------------------+
      | Public Data network      | connected to the one of the high-speed NICs |
      +--------------------------+---------------------------------------------+
      | Private Data network     | connected to another high-speed NIC         |
      +--------------------------+---------------------------------------------+
	  
      **Note** 
	  
	The Management and Public data networks are required the L3 connectivity from outside. If both of them connect to different VLAN, the IPROUTE2 is required to enabled multiple gateway for the JBODs.
	  
	  Example:
	   
	   Management network : 
		- IP: 10.0.1.10/24 
		- Gateway: 10.0.1.1
		- Device: eth0
		- /etc/sysconfig/network-scripts/ifcfg-eth0
		
		::
		
			TYPE=Ethernet
			BOOTPROTO=none
			NAME=eth0
			DEVICE=eth0
			ONBOOT=yes
			MTU=9000
			IPADDR=10.0.1.10
			PREFIX=24
			GATEWAY=10.0.1.1
			ZONE=public
		
	   Public data network: 
		- IP: 10.0.2.10/24
		- Gateway: 10.0.2.1
		- Device: eth1
		- /etc/sysconfig/network-scripts/ifcfg-eth1
		
		::
		
			TYPE=Ethernet
			BOOTPROTO=none
			NAME=eth1
			DEVICE=eth1
			ONBOOT=yes
			MTU=9000
			IPADDR=10.0.2.10
			PREFIX=24
			ZONE=public-data-zone
		
		- Please note **NO GATEWAY** directive configured for eth1
		- The iproute2 configuration:
			- Append "2		eth1" into /etc/iproute2/rt_tables as below example
			
			::
			
				#
				# reserved values
				#
				255     local
				254     main
				253     default
				0       unspec
				#
				# local
				#
				#1      inr.ruhep
				2       eth1

			- Create file /etc/sysconfig/network-scripts/route-eth1 as content below
			
			::
			
				10.0.2.0/24 dev eth1 src 10.0.2.10 table eth1
				default via 10.0.2.1 dev eth1 table eth1
			
			- Create file /etc/sysconfig/network-scripts/rule-eth1 as content below
			
			::
			
				from 10.0.2.0/24 table eth1
				to 10.0.2.0/24 table eth1
  
			- Restart eth1 interface and verify the configuration
			
			::
			
				# ifdown eth1; ifup eth1
				# ip rule
				0:      from all lookup local
				32762:  from all to 10.0.2.0/24 lookup eth1
				32763:  from 10.0.2.0/24 lookup eth1
				32764:  from all to 10.0.2.0/24 lookup eth1
				32765:  from 10.0.2.0/24 lookup eth1
				32766:  from all lookup main
				32767:  from all lookup default

	  After complete this configuration, the both Management and Public data IP Address shoud be accessible from external.	  

2. Connect the servers to the networks and the JBODs as per the guidelines provided above.

3. Install CentOS 7.7 (1908 release) operating system on all three servers in the cluster.

**Note**: The release must match exactly, as the other versions and distributions of Linux are not supported. You can verify the release by running the following commands and view the appropriate outputs.
  
- **lsb_release -r**

  - Appropriate Output: 7.7.1908

- **uname -r**

  - Appropriate Output: 3.10.0-1062.el7.x86_64
  
  **Warning**: Do not update CentOS 7.7 release as it will break CORTX. Operating system updates are not supported at the moment.

  While there are no specific requirements for installing the CentOS 7.7, we recommend you to perform the following 4 steps.

Step 1 -  Use at least two identical internal HDDs in each server (see Server Reference Configuration above).
  
Step 2 -  On each drive, configure the partitions as per the following guidelines.

  +-----------------------+-------------+-------------------------------------------+
  | **Partition number**  |  **Size**   |        **Notes**                          |
  |                       |             |                                           |
  +-----------------------+-------------+-------------------------------------------+
  |     1                 | 256 MB      | to be mounted to /boot/efi or /boot/efi2  |
  +-----------------------+-------------+-------------------------------------------+
  |     2                 |  1 GB       | to be used as part of md0 RAID-1 volume   |
  +-----------------------+-------------+-------------------------------------------+
  |     3                 | rest of     | to be used as part of md1 RAID-1 volume   |
  |                       | disk        |                                           |
  +-----------------------+-------------+-------------------------------------------+

**Note**: The partitioning schema is assuming the servers support UEFI for booting. If the servers do not support UEFI, partition #1 is not required. CentOD Linux implementation of UEFI does not support RAID configuration at the moment, therefore two separate EFI partitions will be needed to be able to boot the server in case of one of the disk fails. These partions should be mounted to /boot/efi (the partition on disk #1) and /boot/efi2 (the partition on disk #2).

Step 3 - Create two RAID-1 volumes.

+------------------+------------------------------------------+
| **Volume name**  |   **Purpose / mount point**              |
|                  |                                          |
+------------------+------------------------------------------+
|  md0             |  /boot                                   |
+------------------+------------------------------------------+
|  md1             |  To be used as physical volume for LVM   |
+------------------+------------------------------------------+

Step 4 - Create LVM configuration for the remaining OS partitions using md1 RAID-1 volume. We recommend you the following LVM disk group and volumes structure.

+--------------------------------+-----------------+----------+--------------+
|    **LVM device name**         | **Mount point** | **Size** | **FS type**  |
|                                |                 |          |              |
+--------------------------------+-----------------+----------+--------------+
| /dev/mapper/vg_sysvol-lv_root  | /               | 200GB    | ext4         |
+--------------------------------+-----------------+----------+--------------+
| /dev/mapper/vg_sysvol-lv_tmp   | /tmp            | 200GB    | ext4         |
+--------------------------------+-----------------+----------+--------------+
| /dev/mapper/vg_sysvol-lv_var   | /var            | 200GB    | ext4         |
+--------------------------------+-----------------+----------+--------------+
| /dev/mapper/vg_sysvol-lv_log   | /var/log        | 200GB    | ext4         |
+--------------------------------+-----------------+----------+--------------+
| /dev/mapper/vg_sysvol-lv_audit | /var/log/audit  | 128MB    | ext4         |
+--------------------------------+-----------------+----------+--------------+
| /dev/mapper/vg_swap            | none            | 100GB    | linux-swap(*)|
+--------------------------------+-----------------+----------+--------------+

**Note**: The information in the table above is provided for reference purposes. You can choose a different structure and/or use different sizes for the partitions (LVM volumes). The minimal size of the / (root) partition should be 20 GB to allow installation of the operating system and the CORTX software. Please adjust the size or / (root) partition accordingly if you do not create separate /var and /var/log partitions.
    
4. Allow the root login over SSH on all three servers. This is required for the installation and operations of the cluster.

**Notes**

- This setting cannot be changed after the installation is complete.

- You can create another non-root user to avoid logging in to the servers as root all the time. Please allow this user to run all commands using sudo (add it to the "wheel" group).
    
5. If you have Mellanox HCAs on your servers, please proceed to the next step. If not, proceed to step 8.

6. Install Mellanox OFED from http://linux.mellanox.com/public/repo/mlnx_ofed/4.7-3.2.9.0/rhel7.7/x86_64/MLNX_LIBS/. 

   **Note**: You must reboot the system after completing the installation of Mellanox OFED.

   - Supported Version - 4.7-3.2.9.0

     - Other versions are not supported.

7. Download CORTX and dependency ISO files from `here <https://github.com/Seagate/cortx/releases/tag/JBOD>`_.

8. Upload the ISOs to the first server in the cluster that you are planning to install. It is recommended to have the ISOs in the same location.

9. On all three servers, setup Python 3.6 virtual environment. Refer https://docs.python.org/3.6/library/venv.html.

   - Supported Version - 3.6
   
     - Other versions are not supported.
    
   **Note**: You can install Python 3.6 without the use of the virtual environments. This is a supported configuration.
    
10. Configure DNS and DHCP server, if used, with the host names and IP addresses for each server.

    - Each server should have FQDN assigned to it. The FQDN should be associated with the IP address of the management network interface.

    - Configure IP addresses on Management and Public Data network interfaces on each server using one of the following methods:

    - static IP addresses for each of the network interfaces

    - dynamic IP addresses for each of the network interfaces

    **Important Notes**

    - CORTX does not support IPv6. Only IPv4 is supported.

    - If you are using dynamic IP addresses, please map the MAC addresses of the respective interfaces to the IP address in the configuration of your DHCP server. This is required to avoid possible IP changes when the leases associated with DHCP expire.

    - If DHCP server is used, ensure that DHCP server passes host names to the servers.

    - Do not configure DHCP to assign the IP address to the private data interfaces. This interface is configured by the CORTX software installer. By default, the configuration uses **192.168.0.0/24** subnet. This setting can be changed by providing necessary information in the config.ini file. For more information, move to step 12.

    You also need two static IPs to be used as Virtual IPs (VIPs). One VIP will be used as Management VIP and another VIP will be used as Cluster (Data) VIP.

    - The Management VIP should be from the same subnet as the rest of the Management network IPs.

    - The Cluster (Data) VIP should be from the same subnet as the rest of the Public Data network IPs.

    **Notes**
 
    - VIPs utilize CLUSTERIP ip tables module that relies on multicast. For CORTX to function appropriately, multicasts should be allowed for Management and Public Data networks.

    - These static IPs are required regardless of whether DHCP is used to provide IP addresses for each server interface or not.

    - You must configure DNS resolution for these VIPs.
   
  
11. Collect all the required information and prepare **config.ini** file for your installation. Refer to `Config.ini File <Configuration_File.rst>`_ for complete information. After the file is prepared, upload it to the first server in the cluster you are planning to install.

**Important**: By default, port 80 is closed. Run the below mentioned commands to open port 80.

::
               
 salt '*' cmd.run "firewall-cmd --zone=public-data-zone --add-port=80/tcp --permanent"
 
 salt '*' cmd.run "firewall-cmd --reload"
 
 
If you have a firewall within your infrastructure, including but not limited to S3 clients, web browser, and so on, ensure that the  ports mentioned below are open to provide access.
  
+----------------------+-------------------+------------------------------------------------+
|    **Port number**   |   **Protocols**   |   **Destination network (on VA)**              |
+----------------------+-------------------+------------------------------------------------+
|          22          |        TCP        |           Management network                   |
+----------------------+-------------------+------------------------------------------------+ 
|          53          |      TCP/UDP      | Management network and Public Data network     |
+----------------------+-------------------+------------------------------------------------+ 
|         123          |      TCP/UDP      |              Management network                |
+----------------------+-------------------+------------------------------------------------+
|         443          |       HTTPS       |             Public Data network                |
+----------------------+-------------------+------------------------------------------------+
|         9443         |       HTTPS       |              Public Data network               |
+----------------------+-------------------+------------------------------------------------+
|         28100        |   TCP (HTTPS)     |              Management network                |
+----------------------+-------------------+------------------------------------------------+
|          67          |     TCP/UDP       | Management network and Public Data network     |
|                      |                   | (only if using DHCP to obtain the IP addresses)|
+----------------------+-------------------+------------------------------------------------+
|          68          |     TCP/UDP       | Management network and Public Data network     |
|                      |                   | (only if using DHCP to obtain the IP addresses)|
+----------------------+-------------------+------------------------------------------------+
