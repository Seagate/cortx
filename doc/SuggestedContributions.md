SUGGESTED CONTRIBUTIONS
=======================
Thank you for your interest in participating in our community and helping us achieve our vision to produce open source software designed to reduce storage costs and help the world save the Datasphere!

This file contains information about the different ways to contribute to the CORTX Community as well as a list of specific suggested contributions.  In order to ensure that CORTX remains open and redistributable under our current licensing, we do require DCO sign-off for all contributions and may request CLA for large contributions.  Please read more about our DCO/CLA policy [here](doc/dco_cla.md).

GENERAL WAYS TO CONTRIBUTE
--------------------------
There are many ways to participate in our community.
1. Participate in discussion forums
2. Create GitHub Issues requesting features
3. Create GitHub Issues reporting bugs in the code
4. Create GitHub Issues reporting missing, incomplete, or incorrect documentation
5. Create GitHub Pull Requests improving documentation
6. Create GitHub Pull Requests fixing bugs
7. Create GitHub Pull Requests adding third-party integrations
8. Create GitHub Pull Requests adding new features to the code
9. Test CORTX performance• Test CORTX scalability
10. Test CORTX interoperability across a range of hardware platforms (both physical and virtual)
11. Test CORTX security via penetration testing and other mechanisms
12. Create GitHub Pull Requests adding new test frameworks
13. Write content such as solution briefs, reference architectures, recipes, and benchmarks
14. Improve automation, devops, CI/CD, etc.
15. Anything that improves the CORTX Community!

Note that not all of these require actively committing new code or documentation to the repositories but in general we prefer that all of them do result in code or documentation commits.  For example, if you participate in an important QA session in one of our discussion forums, this suggests that our documentation is somewhere lacking.  Please take what you learn from that QA session and improve our documentation so that the next visitor to our community won't have the same question.

SPECIFIC SUGGESTED CONTRIBUTIONS
--------------------------------
Here is a list of our current ideas about useful specific contributions that you can make.  Some of them have the name of a CORTX Team member who can help with additional details.
* Improve the documentation 
  * You can either improve the existing documentation by making it more clear or addressing possible error cases or you can convert existing documents from binary formats into text formats like [reStructuredText](https://docutils.sourceforge.io/rst.html) or [MarkDown](https://www.markdownguide.org/).  In general, please use reStructuredText for most documents and only use MarkDown for very simple pages which are not expected to grow in the future.
  * Click [here](SuggestedDocumentConversions.md) for a list of documents needing conversion and instructions about how to do it
* Fix bugs 
  * TODO: How can we share our bug backlog with innersource community? 
* FDMI extension 
  * Team Member: Ganesan
* Stress testing 
* Performance testing of kvs 
  * Team Member: John Bent
* IOR integration with clovis 
  * Team Member: John Bent
* Fio with clovis 
* Use of locks with clovis 
* Performance impact of lock usage 
* Test complete path with net socket transport 
* Stabilization of net socket transport 
* Performance with net socket transport 
* S3server/scripts/env/dev/init.sh 
  * [DEPRECATION WARNING]: Invoking "yum" only once while using a loop via squash_actions is deprecated. Instead of using a loop to supply multiple items and specifying `pkg: "{{item}}"`, please use `pkg: ['openldap-servers', 'openldap-clients']` and remove the loop. This feature will be removed in version 2.11. Deprecation warnings can be disabled by setting deprecation_warnings=False  in ansible.cfg. 
* dstat performance plugins 
  * Team Member: Anatoliy
* Data-, metadata- path flows visualization tools. 
  * Team Member: Anatoliy
* BErta(BE run time analysis) – metadata traversal python scripts used for debugging, visualization of metadata patterns and etc.
  * Team Member: Anatoliy
* Make chronometry/addb tools to be useable for other people. Decreases slope of the Mero learning curve a lot. 
  * Team Member: Anatoliy
* Chronometry/addb provements (at least Pearson Correlation) and Machine learning to identify abnormal behavior (for ex: some layouts may produce irregular network load pattern). 
  * Team Member: Anatoliy
* FSCK for a single node. 
  * Team Member: Anatoliy
* Expose low level block allocation algorithm interfaces and apply own allocation strategy. Will be useful for research, in production will hit “updates” case. 
  * Team Member: Anatoliy
* Expose low level btree (KVS) interfaces. Will be useful for research, in production will hit “updates” case. 
  * Team Member: Anatoliy
* System Performance model. 
  * Team Member: Anatoliy
* System HA model. 
  * Team Member: Anatoliy
* Simplify build process: install dependencies automatically 
  * Team Member: Nikita
* Simplify deployment: run in a “demo” mode on a single node without root access and without real storage devices (files or loop devices) 
  * Team Member: Nikita
* See doc/todo in the mero repository 
  * Team Member: Nikita
* Get it working on Windows Subsystem Linux.  Add documentation and a downloadable VM image. 
* Improve CI/CD and other automated tasks.  When you do these, please document them in docs/CI_CD.md. 
* Add documentation for testing CORTX in a scale-out cluster. 
* Make it easier to build and test CORTX across a variety of kernel versions and Linux distributions and document this. 
* Replace LNet which requires kernel and results in many build dependencies and complexities with [DAOS Cart](https://github.com/daos-stack/cart) which runs in user-space
  * Team Member: John Bent


