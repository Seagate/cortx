
# CORTX-S3 Server Quick Start Guide
This guide provides a step-by-step walkthrough for getting you CORTX-S3 Server-ready.

- [1.1 Prerequisites](#1.1-Prerequsites)
  * [1.2 Clone CORTX-S3 Server Repository](#1.2-Clone-the-CORTX-S3-Server-Repository)
    + [1.2.1 Create a local repository](#1.2.1-Create-a-local-repository)
- [1.3 Installing Dependencies](#1.3-Installing-dependencies)
  * [1.4 Code Compilation and Unit Test](#1.4-Code-Compilation-and-Unit-Test)
    + [1.4.1 Perform Unit and System Tests](#1.4.1-Perform-Unit-and-System-Tests)
- [1.5 Test your Build using S3-CLI](#1.5-Test-your-Build-using-S3-CLI)
  * [1.5.1 Test Cases](#1.5.1-Test-Cases)
- [1.6 Test a specific MOTR Version using CORX-S3 Server](#1.6-Test-a-specific-MOTR-Version-using-CORX-S3-Server)

## 1.1 Prerequisites

1. You'll need to set up SSC, Cloud VM, or a local VM on VMWare Fusion or Oracle VirtualBox. To know more, refer to the [LocalVMSetup](LocalVMSetup.md) section.

2. Access codes the right way:
   our CORTX Contributors will refer, clone, contribute, and commit changes via the GitHub server. Tou can access the latest code via [Github](https://github.com/Seagate/cortx). 

3. Follow these steps to access the GitHub server: 
   
    > You'll need to use the root access to access your Git Repository: 
    > 1. You'll need to set the root user password using `sudo passwd`. Enter your password.
    > 2. Type `su -` and enter the root password to switch to the root user mode.

4. Before you clone your Git repository, you'll need to create the following:
    > 1. Follow the link to generate the [SSH Public Key](https://git-scm.com/book/en/v2/Git-on-the-Server-Generating-Your-SSH-Public-Key).
    >   * Add the newly created SSH Public Key to [Github](https://github.com/settings/keys) and [Enable SSO](https://docs.github.com/en/github/authenticating-to-github/authorizing-an-ssh-key-for-use-with-saml-single-sign-on).
    > 2. When you clone your Github repository, you'll be prompted to enter your GitHub Username and Password.  
      >     *  Refer to the article to [Generate Personal Access Token or PAT](https://github.com/settings/tokens). Once you generate your Personal Access Token, enable SSO. 
      >    * Copy your newly generated [PAT](https://github.com/settings/tokens) and enter it when prompted.
       
3. We've assumed that `git` is preinstalled. If not then follow these steps to install [Git](https://github.com/Seagate/cortx/blob/master/doc/ContributingToCortxS3.md). 

    * To check your Git Version, use the command `$ git --version`.
    
    **Note:** We recommended that you install Git Version 2.x.x.

4. Ensure that you've installed the following packages on your VM instance: 
    
  * Python Version 3.0
    >  * To check whether Python is installed on your VM, use one of the following commands: `--version`  , `-V` , or `-VV`
    >  * To install Python version 3.0: `$ yum install -y python3`
  
  * pip version 3.0: 
    > * To verify your pip version: `pip --version` 
    > * To install pip3: `yum install python-pip3` 
       
  * To install ansible:

      `$ yum install -y ansible`
        
   * To install Extra Packages for Enterprise Linux:

      `$ yum install -y epel-release`

  * To verify that kernel-devel-3.10.0-1062 version package is installed: 

      `uname -r`

5. You'll need to disable selinux and firewall. Follow the steps below:
  
    ```
    $ systemctl stop firewalld
    $ systemctl disable firewalld
    $ sestatus
    $ setenforce 0
    sed 's/SELINUX=enforcing/SELINUX=disabled/' /etc/sysconfig/selinux
    
    $ shutdown -r now` - your VM will shut-down. You will need to restart your VM.
    
    To verify if selinux and firewall are disabled on your VM:

    $ getenforce` - you'll get a 'disabled' status.
    ```

6. You'll need to install Lustre. Lustre is an open-source and parallel file system that supports High Performance Computing simulation environments.  

    * To verify whether Lustre is installed on your VM instance:
    
      `$rpm -qa | grep lustre`
  
    * If you haven't installed Lustre on your VM, you'll have to copy the Lustre repository from the server where you've installed MOTR. You'll also need to install the lustre client.
    
      `$ ls -lrt /var/lib/yum/localrepos/lustre-local` 

      ```
      -rw-r--r--. 1 root root 417384 Jul  6 21:04 lustre-client-devel-2.12.4-99.el7.x86_64.rpm
      drwxr-xr-x. 2 root root   4096 Jul  6 21:05 repodata
    
      ```

      `$ ls -lrt /etc/yum.repos.d/lustre-`
      
      ```
      -rw-r--r--. 1 root root 1327 Jul  6 21:03 /etc/yum.repos.d/lustre-whamcloud.repo
      -rw-r--r--. 1 root root  115 Jul  6 21:03 /etc/yum.repos.d/lustre-local.repo

      ```

    * To install Lustre from the cloned repository:
      
      `$ yum install -y lustre*` 

All done! You are now ready for fetching CORTX-S3 Server repository!  


## 1.2 Clone the CORTX-S3 Server Repository

You'll need to clone the S3 Server Repository from the main branch. To clone the S3 Server Repository, follow these steps: 

```
$ git clone --recursive git@github.com:Seagate/cortx-s3server.git -b main   
$ cd cortx-s3server
$ git submodule update --init --recursive && git status
``` 
### 1.2.1 Create a local repository 

If rpms are stored in github release, you can create a local repository on your VM. Follow these steps to create and configure a local repository:

```
$ pip install githubrelease
$ mkdir /root/releases_eos_s3deps
$ cd /root/releases_eos_s3deps
$ GITHUB_TOKEN=<AUTH TOKEN GITHUB>
$ githubrelease --github-token $GITHUB_TOKEN asset seagate/cortx-s3server download $(curl -H "Authorization: token $GITHUB_TOKEN" -s https://api.github.com/repos/Seagate/cortx-s3server/releases/latest | grep "tag_name" | sed -E 's/.*"([^"]+)".*/\1/')`
```

Create the file `/etc/yum.repos.d/releases_eos_s3deps.repo` using the following commands:

> *  `$ createrepo -v /root/releases_eos_s3deps`
> * `$ yum clean all`
> * `$ yum repolist`

You will see the following details once your file is created:
```
[releases_eos_s3deps]
name=Cortx-S3 Repository
baseurl=file:///root/releases_eos_s3deps
gpgcheck=0
enabled=1
```
    
## 1.3 Installing dependencies

**Before you begin:**

1. At some point during the execution of the `init.sh` script, it will prompt for the following passwords. Enter them as mentioned below:

    * SSH password: `<Enter root password of VM>`
    * Enter new password for openldap rootDN:: `seagate`
    * Enter new password for openldap IAM admin:: `ldapadmin`

2. While cloning your repository or in an event where there are changes to the dependent packages, you'll be need to initialize your package on a one-time basis:

   ```
    $ cd ./scripts/env/dev
    $ ./init.sh
    ```

    > * In some cases, the `./init.sh` fails to run. 
    > * If the above command fails, run `./upgrade-enablerepo.sh` and then run `./init.sh`.
  
Refer to the image below to view the output of a successful `./init.sh` run.
The where `failed` field value should be zero.

<p align="center"><img src="../../cortx-s3server/images/init_script_output 1.png? raw=true"></p>

Please refer to our [FAQs](https://github.com/Seagate/cortx/blob/master/doc/Build-Installation-FAQ.md) for troubleshooting errors.

## 1.4 Code Compilation and Unit Test

**Before you begin**

You'll need to run the following commands from the main source directory.

### 1.4.1 Perform Unit and System Tests

> Before you test your build, you'll need to setup the host system using the command:
> `$ ./update-hosts.sh`

 1. Run the `jenkins-build.sh` script.
    * The above script automatically builds the code, and runs the unit & system tests in your local system. 
    * Check help for more details.  
2. If the `/usr/local/bin` does not exist, you'll need to add the path using:  

    `$PATH=$PATH:/usr/local/bin`
  
   * The image below illustrates the output log of a system test that is successful.
  
<p align="center"><img src="../../cortx-s3server/images/init_script_output 2.png? raw=true"></p>

## 1.5 Test your Build using S3-CLI

**Before you begin:**

Before your test your build, ensure that you have installed and configured the following:

1. Make sure you have installed `easy_install`.

    * To check if you have `easy_install`, run the command: 
   
      `$ easy_install --version`
    * To install `easy_install`, run the command: 
    
      `$ yum install python-setuptools python-setuptools-devel`
2. Ensure you've installed `pip`.
    
    * To check if you have pip installed, run the command: 
    
      `$ pip --version`
    
    * To install pip, run the command: 
    
      `$ python --version`

3. If you don't have Python Version 2.6.5+, then install python using: 

      `$ python3 --version`.    

    *  If you don't have Python Version 3.3, then install python3 using:

        `$ easy_install pip`

4. Ensure that CORTX-S3 Server and its dependent services are running.

    1. To start CORTX-S3 Server and its dependent services, run the command:
        
         `$ ./jenkins-build.sh --skip_build --skip_tests` 

    2. To view the `PID` of the active S3 service, run the command:
      
        `$ pgrep s3` 

    3. To view the `PID` of the active Motr service, run the command: 
    
        `$ pgrep m0`

5. Install the aws client and plugin

    1. To install aws client, use:
          
          `$ pip install awscli`

      2. To install the aws plugin, use:
      
          `$ pip install awscli-plugin-endpoint`
  
      3. To generate the aws Access Key ID and aws Secret Key, run commands:
          
         1. To check for help messages, run the command:
          
            `$ s3iamcli -h`
            
          2. Run the following command to create a new User:
          
              `$ s3iamcli CreateAccount -n < Account Name > -e < Email Id >` 
          
              *   Enter the following ldap credentials:
            
                  User Id : `sgiamadmin`
          
                  Password : `ldapadmin`
               
              > * Running the above command lists details of the newly created user including the `aws Access Key ID` and the `aws Secret Key`. 
              > * Copy and save the Access and Secret Keys for the new user. 
  
6. To Configure AWS run the following commands:

    **Before you begin:**
    
    You'll need to keep the Access and Secret Keys generated in Step - 3.2 handy. 

   1.  Run `$ aws configure` and enter the following details:

        * `AWS Access Key ID [None]: < ACCESS KEY >`

        * `AWS Secret Access Key [None]: < SECRET KEY >`

        * `Default region name [None]: US`

        * `Default output format [None]: text`

    2. Configure the aws plugin Endpoint using:

        * `$ aws configure set plugins.endpoint awscli_plugin_endpoint`

        * `$ aws configure set s3.endpoint_url http://s3.seagate.com`
        
        *  `$ aws configure set s3api.endpoint_url http://s3.seagate.com`
        
        * Run the following command to view the contents of your aws config file: 

          `$ cat ~/.aws/config`

        * The output is as shown below:

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
          
    3. Ensure that your aws credential file contains your Access Key Id and Secret Key by using: 

        `$ cat ~/.aws/credentials`

### 1.5.1 Test Cases

Run the following test cases to check if your aws S3 Server build is working properly.

1. To Make a Bucket:

    `$ aws s3 mb s3://seagatebucket` 
    
    You will get the following output: 
  
    `make_bucket: seagatebucket`

2. To List your newly created Bucket:

    `$ aws s3 ls`

3. To Copy your local file (test_data) to remote (PUT):
  
    `$ aws s3 cp test_data s3://seagatebucket/`

   This will create a test_data object in your bucket. You can use any file for to test this step. 
   
   If you want to create a test_data file, use the command:

    `touch filepath/test_data`

4. To Move your local file to remote (PUT):

    `$ aws s3 mv test_data s3://seagatebucket/` 
    
    This command moves your local file test_data to the bucket and create a test_data object. 

5. To List your moved object:

    `$ aws s3 ls s3://seagatebucket`
    
6. To Remove an object:

    `$ aws s3 rm s3://seagatebucket/test_data` 
    
    You'll not be able to view the object when you list objects.

7. To Remove Bucket:
    
    `$ aws s3 rb s3://seagatebuckettest`

## 1.6 Test a specific MOTR Version using CORX-S3 Server

Let's say there is a version change in the Motr repository, and you want to skip re-installing the S3 Server. You can do so by using specific Motr commits and test the same on your S3 Server.

You'll need to copy the commit-id of your Motr code. You can search for specific commit-ids using:

`git log`

While viewing the log, to find the next commit, type `/^commit`, then use `n` and `N` to move to the next or previous commit. To search for the previous commit, use `?^commit`.

**Before you begin**

You'll need to work out of the main directory of your S3 Server repository.


1. Run `$ cd third_party/motr`. 

2. Paste the commit-id shown below:
   
   `git checkout Id41cd2b41cb77f1d106651c267072f29f8c81d0f`
   
3. Update your submodules:

    `$ git submodule update --init --recursive`

4. Build Motr:

    `cd ..`
    
    `./build_motr.sh` 

6. Run the jenkins script to make sure that build and test is passed:

    `cd ..`

    `./jenkins-build.sh`

    Your success log will look like the output in the image below:

<p align="center"><img src="../../cortx-s3server/images/jenkins_script_output 3.png? raw=true"></p>

## You're all set & you're awesome!

In case of any queries, feel free to reach out to our [SUPPORT](SUPPORT.md) team.

Contribute to Seagate's open-source initiative and join our movement to make data storage better, efficient, and more accessible.

Seagate CORTX Community Welcomes You! :relaxed:
