# Cortx-SSPL QuickStart guide
This is a step by step guide to get Cortx-SSPL ready for you on your system.
Before cloning, however, you need to have an SSC / Cloud VM or a local VM setup in either VMWare Fusion or Oracle VirtualBox [LocalVMSetup](LocalVMSetup.md).If you prefer video, here is a [link to a video](https://seagatetechnology.sharepoint.com/:v:/s/gteamdrv1/tdrive1224/EZbJ5AUWe79DksiRctCtsnUB9sILRr5DqHeBzdrwzNNg6w?e=Xamvex) produced by Seagate engineer Puja Mudaliar following these instructions.

## Accessing the source code right way
(For phase 1) Latest code which is getting evolved, advancing and contributed is on the current github server.
Seagate contributor will be referencing, cloning and committing code to/from this [Github](https://github.com/Seagate/).

Following steps as sudo user(sudo -s) will make your access to server hassel free.

1. Create SSH Public Key
    * [SSH generation](https://git-scm.com/book/en/v2/Git-on-the-Server-Generating-Your-SSH-Public-Key) will make your key generation super easy. follow the instructions throughly.

2. Add New SSH Public Key on [Github](https://github.com/settings/keys) and [Enable SSO](https://docs.github.com/en/github/authenticating-to-github/authorizing-an-ssh-key-for-use-with-saml-single-sign-on).

WoW! :sparkles:
You are all set to fetch cortx-sspl repo now.


## Cloning cortx-sspl repository
1. `$ git clone --recursive git@github.com:Seagate/cortx-sspl.git` (It has been assumed that `git` is preinstalled. if not then follow git installation specific steps provided [here](https://github.com/Seagate/cortx/blob/master/doc/ContributingToCortxSSPL.md). Recommended git version is 2.x.x . Check your git version using `$ git --version` command.)

2. `$ cd cortx-sspl`


## Prerequisites
1. Setup yum repo.
    * `$ curl http://gitlab.mero.colo.seagate.com/eos/provisioner/ees-prvsnr/raw/Cortx-v1.0.0_Beta/cli/src/cortx-prereqs.sh -o cortx-prereqs.sh; chmod a+x cortx-prereqs.sh; ./cortx-prereqs.sh --disable-sub-mgr`
    * `$ BUILD_URL="http://cortx-storage.colo.seagate.com/releases/eos/github/dev/rhel-7.7.1908/provisioner_last_successful"`
    * `$ yum-config-manager --add-repo  $BUILD_URL`
    * `$ rpm --import  $BUILD_URL/RPM-GPG-KEY-Seagate`

2. Please make sure python3,pip and kernel-devel-3.10.0-1062 packages are installed on the VM.
    * `$ yum install -y python36`


## Building the Cortx-sspl source code

1. Before building RPM, install necessaries dependencies
    * `$ yum install rpm-build`
    * `$ yum install autoconf automake libtool check-devel doxygen gcc graphviz openssl-devel python-pep8`

2. Build RPMS
    * Go to the directory where cortx-sspl is cloned.
    * `$ jenkins/build.sh`
    * You can see generated RPMS at **/root/rpmbuild/RPMS/noarch/**

3. Install cortx-sspl RPMS
    - Create repository and copy all required rpms there.
    * mkdir MYRPMS
    * cp -R /root/rpmbuild/RPMS/x86_64/cortx-libsspl_sec-* /root/rpmbuild/RPMS/noarch/cortx-sspl-* MYRPMS
    * find MYRPMS -name \*.rpm -print0 | sudo xargs -0 yum install -y

4. Start SSPL service
    * `/opt/seagate/cortx/sspl/sspl_init`
    * `systemctl start sspl-ll`
    * `systemctl status sspl-ll`
    Service status should be **Active: active (running)**


## Running Tests
Make sure *cortx-sspl-1.0.0-XXXX.el7.noarch.rpm* and *cortx-sspl-test-1.0.0-XXXX.el7.noarch.rpm* rpms are installed and sspl service is in Active state.

Runnning sanity test
  * `/opt/seagate/cortx/sspl/bin/sspl_test sanity` or `/opt/seagate/cortx/sspl/sspl_test/run_tests.sh`

### You're all set & You're awesome

In case of any queries, feel free to write to our [SUPPORT](SUPPORT.md).

Let's start without a delay to contribute to Seagate's open source initiative and join this movement with us, keeping a common goal of making data storage better, more efficient and more accessible.

Seagate welcomes You! :relaxed:
