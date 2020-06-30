.. vim: syntax=rst

|image0|


======================
**System Level View**
======================

|image1|


===============
**Features**
===============
 

* 0-copy, 2-phase IO

* extensible meta-data: distributed key-value store

* layouts (data, meta-data, fault-tolerance)

* network striping (parity de-clustering)

* distributed transactions (consistency)

* resource manager (coherency)

* addb

* user space, portable

..

==========================================
**EOS Core - Scalable Storage Platform**
==========================================


|image2|



===========
**Clovis**
===========



* object: network striped, flat

* index: distributed key-value store

* operation: asynchronous, state machine.

* layout: placement of data on storage

* 120-bit persistent identifiers, assigned by user

* all entry-points are asynchronous



..

===========
**Clovis**
===========


Clovis the access layer to eos core over which IO and KV operations can be performed.


|image3|



======================
**Clovis: Operation**
======================



|image4|


===========
**Clovis**
===========


Clovis provides the following abstractions:

* object (m0_clovis_obj) is an array of fixed-size blocks.

* index (m0_clovis_idx) is a key-value store.

* operation (m0_clovis_op) is a process of querying or updating system state.

..

===============
**Clovis: io**
===============


|image5|


==================
**Clovis: index**
==================



|image6|


===========
**Layout**
===========


-  determines how an object is stored in underlying containers

-  layouts for data and meta-data

-  examples:

   -  network striping with parity de-clustering (default)

   -  compression

   -  encryption

   -  de-duplication

   -  composite (NBA, small files, migration)

-  layout formulae


=================
**Parity Groups**
=================


|image7|



===============
**IO: Layout**
===============


|image8|


====================
**Failure Domains**
====================

Failure Domains


-  A failure domain is any physical entity unavailability of which can make object data inaccessible. Eg. Sites, Racks, enclosures, controllers, disks etc.

-  Failure domains algorithm translates resilience for disks to higher level of failure domains (sites, racks, controllers etc.) without altering the layout parameters N, K, S.

.. ===================
.. **Failure Domains**
.. ===================


Input:
######

1. Failure domains tree.

2. Layout parameters: N, K.

Output:
########

1. Tolerance vector, 'i'th entry of which represents the supportable
   tolerance for the 'i'th level of failure domains tree.

2. A formula for mapping object data so that tolerance in (1) is met.


..

.. ====================
.. **Failure Domains**
.. ====================



|image9|


================
**Conf schema**
================



|image10|


==============
**FOP & FOM**
==============


FOP means Formatted file operation packet. Each fop contains opcode for 
corresponding operation. Fop also contains reply which is sent be
service after completion of the request.

For each operation type server-side state machine (FOM) service is
registered with request handler. Whenever a fop is received from network
layer, it's fom is created and is submitted to locality queue.

========
**FOM**
========



|image11|



thread-per-request:

-  multiple cores, NUMA, locking


Reqh:

-  thread per core

-  non-blocking scheduler

-  locality of reference

-  load balancing


..


===============
**FOP to FOM**
===============



|image12|





EOS core servers are called m0d's. M0d for IOSERVICE contains ioservice,
dixservice, rmservice and other services. M0D for CONF contains
confservice and rmservice only. Here assuming 8 cores are available then
foms are scheduled among 8 locality threads.



========
**RPC**
========


-  Fops are posted to RPC layer and in formation they are aggregated.

-  RPC layer checks for request-reply matching

-  If RPC reply is not received with an interval it resends the RPC
   request

-  If timeout happens RPC layer sends timeout callback and nr_sends
   reaches a maximum value set.

-  Each RPC associates with a connection and RPC item contains source
   endpoint and destination endpoint.

**xcode**: serialisation library



========
**NET**
========



network: LNet, 0-copy, unreliable message passing

Lnetsupports tcp, RoCEand IB.

New Transport based on Sockets (recently added in Master).



..


=========
**Stob**
=========


-  Array of data-blocks, [0, 264), initially a hole

-  create, delete, read, write, alloc, free operations

-  IO at block granularity

-  No usual meta-data (attributes, *etc*.)

-  Block attributes can be used for checksums, encryption keys, hash
   fingerprints

-  Scatter-gather-scatter operations: data and blockattributes



..


|image13|

..


=========================
**Stob: Implementations**
=========================



-  linuxstob (*aka* devstob)

   -  stob = file

   -  aio

-  adstob (allocation data stob)

   -  multiple stobs stored in a backend stob

   -  block allocator


..


=====================
**Clovis Object IO**
=====================


Healthy read/write
###################

-  **Input:**

   -  Offset in object, length of data, user buffer for copying to/from




-  **Operations (assuming write):**

   -  Translate object-offset of data into appropriate parity group(s).

   -  Calculate parity over data blocks of a parity group.

   -  Use layout formula to map each unit to appropriate target and
      frame (a logical offset on target).

   -  Prepare and send control fops (hold info about bulk layer and
      other parameters (which target, frame etc)).

   -  Wait for: receiving data via bulk layer and receiving replies to
      control fops.

   -  Handle the error.


