# S3Server QuickStart guide
This is a step by step guide to get S3Server ready for you on your system.
Before cloning, however, you have your VMs setup with specifications mentioned in [Virtual Machine](VIRTUAL_MACHINE.md).

## Accessing the code right way
(For phase 1) Latest code which is getting evolved, advancing and contributed is on the gerrit server.
Seagate contributor will be referencing, cloning and committing code to/from this [Gerrit server](http://gerrit.mero.colo.seagate.com:8080).

Following steps will make your access to server hassel free.
1. From here on all the steps needs to be followed as root user.
  * Set the root user password using `sudo passwd` and set the password.
  * Type `su -` and enter root password to switch in to a root user mode.
2. Create SSH Public Key
  * [SSH generation](https://git-scm.com/book/en/v2/Git-on-the-Server-Generating-Your-SSH-Public-Key) will make your key generation super easy. follow the instructions throughly.
3. Add SSH Public Key on [Gerrit server](http://gerrit.mero.colo.seagate.com:8080).
  * Log into the gerrit server with your seagate gid based credentials.
  * On right top corner you will see your name, open drop down menu by clicking and choose settings.
  * In the menu on left, click SSH Public Keys, and add your public key (which is generated in step one) right there.

WoW! :sparkles:
You are all set to fetch S3Server repo now. 

## Cloning S3Server Repository
Getting the main S3Server code on your system is straightforward.
1. `$ cd path/to/your/dev/directory`
2. `$ export GID=<your_seagate_GID>` # this will make subsequent steps easy to copy-paste :)
3. `$ git clone "ssh://g${GID}@gerrit.mero.colo.seagate.com:29418/s3server"` 
4. Enable some pre-commit hooks required before pushing your changes to remote.
  * `$ scp -p -P 29418 g${GID}@gerrit.mero.colo.seagate.com:hooks/commit-msg "s3server/.git/hooks/"`
    
    if permission denied, then do following
    
    `$ chmod 600 /root/.ssh/id_rsa`

5. `$ cd s3server`
6. `$ git submodule update --init --recursive && git status`  # this can take about 20 minutes to run

## Installing dependency
This is a one time initialization when we do clone the repository or there is a changes in dependent packages.

  * At some point during the execution the `init.sh` script will prompt for following password, enter those as mentioned below.
    * SSH password: `XYRATEX`
    * Enter new password for openldap rootDN:: `seagate`
    * Enter new password for openldap IAM admin:: `ldapadmin`

1. `$ cd ./scripts/env/dev`
2. `$ ./init.sh`, For some system `./init.sh` fails sometimes. If it is failing run `./upgrade-enablerepo.sh` and re run `./init.sh`. Refer below image of successful run of `./init.sh` where `failed` field should be zero.  # this can take 10-15 minutes to run

<p align="center"><img src="../../assets/images/init_script_output.PNG?raw=true"></p>

## Compilation and Running Unit Test
All following commands assumes that user is already into it's main source directory.
1. Setup host system
  * `$ ./update-hosts.sh`
2. Following script by default will build the code, run the unit test and system test in your local system. Check for help to get more details.
  * `$ ./jenkins-build.sh`. If you face issue with clang-format, to install refer [here](MeroQuickStart.md#getting-git--gerit-to-work).
  Make sure output log has message as shown in below image to unsure successful run of system test in `./jenkins-build.sh`.
  
<p align="center"><img src="../../assets/images/jenkins_script_output.PNG?raw=true"></p>

KABOOM!!!
  
## Running Jenkins / System tests

TODO

## Code reviews and commits

To follow step by step process refer [here](MeroQuickStart.md#Code-reviews-and-commits).

### You're all set & You're awesome

In case of any query feel free to write to our [SUPPORT](support.md).

Let's start without a delay to contribute to seagate's open source initiative and join this movement with us keeping a common goal of making data storage better, more efficient and more accessible.

Seagate welcomes You! :relaxed:

