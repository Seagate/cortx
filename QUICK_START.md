# CORTX Development Quickstart Guide

This guide is intended to provide quick start instructions for developers who want to build, test, and contribute to the CORTX software.  If you are merely looking to _test_ CORTX, please use [these instructions](doc/CORTX_on_Open_Virtual_Appliance.rst).

**Develoment Environment:** First you need to configure your [development environment](doc/BUILD_ENVIRONMENT.md). 

**Core Functionality:** For basic development and testing of CORTX, you only need the motr and S3 submodules:

1. [CORTX-Motr](https://github.com/Seagate/cortx-motr/blob/dev/doc/Quick-Start-Guide.rst)
   - The main data path of the system responsible for the actual storage and distribution of objects and key-value pairs.
2. [CORTX-S3](https://github.com/Seagate/cortx-s3server/blob/dev/docs/CORTX-S3%20Server%20Quick%20Start%20Guide.md)
   - The S3 interface to CORTX.

**Complete System:** For the complete CORTX experience, you will also need the remaining submodules:

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
6. [CORTX POSIX](https://github.com/Seagate/cortx-posix/blob/dev/doc/Quick_Start_Guide.md)
   - The module providing a file interface to CORTX.  
7. [CORTX Provisioner](https://github.com/Seagate/cortx-prvsnr/blob/main/Cortx-ProvisionerQuickstartGuide.md)
   - The module which assists the users is satisfying dependencies, configuring the components and the other modules, and initializing the CORTX environment.

## Additional Resources

- Learn more about [CORTX CI/CD and Automation](doc/CI_CD.md).
- Setup and test [a scale-out distributed CORTX](doc/scaleout/README.rst).
- Download and run a [single node CORTX VM](doc/CORTX_on_Open_Virtual_Appliance.rst) for testing purposes.
- Learn more about the [CORTX Architechture](doc/architecture.md).

## Contribute to CORTX Community

Refer to the CORTX [Contribution Guide](doc/CORTXContributionGuide.md) that hosts all information about community values, code of conduct, how to contribute code and documentation, code style guides, and how to reach out to us. 

We are excited for your interest in CORTX and hope you will join us. We take community very seriously and we are committed to creating a community built on respectful interactions and inclusivity as documented in our [Code of Conduct](CODE_OF_CONDUCT.md).

- Chat with us in our CORTX-Open Source [![Slack](https://img.shields.io/badge/chat-on%20Slack-blue)](https://join.slack.com/t/cortxcommunity/shared_invite/zt-femhm3zm-yiCs5V9NBxh89a_709FFXQ?) in order to connect with the rest of the community and learn more. 
- Ask questions and share ideas in our [CORTX Discussions.](https://github.com/Seagate/cortx/discussions)
- Submit [requests and bugs using  Issues](https://github.com/Seagate/cortx/issues).
- Contact us directly if you prefer by emailing us at cortx-question@seagate.com.

## Thank You!

We thank you for stopping by to check out the CORTX Community. We are fully dedicated to our mission to build open source technologies that help the world save unlimited data and solve challenging data problems. Join our mission to help reinvent a data-driven world.