..

============
**IO Flow**
============



|image14|



..

==================
**Write IO: RMW**
==================


What if write request does not span entire parity group?

-  We have to overwrite the part of parity group being spanned, but
   other units of parity group need to be read as they are required for
   updating parity.

Can IO operations be optimised?

-  Suppose a group has data units D0 to DN -1, and incoming request
   spans W units.

   -  **Read old approach:** read older copies of spanned W units, read
      K parity units, apply the differential parity to K units and write
      them back. Thus we have (W + K) reads and (W + K) number of
      writes.

   -  **Read rest approach:** Read all the remaining (N -W) units along
      with K parity units, recalculate the parity and write W + K units
      back.

..

Writes are same in both cases (as expected).



==================
**Write IO: RMW**
==================


Can we optimise?

-  Read old approach: (W + K) reads.

-  Read rest approach: (N -W + K) reads.


We pick that approach for which reads are minimum.

..


==============================
**Write IO: Error Handling**
==============================


-  If write IO fails it returns an error. Then the layer above is then
   supposed to delete the attempted object, and try recreating it.

-  If the failure was due to unavailability of some disk, the subsequent
   attempt to create the file would end up assigning a new pool version.

-  If SNS repair is yet to touch the file, skip the failed device for
   writing but consider Data intended for it while calculating the
   parity.


..


====================
**Read IO: dgread**
====================


Clovis regenerates data from failed or unavailable units per parity
group.

-  Once read IO fails, clovis checks (per parity group) how many units
   are unavailable.

-  If W units were requested to be read, and K units are unavailable, it
   sends request for remaining N -W units and uses Reed-Solomon to
   recover the unavailable units of a parity group.

..

================================
**Read IO: parity-verify mode**
================================



When clovis app is mounted with parity-verify option, reading operation
reads all the units of a parity group.

Parity is re-calculated using the read units, and compared against the
read parity.


====================
**IO: Conf Update**
====================



Callbacks for configuration update cancel all RPC sessions that are
established with clovis instances.


**Ongoing IO:** fails immediately or eventually due to failed RPC
sessions.

**New IO requests:** These are made to wait till configuration is
updated.


The last ongoing IO request updates the in-memory data structures for
configuration (pool versions/pool-machines etc).



===============
**IO Service**
===============



|image15|


===================
**Read Data Flow**
===================



|image16|



==============
**IOSERVICE**
==============


|image17|



=========
**ADDB**
=========

-  Instrumentation on client and server

-  data about operation execution and system state passed through
   network

-  always on (post-mortem analysis, first incident fix)


|image18|


..

======================
**Read Request Flow**
======================



|image20|



===========================
**Key-Value Request Flow**
===========================



|image21|



========
**SNS**
========


- guaranteed IO performance during repair

- fast repair

- copy machine

- Repair

- Rebalance

- Pool

- flattening

..


.. =========
.. **SNS**
.. =========


repair/rebalance copy machine service
######################################


- Repair and Rebalance are implemented as Mero services.

- Both the services run on every ioservice node.

- Copy machine service initialises and finalises (start/stop) the fop and fom types for,
   - Copy packet fop and fom
   - Sw update fop and fom
   - Trigger fop and fom

..



=========================
**SNS: Trigger fop/fom**
=========================



Operations
###########


- Repair

- Rebalance

- Repair quiesce/resume

- Rebalance quiesce/resume

- Repair abort

- Rebalance abort

- Repair status

- Rebalance status

..


=====================
**SNS: Copy Machine**
=====================



1. Prepare
###########

   -  RM init

   -  Buffer pool provisioning

   -  Ag iterator init

2. Ready (generic)
###################

   -  Start ast thread

   -  Update remote replicas

3. Start
##########

   -  Start pump

   -  Start iterator

4. Stop
#########

   -  Stop iterator

   -  Finalise RM

   -  Prune bufferpools

   -  Stop ast thread (generic)

..


================
**SNS: Repair**
================



|image22|


..

========
**SNS**
========



|image23|


..

=======================
**SNS: Data Iterator**
=======================


|image24|


..


=====================
**SNS: Copy Packet**
=====================



|image25|


..

=================
**SNS: Receive**
=================



|image26|


..

=================
**SNS: CM Stop**
=================



|image27|


..


===================
**SNS: Operation**
===================



|image28|

..


=========
**SNS**
=========



|image29|

..


=======
**RM**
=======



-  **resource**: anything with ownership. An extent in an object, an
   entire object, a key in an index, *etc*.

-  credit: a right to use a resource in a particular way (lock)

-  credits can be borrowed and sublet

-  resource manager is separate from resource

-  resource manager resolves conflicts

-  user can define new resource types

**RM: Use Case**

Example: fid extent allocation. Fid: 128 bit.


|image30|


..

=======
**BE**
=======



It is used to store the metadata. There are two kinds of metadata in BE:

