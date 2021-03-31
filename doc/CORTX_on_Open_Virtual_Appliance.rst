
===============================
CORTX on Open Virtual Appliance
===============================
An Open Virtual Appliance (OVA) is a Virtual Machine (VM) image that consists of a pre-installed and pre-configured operating system as well as one or more applications packaged for easy deployment and testing.  This document describes how to use a CORTX OVA for the purposes of single-node CORTX testing.  The minimum recommended system should have at least 4 CPU cores, at least 8 GB of RAM, and at least 120 GB of local storage.  For our Japanese community, this document has been translated and is available `here <https://qiita.com/Taroi_Japanista/items/0ac03f55dce3f7433adf>`_.

***********************
Recommended Hypervisors
***********************
All of the following hypervisors should work: `VMware ESX Server <https://www.vmware.com/products/esxi-and-esx.html>`_,
`VMware vSphere <https://www.vmware.com/products/vsphere.html>`_,
`VMware Fusion <https://www.vmware.com/products/fusion.html>`_, and
`VMware Workstation <https://www.vmware.com/products/workstation-pro.html>`_. 

**Important**: If you are running the VM in any of the VMWare hypervisors, it is not recommended to use VMware Tools, as CORTX may break due to kernel dependencies.  For the same reason, please do not update the operating system in the image as that also might cause it to fail.


*********
Procedure
*********
The procedure to install CORTX on OVA is mentioned below.

#. From `our release page <https://github.com/Seagate/cortx/releases/tag/VA>`_, download and then uncompress the `cortx-va-1.0.2.zip <https://github.com/Seagate/cortx/releases/download/VA/cortx-va-1.0.2.zip>`_ file that contains the virtual machine image.

#. Import the OVA image by referring to `these instructions <Importing_OVA_File.rst>`_. 

   - For VMware related troubleshooting, please refer to `VM Documents <https://docs.vmware.com/en/VMware-vSphere/index.html>`_. 
  
#. Open the VM console, and login with the below credentials.

   * Username: cortx 
   * Password: opensource!

#. Become the **root** user by running this:
   ::
     sudo su -
   
#. Change the hostname by running the following command:

   * **hostnamectl set-hostname --static --transient --pretty <new-name>**
  
     If you receive **Access denied** message, remove immutable settings on the **/etc/hostname** file and run the command again. To remove immutable setting from **/etc/hostname**, run the following command.
     
     * **chattr -i /etc/hostname**
  
 
     To verify the change in hostname, run the following command:
 
     * **hostnamectl status**
   
   **Note**: Both short hostnames and FQDNs are accepted. If you do not have a DNS server with which to register the VM, you can access it directly using its IP addresses. However, the hostname is mandatory and should be configured.

#. Start the CORTX services by running this bootstrap.sh script:
   ::
     sh /opt/seagate/cortx/provisioner/cli/virtual_appliance/bootstrap.sh
     
   Run the bootstrap script to ensure all the necessary services are operational.
   
#. **Before you begin:**
   
   - Ensure that you have configured your ipv4 network.
      - If you do not see an ipv4 network configured, you might need to change your virtual networking configuration using  `these instructions <troubleshoot_virtual_network.rst>`_.
   - From the Virtual Network Editor dialog, ensure you uncheck Automatic Settings and select the correct VMNet connection and NIC.
      - Once you select an NIC, ensure that you do not ave conflicting NICs selected. 
   - If you using WMware workstation 15.5.0, you could refer to the network setup `here <https://github.com/Seagate/cortx/doc/vmware_workstation_15_5_0_setup.rst>`_.
   
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
 
   The below images shows the output of a successful *systemctl* command; notice how the service is *active*.
   
   .. image:: images/systemctl_output.png
   
   If the SSPL service is inactive, run the below commands.

   ::

    /opt/seagate/cortx/sspl/bin/sspl_setup post_install -p SINGLE
    
    /opt/seagate/cortx/sspl/bin/sspl_setup config -f 
    
    systemctl start sspl-ll    

   If any other service is inactive, run the below mentioned command.

   ::

    systemctl start|restart <service_name>
    
#. By default, port 80 may be closed. Run the below mentioned command to open port 80.

   ::
               
    salt '*' cmd.run "firewall-cmd --zone=public-data-zone --add-port=80/tcp --permanent"
    
    salt '*' cmd.run "firewall-cmd --reload"
      
Run **ip a l** and record the IP addresses of the following interfaces:

   * ens192 - management 
   * ens256 - public data
   
   .. image:: images/networks.png
   
   
#. At this point, CORTX should be running on your system.  Confirm this by running the S3 sanity test using the script mentioned below.

   ::
   
    sh /opt/seagate/cortx/s3/scripts/s3-sanity-test.sh

    * The script performs several operations on S3 API and LDAP backend:
      create account
      create user
      create bucket
      put object
      delete all the above in reverse order
      
   If s3client(s) is / are deployed in separate VMs, then the below entry must be updated in s3client **/etc/hosts** file as follows:
    
   - <<Data IP>> s3.seagate.com sts.seagate.com iam.seagate.com  sts.cloud.seagate.com

#. Using the management IP from the **ip a l** command,  refer to these instructions to `configure the CORTX GUI <Preboarding_and_Onboarding.rst>`_. 

