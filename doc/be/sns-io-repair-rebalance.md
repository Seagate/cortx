# SNS (io, repair, rebalance)

## Overview

- Server network striping ([find more details for io here](https://seagatetechnology-my.sharepoint.com/:w:/g/personal/hua_huang_seagate_com/Eb0CTiI3VbdAhLzn8NufzisBj_OFeA4bnKw3Pu0X0jJGVA?e=4%3A8psKWK&at=9&CID=c7fca889-7248-8d1b-0701-0641fb87566d "find more details for io here"))
- Implements Erasure coding

![SNS Overview](/doc/be/images/sns-overview.PNG)

## Repair/Rebalance

- Generic Components
	- CM, CP
	- Pump
	- Proxy
	- Ag store
	- Sliding window

- SNS Specific Components
	- Trigger fop/fom
	- SNS repair/rebalance copy machine
	- SNS repair copy packet
	- SNS repair data iterator
	- SNS repair incoming aggregation groups iterator

## SNS repair/rebalance copy machine service

- Repair and Rebalance are implemented as Mero services
	- $MERO_SRC/sns/cm/repair/service.[ch]
	- $MERO_SRC/sns/cm/rebalance/service.[ch]
- Both the services run on every ioservice node.

- Copy machine service initialises and finalises (start/stop) the fop and fom types for,
	- Copy packet fop and fom
	- Sw update fop and fom
	- Trigger fop and fom

## Trigger fop/fom

- Operations
	- Repair
	- Rebalance
	- Repair quiesce/resume
	- Rebalance quiesce/resume
	- Repair abort
	- Rebalance abort
	- Repair status
	- Rebalance status

- Source: $MERO_SRC/sns/cm/trigger_{fop, fom}.[ch]

## Trigger fom

Sources :
- $MERO_SRC/cm/repreb/trigger_fom.c : Generic trigger fom implementation for
PREPARE, READY, START and FINI phases.
-  $MERO_SRC/sns/cm/trigger_fom.c : sns repair/rebalance trigger fom
implementation.

## Trigger fop/fom contd..

- Triggers sns repair/rebalance

Phases
- PREPARE
	- Quiesce/Abort/Status
	- Invokes copy machine prepare
		- Buffer pool provisioning, initialises ag, data iterator
- READY - Invokes copy machine ready
	- Starts ast thread, updates initial sliding window
- START - Invokes copy machine start
	- Starts pump fom, data iterator, initialises size data structures

## Copy machine

> CM start

![CM Start](/doc/be/images/sns-cm-start.PNG)

## Repair/Rebalance copy machine

- Inherits generic copy machine
	- Repair and Rebalance implemented as separate m0_reqh_service
	- Allocate - allocates and initialises struct m0_cm
	- Start - sets up copy machine, initialises fop/foms
	- Stop - finalises copy machine
- Setup
	- Initialises data structures
	- Invokes corresponding copy machine (repair/rebalance) setup, mainly initialises buffer pools
		- Buffer pools - incoming and outgoing
	- Initialises sns data iterator
- Prepare (generic)
	- Setup proxies
	- Start sw store fom
	- Setup pump
	- AG store fom start
- Prepare
	- RM init
	- Buffer pool provisioning
	- Ag iterator init
- Ready (generic)
	- Start ast thread
	- Update remote replicas
- Start
	- Start pump (generic)
	- Start iterator
- Stop
	- Stop iterator
	- Finalise RM
	- Prune bufferpools
	- Stop ast thread (generic)

> SNS data iterator

![SNS data iterator](/doc/be/images/sns-data-iterator.png)

> Copy packet

![Copy packet](/doc/be/images/sns-copy-packet.PNG)

> Copy packet receive

![Copy packet receive](/doc/be/images/sns-copy-packet-receive.PNG)

> Sliding window

![Sliding window](/doc/be/images/sns-sliding-window.PNG)

> Failure Handling

![Failure handling](/doc/be/images/sns-failure-handling.PNG)

> CM stop

![CM Stop](/doc/be/images/sns-cm-stop.PNG)

## Additional functionality

- Abort
- Quiesce/Resume
- Concurrent io with repair/rebalance
- Concurrent delete with repair/rebalance
- Impose resource restrictions with help of sliding window

## References

- [HLD of SNS Repair](https://seagatetechnology-my.sharepoint.com/personal/mandar_sawant_seagate_com/Documents/GoogleDrive/HLD%20of%20SNS%20Repair.docx?web=1)
- [Copy macine and Copy packet redesign discussion](https://docs.google.com/document/d/1IPlMzMZZ7686iCpvt1LyMzglfd9KAkKKhSAlu2Q7N_I/edit)
- DLD part of source code, use doxygen.