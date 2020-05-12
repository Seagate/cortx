# S3Server QuickStart guide
This is a step by step guide to get S3Server ready for you on your system.
Before cloning, however, you need to have an SSC / Cloud VM or a local VM setup in either VMWare Fusion or Oracle VirtualBox [LocalVMSetup](LocalVMSetup.md).

## Accessing the code right way
(For phase 1) The latest code which is getting evolved and contributed is on the gerrit server.
Seagate contributors will be referencing, cloning and committing their code to/from this [Gerrit server](http://gerrit.mero.colo.seagate.com:8080).

To simply pull the code in which to build `git clone --recursive "http://gerrit.mero.colo.seagate.com/s3server" -b innersource`

Following steps will make your access to server hassle free.
1. From here on all the steps needs to be followed as the root user.
  * Set the root user password using `sudo passwd` and enter the required password.
  * Type `su -` and enter the root password to switch to the root user mode.
2. Create SSH Public Key
  * [SSH generation](https://git-scm.com/book/en/v2/Git-on-the-Server-Generating-Your-SSH-Public-Key) will make your key generation super easy. Follow the instructions throughly.
3. Add SSH Public Key on [Gerrit server](http://gerrit.mero.colo.seagate.com:8080).
  * Log into the gerrit server with your seagate gid based credentials.
  * On right top corner you will see your name, open drop down menu by clicking and choose settings.
  * In the menu on the left, click on the SSH Public Keys, and add your public key (which is generated in step one) right there.

WoW! :sparkles:
You are all set to fetch S3Server repo now! 

## Cloning S3Server Repository
Getting the main S3Server code on your system is straightforward.
1. `$ cd path/to/your/dev/directory`
2. `$ export GID=<your_seagate_GID>` # this will make subsequent steps easy to copy-paste :)
3. `$ git clone "ssh://g${GID}@gerrit.mero.colo.seagate.com:29418/s3server" -b innersource` ( It has been assumed that `git` is preinstalled. If not then follow git installation specific steps provided [here](#getting-git--gerit-to-work). Recommended git version is 2.x.x . Check your git version using `$ git --version` command.)
4. Enable some pre-commit hooks required before pushing your changes to remote.
  * `$ scp -p -P 29418 g${GID}@gerrit.mero.colo.seagate.com:hooks/commit-msg "s3server/.git/hooks/"`
    
    if permission denied, then do the following
    
    `$ chmod 600 /root/.ssh/id_rsa`

5. `$ cd s3server`
6. `$ git submodule update --init --recursive && git status`

## Installing dependency
This is a one time initialization when we do clone the repository or there is a changes in dependent packages.

  * At some point during the execution of the `init.sh` script, it will prompt for the following passwords. Enter them as mentioned below.
    * SSH password: `XYRATEX`
    * Enter new password for openldap rootDN:: `seagate`
    * Enter new password for openldap IAM admin:: `ldapadmin`

1. `$ cd ./scripts/env/dev`
2. `$ ./init.sh`, For some system `./init.sh` fails sometimes. If it is failing run `./upgrade-enablerepo.sh` and re run `./init.sh`. Refer below image of successful run of `./init.sh` where `failed` field should be zero.

<p align="center"><img src="../../assets/images/init_script_output.PNG?raw=true"></p>

## Compilation and Running Unit Test
All the following commands assume that user is already in its main source directory.
### Running Unit test and System test
1. Setup the host system
  * `$ ./update-hosts.sh`
2. Following script by default will build the code, run the unit test and system test in your local system. Check for help to get more details.  
  * `$ ./jenkins-build.sh`. If you face any issue with clang-format, recommended git version and clang-format needs to be installed. Do it from [here](#getting-git--gerit-to-work).
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
  * Make sure S3Server and it's dependent services are running.
    * `$ ./jenkins-build.sh --skip_build --skip_tests` so that it will start S3Server and it's dependent services.
    * `$ pgrep s3`, it should list the `PID` of S3 processes running.
    * `$ pgrep mero`, it should list the `PID` of mero processes running.
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
    * `$ aws s3 cp test_data s3://seagatebucket/`, will copy the local file test_data and test_data object will be created under bucket.
  * List Object
    * `$ aws s3 ls s3://seagatebucket`, will show the object named as test_data
  * Move local file to remote(PUT)
    * `$ aws s3 mv test_data s3://seagatebucket/`, will move the local file test_data and test_data object will be created under bucket.
  * Remove object 
    * `$ aws s3 rm  s3://seagatebucket/test_data`, the test_data object should be removed and it should not be seen if we do list object.

KABOOM!!!

## Testing specific MERO version with S3Server
For this demand also we are having solution :

1. Get desired mero commit HASH *or* commit REFSPECS on clipboard (you'll be asked to paste in step 4)
* To get REFSPECS

 > Search your desired commit [here](http://gerrit.mero.colo.seagate.com/q/project:mero+branch:innersource+status:open)
 
 > Go to desired commit and then click *Download* button and copy the highlighted part(which is your REFSPECS actually) as shown below. 
  
  <p align="center"><img src="../../assets/images/mero_Refspecs.JPG?raw=true"></p>
  
2. `$ cd third_party/mero` (It is assumed that you are into main directory of your s3server repo)
3. Use copied commit HASH/REFSPEC in step 1 as shown below.
   
 > git checkout Id41cd2b41cb77f1d106651c267072f29f8c81d0f
   
 or
   
 > git pull "http://gerrit.mero.colo.seagate.com/mero" refs/changes/91/19491/4

4. Update submodules 
> `$ git submodule update --init --recursive`
5. Build mero

> `cd ..`

> `./build_mero.sh`

6. Run jenkins script to make sure that your build & tests passes.

> `cd ..`

> `./jenkins-build.sh`

* success logs looks like this :point_down:

<p align="center"><img src="../../assets/images/jenkins_script_output.PNG?raw=true"></p>

### You're all set & You're awesome

In case of any queries, feel free to write to our [SUPPORT](doc/SUPPORT.md).

Let's start without a delay to contribute to Seagate's open source initiative and join this movement with us, keeping a common goal of making data storage better, more efficient and more accessible.

Seagate welcomes You! :relaxed:

