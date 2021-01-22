# Contribute to the CORTX Project

CORTX is about building the world's best scalable mass-capacity object storage system. If you’re interested in what we’re building and intrigued by hard challenges, here's everything you need to know about contributing to the project and how to get started. 

This guide is intended to provide quick start instructions for developers who want to build, test, and contribute to the CORTX software.  If you are merely looking to _test_ CORTX, please use [these instructions](doc/CORTX_on_Open_Virtual_Appliance.rst).

After reading this guide, you'll be able to pick up topics and issues to contribute, submit your codes, and how to turn your pull request into a successful contribution. And if you have any suggestions on how we can improve this guide, or anything else in the project, we want to hear from you!

## Code of Conduct

Thanks for joining us and we're glad to have you. We take community very seriously and we are committed to creating a community built on respectful interactions and inclusivity as documented in our [Contributor Covenant Code of Conduct](CODE_OF_CONDUCT.md). 

You can report instances of abusive, harassing, or otherwise unacceptable behavior by contacting the project team at opensource@seagate.com.

## Deployment and Testing
- Download a CORTX virtual machine image for easy testing
    - Run it on a single node: [LINK](doc/CORTX_on_Open_Virtual_Appliance.rst); run it on a cluster: TODO
- Download CORTX as prepackaged software releases
    - Run it on a single node: TODO; run it across a cluster: [DONE?](doc/scaleout/README.rst)
- Build motr from the source: [LINK](https://github.com/Seagate/cortx-motr/blob/main/doc/Quick-Start-Guide.rst)
    - Run freshly built motr on a single node: [LINK](https://github.com/Seagate/cortx-motr/blob/main/doc/Quick-Start-Guide.rst); run it on a cluster: TODO
- Build motr + S3 from the source: [DONE?](https://github.com/Seagate/cortx-s3server/blob/main/docs/CORTX-S3%20Server%20Quick%20Start%20Guide.md)
    - Run it on a single node [DONE?](https://github.com/Seagate/cortx-s3server/blob/main/docs/CORTX-S3%20Server%20Quick%20Start%20Guide.md); run it on a cluster [TODO]
- Build entire CORTX from the source: [DONE?](https://github.com/Seagate/cortx-hare/blob/main/README.md)
    - Run it on a single node: TODO; Run it on a cluster: TODO

## Contribution Process

### Prerequisites

- Please read our [CORTX Code Style Guide](doc/CodeStyle.md).
- Get started with [GitHub Tools and Procedures](doc/GitHub_Processes_and_Tools.rst), if you are new to GitHub.
   - Please find additional information about [working with git](doc/working_with_git.md) specific to CORTX.
- Please read about our [DCO and CLA policies](doc/dco_cla.md).

### Development

**Development Environment:** First you need to configure your [development environment](doc/BUILD_ENVIRONMENT.md). 

**Core Functionality:** For basic development and testing of CORTX, you only need the motr and S3 submodules.

1. [CORTX Motr](https://github.com/Seagate/cortx-motr/blob/main/doc/Quick-Start-Guide.rst)
   - The main data path of the system responsible for the actual storage and distribution of objects and key-value pairs.
2. [CORTX S3](https://github.com/Seagate/cortx-s3server/blob/main/docs/CORTX-S3%20Server%20Quick%20Start%20Guide.md)
   - The S3 interface to CORTX.

**Complete System:** For the complete CORTX experience, you will need to install the remaining submodules:

1. [CORTX HA](https://github.com/Seagate/cortx-ha/blob/main/Quick-Start-Guide.rst)
   - The module responsible for maintaining highly available access to shared storage.
2. [CORTX Hare](https://github.com/Seagate/cortx-hare/blob/main/README.md)
   - The module responsible for monitoring the distributed health of the system and maintaining consensus.   
3. [CORTX Management Portal](https://github.com/Seagate/cortx-management-portal/blob/main/README.md)
   - The module providing a user interface for management and monitoring of CORTX.   
4. [CORTX Manager](https://github.com/Seagate/cortx-manager/blob/main/README.md)
   - The module providing API's with which the management portal communicates with the other modules.   
5. [CORTX Monitor](https://github.com/Seagate/cortx-monitor/blob/main/cortx-monitorQuickstartGuide.md)
   - The module tracking platform health and raising alerts on sensing any unintended state.   
6. [CORTX POSIX](https://github.com/Seagate/cortx-posix/blob/main/doc/Quick_Start_Guide.md)
   - The module providing a file interface to CORTX.  
7. [CORTX Provisioner](https://github.com/Seagate/cortx-prvsnr/blob/main/Cortx-ProvisionerQuickstartGuide.md)
   - The module which assists the users is satisfying dependencies, configuring the components and the other modules, and initializing the CORTX environment.
 
## Additional Resources

- Download and run a [single node CORTX VM](doc/CORTX_on_Open_Virtual_Appliance.rst) for testing purposes.
- Setup and test [a scale-out distributed CORTX](doc/scaleout/README.rst).
- Learn more about the [CORTX Architechture](doc/architecture.md). 
- Learn more about [CORTX CI/CD and Automation](doc/CI_CD.md).
- Browse our [suggested list of contributions](https://github.com/Seagate/cortx/blob/main/doc/SuggestedContributions.md).

## Communication Channels

Please refer to the [Support](SUPPORT.md) section to know more about the various channels by which you can reach out to us. 

## Thank You!

We thank you for stopping by to check out the CORTX Community. We are fully dedicated to our mission to build open source technologies that help the world save unlimited data and solve challenging data problems. Join our mission to help reinvent a data-driven world.
