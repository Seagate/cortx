=========================
Description of config.ini
=========================

The purpose of the config.ini file is to provide configuration details about the CORTX cluster. It is required for the cluster's installation. 

The config.ini file contains several sections. All sections are mandatory. Each section has several keys or value pairs, some of which are mandatory and some are optional.

For the formatting guidelines of the INI file, refer https://www.nongnu.org/chmspec/latest/INI.html.

**********
Sections
**********
The following sections are displayed.

- Cluster

- Storage_enclosure

- srvnode-X

Cluster
========
This section describes the VIP address for management and public data networks.

Mandatory Keys
---------------
The mandatory keys are listed in the table below.

    +------------+---------------------------------------------+
    |     Key    |  Description / default value (if provided)  |
    +------------+---------------------------------------------+     
    | cluster_ip | IP address of Cluster (Data) VIP>           |
    +------------+---------------------------------------------+
    | mgmt_vip   | IP address of Management VIP>               |
    +------------+---------------------------------------------+

Optional Keys
--------------
- None
