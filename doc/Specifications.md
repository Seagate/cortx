# VM Specifications - Single-Node VM deployment

### Pre-requisite:

- Please create VM(s) with at least 2 cores (vCPU) and 4GB of RAM.
- Ensure the VM is created with minimum 6 attached disks (raw) per node i.e. /dev/sdb (Data Disk), /dev/sdc (Metadata Disks)

  - Data Disks - Min 4 Disks per node (Capacity 10G+)
  - Metadata Disks - Min 2 Disks per node (Capacity - 10% of total Data Disk Size)
  
- Ensure the VM is created with minimum 3 network Interfaces

NOTE: For single-node VM, the VM node itself is treated as primary node.
