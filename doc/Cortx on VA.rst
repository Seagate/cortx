==========================
Cortx on Virtual Appliance
==========================
A Virtual Appliance (VA) is a virtual machine image file consisting of a pre-configured operating system (OS) environment and a single application.

This document provides information on the procedure that must be followed to install Cortx (all components included) on a VA.

**************
Prerequisites
**************
The prerequisites required to install Cortx on a VA is listed below.

- GitHub Access
- ESX Server

**********
Procedure
**********
The procedure to install Cortx on VA is mentioned below.

1. Login to `GitHub <https://github.com/>`_ with your credentials.
2. Select the relevant repository, and navigate to the appropriate directory. *<<we need to give links of the repository and directory>>*

3. From the directory, download the Open Virtualization Appliance (OVA) file.

4. Import the OVA file into ESX, and install Cortx. Refer `Help Docs <https://docs.vmware.com/en/VMware-vSphere/6.5/com.vmware.vsphere.html.hostclient.doc/GUID-8ABDB2E1-DDBF-40E3-8ED6-DC857783E3E3.html>`_ to know more about the process of installation.

5. Run the following command:

 - **hctl bootstrap --mkfs /var/lib/hare/cluster.yaml**

  You must run the above command with **--mkfs** only once. Further usage erases data.

6. Ensure that the I/O stack is running by running the following command:

 - **hctl status**

7. Ensure that the CSM service is operational by running the following commands:

 - **systemctl status csm_agent**
 - **systemctl status csm_web**

   If the above services are not active, run the following command:

  - **systemctl start <csm_agent|csm_web>**
