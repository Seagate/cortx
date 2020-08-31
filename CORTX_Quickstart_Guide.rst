.. _CORTX_QuickstartGuide:

CORTX Quickstart Guide
#######################

To contribute to the CORTX Open-Source project, you'll need to understand the way the CORTX repository is organized. 

The CORTX repository is the first and the parent repository that contains top-level documentation about the CORTX Community. The CORTX-Motr repository contains Motr files and is the second component of the CORTX project. Motr is the central component that stores Objects and Key-Values. The S3 Server component is built on Motr and all component related information is posited in the CORTX-S3 Server repository. 

Let's get CORTX ready!
======================

**Before you Begin**

- You'll need to `Build and Test your VM Environment <../main/doc/BUILD_ENVIRONMENT.md>`_.
- Ensure that you have RoCE(RDMA over Converged Ethernet) and TCP connectivity.

⚠️**Known Limitation**

The CORTX stack currently does not work on Intel's OmniPath cards.

"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

Since CORTX is composed of the Motr and S3 Server components, you'll need to:

1. `Set up CORTX-Motr <../main/doc/CortxMotrQuickStart.md>`_

2. `Get CORTX-S3 Server ready <https://github.com/Seagate/cortx-s3server/blob/dev/docs/CORTX-S3%20Server%20Quick%20Start%20Guide.md>`_

Watch our CORTX Engineer, Kevin Price, demonstrate **<link to the video>** the process.

"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

Additional Resources
---------------------

- Know more about `CORTX CI/CD and Automation <../main/doc/CI_CD.md>`_.
- Learn more about the `CORTX Architechture <../main/doc/architecture.md>`_
- You can `submit requests and bugs using GitHub Issues <https://github.com/Seagate/cortx/issues>`_

Contribute to CORTX Community
-----------------------------

Refer to the CORTX Community Guide **<link placeholder>** that hosts all information about community values, code of conduct, how to contribute code and documentation, community and code style guide, and how to reach out to us.

We excited for your interest in CORTX and hope you will join us. We take community very seriously and we are committed to creating a community built on respectful interactions and inclusivity as documented in our `Code of Conduct <CODE_OF_CONDUCT.md>`_.

Support and Community Discussions
*********************************

- Join our CORTX-Open Source Slack channel |Slack| to interact with your fellow community members and gets your questions answered. 
- If you'd like to contact us directly, drop us a mail at: `opensource@seagate.com <opensource@seagate.com>`_

Thank You!
-----------

We thank you for stopping by to check out the CORTX Community. We are fully dedicated to our mission to build open source technologies that help the world save unlimited data and solve challenging data problems. Join our mission to help reinvent a data-driven world.

.. |Slack| image:: https://img.shields.io/badge/chat-on%20Slack-blue
   :target: https://join.slack.com/t/cortxcommunity/shared_invite/zt-femhm3zm-yiCs5V9NBxh89a_709FFXQ?
