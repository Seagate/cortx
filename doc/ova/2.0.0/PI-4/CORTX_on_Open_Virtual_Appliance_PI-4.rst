
================================
CORTX on Open Virtual Appliance
================================
An Open Virtual Appliance (OVA) is a Virtual Machine (VM) image that consists of a pre-installed and pre-configured operating system as well as one or more applications packaged for easy deployment and testing.  This document describes how to use a CORTX OVA for the purposes of single-node CORTX testing. 
For our Japanese community, this document has been translated and is available `here <https://qiita.com/Taroi_Japanista/items/0ac03f55dce3f7433adf>`_.

***********************
Recommended Hypervisors
***********************
All of the following hypervisors should work: `VMware ESX Server <https://www.vmware.com/products/esxi-and-esx.html>`_,
`VMware vSphere <https://www.vmware.com/products/vsphere.html>`_,
`VMware Workstation <https://www.vmware.com/products/workstation-pro.html>`_

**Important**: If you are running the VM in any of the VMWare hypervisors, it is not recommended to use VMware Tools, as CORTX may break due to kernel dependencies. For the same reason, please do not update the operating system in the image as that also might cause it to fail.

**Prerequisites:**

- To run the CORTX OVA the following minimum configuration is required:

  - RAM: 8GB
  - Processor: 4
  - OS Disk: 1 disk of 20GB
  - Data Disks: 2 disks of 32GB each and 4 partitions of 8GB from each data disks i.e. /dev/sdb1,.../dev/sdb4 and /dev/sdc1,.../dev/sdc4

- Download the `CORTX OVA PI-4 <http://cortx-storage.colo.seagate.com/releases/opensource_builds/ova/cortx-559.ova>`_
- Import the OVA image using the instruction provided in  to `Importing the OVA document <https://github.com/Seagate/cortx/blob/main/doc/Importing_OVA_File.rst>`_.
- Ensure that the Virtualization platform has internet connectivity:
   
  - For VMware related troubleshooting, please refer to `VM Documents <https://docs.vmware.com/en/VMware-vSphere/index.html>`_. 
  - If on the VMware WorkStation, you do not see an IPv4 network configured, then update virtual networking configuration. See `troubleshooting virtual network <https://github.com/Seagate/cortx/blob/main/doc/troubleshoot_virtual_network.rst>`_.

**********
Procedure
**********

#. Open the VM console, and login with the below credentials.

   * Username: cortx 
   * Password: opensource!
  
#. Become the **root** user by running this:
  
   ::
   
       sudo su -
   
#. Run the following command to create a config.ini file:

   ::

       curl -O https://raw.githubusercontent.com/Seagate/cortx/main/doc/ova/2.0.0/PI-4/config.ini   
       vi ~/config.ini

   **Note:** The values used in `config.ini <https://raw.githubusercontent.com/Seagate/cortx/main/doc/ova/2.0.0/PI-4/config.ini>`_ are for example purpose so update the values as per your environment by replacing your network interface names and storage disks with partitions created in step 3.

#. Run **ip a l** and record the IP addresses of the following interfaces:

   * ens32 - Management IP: To access the CORTX GUI.
   * ens33 - Public data IP: To access S3 endpoint and perform IO operations.
   * ens34 - Private data IP: To perform CORTX internal communication.

   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/104networks.png
   
#. Create and run the reconfigure_network.sh script to ensure all the necessary services are operational,

   ::

       curl -O https://raw.githubusercontent.com/Seagate/cortx/mukul-ova-PI4/doc/ova/2.0.0/PI-4/reconfigure_network.sh
       sh reconfigure_network.sh
     
#. Run the following command to start the CORTX cluster and check the status:
   
   ::

       hctl start
       hctl status
  
   **Note:** If the cluster is not running then stop and start cluster once using the following command:
     
   ::

       hctl shutdown
       hctl start
   
#. Use the management IP from the **ip a l** command and configure the CORTX GUI, See `configure the CORTX GUI document <https://github.com/Seagate/cortx/blob/main/doc/Preboarding_and_Onboarding.rst>`_. 

#. The system up and running, use the data IP from the **ip a l** command `to test the system <https://github.com/Seagate/cortx/blob/main/doc/Performing_IO_Operations_Using_S3Client.rst>`_ and observe activity in the GUI. For example, the below picture shows a CORTX dashboard after a user did an *S3 put* followed by an *S3 get*.

   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/dashboard_read_write.png

#. To use the CLI to query and monitor the configuration, health, and activity of your CORTX system, see `Checking Health document. <https://github.com/Seagate/cortx/blob/main/doc/checking_health.rst>`_.

#. BOOM. You're all done and you're AWESOME. 

   Thanks for checking out the CORTX system; we hope you liked it. Hopefully you'll stick around and participate in our community and help make it even better.

   **Note:** The Lyve Pilot (LP) will be available in the future releases.
 
*************
Miscellaneous
*************

If you have a firewall between CORTX and the rest of your infrastructure, including but not limited to S3 clients, web browser, and so on, ensure that the ports mentioned below are open to provide access to CORTX.
  
+----------------------+-------------------+---------------------------------------------+
|    **Port number**   |   **Protocols**   |   **Destination network on CORTX**          |
+----------------------+-------------------+---------------------------------------------+
|         22           |        TCP        |              Management network             |
+----------------------+-------------------+---------------------------------------------+
|         443          |       HTTPS       |             Public Data network             |
+----------------------+-------------------+---------------------------------------------+


***************
Troubleshooting
***************

#. Follow the instructions after you reboot your VM:
   
     ::
   
        systemctl start haproxy
        systemctl start csm_web
        systemctl start csm_agent
        systemctl start s3authserver


Tested by:

- Oct 26, 2021: Mukul Malhotra (mukul.malhotra@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro for OVA [CentOS 7.8]
