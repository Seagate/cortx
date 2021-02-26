 ## CORTX Quick Start Guide
 
This document presents the most common methods by which interested parties can test, install, run, deploy, and ultimately potentially contribute, to CORTX.  CORTX is available in multiple formats.  It is available as a pre-built virtual machine image which presents the easiest path to install, deploy, and test it.  It is also available as source code which can be built into binary formats for installing, deploying, and testing.  Finally, it is possible to install, deploy, and test different configurations of CORTX ranging from the full installation to just the core object storage layer (motr) by itself).  We have attempted to create easy-to-follow instructions for all of these installation/test methods which are organized below in order of difficulty.
 
 1. Run a pre-built CORTX OVA (virtual machine image)
    1. On a single node: [LINK](doc/CORTX_on_Open_Virtual_Appliance.rst)
    1. On an AWS EC2 instance: [LINK](doc/integrations/AWS_EC2.md)
    1. Across a cluster: TODO
1. Build motr from the source: [LINK](https://github.com/Seagate/cortx-motr/blob/main/doc/Quick-Start-Guide.rst)
    1. Run it on a single node: [LINK](https://github.com/Seagate/cortx-motr/blob/main/doc/Quick-Start-Guide.rst)
    1. Run it across a cluster: [LINK](https://github.com/Seagate/cortx-motr/wiki/Build-Motr-from-Source-in-a-Cluster)
1. Build motr + S3 from the source: [requires verification](https://github.com/Seagate/cortx-s3server/blob/main/docs/CORTX-S3%20Server%20Quick%20Start%20Guide.md)
    1. Run it on a single node [requires verification](https://github.com/Seagate/cortx-s3server/blob/main/docs/CORTX-S3%20Server%20Quick%20Start%20Guide.md)
    1. Run it across a cluster [TODO]
1. Run a prepackaged CORTX software releases
    1. On a single node: TODO
    1. Across a cluster: [requires verification](doc/scaleout/README.rst)
1. Build entire CORTX into binaries: [requires verification](https://github.com/Seagate/cortx/blob/main/doc/Release_Build_Creation.rst)
    1. Run it on a single node: TODO 
    1. Run it on a cluster: TODO
1. Build entire CORTX into OVA (virtual machine image): TODO
    1. Run it on a single node: TODO 
    1. Run it on a cluster: TODO

Note that some of the links above are tagged 'requires verification' and some are tagged 'LINK'.  We apply the 'LINK' tag only after instructions have been verified by at least two people.  The 'required verification' instructions are instructions which are still awaiting a second verification.  If you verify them, please add your name to the 'Tested By' section at the bottom of the page and change the tag on this page as well.  
    
Once you're ready to make more contributions, please check out our [Contribution Guide](CONTRIBUTING.md).


