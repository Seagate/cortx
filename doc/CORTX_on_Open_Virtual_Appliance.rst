
===============================
CORTX on Open Virtual Appliance
===============================
An Open Virtual Appliance (OVA) is a virtual machine (VM) image that consists of a pre-installed and pre-configured operating system as well as one or more applications packaged for easy deployment and testing.  This document describes how to use a CORTX OVA for the purposes of single-node CORTX testing.

***********************
Recommended Hypervisors
***********************
All of the following hypervisors should work `VMware ESX Server <https://www.vmware.com/products/esxi-and-esx.html>`_,
`VMware vSphere <https://www.vmware.com/products/vsphere.html>`_,
`VMware Fusion <https://www.vmware.com/products/fusion.html>`_, and
`VMware Workstation <https://www.vmware.com/products/workstation-pro.html>`_. 

**Important**: If you are running the VM in any of the VMWare hypervisors, it is not recommended to use VMware Tools, as CORTX may break due to kernel dependencies.  For the same reason, please do not update the operating system in the image as that also might cause it to fail.


**********
Procedure
**********
The procedure to install CORTX on OVA is mentioned below.

#. From `our release page <https://github.com/Seagate/cortx/releases/tag/VA>`_, download the cortx-va-1.0.0-rc3.zip file that contains the virtual machine images.

#. Extract the contents of the downloaded file into your system. You can also run the below mentioned command to extract the content.

   * **gzip cortx-va-1.0.0-rc3.zip**

#. Import the OVA file by referring to `these instructions <Importing_OVA_File.rst>`_. 

   - In case of troubleshooting, refer to `VM Documents <https://docs.vmware.com/en/VMware-vSphere/index.html>`_.
  
   **Important**: If you are running the VM in any of the products of VMware, it is not recommended to use VMware Tools, as CORTX may break due to kernel dependencies. 

   **Note**:  Operating system updates are not supported due to specific kernel dependencies.
 
  
#. Open the VM console, and login with the below mentioned credentials.


   * Username: **cortx**
  
   * Password: **opensource!**

#. Become the **root** user by running the following command.

   * sudo su -
 
#. Run **ip a l** and record the IP addresses of the following interfaces:

   * ens192 - management
 
   * ens256 - public data
   
   .. image:: images/networks.png
   
   * If you do not see IP addresses like in the above image, you might need to change your virtual networking configuration for which  `these instructions <troubleshoot_virtual_network.rst>`_ are hopefully useful.

#. Change the hostname by running the following command:

   * **hostnamectl set-hostname --static --transient --pretty <new-name>**
  
     If you receive **Access denied** message, remove immutable settings on the **/etc/hostname** file and run the command again. To remove immutable setting from **/etc/hostname**, run the following command.
     
     * **chattr -i /etc/hostname**
  
 
     To verify the change in hostname, run the following command:
 
     * **hostnamectl status**
   
   **Note**: Both short hostnames and FQDNs are accepted. If you do not have a DNS server with which to register the VM, you can access it directly using its IP addresses. However, the hostname is mandatory and should be configured.

#. Start the CORTX services by running the provided bootstrap.sh script:

   * **sh /opt/seagate/cortx/provisioner/cli/virtual_appliance/bootstrap.sh**
   

#. Run the below mentioned commands to check the status of different services that are part of CORTX.

   ::

    systemctl status rabbitmq-server
 
    systemctl status elasticsearch
   
    systemctl status haproxy
 
    systemctl status s3authserver
 
    systemctl status sspl-ll
    
    hctl status
    
    systemctl status csm_agent
    
    systemctl status csm_web
 
  If the SSPL service does not start, run the below mentioned commands.

  ::

   /opt/seagate/cortx/sspl/bin/sspl_setup post_install -p LDR_R1
 
   /opt/seagate/cortx/sspl/bin/sspl_setup config -f
 
   systemctl start sspl-ll
    

 If any service is inactive, run the below mentioned command.

 ::

  systemctl start|restart <service_name>
    

#. At this point, CORTX can run on your system.  Confirm this by running the S3 sanity test using the script mentioned below.

   * **sh /opt/seagate/cortx/s3/scripts/s3-sanity-test.sh**
 
#. Mark down the management IP that you queried in step 6, and refer to `these instructions <Preboarding_and_Onboarding.rst>`_ to configure the CORTX GUI. 

#. Now that you have the complete system up and running, mark down the data IP that you queried in step 6 and use `these instructions <testing_ova.rst>`_ to test the system and observe activity in the  GUI.


#. BOOM.  You're all done and you're AWESOME.  Thanks for checking out the CORTX system; we hope you liked it.  Hopefully you'll stick around and participate in our community and help make it even better.
 
*************
Miscellaneous
*************

If you have a firewall between the OVA and the rest of your infrastructure, including but not limited to S3 clients, web browser, and so on, ensure that the  ports mentioned below are open to provide access to OVA.
  
+----------------------+-------------------+---------------------------------------------+
|    **Port number**   |   **Protocols**   |   **Destination network (on VA)**           |
+----------------------+-------------------+---------------------------------------------+
|          22          |        TCP        |           Management network                |
+----------------------+-------------------+---------------------------------------------+ 
|          53          |      TCP/UDP      | Management network and Public Data network  |
+----------------------+-------------------+---------------------------------------------+ 
|         123          |      TCP/UDP      |              Management network             |
+----------------------+-------------------+---------------------------------------------+
|         443          |       HTTPS       |             Public Data network             |
+----------------------+-------------------+---------------------------------------------+
|         9443         |       HTTPS       |              Public Data network            |
+----------------------+-------------------+---------------------------------------------+
|         28100        |   TCP (HTTPS)     |              Management network             |
+----------------------+-------------------+---------------------------------------------+

Restarting CORTX OVA
====================
To restart the CORTX OVA, follow the below mentioned procedures, in the order of listing.

- Shutdown the OVA

- Start the OVA

Shutdown the OVA
----------------

.. raw:: html

    <details>
   <summary><a>Click here to view the procedure.</a></summary>
   
1. Stop all S3 I/O traffic from S3 clients to VA.

2. Login to the CORTX Virtual Appliance as **cortx** and run the following.

   * **sudo su -**

3. Stop CORTX I/O subsystem by running the following command.

   * **hctl shutdown** 

4. After executing the previous command, shutdown the OVA by running the following command.

   * **poweroff**
   
.. raw:: html
   
   </details>
 

Start the OVA
--------------

.. raw:: html

    <details>
   <summary><a>Click here to view the procedure.</a></summary>

1. Power on the Virtual Appliance VM.

2. Login to the CORTX OVA as cortx and run the following.

   - **sudo su -**

3. Start CORTX I/O subsystem by running the following command.

   - **hctl bootstrap -c /var/lib/hare/**
   

   
.. raw:: html
   
   </details>


