Server Network Striping Repair 
------------------------------

Overview
+++++++++

Redundant striping is a proven technology to achieve higher throughput and data availability.  SNS (a.k.a Server Network Striping) applies this technology to networked devices, and achieves the similar goals as local RAID.  

In the presence of storage and/or server failure, SNS repair will reconstruct lost data from surviving data reliably and quickly, without major impact to the product systems. 

Functional description 
+++++++++++++++++++++++

**Fundamental core concept**
***************************

**Cluster-wide object:** is an array of bytes (the object's linear name-space, called cluster-wide object data) and accompanying meta-data, stored in containers and accessed through at least read and write operations. Index in this array is called an offset; A cluster-wide object can appear as a file if it is visible in file system namespace. 

**Cluster-wide object layout:** roughly speaking, is a map, determining where on the storage a particular element of its byte array is located.  The striping layouts describe the data location of an object. Cluster-wide object layout specifies a location of its data or redundancy information as a pair (component-id, component-offset), where component-id is a fid of a component that is stored in a  certain container. 

**Component:** is an object, and, like a cluster-wide object, an array of bytes identified by a (component) offset plus some meta-data. 

In addition to placing object data, striping layouts define the contents and placement of redundancy information, used to recreate lost data. Simplest example of redundancy information is given by RAID5 parity. In general, redundancy information is some form of check-sum over data. 

- **Data unit:** a striping unit to which cluster-wide object or component data are mapped. 
- **Parity unit** a striping unit to which redundancy information is mapped. 
- **Striping layout:** belongs to a striping pattern (N+K)/G if it stores K parity units with redundancy information for every N data units and units are stored in G containers. Typically G is equal to the number of storage devices in the pool. Where G is not important or clear from the context, one talks about N+K striping pattern (which coincides with the standard RAID terminology). 
- **Parity group:** is a collection of data units and their parity units. 

**SNS reapair:** when a failure is detected and the system decides to do SNS repair, the repair is able to concurrently read data from multiple storage devices, aggregates them, transfer them over the network and place them into distributed spare space. The whole process can utilize the system resources with full bandwidth. If another failure happens during this process, it is reconfigured with new parameters and starts repair again, or fails gracefully. 

**Redundancy level:** A pool using N+K striping pattern can recover from at most K drives failures. System can reconstruct lost units from the surviving unit unit. K can be selected so that a pool can recover from a given number Kd or device failures and a given number Ks of server failures (assuming uniform distribution of units across servers). 

The SNS manager gets an input set configuration and output set configuration as the repair initiated. These input/output set can be described by some form of layout. SNS repair will read data/parity from the devices described with the input set and reconstruct missing data. In the process of reconstruction object layouts affected by the data reconstruction (i.e., layouts with data located on the lost storage device or node) are transactionally updated to reflect changed data placement. Additionally, while the reconstruction is in progress, all affected layouts are switched into a degraded mode so that clients can continue to access and modify data. 

Logical description 
*******************

The main sub-components of SNS repair components are: 

**Pool machine:** a replicated state machine responsible for organising IO with a pool. External events such as failures, liveness layer and management tool call-backs incur state transitions in a pool machine. A pool machine, in turn, interacts with entities such as layout manager and layout IO engines to control pool IO. A pool machine uses quorum based consensus to function in the face of failures; 

**Copy machine:** a replicated state machine responsible for carrying out a particular instance of a data restructuring. A copy machine is used to move, duplicate or reconstruct data. In the case of an SNS repair, a copy machine is created when a pool machine transitions into degraded state. A copy machine creates an ensemble of storage, network and collecting agents that collaborate to reconstruct pool data. Agent completion events cause copy machine to interact with the layout manager to indicate that data have been reconstructed. Client IO forces copy machine to reconstruct data out of order to serve client requests. On additional failures pool machine reconfigures the copy machine to continue the repair. Eventually, copy machine transfers its parent pool into either normal or failed mode and self-destructs; 

**Storage-in agent:** a state machine, associated with a storage device. A storage-in agent, created as part of a copy machine, asynchronously reads data from the device and directs them to a collecting, network-out or storage-out agent.  

**Storage-out agent:** a state machine, associated with a storage device. A storage-out agent, created as part of a copy machine, collects data from other agents (storage-in, network-in or collecting ones) and submits them to the storage. 

**Network-in agent:** a state machine, associated with a network interface. A network-in agent, created as part of a copy machine, asynchronously reads data, sent by other agents across the network and directs them to a collecting or storage-out agent. 

**Network-out agent:** a state machine, associated with a network interface. A network-out agent, created as part of a copy machine, collects data from other agents (storage-in or collecting ones) and asynchronously submits them to the RPC layer. 

**Collecting agent:** a state machine, created as part of a copy machine, typically associated with a server node, collecting data from other agents, reconstructing data by using transformation function associated with the copy machine and forwarding data to other agents. 

**Copy machine buffer pool:** a buffer pool limiting the amount of memory copy machine consumes on a given node. 

The following diagram depicts these sub-components of SNS repair: 

**TODO:** Add a diagram depicts of the sub-components of SNS repair: 

You configure agents in various ways. In the simplest case, storage-in and storage-out agents are located in the same address space and directly exchange data through a pool of shared buffers (this is a scenario of local data duplication, e.g., for the purpose of a snapshot creation). Next in complexity is a configuration where storage-in agents, associated with a storage devices on a node, deliver data into buffers allocated by a network-out agent running on the node. The latter sends data to a set of network-in agents across the network. The network-in agent forwards data to a collecting agent, running on the same node. This collecting agent, collects and optionally reconstructs the data, and sends them to the storage-out agent running on the same node. This is a scenario of a network repair. 

**TODO:** Add a figure of a copy machine data-flow 

Key design highlights
**********************

- The SNS and SNS repair code and algorithm is shared by server network striping and the local RAID. 
- SNS repair will try to use maximal system bandwidth, by balance the normal I/O operation and repair activities. 

Use cases
**********

- Lost data is reconstructed by SNS repair.  In the presence of some failure, including disk failure, node failure, lost data can be reconstructed from redundancy data. 

- Liveness layer detected the failure, trigger SNS repair. 

- SNS repair manage initializes copy machines, instruct them to use new layout for ongoing I/O, and repair the lost data out of order. 

- Disk bandwidth/network bandwidth/CPU utilization can be controlled by limiting the usage of these resources. 

- SNS layout. When a global (cluster-wide) object is created or accessed, its layout is used to locate the data/metadata contents of this object. 

- Parity data is also modified or saved in parity group. Parity is de-clustered. This means the parity data is stored evenly in the parity group. SNS repair can utilize the system throughput as much as possible. 

- Spare space. Spare space is distributed among the whole system. This also insures the fast repair. 
