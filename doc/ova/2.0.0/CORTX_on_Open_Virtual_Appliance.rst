
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
`VMware Fusion <https://www.vmware.com/products/fusion.html>`_,
`VMware Workstation <https://www.vmware.com/products/workstation-pro.html>`_, and
`Oracle VM VirtualBox <https://www.oracle.com/virtualization/>`_. 

**Important**: If you are running the VM in any of the VMWare hypervisors, it is not recommended to use VMware Tools, as CORTX may break due to kernel dependencies. For the same reason, please do not update the operating system in the image as that also might cause it to fail.


**Prerequisites:**

- To run the CORTX OVA the following minimum configuration is required:

   - RAM: 8GB
   - Number of core per processor: 4
   - Storage: 80GB

     Note: The CORTX OVA VM will create 10 disks including OS disk.

- Download the `CORTX OVA <https://github.com/Seagate/cortx/releases/>`_ file from `our release page <https://github.com/Seagate/cortx/releases/latest>`_. 
- Import the OVA image using the instruction provided in  to `Importing the OVA document <https://github.com/Seagate/cortx/blob/main/doc/Importing_OVA_File.rst>`_.
- Ensure that the Virtualization platform has internet connectivity:
   
   - For VMware related troubleshooting, please refer to `VM Documents <https://docs.vmware.com/en/VMware-vSphere/index.html>`_. 
   - If on the VMware WorkStation, you do not see an IPv4 network configured, then update virtual networking configuration. See `troubleshooting virtual network <https://github.com/Seagate/cortx/blob/main/doc/troubleshoot_virtual_network.rst>`_.
   - For Oracle Virtual Box network configuration, see `network configuration for Oracle VirtualBox <https://github.com/Seagate/cortx/blob/main/doc/Oracle_Virtual_Box_Network_Configuration.md>`_.


**********
Procedure
**********

#. Open the VM console, and login with the below credentials.

   * Username: cortx 
   * Password: opensource!
  
#. Become the **root** user by running this:
   
   ::
   
     sudo su -

#. Start the CORTX services by running this bootstrap.sh script:
   
   ::
   
      sh /opt/seagate/cortx/provisioner/cli/virtual_appliance/bootstrap.sh
     
   Run the bootstrap script to ensure all the necessary services are operational.
      
#. (Optional) To configure the static IPs instead of DHCP:

   - For Management Network static IP, run the following command:

      ::

         # Set Management Network
         provisioner pillar_set "cluster/srvnode-1/network/mgmt/public_ip" \"<IP address for management network>\"
         provisioner pillar_set "cluster/srvnode-1/network/mgmt/netmask" \"<Netmask for management network>\"
         provisioner pillar_set "cluster/srvnode-1/network/mgmt/gateway" \"<IP address for management network gateway>\"
         salt-call state.apply components.system.network.mgmt.public

   - For Data Network static IP, run the following command:

      ::
      
         # Set Data Network
         provisioner pillar_set "cluster/srvnode-1/network/data/public_ip" \"<IP address for public network>\"
         provisioner pillar_set "cluster/srvnode-1/network/data/netmask" \"<Netmask for public data network>\"
         salt-call state.apply components.system.network.data.public

   **Note:** To verify the static IPs are configured, run the following command:

      ::

         cat /etc/sysconfig/network-scripts/ifcfg-ens32 |grep -Ei "ip|netmask|gateway"
         cat /etc/sysconfig/network-scripts/ifcfg-ens33 |grep -Ei "ip|netmask|gateway"

#. To check the CORTX cluster status, run the following command:
   
   ::
   
      pcs status
   
   **Note:** If the cluster is not running then stop and start cluster once using the following command:
      
      ::

         cortx cluster stop
         cortx cluster start

#. Run **ip a l** and record the IP addresses of the following interfaces:

   * ens32 - Management IP: To access the CORTX GUI.
   * ens33 - Public data IP: To access S3 endpoint and perform IO operations.
   * ens34 - Private data IP: To perform CORTX internal communication.

   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/104networks.png

   
#. Use the management IP from the **ip a l** command and configure the CORTX GUI, See `configure the CORTX GUI document <https://github.com/Seagate/cortx/blob/main/doc/Preboarding_and_Onboarding.rst>`_. 

#. Run the following command and verify the S3 authserver and HA proxy are active and running:

   ::

      systemctl status s3authserver
      systemctl status haproxy
   
   - If any service is in failed state, run the following command active the services:

      ::

         systemctl start <service name>

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
|          22          |        TCP        |           Management network                |
+----------------------+-------------------+---------------------------------------------+
|         443          |       HTTPS       |             Public Data network             |
+----------------------+-------------------+---------------------------------------------+
|         28100        |   TCP (HTTPS)     |              Management network             |
+----------------------+-------------------+---------------------------------------------+

   
Known Issues
--------------

.. raw:: html

    <details>
   <summary><a>Click here to view the known issues.</a></summary>

#. On the CORTX GUI, the S3 audit logs are not displayed.

#. After configuring the CORTX GUI, if any system alerts are displayed. You can ignore these system alerts. 

   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/AlertsError.png

#. On the CORTX GUI, the About page displays the error pop-up. This is an known issue to CORTX:

   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/CSM_GUI.png

#. As the Consul service is not running, you will encounter the below depicted error.

   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/consul.PNG

   **Workaround:** Run the followind mentioned commands:
   
   ::
   
    sed -i '11s/host:/host: 127.0.0.1/' /etc/csm/database.yaml
    
    systemctl restart csm_agent 


.. raw:: html
   
   </details>


Tested by:

- June 21, 2021: Ashwini Borse (ashwini.borse@seagate.com) using OVA release 2.0.0 on Vsphere.

- June 21, 2021: Mukul Malhotra (mukul.malhotra@seagate.com) using OVA release 2.0.0 on VMWare WorkStation Pro 16.
