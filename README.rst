.. _CORTX_README:
|license| |Codacy Badge| |codacy-analysis-cli|

.. image:: ../assets/images/cortx-logo.png

CORTX: World's First Scalable Mass-Capacity Object Storage System
==============================================================================

The volume of data created and collected worldwide is massively increasing. In comparison, only a small proportion of this data is being retained. The machine learning models at the forefront of the most significant research studies today, depend on access to complete data sets. We're continuously creating efficient and economical storage solutions to enable research that's transforming the world.

.. image:: ../assets/images/at_risk_data.jpg

The CORTX Project
-----------------

CORTX is a distributed object storage system designed for great efficiency, massive capacity, and high HDD-utilization. 

CORTX Project Scope & Core Design Goals
*****************************************

We've listed the project scope and core design goals below:

.. csv-table::
   :header: "Project Scope", "Core Design Goals"
   :widths: 20, 80
   
   "Processor", "Agnostic, adapts to any processor."
   "Flexibility", "Highly flexible, works with HDD, SSD, and NVM."
   "Scalability", "Massively Scalable. Scales up to a billion billion billion billion billion exabytes (2^206) and 1.3 billion billion billion billion (2^120) objects with unlimited object sizes."
   "Responsiveness", "Rapidly Responsive. Quickly retrieves data regardless of the scale using a novel Key-Value System that ensures low search latency across massive data sets."
   "Resilience", "Highly Resilient. Ensures a high tolerance for hardware failure and faster rebuild and recovery times using Network Erasure Coding, while remaining fully RAID-compatible."
   "Transparency", "Provides specialized telemetry data and unmatched insight into system performance."

The CORTX project is licensed under the `Apache 2.0 License <LICENSE>`__.

CORTX Community
---------------
Refer to the CORTX Community Guide **<link placeholder>** that hosts all information about community values, code of conduct, how to contribute code and documentation, community and code style guide, and how to reach out to us. 

To Start Using CORTX
********************

- Refer to the `CORTX Quickstart Guide <../master/CORTX_Quickstart_Guide.rst>`_ to build and test CORTX.

- Use `Opengrok <https://oracle.github.io/opengrok/>`_ to browse through the source code. Opengrok is a source code search and cross-reference engine. 

Thank You
----------

We thank you for stopping by to check out the CORTX Community. Seagate is fully dedicated to its mission to build open source technologies that help the world save unlimited data and solve challenging data problems. Join our mission to help reinvent a data-driven world. 

.. |license| image:: https://img.shields.io/badge/License-Apache%202.0-blue.svg
   :target: https://github.com/Seagate/EOS-Sandbox/blob/master/LICENSE
.. |Codacy Badge| image:: https://api.codacy.com/project/badge/Grade/c099437792d44496b720a730ee4939ce
   :target: https://www.codacy.com?utm_source=github.com&utm_medium=referral&utm_content=Seagate/mero&utm_campaign=Badge_Grade
.. |codacy-analysis-cli| image:: https://github.com/Seagate/EOS-Sandbox/workflows/codacy-analysis-cli/badge.svg
