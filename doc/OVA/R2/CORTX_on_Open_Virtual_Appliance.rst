
===============================
CORTX on Open Virtual Appliance
===============================
An Open Virtual Appliance (OVA) is a Virtual Machine (VM) image that consists of a pre-installed and pre-configured operating system as well as one or more applications packaged for easy deployment and testing.  This document describes how to use a CORTX OVA for the purposes of single-node CORTX testing. Current version of the OVA requires DHCP server to assign IPs to all 3 network interfaces. 
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
   - Processor: 4 core CPU
   - Storage: 60GB

     Note: The CORTX OVA VM will create 9 disk partitions. 

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

#. Change the hostname by running the following command:

   ::
   
     hostnamectl set-hostname --static --transient --pretty <new-name>

#. Add the host name to the config.ini file:

   #. To open the config.ini file, run:

      ::
       
        vi config.ini

   #. Add the hostname.
   #. Save and close the config.ini file.
   #. Reboot the VM.

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
   
   The output should be similar to the image below

   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/hctl_status_output.png

#. Run **ip a l** and record the IP addresses of the following interfaces:

   * ens32 - Management IP
   * ens33 - Public data IP
   * ens34 - Private data IP (if present)

   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/networks.png

   
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

#. After configuring the CORTX GUI, if any system alerts are displayed. You can ignore these system alerts. 

   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/AlertsError.png

#. As the Consul service is not running, you will encounter the below depicted error.

   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/consul.PNG

   **Workaround:** Run the followind mentioned commands:
   
   ::
   
    sed -i '11s/host:/host: 127.0.0.1/' /etc/csm/database.yaml
    
    systemctl restart csm_agent 


.. raw:: html
   
   </details>


Tested by:

- May 10, 2021: Shiji Zhang (shiji.zhang@tusimple.ai) using OVA release 1.0.4 on KVM 5.1

- Apr 30, 2021: Ashwini Borse (ashwini.borse@seagate.com) using OVA release 1.0.4 on Vsphere.

- Apr 12, 2021: Mukul Malhotra (mukul.malhotra@seagate.com) using OVA release 1.0.3 on MAC running VMWare Fusion 12.1.0.

- April 6, 2021: Harrison Seow (harrison.seow@seagate.com) using OVA release 1.0.3 on Windows 10 running VMware Workstation 16 Player.

- Mar 25, 2021: Mukul Malhotra (mukul.malhotra@seagate.com) using OVA release 1.0.3 on Windows 10 running Oracle VirtualBox & VMware Workstation 6.1.16.