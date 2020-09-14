================
Config.ini File
================

The purpose of the config.ini file is to provide configuration details about the CORTX cluster. It is required for the cluster's installation. 

The config.ini file contains several sections. All sections are mandatory. Each section has several keys-value pairs, some of which are mandatory and some are optional.

For the formatting guidelines of the INI file, refer https://www.nongnu.org/chmspec/latest/INI.html.

**********
Sections
**********
The following sections are displayed.

- [cluster]

- [storage_enclosure]

- [srvnode-X]

[cluster]
=========
This section describes the VIP address for management and public data networks.

Mandatory Keys
---------------
The mandatory keys are listed in the table below.

    +------------+-------------------------------------------------+
    |  **Key**   |  **Description / default value (if provided)**  |
    +------------+-------------------------------------------------+     
    | cluster_ip | IP address of Cluster (Data) VIP>               |
    +------------+-------------------------------------------------+
    | mgmt_vip   | IP address of Management VIP>                   |
    +------------+-------------------------------------------------+

Optional Keys
--------------
- None

[storage_enclosure]
===================
This section describes the storage enclosure type. JBOD is the only supported type.

Mandatory Keys
---------------
The mandatory keys are listed in the table below.

    +------------+------------------------------------------------+
    | **Key**    | **Description / default value (if provided)**  |
    +------------+------------------------------------------------+     
    | type       | JBOD                                           |
    +------------+------------------------------------------------+

**Note**: The value in this section is case-sensitive.

Optional Keys
--------------
- None

[srvnode-X]
============
This section provides information about every server in the cluster. 

**Note**: X should be replaced with the number. The actual sections should be named [srvnode-1], [srvnode-2], and [srvnode-3]. 

Mandatory Keys
---------------
The mandatory keys are listed in the table below.

  +--------------------------------+------------------------------------------+
  |             **Key**            |  **Description/value (if provided)**     |
  +--------------------------------+------------------------------------------+
  | hostname                       | FQDN of the server                       |
  +--------------------------------+------------------------------------------+
  | network.mgmt_nw.iface          | Name of the Management interface         |
  |                                |                                          |
  |                                | For example:                             |
  |                                |    network.mgmt_nw.iface=eno1            |
  +--------------------------------+------------------------------------------+
  | network.mgmt_nw.public_ip_addr | IP address of the Management network     |
  |                                | interface                                |
  |                                | If keyword "None" is specified, the      |
  |                                | network will be configured with an       |
  |                                | assumption that DHCP addresses are       |
  |                                | being used                               |
  +--------------------------------+------------------------------------------+
  | network.data_nw.iface          | A comma-separated list of the data       |
  |                                | network interfaces. The Public Data      |
  |                                | interface should be specified first,     |
  |                                | followed by the Private Data interface   |
  |                                |                                          |
  |                                | For example:                             |
  |                                |    network.data_nw.iface=eth2,eth3       |
  +--------------------------------+------------------------------------------+
  | network.data_nw.public_ip_addr | IP address of the Public Data network    |
  |                                | interface                                |
  |                                | If keyword "None" is specified, the      |
  |                                | network will be configured with an       |
  |                                | assumption that DHCP addresses are       |
  |                                | being used                               |
  +--------------------------------+------------------------------------------+
  | network.data_nw.pvt_ip_addr    | Private Data network IP address          |
  +--------------------------------+------------------------------------------+

    
Optional Keys
--------------

   +--------------------------------+-----------------------------------------+
   |             **Key**            |  **Description/value (if provided)**    |
   +--------------------------------+-----------------------------------------+
   | is_primary                     | Designates the first server in the      |
   |                                | cluster. The value should be set to     |
   |                                | True for the first server and False     |
   |                                | for the remaining two servers           |
   +--------------------------------+-----------------------------------------+
   | network.mgmt_nw.netmask        | Subnet mask for the Management network  |
   |                                |                                         |
   |                                | Accepted formats:                       |
   |                                |     AAA.BBB.CCC.DDD                     |
   |                                |                                         |
   |                                |     XY                                  |
   |                                |                                         |
   |                                |  NOTE: This key becomes mandatory       |
   |                                |        if the statis IP address is used |
   |                                |        for the Management network       |
   |                                |        interface                        |
   +--------------------------------+-----------------------------------------+
   | network.mgmt_nw.gateway        | IP address of the Management network    |
   |                                | gateway. If there's no gateway, keyword |
   |                                | "None" should be used                   |
   +--------------------------------+-----------------------------------------+
   | network.data_nw.netmask        | Subnet mask for the Public Data network |
   |                                |                                         |
   |                                | Accepted formats:                       |
   |                                |     AAA.BBB.CCC.DDD                     |
   |                                |                                         |
   |                                |     XY                                  |
   |                                |                                         |
   |                                |  NOTE: This key becomes mandatory       |
   |                                |        if the statis IP address is used |
   |                                |        for the Public Data network      |
   |                                |        interface                        |
   +--------------------------------+-----------------------------------------+
   | network.data_nw.gateway        | IP address of the Public Data network   |
   |                                | gateway. If there's no gateway, keyword |
   |                                | "None" should be used                   |
   +--------------------------------+-----------------------------------------+
   | bmc.user                       | BMC user name                           |
   |                                |                                         |
   |                                | For example:                            |
   |                                |    ADMIN                                |
   +--------------------------------+-----------------------------------------+
   | bmc.secret                     | BMC password (in single quotes)         |
   |                                |                                         |
   |                                | For example:                            |
   |                                |    'adminBMC123!'                       |
   +--------------------------------+-----------------------------------------+
   
****************************
Template of Config.ini file
****************************

| [cluster]
| cluster_ip=
| mgmt_vip=


| [storage_enclosure]
| type=JBOD


| [srvnode-1]
| hostname=
| network.mgmt_nw.iface=
| network.mgmt_nw.public_ip_addr=
| network.mgmt_nw.netmask=
| network.mgmt_nw.gateway=
| network.data_nw.iface=
| network.data_nw.public_ip_addr=
| network.data_nw.netmask=
| network.data_nw.gateway=
| network.data_nw.pvt_ip_addr=
| is_primary=
| bmc.user=
| bmc.secret=

- [srvnode-2]

  hostname=

  network.mgmt_nw.iface=

  network.mgmt_nw.public_ip_addr=

  network.mgmt_nw.netmask=

  network.mgmt_nw.gateway=

  network.data_nw.iface=

  network.data_nw.public_ip_addr=
 
  network.data_nw.netmask=

  network.data_nw.gateway=

  network.data_nw.pvt_ip_addr=

  is_primary=

  bmc.user=

  bmc.secret=

- [srvnode-3]

  hostname=

  network.mgmt_nw.iface=

  network.mgmt_nw.public_ip_addr=

  network.mgmt_nw.netmask=

  network.mgmt_nw.gateway=

  network.data_nw.iface=

  network.data_nw.public_ip_addr=

  network.data_nw.netmask=

  network.data_nw.gateway=

  network.data_nw.pvt_ip_addr=

  is_primary=

  bmc.user=

  bmc.secret=