#. Now that you have the complete system up and running, using the data IP from the **ip a l** command, use these instructions `to test the system <testing_io.rst>`_  and observe activity in the GUI.  For example, the below picture shows a CORTX dashboard after a user did an *S3 put* followed by an *S3 get*.

   .. image:: images/dashboard_read_write.png

#. Please use these instructions which describe how to use the `command line interface to query and monitor <checking_health.rst>`_ the configuration, health, and activity of your CORTX system.

#. BOOM.  You're all done and you're AWESOME.  Thanks for checking out the CORTX system; we hope you liked it.  Hopefully you'll stick around and participate in our community and help make it even better.
 
*************
Miscellaneous
*************

If you have a firewall between CORTX and the rest of your infrastructure, including but not limited to S3 clients, web browser, and so on, ensure that the ports mentioned below are open to provide access to CORTX.
  
+-----------------+---------------+--------------------------------------------+
| **Port number** | **Protocols** | **Destination network on CORTX**           |
+-----------------+---------------+--------------------------------------------+
| 22              | TCP           | Management network                         |
+-----------------+---------------+--------------------------------------------+
| 53              | TCP/UDP       | Management network and Public Data network |
+-----------------+---------------+--------------------------------------------+
| 123             | TCP/UDP       | Management network                         |
+-----------------+---------------+--------------------------------------------+
| 443             | HTTPS         | Public Data network                        |
+-----------------+---------------+--------------------------------------------+
| 9443            | HTTPS         | Public Data network                        |
+-----------------+---------------+--------------------------------------------+
| 28100           | TCP (HTTPS)   | Management network                         |
+-----------------+---------------+--------------------------------------------+

If your disk does not have space, run the following command to clean up the logs from the **/var/log** file.

::

 rm /var/log/<file to be deleted>
 
This step is applicable only if the earlier s3 sanity fails. Run the below mentioned script to delete the account and objects that were created as part of the earlier sanity.

::

 sh /opt/seagate/cortx/s3/scripts/s3-sanity-test.sh -c

Restarting CORTX OVA
====================
To restart the CORTX OVA, follow the below mentioned procedures, in the order of listing.

- Shutdown CORTX

- Restart CORTX

Shutdown CORTX
--------------

.. raw:: html

    <details>
   <summary><a>Click here to view the procedure.</a></summary>
   
#. Stop all S3 I/O traffic from S3 clients to VA.

#. Login to the CORTX Virtual Appliance as **cortx** and run the following.

   * **sudo su -**

#. Stop CORTX I/O subsystem by running the following command.

   * **hctl shutdown** 

#. After executing the previous command, shutdown the OVA by running the following command.

   * **poweroff**
   
.. raw:: html
   
   </details>
 

Restart CORTX
-------------

.. raw:: html

    <details>
   <summary><a>Click here to view the procedure.</a></summary>

#. Power on the Virtual Appliance VM.

#. Login to the CORTX OVA as cortx and run the following.

   - **sudo su -**
   
#. Restart openldap and s3 auth server services by the below mentioned commands.

   ::
   
    $ systemctl restart slapd
    
    $ systemctl restart s3authserver

#. Start CORTX I/O subsystem by running the following command.

   - **hctl start**
   

   
.. raw:: html
   
   </details>
   
Tested by:

- Dec 10, 2020: Suprit Shinde (suprit.shinde@seagate.com) using OVA release 1.0.2 on a Windows laptop running VMWare Workstation.

- Nov 3, 2020: Justin Woo (justin.woo@seagate.com) using OVA release 1.0.2 on a Windows laptop running VMWare Workstation.

- Oct 26, 2020: Gregory Touretsky (gregory.touretsky@seagate.com) using OVA release 1.0.2 on a Windows laptop running VMWare Workstation.

- Oct 11, 2020: Saumya Sunder (saumya.sunder@seagate.com) using OVA release 1.0.2 on a Windows laptop running VMWare Workstation.

- Oct 5, 2020: Andriy Tkachuk (andriy.tkachuk@seagate.com) using OVA release 1.0.2 by running VMWare Fusion 11.

- Sep 18, 2020: Sarang Sawant (sarang.sawant@seagate.com) using OVA release 1.0.2 on a Windows laptop running VMWare Workstation.

- Sep 19, 2020: Divya Kachchwaha Kachchwaha (divya.kachhwaha@seagate.com) using OVA release 1.0.1 on a Windows laptop running VMWare Workstation.

- Sep 19, 2020: Venkataraman Padmanabhan (venkataraman.padmanabhan@seagate.com) using OVA release 1.0.0 and 1.0.1 on a Windows laptop running VMWare Workstation.

- Sep 12, 2020: Mukul Malhotra (mukul.malhotra@seagate.com) using OVA release 1.0.0 and 1.0.1 on a Windows laptop running VMWare Workstation.

- Sep 12, 2020: Puja Mudaliar (puja.mudaliar@seagate.com) using OVA release 1.0.0 on a Windows laptop running VMWare Workstation.

- Sep 12, 2020: Gaurav Chaudhari (gaurav.chaudhari@seagate.com) using OVA release 1.0.0 on a Windows laptop running VMWare Workstation.


