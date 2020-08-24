### BUILD AND TEST ENVIRONMENT FOR CORTX

Building and testing CORTX can be done using a single system.  Obviously CORTX is designed to be a scale-out distributed object storage system but these instructions currently only cover setting up a single node environment.

Currently CORTX requires the CentOS 7.7.1908 distribution running the 3.10.0-1062 kernel. Building it and testing it can be done on either a physical or virtual machine matching these requirements.  

Please refer to [this page](LocalVMSetup.md) for information about running with VMWare Fusion or Virtual Box as well as links to downloadable VM images which you can use.  Please note that if you are a Seagate employee, you cannot use Virtualbox on Seagate machines and VMWare Fusion is your only option. Also note that you must file an [Exception Request](https://seagatetechnology-my.sharepoint.com/:w:/g/personal/dhiren_t_sutaria_seagate_com/Eev4uwgFL51Cp6GfQUAv04wBPHgzd0dl2UaCR6gcQAwh-A) to run a local VM on your desktop/laptop computer.

TODO:
1. Provide a downloadable image for Windows Subsystem Linux and any necessary instructions.

If you are unable to procure your own build system, a small number of supported virtual machines can be made available on a limited first come first serve basis.  Unfortunately this offer is only possible for Seagate employees and contractors.  Please click [here](DEV_VM.md) for instructions about how to request one.

Planning to create your own VM, please check [this page](VIRTUAL_MACHINE.md) for helpful pre-build steps pertaining to motr, s3server and hare. It also contains information to resolve a few issues that can come up at runtime.
