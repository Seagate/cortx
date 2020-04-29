# S3Server QuickStart guide
This is a step by step guide to get S3Server ready for you on your system.
Before cloning, however, you need to have a SSC / Cloud VM or a local VM setup in either VMWare Fusion or Oracle VirtualBox [LocalVMSetup](LocalVMSetup.md).

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
3. `$ git clone "ssh://g${GID}@gerrit.mero.colo.seagate.com:29418/s3server" -b innersource` ( It has been assumed that `git` is preinstalled. If not then follow git installation specific steps provided [here](#getting-git--gerit-to-work). Recommended git version is 2.x.x . Check your git version using `$ git --version` command.)
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
### Running Unit test and System test
1. Setup host system
  * `$ ./update-hosts.sh`
2. Following script by default will build the code, run the unit test and system test in your local system. Check for help to get more details.
  * `$ ./jenkins-build.sh`. If you face issue with clang-format, to install refer [here](#getting-git--gerit-to-work).
  Make sure output log has message as shown in below image to ensure successful run of system test in `./jenkins-build.sh`.
  
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
  
## Code reviews and commits

### Getting Git / Gerit to Work
Update Git to the latest version
with older git version, you might be getting errors with commit hook, like this one:


> $ git commit
> git: 'interpret-trailers' is not a git command. See 'git --help'.
cannot insert change-id line in .git/COMMIT_EDITMSG

Fix (for CentOS 7.x):

 > $ yum remove git
 
 > $ yum -y install  https://centos7.iuscommunity.org/ius-release.rpm
 
 > $ yum -y install  git2u-all
 
 > $ yum-config-manager --disable ius/x86_64 # prevent accidental updates from this repo

Setup the git config options

 > $ git config --global user.name ‘Your Name’
 
 > $ git config --global user.email ‘Your.Name@seagate.com’
 
Installing clang-format

 > $ mkdir -p ~/Downloads/clang ~/bin
 
 > $ cd ~/Downloads/clang
 
 > $ wget http://llvm.org/releases/3.8.0/clang+llvm-3.8.0-linux-x86_64-centos6.tar.xz
 
 > $ tar -xvJf clang+llvm-3.8.0-linux-x86_64-centos6.tar.xz
 
 > $ ln -s ~/Downloads/clang/clang+llvm-3.8.0-linux-x86_64-centos6/bin/git-clang-format ~/bin/git-clang-format
 
### To work on a feature and save your code to git
Ensure you have checkout out ‘innersource’ branch

> $ git checkout innersource

Now checkout your new branch for saving your code
Example git checkout -b dev/<username>/<feature>
Username = name or initials, example “John” or just “JB”

> $ git checkout -b JB/S3_sync

Make Changes

Add files to be pushed to git to staged area

> $ git add foo/somefile.c

> $ git add foo_ut/someotherfile.c

Add all such files

Now commit your changes

> $ git commit -m ‘GID - Appropriate Feature/Change Description’

Check git log to see your commit, verify the author name

> $ git log 

If author name is not set properly then set using following command

> $ git commit --amend --author="Author Name <email@address.com>"

Once your changes are committed locally, it's time to push up to server
push to your branch [Use this only if you want to backup your code, else prefer gerrit steps below]

> $ git push origin JB/S3_sync

### To work on a feature and submit review to gerrit
Ensure you have checkout “innersource” branch

> $ git checkout innersource

> $ git checkout -b JB/S3_sync

Make code changes

Add files to be pushed to git to staged area

> $ git add foo/somefile.cc

Add all such files

Now commit your changes

> $ git commit -m ‘GID - Appropriate Feature/Change Description’

Check git log to see your commit, verify the author name

> $ git log 

If author name is not set properly then set using following command

> $ git commit --amend --author="Author Name <email@address.com>"

Note here the commit hook should add a ChangeID, something like 
~~~
commit a41a6d839148026cc0c3b838352529e10898e5dc
Author: Rajesh Nambiar <rajesh.nambiar@seagate.com>
Date:   Thu Apr 16 00:55:01 2020 -0600

    EOS-7148: This parameter not supported by systemd in our hardware

    Change-Id: I1ce3d04e74d56c11645a95b1d523e72b0cc01e17
~~~

Once your changes are committed locally, it's time to push up the changes to gerrit for starting jenkins job and review request.
push to ‘innersource’ branch
> $ git push origin HEAD:refs/for/innersource%r=nilesh.govande@seagate.com,r=basavaraj.kirunge@seagate.com,r=john.bent@seagate.com,r=nikita.danilov@seagate.com

Upon pushing changes, you will get the URL of gerrit server as shown below. Use that URL to visit gerrit server.

<p align="center"><img src="../../assets/images/gerrit_review_link.JPG?raw=true"></p>

Make sure specified reviewers are added and jenkins job is started automatically. If ever needed, to start jenkins job manually follow the steps from [here](#Running-Jenkins--System-tests). Also you can add other reviewer manually as shown below.

<p align="center"><img src="../../assets/images/gerrit_review.PNG?raw=true"></p>

You can also visit [track review status](http://gerrit.mero.colo.seagate.com/q/project:s3server+branch:innersource+status:open) to monitor your review request.

If you want to make more changes, perform locally and use amend, so that last commit is updated with new changes and gerrit treats this as new patchset on the same review associated with the same changeid created earlier.
> $ git commit --amend

### How to rebase?
Let’s say you want to rebase JB/S3_sync with latest changes in innersource branch.
Here are the steps:
Ensure there are no local changes, if yes take a backup and git stash so local is clean
> $ git stash

Update local master branch
> $ git checkout innersource

> $ git pull origin innersource  (alternatively git pull --rebase)

Update local JB/S3_sync branch
> $ git checkout JB/S3_sync

> $ git pull origin JB/S3_sync  (alternatively git pull --rebase)

Start the rebase to pull master in currently checked out dev branch
> $ git rebase innersource

This might raise merge conflicts. fix all the merge conflicts cautiously.
Test your local rebase and push upstream using step
> $ git push origin HEAD:refs/for/innersource

## Running Jenkins / System tests

* To get familiar with jenkins please visit [here](https://en.wikipedia.org/wiki/Jenkins_(software)).

### How to start S3Server jenkins job?

1. Open S3Server jenkins [link](http://eos-jenkins.colo.seagate.com/job/Release_Engineering/job/InnerSource/job/S3Server/job/s3-gerrit-test/).
2. Login with Username as `eos-s3server` and password as `eos-s3server`.
3. Click on `Build With Parameters` option.
4. You need to enter the `GIT_REFSPEC` and `label`, follow the steps as mentioned below.
  * To get `GIT_REFSPEC` goto your gerrit server branch where you pushed your changes and click on`Download` section. The highlighted part `refs/changes/31/19331/4` in `checkout` section as shown below is your `GIT_REFSPEC`. Copy and paste it on `GIT_REFSPEC` section of jenkins.
  
  
<p align="center"><img src="../../assets/images/git_refspec.PNG?raw=true"></p>

  * `label` field could be anything which is easy to recognize. It isn't mandatory field though.
5. Press on `Build` button to start building the code. 
6. All the currently 
builds can be monitored from `Build History` section on left side pane. Specific build can be monitored by clicking on build number/lable from `Build History` section.

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

In case of any query feel free to write to our [SUPPORT](support.md).

Let's start without a delay to contribute to seagate's open source initiative and join this movement with us keeping a common goal of making data storage better, more efficient and more accessible.

Seagate welcomes You! :relaxed:

