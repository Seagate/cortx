
===============================
CORTX on Open Virtual Appliance
===============================
Open Virtual Appliance (OVA) is a virtual machine (VM) image file that consists of pre-installed and pre-configured operating system environment, and a single application.

This document describes how to use a VM image pre-packaged with CORTX for the purposes of single-node CORTX testing.

***********************
Recommended Hypervisors
***********************
All of the following hypervisors should work:

* `VMware ESX Server <https://www.vmware.com/products/esxi-and-esx.html>`_
* `VMware vSphere <https://www.vmware.com/products/vsphere.html>`_
* `VMware Fusion <https://www.vmware.com/products/fusion.html>`_
* `VMware Workstation <https://www.vmware.com/products/workstation-pro.html>`_  

**********
Procedure
**********
The procedure to install CORTX on OVA is mentioned below.

#. From `our release page <https://github.com/Seagate/cortx/releases/tag/VA>`_, download the cortx-va-1.0.0-rc3.zip file that contains the virtual machine images.

#. Extract the contents of the downloaded file into your system. You can also run the below mentioned command to extract the content.

   * **gzip cortx-va-1.0.0-rc3.zip**

#. Import the OVA file by referring to `Importing OVA <Importing_OVA_File.rst>`_. Set the **Network Translation Address** (NAT) in the hypervisor settings for the imported OVA. 

   * In case of troubleshooting, refer to `VM Documents <https://docs.vmware.com/en/VMware-vSphere/index.html>`_.
  
   **Important**: If you are running the VM in any of the products of VMware, it is not recommended to use VMware Tools, as CORTX may break due to kernel dependencies.
 
#. Open the VM console, and login with the below mentioned credentials.

   * Username: **cortx**
  
   * Password: **opensource!**

#. Become the **root** user by running the following command.

   * sudo su -
 
#. Run **ip a l** and note the IP addresses of the following interfaces:

   * ens192 - management
 
   * ens256 - public data
 
#. Change the hostname by running the following command:

   * **hostnamectl set-hostname --static --transient --pretty <new-name>**
  
     If you receive **Access denied** message, remove immutable settings on the **/etc/hostname** file and run the command again. To remove immutable setting from **/etc/hostname**, run the following command.
     
     * **chattr -i /etc/hostname**
  
 
   To verify the change in hostname, run the following command:
 
   * **hostnamectl status**
   
   **Note**: Both short hostnames and FQDNs are accepted. If you do not have DNS server to register the VM with, you can access it using the IP address. However, the hostname is mandatory and should be configured.

#. Bring up the OVA by running the below mentioned script.

   * **sh /opt/seagate/cortx/provisioner/cli/virtual_appliance/bootstrap.sh**
   
#. Perform the S3 sanity test by running the following.

   * **sh /opt/seagate/cortx/s3/scripts/s3-sanity-test.sh**
 
#. Using the management IP that you queried in step 6, open the web browser and navigate to the following location:

   * **https://<management IP>:28100/#/preboarding/welcome**
  
   **Note**: Operating system updates are not supported due to specific kernel dependencies.

11. Refer to `Onboarding into CORTX <Preboarding_and_Onboarding.rst>`_ to execute the preboarding and onboarding process.

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

Status of Services
==================

Run the below mentioned commands to check the status of different services that are part of CORTX.

::

 systemctl status rabbitmq-server
 
 systemctl status elasticsearch
 
 systemctl status haproxy
 
 systemctl status s3authserver
 
 systemctl status sspl-ll

If any service is inactive, run the below mentioned command.

::

 systemctl start|restart <service_name>

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
   
4. Run the below mentioned command to verify that CORTX I/O subsystem has started.

   - **hctl status**
   
5. Run the below mentioned commands to check if CORTX Management subsystem (CSM) has started.
   
   - **systemctl status csm_agent**
   
   - **systemctl status csm_web**
   
6. If the above services are not active, run the following command.

   - **systemctl start <csm_agent|csm_web>**

   
.. raw:: html
   
   </details>


