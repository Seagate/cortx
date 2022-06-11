 ## CORTX Quick Start Guide
 
This document provides quick access to several different ways to set up CORTX for installation, testing, running, or deployment. CORTX is available in multiple formats, including as a pre-built virtual machine image - the easiest and fastest way to get CORTX up and running. Alternatively, CORTX can be built into a binary format from source code.  Finally, for those interested in running different configurations of CORTX, there are a variety of different options available. These include everything from the full installation to more slimmed down installations, such as configuring and running motr, the core object storage layer, on its own.  We have attempted to create easy-to-follow instructions for all of these installation/test methods which are organized below in order of difficulty.
 
1. Download a CORTX virtual machine image to get up and running in minutes
    1. On a single node on Kubernetes inside an OVA: [LINK](doc/ova/2.0.0/PI-7/CORTX_on_Open_Virtual_Appliance_PI-7.rst)
    1. On an AWS EC2 instance: [LINK](doc/integrations/AWS_EC2/README.md)
    1. Across a cluster: TODO 
1. Run CORTX on Kubernetes: [LINK](https://github.com/Seagate/cortx-k8s/blob/integration/README.md)
    1. Run CORTX on Kubernetes using Amazon Web Services: [LINK](https://github.com/Seagate/cortx-k8s/blob/main/doc/cortx-aws-k8s-installation.md)
1. Build entire CORTX into binaries: [LINK](./doc/community-build/docker/cortx-all/README.md)
1. Build just the block storage layer (motr) from the source: [LINK](https://github.com/Seagate/cortx-motr/blob/main/doc/Quick-Start-Guide.rst)
    1. Run motr on a single node: [LINK](https://github.com/Seagate/cortx-motr/blob/main/doc/Quick-Start-Guide.rst)
    1. Run motr across a cluster: [LINK](https://github.com/Seagate/cortx-motr/blob/main/doc/Running_Motr_Across_a_Cluster.md)
1. Use a pre-built virtual machine to quickly test the block storage layer (motr)
    1. Run CORTX on a single node: [LINK](https://github.com/Seagate/cortx-motr/releases/tag/ova-centos79)

Once you have your CORTX system running, you can use [these code samples](cortx-s3samplecode) to show you how to connect to it and perform common S3 operations.

Note that some of the links above are tagged 'requires verification' and some are tagged 'LINK'.  We apply the 'LINK' tag only after instructions have been verified by at least two people.  The 'required verification' instructions are instructions which are still awaiting a second verification.  If you verify them, please add your name to the 'Tested By' section at the bottom of the page and change the tag on this page as well. If you have any questions, please visit our [resources page](https://github.com/Seagate/cortx/blob/main/SUPPORT.md) or join us on [Slack.](https://cortx.link/slack_invite)
    
Once you're ready to make more contributions, please check out our [Contribution Guide](CONTRIBUTING.md). 



