# Cortx-SSPL QuickStart guide
This is a step by step guide to get Cortx-SSPL ready for you on your system.
Before cloning, however, you need to have an SSC / Cloud VM or a local VM setup in either VMWare Fusion or Oracle VirtualBox [LocalVMSetup](LocalVMSetup.md).

## Accessing the source code right way
(For phase 1) Latest code which is getting evolved, advancing and contributed is on the current github server.
Seagate contributor will be referencing, cloning and committing code to/from this [Github](https://github.com/Seagate/).

Following steps as sudo user(sudo -s) will make your access to server hassel free.

1. Create and Add SSH Public Key:
    * For creating and adding SSH key please follow refer "Github setup" section from [ContributingToSSPL](https://github.com/Seagate/cortx/blob/master/doc/ContributingToSSPL.md#GitHub-setup)
    * You can also refer [SSH generation](https://git-scm.com/book/en/v2/Git-on-the-Server-Generating-Your-SSH-Public-Key).

WoW! :sparkles:
You are all set to fetch cortx-sspl repo now.


## Prerequisites
1. Setup yum repos.
    * `$ curl https://raw.githubusercontent.com/Seagate/cortx-prvsnr/dev/cli/src/cortx-prereqs.sh?token=APA75GY34Y2F5DJSOKDCZAK7ITSZC -o cortx-prereqs.sh; chmod a+x cortx-prereqs.sh`
    * For Cent-OS VMs : `$ sh cortx-prereqs.sh`, For Rhel VMs: `$ sh cortx-prereqs.sh --disable-sub-mgr`
    * `$ BUILD_URL="http://cortx-storage.colo.seagate.com/releases/eos/github/dev/rhel-7.7.1908/provisioner_last_successful"`
    * `$ yum-config-manager --add-repo  $BUILD_URL`
    * `$ rpm --import  $BUILD_URL/RPM-GPG-KEY-Seagate`

    Note: If "https://raw.githubusercontent.com/Seagate/cortx-prvsnr/dev/cli/src/cortx-prereqs.sh?token=APA75GY34Y2F5DJSOKDCZAK7ITSZC" link is not accessible, Generate new one - go to https://github.com/Seagate/cortx-prvsnr/blob/dev/cli/src/cortx-prereqs.sh and click on "RAW" tab, copy url and use it for deployment.

2. Please make sure python3,pip and kernel-devel-3.10.0-1062 packages are installed on the VM.
    * `$ yum install -y python36`


## Cloning cortx-sspl repository
1. `$ git clone --recursive git@github.com:Seagate/cortx-sspl.git` (It has been assumed that `git` is preinstalled. if not then follow git installation specific steps provided [here](https://github.com/Seagate/cortx/blob/master/doc/ContributingToSSPL.md). Recommended git version is 2.x.x . Check your git version using `$ git --version` command.)

2. `$ cd cortx-sspl`


## Building the Cortx-sspl source code

1. Before building RPM, install necessaries dependencies
    * `$ yum install rpm-build`
    * `$ yum install autoconf automake libtool check-devel doxygen gcc graphviz openssl-devel python-pep8`

2. Build RPMS
    * Go to the directory where cortx-sspl is cloned.
    * `$ jenkins/build.sh`  #It takes 2-3 minutes to generate RPMS.
    * You can see generated RPMS at **/root/rpmbuild/RPMS/noarch/** and **/root/rpmbuild/RPMS/x86_64/**.

    Example output:
    ~~~
    [root@ssc-vm-c-466 cortx-sspl]# ls -lrt /root/rpmbuild/RPMS/noarch/
    total 59056
    -rw-r--r-- 1 root root 34025516 Aug 18 07:26 cortx-sspl-1.0.0-1_git8907300.el7.noarch.rpm
    -rw-r--r-- 1 root root 16795340 Aug 18 07:27 cortx-sspl-cli-1.0.0-1_git8907300.el7.noarch.rpm
    -rw-r--r-- 1 root root  9643028 Aug 18 07:27 cortx-sspl-test-1.0.0-1_git8907300.el7.noarch.rpm


    [root@ssc-vm-c-466 cortx-sspl]# ls -lrt /root/rpmbuild/RPMS/x86_64/
    total 284
    -rw-r--r-- 1 root root  65968 Aug 18 07:23 systemd-python36-1.0.0-1_git8907300.el7.x86_64.rpm
    -rw-r--r-- 1 root root  60904 Aug 18 07:23 systemd-python36-debuginfo-1.0.0-1_git8907300.el7.x86_64.rpm
    -rw-r--r-- 1 root root   6552 Aug 18 07:27 cortx-libsspl_sec-1.0.0-1_git8907300.el7.x86_64.rpm
    -rw-r--r-- 1 root root  28448 Aug 18 07:27 cortx-libsspl_sec-debuginfo-1.0.0-1_git8907300.el7.x86_64.rpm
    -rw-r--r-- 1 root root   5760 Aug 18 07:27 cortx-libsspl_sec-method_none-1.0.0-1_git8907300.el7.x86_64.rpm
    -rw-r--r-- 1 root root   9592 Aug 18 07:27 cortx-libsspl_sec-method_pki-1.0.0-1_git8907300.el7.x86_64.rpm
    -rw-r--r-- 1 root root 101004 Aug 18 07:27 cortx-libsspl_sec-devel-1.0.0-1_git8907300.el7.x86_64.rpm
    ~~~


3. Install cortx-sspl RPMS

    Create repository and copy all required rpms there.

      * `$ mkdir MYRPMS`
      * `$ cp -R /root/rpmbuild/RPMS/x86_64/cortx-libsspl_sec-* /root/rpmbuild/RPMS/noarch/cortx-sspl-* MYRPMS`
      * `$ find MYRPMS -name \*.rpm -print0 | sudo xargs -0 yum install -y`

4. Start SSPL service
    * `$ /opt/seagate/cortx/sspl/sspl_init`  #It takes 3-4 minutes to complete.
    * `$ systemctl start sspl-ll`
    * `$ systemctl status sspl-ll`
    Service status should be **Active: active (running)**


## Running Tests
  Make sure *cortx-sspl-1.0.0-XXXX.el7.noarch.rpm* and *cortx-sspl-test-1.0.0-XXXX.el7.noarch.rpm* rpms are installed and sspl service is in Active state.

    * `$ rpm -qa | grep cortx-sspl`

    Example output:
    ~~~
    [root@ssc-vm-c-466 cortx-sspl]# rpm -qa | grep cortx-sspl
    cortx-sspl-1.0.0-1_git8907300.el7.noarch
    cortx-sspl-test-1.0.0-1_git8907300.el7.noarch
    ~~~

  * `$ systemctl status sspl-ll`

  Runnning sanity test

    * `$ /opt/seagate/cortx/sspl/bin/sspl_test sanity` or `/opt/seagate/cortx/sspl/sspl_test/run_tests.sh`

    Example output:
    ~~~
    ******************************************************************************************
    TestSuite                                                    Status     Duration(secs)
    ******************************************************************************************
    alerts.node.test_node_disk_actuator                          Pass               10s
    alerts.realstor.test_real_stor_controller_sensor             Pass                4s
    alerts.realstor.test_real_stor_disk_sensor                   Pass                4s
    alerts.realstor.test_real_stor_fan_sensor                    Pass                4s
    alerts.realstor.test_real_stor_fan_actuator                  Pass                4s
    alerts.realstor.test_real_stor_psu_sensor                    Pass                4s
    alerts.realstor.test_real_stor_for_platform_sensor           Pass                6s
    alerts.realstor.test_real_stor_sideplane_expander_sensor     Pass                4s
    alerts.realstor.test_real_stor_disk_actuator                 Pass                4s
    alerts.realstor.test_real_stor_psu_actuator                  Pass                4s
    alerts.realstor.test_real_stor_controller_actuator           Pass                6s
    alerts.realstor.test_real_stor_sideplane_actuator            Pass                4s
    alerts.node.test_node_psu_actuator                           Pass                6s
    alerts.node.test_node_fan_actuator                           Pass                6s
    alerts.node.test_node_bmc_interface                          Pass               25s

    ****************************************************
    TestSuite:15 Tests:17 Passed:17 Failed:0 TimeTaken:108s
    ******************************************************
    ~~~


### You're all set & You're awesome

In case of any queries, feel free to write to our [SUPPORT](SUPPORT.md).

Let's start without a delay to contribute to Seagate's open source initiative and join this movement with us, keeping a common goal of making data storage better, more efficient and more accessible.

Seagate welcomes You! :relaxed:
