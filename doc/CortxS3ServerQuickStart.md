# Cortx-S3Server QuickStart guide
This is a step by step guide to get Cortx-S3Server ready for you on your system.
Before cloning, however, you need to have an SSC / Cloud VM or a local VM setup in either VMWare Fusion or Oracle VirtualBox [LocalVMSetup](LocalVMSetup.md).

## Accessing the code right way

(For phase 1) The latest code which is getting evolved and contributed is on the Github server.
CORTX Contributors will be referencing, cloning and committing their code to/from this [Github](https://github.com/Seagate/cortx).

Following steps will make your access to server hassle free.
1. From here on all the steps needs to be followed as the root user.
  * Set the root user password using `sudo passwd` and enter the required password.
  * Type `su -` and enter the root password to switch to the root user mode.
2. Create SSH Public Key
  * [SSH generation](https://git-scm.com/book/en/v2/Git-on-the-Server-Generating-Your-SSH-Public-Key) will make your key generation super easy. Follow the instructions throughly.
3. Add New SSH Public Key on [Github](https://github.com/settings/keys) and Enable SSO.

WoW! :sparkles:

You are all set to fetch Cortx-S3Server repo now! 

## Cloning S3Server Repository

1. `$ git clone --recursive git@github.com:Seagate/cortx-s3server.git -b main`  Note:If username prompted than enter github username and for password copy from [PAT](https://github.com/settings/tokens) or generate a new one using [Generate PAT](https://github.com/settings/tokens) and enable SSO ( It has been assumed that `git` is preinstalled. If not then follow git installation specific steps provided [here](https://github.com/Seagate/cortx/blob/master/doc/ContributingToCortxS3.md). Recommended git version is 2.x.x . Check your git version using `$ git --version` command.) 
2. `$ cd cortx-s3server`
3. `$ git submodule update --init --recursive && git status`

## Prerequisites
1. Please make sure python3,pip,ansible and kernel-devel-3.10.0-1062 packages are installed on the VM.
   * `$ yum install epel-release`
   * `$ yum install -y python3`
   * `$ yum install -y ansible`
2. Disable selinux and firewall
   * `$ systemctl stop firewalld`
   * `$ systemctl disable firewalld`
   * `$ sestatus`
   * `$ setenforce 0`
   * `$ sed 's/SELINUX=enforcing/SELINUX=disabled/' /etc/sysconfig/selinux`
   * `$ shutdown -r now`
   * `$ getenforce` (It should show disabled)
   
## Create a local repository 
1. Create and configure a local repository if rpms are stored in github release.
   * `$ mkdir /root/releases_eos_s3deps`
   * `$ cd /root/releases_eos_s3deps`
   * `$ GITHUB_TOKEN=<AUTH TOKEN GITHUB>`
   * `$ githubrelease --github-token $GITHUB_TOKEN asset seagate/cortx-s3server download $(curl -H "Authorization: token $GITHUB_TOKEN" -s https://api.github.com/repos/Seagate/cortx-s3server/releases/latest | grep "tag_name" | sed -E 's/.*"([^"]+)".*/\1/')`
   * `$ cat /etc/yum.repos.d/releases_eos_s3deps.repo`
       ```
       [releases_eos_s3deps]
       name=Cortx-S3 Repository
       baseurl=file:///root/releases_eos_s3deps
       gpgcheck=0
       enabled=1
   * `$ createrepo -v /root/releases_eos_s3deps`
   * `$ yum clean all`
   * `$ yum repolist`
   
 
  
## Install lustre if not available
1. Copy lustre repository from a server where MOTR is installed and install the lustre client.
* `$ ls -lrt /var/lib/yum/localrepos/lustre-local`
  ```
  -rw-r--r--. 1 root root 417384 Jul  6 21:04 lustre-client-devel-2.12.4-99.el7.x86_64.rpm
  drwxr-xr-x. 2 root root   4096 Jul  6 21:05 repodata
* `$ ls -lrt /etc/yum.repos.d/lustre-*`
  ```
  -rw-r--r--. 1 root root 1327 Jul  6 21:03 /etc/yum.repos.d/lustre-whamcloud.repo
  -rw-r--r--. 1 root root  115 Jul  6 21:03 /etc/yum.repos.d/lustre-local.repo
<<<<<<< HEAD
* `$ yum install -y lustre* --skip-broken` 
>>>>>>> Update S3ServerQuickStart.md


## Prerequisites
1. Please make sure python3,pip,ansible and kernel-devel-3.10.0-1062 packages are installed on the VM.
   * `$ yum install -y epel-release`
   * `$ yum install -y python3`
   * `$ yum install -y ansible`
2. Disable selinux and firewall
   * `$ systemctl stop firewalld`
   * `$ systemctl disable firewalld`
   * `$ sestatus`
   * `$ setenforce 0`
   * `$ sed 's/SELINUX=enforcing/SELINUX=disabled/' /etc/sysconfig/selinux`
   * `$ shutdown -r now`
   * `$ getenforce` (It should show disabled)
   
## Create a local repository 
1. Create and configure a local repository if rpms are stored in github release.
   * `$ pip install githubrelease`
   * `$ mkdir /root/releases_eos_s3deps`
   * `$ cd /root/releases_eos_s3deps`
   * `$ GITHUB_TOKEN=<AUTH TOKEN GITHUB>`
   * `$ githubrelease --github-token $GITHUB_TOKEN asset seagate/cortx-s3server download $(curl -H "Authorization: token $GITHUB_TOKEN" -s https://api.github.com/repos/Seagate/cortx-s3server/releases/latest | grep "tag_name" | sed -E 's/.*"([^"]+)".*/\1/')`
   * `$ cat /etc/yum.repos.d/releases_eos_s3deps.repo`
       ```
       [releases_eos_s3deps]
       name=Cortx-S3 Repository
       baseurl=file:///root/releases_eos_s3deps
       gpgcheck=0
       enabled=1
   * `$ createrepo -v /root/releases_eos_s3deps`
   * `$ yum clean all`
   * `$ yum repolist`
   
 
  
## Install lustre if not available
1. Copy lustre repository from a server where MOTR is installed and install the lustre client.
* `$ ls -lrt /var/lib/yum/localrepos/lustre-local`
  ```
  -rw-r--r--. 1 root root 417384 Jul  6 21:04 lustre-client-devel-2.12.4-99.el7.x86_64.rpm
  drwxr-xr-x. 2 root root   4096 Jul  6 21:05 repodata
* `$ ls -lrt /etc/yum.repos.d/lustre-*`
  ```
  -rw-r--r--. 1 root root 1327 Jul  6 21:03 /etc/yum.repos.d/lustre-whamcloud.repo
  -rw-r--r--. 1 root root  115 Jul  6 21:03 /etc/yum.repos.d/lustre-local.repo
* `$ yum install -y lustre*` 

=======
* `$ yum install -y lustre*` 
>>>>>>> Update S3ServerQuickStart.md

## Installing dependency
This is a one time initialization when we do clone the repository or there is a changes in dependent packages.

  * At some point during the execution of the `init.sh` script, it will prompt for the following passwords. Enter them as mentioned below.
    * SSH password: `<Enter root password of VM>`
    * Enter new password for openldap rootDN:: `seagate`
    * Enter new password for openldap IAM admin:: `ldapadmin`

1. `$ cd ./scripts/env/dev`
2. `$ ./init.sh`, For some system `./init.sh` fails sometimes. If it is failing run `./upgrade-enablerepo.sh` and re run `./init.sh`. Refer below image of successful run of `./init.sh` where `failed` field should be zero.For any other errors, please refer [FAQs](Link in PR state)

<p align="center"><img src="../../assets/images/init_script_output.PNG?raw=true"></p>

## Compilation and Running Unit Test
All the following commands assume that user is already in its main source directory.
### Running Unit test and System test
1. Setup the host system
  * `$ ./update-hosts.sh`
2. Following script by default will build the code, run the unit test and system test in your local system. Check for help to get more details.  
  * `$ ./jenkins-build.sh`. 
  * You may have to add `/usr/local/bin` to PATH variable using command `$PATH=$PATH:/usr/local/bin` if it is not there already.
  
  Make sure the output log has a message as shown in below image to ensure successful execution of system test in `./jenkins-build.sh`.
  
<p align="center"><img src="../../assets/images/jenkins_script_output.PNG?raw=true"></p>

### Testing using S3CLI
1. Installation and configuration
  * Make sure you have `easy_install` installed using `$ easy_install --version`. If it is not installed run the following command.
    * `$ yum install python-setuptools python-setuptools-devel`
  * Make sure you have `pip` installed using `$ pip --version`. If it is not installed run the following command.
    * `$ python --version`, if you don't have python version 2.6.5+ then install python.
    * `$ python3 --version`, if you don't have python3 version 3.3+ then install python3.
    * `$ easy_install pip`
  * Make sure Cortx-S3Server and it's dependent services are running.
    * `$ ./jenkins-build.sh --skip_build --skip_tests` so that it will start Cortx-S3Server and it's dependent services.
    * `$ pgrep s3`, it should list the `PID` of S3 processes running.
    * `$ pgrep mero`, it should list the `PID` of mero processes running. (Note: Need changes if required pgrep mero or pgrep motr ?)
  * Install aws client and it's plugin
    * `$ pip install awscli`
    * `$ pip install awscli-plugin-endpoint`
  * Generate aws access key id and aws secret key
    * `$ s3iamcli -h` to check help messages.
    * `$ s3iamcli CreateAccount -n < Account Name > -e < Email Id >` to create new user. Enter the ldap `User Id` and `password` as mentioned below. Along with other details it will generate `aws access key id` and `aws secret key` for new user, make sure you save those.
      * Enter Ldap User Id: `sgiamadmin`
      * Enter Ldap password: `ldapadmin`
  * Configure AWS
    * `$ aws configure` and enter the following details.
      * AWS Access Key ID [None]: < ACCESS KEY >
      * AWS Secret Access Key [None]: < SECRET KEY >
      * Default region name [None]: US
      * Default output format [None]: text
  * Set Endpoint
    * `$ aws configure set plugins.endpoint awscli_plugin_endpoint`
    * `$ aws configure set s3.endpoint_url http://s3.seagate.com`
    * `$ aws configure set s3api.endpoint_url http://s3.seagate.com`
  * Make sure aws config file has following content
    * `$ cat ~/.aws/config`
  ```
  [default]
  output = text
  region = US
  s3 =
      endpoint_url = http://s3.seagate.com
  s3api =
      endpoint_url = http://s3.seagate.com
  [plugins]
  endpoint = awscli_plugin_endpoint
  ```
  * Make sure aws credential file has your access key Id and secret key.
    * `$ cat ~/.aws/credentials`
2. Test cases
  * Make Bucket
    * `$ aws s3 mb s3://seagatebucket`, should be able to get following output on the screen
      * `make_bucket: seagatebucket`
  * List Bucket
    * `$ aws s3 ls`, should be able to see the bucket we have created.
  * Remove Bucket
    * `$ aws s3 rb s3://seagatebucket`, bucket should get remove and should not be seen if we do list bucket.
  * Copy local file to remote(PUT)
    * `$ aws s3 cp test_data s3://seagatebucket/`, will copy the local file test_data(use any file for test purpose,in this case we assume there is a file name test_data) and test_data object will be created under bucket.
  * List Object
    * `$ aws s3 ls s3://seagatebucket`, will show the object named as test_data
  * Move local file to remote(PUT)
    * `$ aws s3 mv test_data s3://seagatebucket/`, will move the local file test_data and test_data object will be created under bucket.
  * Remove object 
    * `$ aws s3 rm  s3://seagatebucket/test_data`, the test_data object should be removed and it should not be seen if we do list object.

KABOOM!!!

<<<<<<< HEAD
## Testing specific MOTR version with Cortx-S3Server

=======
## Testing specific Motr version with S3Server
>>>>>>> Update S3ServerQuickStart.md
For this demand also we are having solution :

1. Get desired mero commit HASH *or* commit REFSPECS on clipboard (you'll be asked to paste in step 4)
* To get REFSPECS

 > Search your desired commit [here](http://gerrit.mero.colo.seagate.com/q/project:mero+branch:innersource+status:open) (Note: This link should change to github)
 
 > Go to desired commit and then click *Download* button and copy the highlighted part(which is your REFSPECS actually) as shown below. (Note: Images below need to change to point to github)
  
  <p align="center"><img src="../../assets/images/mero_Refspecs.JPG?raw=true"></p>
  
2. `$ cd third_party/mero` (It is assumed that you are into main directory of your s3server repo) (Note: cd third_party/motr ?)
3. Use copied commit HASH/REFSPEC in step 1 as shown below.
   
 > git checkout Id41cd2b41cb77f1d106651c267072f29f8c81d0f
   
 or
   
 > git pull "http://gerrit.mero.colo.seagate.com/mero" refs/changes/91/19491/4  (Note: git pull "https://github.com/Seagate/cortx-motr.git")

4. Update submodules 
> `$ git submodule update --init --recursive`
5. Build Motr

> `cd ..`

<<<<<<< HEAD
> `./build_mero.sh` 

6. Run jenkins script to make sure that your build & tests passes.

> `cd ..`

> `./jenkins-build.sh`

* success logs looks like this :point_down:

<p align="center"><img src="../../assets/images/jenkins_script_output.PNG?raw=true"></p>

### You're all set & You're awesome

In case of any queries, feel free to write to our [SUPPORT](SUPPORT.md).

Let's start without a delay to contribute to Seagate's open source initiative and join this movement with us, keeping a common goal of making data storage better, more efficient and more accessible.

Seagate welcomes You! :relaxed:



