
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
`VMware Workstation <https://www.vmware.com/products/workstation-pro.html>`_,
`VMware Fusion <https://www.vmware.com/in/products/fusion/fusion-evaluation.html>`_

**Important**: If you are running the VM in any of the VMWare hypervisors, it is not recommended to use VMware Tools, as CORTX may break due to kernel dependencies. For the same reason, please do not update the operating system in the image as that also might cause it to fail.

**Specifications:**

- CORTX OVA image is created with following minimum configuration:

  - RAM: 10GB
  - Processor: 8
  - NIC: 1
  - OS Disk: 1 disk of 20GB
  - Data Disks: 3 disks of 5GB each

**Prerequisites:**

- Download the `CORTX OVA <https://cortxova.s3.us-west-2.amazonaws.com/ova-2.0.0-790.ova>`_ from `our release page <https://github.com/Seagate/cortx/releases/latest>`_.
- To perform the S3 IO operations,refer to these instructions in `S3 IO Operations <https://github.com/Seagate/cortx/blob/main/doc/ova/2.0.0/PI-7/S3_IO_Operations.md>`_.
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
       
   **Note** Wait for 5 - 10 mins till the cluster services are started.
   
#. Run the following command to check the health of CORTX using hctl by running this command:

   ::

       hctl status

 
#. To perform the S3 IO operations, refer the instructions in `S3 IO operations <https://github.com/Seagate/cortx/blob/main/doc/ova/2.0.0/PI-7/S3_IO_Operations.md>`_.

#. BOOM. You're all done and you're AWESOME. 

   Thanks for checking out the CORTX system; we hope you liked it. Hopefully you'll stick around and participate in our community and help make it even better.

**********************************
Validate CSM endpoint via REST API
**********************************

#. Validate the CSM endpoint using below command:
   
   ::

      export IPADDRESS=`ifconfig ens32 | grep inet -w | awk '{print $2}'` && echo $IPADDRESS
      curl --request POST "https://$IPADDRESS:31169/api/v2/login?debug" --header 'Content-Tycation/json' -d '{"username":"cortxadmin","password":"Cortxadmin@123"}' -k

   Note: If encounter error "-bash: ifconfig: command not found", install ifconfig first with the following.
   
   ::

      yum install net-tools

 

Tested by:

- June 15, 2022: Rose Wambui (rose.wambui@seagate.com) on a Windows laptop with VMWare Workstation 16 Pro.
- June 11, 2022: Harrison Seow (harrison.seow@seagate.com) on a Windows desktop with VMWare Workstation 16 Player.
- May 28, 2022: Mukul Malhotra (mukul.malhotra@seagate.com) on a Windows laptop with VMWare Workstation 16 Pro.