-  The metadata about the data stored on storage devices. Consists of:

   - balloc: what space is free on the data storage device and what is not.
	  
   - extmap in ad stob domain: if we have an ad stob it has the information where the ad stob data is stored on the storage device.
	  
   - cob: the gob (file) attributes, pver, lid, size.

-  The metadata exported to user. It's DIX which is exported through
   Clovis.
   

..




How BE Works
#############


-  Data from segments is mmap()ed to memory;

-  Changes to segments are captured to transactions;

-  The captured changes are:

   - written to the log - at this point the tx becomes persistent, and then

   - written in-place into the segments.

-  In case of failure the changes from the log are applied to the
   segments.
   


Top Level Components
#####################

-  BE domain: handles BE startup/shutdown

-  BE engine: the transaction engine. Manages transactions and
   transaction groups.

-  BE segment: data is stored there. Consists of backing store and
   in-memory "mapping".

-  BE tx: the transaction. The changes in segments are captured to the
   transactions.

-  BE log: all the segment changes that need to become persistent go
   there.

-  The changes that didn't go to the segments are replayed during BE
   recovery.


|image31|



..

======================
**Function Shipping**
======================


-  move computation closer to data (compute-in-storage)

-  reduce network transmission overhead

..


Implementations
###################


-  Uses in-storage-compute service

-  low level trusted mechanism:

   -  dynamically load shared library into Mero service process

   -  invoke computations remotely, argument-result passing

-  untrusted mechanism:

   - run untrusted code (e.g., Python) in a separate address space

-  client uses layouts to start execution and recover from failures


..

===============
**References**
===============



EOS Core Training Documents:

`<https://drive.google.com/drive/u/0/folders/1_oq-i20X7lzWHeLxcSiwfUIZMxgGxHHI>`_

Mero Technical Long:

`<https://drive.google.com/drive/u/0/folders/1_oq-i20X7lzWHeLxcSiwfUIZMxgGxHHI>`_

Mero Function shipping:

`<https://docs.google.com/presentation/d/1kCNlM78b7F0yRJLhq5seymRLU6a2adRznbN_hkhjt5c/edit#slide=id.g2b85cd7800_0_23>`_


..

=========
**Demo**
=========


Clovis sample Apps Usage,

$ dd if=abcd of=abcd-512K bs=4K count=128

$ c0cp -l 172.16.0.124@o2ib:12345:44:301 -H 172.16.0.124@o2ib:12345:45:1
-p 0x7000000000000001:0 -P 0x7200000000000000:0 -o 12:34 abcd-512K -s
4096 -c 128 -L 1

$c0cat -l 172.16.0.124@o2ib:12345:44:301 -H 172.16.0.124@o2ib:12345:45:1
-p 0x7000000000000001:0 -P 0x7200000000000000:0 -o 12:34 -s 4096 -c
128-L 1 > abcd-512K-read

$ diffabcd-512Kabcd-512K-read

$ m0clovis for index create, put, get, delete ops.


..

===============
**Questions?**
===============


..




..

.. |image0| image:: images/1_EOS_Core_Deep_Dive.png
   
.. |image1| image:: images/2_System_Level_View.png
   
.. |image2| image:: images/3_EOS_Core_Scalable_Storage_Platform.png
   
.. |image3| image:: images/4_Clovis_Layer.png
   
.. |image4| image:: images/5_Clovis_Operation.png
   
.. |image5| image:: images/6_Clovis_IO.png
   
.. |image6| image:: images/7_Clovis_Index.png
   
.. |image7| image:: images/8_Parity_Groups.png
   
.. |image8| image:: images/9_IO_Layout.png
   
.. |image9| image:: images/10_Failure_Domains.png
  
.. |image10| image:: images/11_Conf_Schema.png
   
.. |image11| image:: images/12_FOM.png
   
.. |image12| image:: images/13_FOP_to_FOM.png
   
.. |image13| image:: images/14_STOB.png
  
.. |image14| image:: images/15_IO_Flow.png
   
.. |image15| image:: images/16_IO_Service.png
   
.. |image16| image:: images/17_Read_Data_Flow.png
 
.. |image17| image:: images/18_FOP_Execution.png
   
.. |image18| image:: images/19_ADDB.png
   
.. |image20| image:: images/20_Read_Request_Flow.png
   
.. |image21| image:: images/21_Key_Value_Request.png

.. |image22| image:: images/22_SNS_Repair.png

.. |image23| image:: images/23_SNS.png

.. |image24| image:: images/24_SNS_Data_Iterator.png

.. |image25| image:: images/25_SNS_Copy_Packet.png

.. |image26| image:: images/26_SNS_Receive.png

.. |image27| image:: images/27_SNS_CM_Stop.png

.. |image28| image:: images/28_SNS_Operation.png

.. |image29| image:: images/29_SNS_Parity_Block.png

.. |image30| image:: images/30_RM_Use_Case.png

.. |image31| image:: images/31_BE_Data_Flow.png

