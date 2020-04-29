# Mero QuickStart guide
This is a step by step guide to get CORTX ready for you on your system.
Before cloning, however, you need to have a SSC / Cloud VM or a local VM setup in either VMWare Fusion or Oracle VirtualBox [LocalVMSetup](LocalVMSetup.md).

## Accessing the code right way
(For phase 1) Latest code which is getting evolved, advancing and contributed is on the gerrit server.
Seagate contributor will be referencing, cloning and committing code to/from this [Gerrit server](http://gerrit.mero.colo.seagate.com:8080).

Following steps as sudo user(sudo -s) will make your access to server hassel free.

1. Create SSH Public Key
  * [SSH generation](https://git-scm.com/book/en/v2/Git-on-the-Server-Generating-Your-SSH-Public-Key) will make your key generation super easy. follow the instructions throughly.
2. Add SSH Public Key on [Gerrit server](http://gerrit.mero.colo.seagate.com:8080).
  * Log into the gerrit server with your seagate gid based credentials.
  * On right top corner you will see your name, open drop down menu by clicking and choose settings.
  * In the menu on left, click SSH Public Keys, and add your public key (which is generated in step one) right there.

WoW! :sparkles:
You are all set to fetch mero repo now. 

## Cloning CORTX
Getting the main CORTX code on your system is straightforward.
1. `$ sudo -s`
2. `$ cd path/to/your/dev/directory`
3. `$ export GID=<your_seagate_GID>` # this will make subsequent sets easy to copy-paste :)
4. `$ git clone --recursive "ssh://g{GID}@gerrit.mero.colo.seagate.com:29418/mero" -b innersource` (It has been assumed that "git" is preinstalled. if not then follow git installation specific steps provided [here](#getting-git--gerit-to-work). Recommended git version is 2.x.x . Check your git version using `$ git --version` command.)  
(If "Permission denied (publickey). fatal: Could not read from remote repository" error occurs while using ssh in this step then use the following alternate command) `$ git clone --recursive "http://gerrit.mero.colo.seagate.com/mero" -b innersource`                                                                                                                                                                                           
5. `$ cd mero`
6. `$ gitdir=$(git rev-parse --git-dir)`
7. Enable some pre-commit hooks required before pushing your changes to remote (command to be run from the parent dir of Mero source).
  * `$ scp -p -P 29418 g${GID}@gerrit.mero.colo.seagate.com:hooks/commit-msg ${gitdir}/hooks/commit-msg`
8. Build necessaries dependencies
  * To install all dependent packages like lustre, pip, etc.
  
    `$ ./scripts/install-build-deps` 
   * Troubleshooting steps:
   * In case pip installation failure using scripts do following
   
     `$ python -m pip uninstall pip setuptools`
     
     `$ Run script ./scripts/install-build-deps`
    
   * If fails with dependency of pip3 , install pip3 using following
    
     `$ yum install python34-setuptools`
    
     `$ easy_install- 3.4 pip`
    
   * If fails for 'ply' dependency, install ply using following
   
     `$ pip3 install ply`
   
  
## Compilation and Running Unit Test

All following commands assumes that user is already into it's main source directory.

1. building mero
 * `$ sudo ./scripts/m0 make`
  
2. Running Unit Tests (UTs)
 * `$ sudo ./scripts/m0 run-ut`
    > Feel free to expore other options of this run-ut command. Try : sudo run-ut --help
    
3. For kernel space UTs
  * `$ sudo ./scripts/m0 run-kut`
  
4. Running a system test  

  To list all ST's 
  * `$ sudo ./scripts/m0 run-st -l`
  
   As an example for clovis module system test can be run using following command :
  * `$ sudo ./scripts/m0 run-st 52mero-singlenode-sanity`
   
   To run all the ST's,
  * `$ sudo ./scripts/m0 run-st`
  
KABOOM!!!
  
## Running Jenkins / System tests

TODO

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

### To work on a feature and save your code to git
Ensure you have checkout out ‘innersource’ branch

> $ git checkout innersource

Now checkout your new branch for saving your code
Example git checkout -b dev/<username>/<feature>
Username = name or initials, example “John” or just “JB”

> $ git checkout -b JB/mero_sync

Make Changes

Add files to be pushed to git to staged area

> $ git add foo/somefile.c

> $ git add foo_ut/someotherfile.c

Add all such files

Now commit your changes

> $ git commit -m ‘mero - Some change’

Check git log to see your commit, verify the author name

> $ git log 

If author name is not set properly then set using following command

> git commit --amend --author="Author Name <email@address.com>"

Once your changes are committed locally, it's time to push up to server
push to your branch [Use this only if you want to backup your code, else prefer gerrit steps below]

> $ git push origin JB/mero_sync

### To work on a feature and submit review to gerrit
Ensure you have checkout “innersource” branch

> $ git checkout innersource

> $ git checkout -b JB/mero_sync

Make code changes

Add files to be pushed to git to staged area

> $ git add foo/somefile.cc

Add all such files

Now commit your changes

> $ git commit -m ‘mero - Some description of change’

Check git log to see your commit, verify the author name

> $ git log 

If author name is not set properly then set using following command

> git commit --amend --author="Author Name <email@address.com>"

Note here the commit hook should add a ChangeID, something like 
~~~
commit a41a6d839148026cc0c3b838352529e10898e5dc
Author: Rajesh Nambiar <rajesh.nambiar@seagate.com>
Date:   Thu Apr 16 00:55:01 2020 -0600

    EOS-7148: This parameter not supported by systemd in our hardware

    Change-Id: I1ce3d04e74d56c11645a95b1d523e72b0cc01e17
~~~

Once your changes are committed locally, it's time to push up the review to gerrit
push to ‘innersource’ branch

* Before pushing, TAKE A PAUSE. Have you [rebased your branch](#How-to-ebase?)? if not then doing right now to avoide merge conflicts on gerrit.

> $ git push origin HEAD:refs/for/innersource%r=madhav.vemuri@seagate.com,r=max.medved@seagate.com,r=john.bent@seagate.com,r=nikita.danilov@seagate.com

If you want to make more changes, perform locally and use amend, so that last commit is updated with new changes and gerrit treats this as new patchset on the same review associated with the same changeid created earlier.
> $ git commit --amend

Folks, Good news is that you can also monitor progress of your applied patch and respond to review comments as well.

* There are two ways to check your commit on gerrit :
 1. You will be provided link into logs after push command.
 * e.g.
 <p align="center"><img src="../../assets/images/gerrit_review_link.JPG?raw=true"></p>
 
 2. you can visit following link, open your commit from the list and browse it.
 * http://gerrit.mero.colo.seagate.com/q/project:mero+branch:innersource+status:open

### How to rebase?
Let’s say you want to rebase JB/mero_sync with latest changes in innersource branch.
Here are the steps:
Ensure there are no local changes, if yes take a backup and git stash so local is clean
> $ git stash

Update local innersource branch
> $ git checkout innersource

> $ git pull origin innersource  (alternatively git pull --rebase)

Update local JB/mero_sync branch
> $ git checkout JB/mero_sync

> $ git pull origin JB/mero_sync  (alternatively git pull --rebase)

Start the rebase to pull innersource in currently checked out dev/kd/myfeature
> $ git rebase innersource

This might raise merge conflicts. fix all the merge conflicts cautiously.
Test your local rebase and push upstream using step
> $ git push origin HEAD:refs/for/innersource%r=madhav.vemuri@seagate.com,r=max.medved@seagate.com,r=john.bent@seagate.com,r=nikita.danilov@seagate.com

## Running Jenkins / System tests

* To get familiar with jenkins please visit [here](https://en.wikipedia.org/wiki/Jenkins_(software)).

### How to start mero jenkins job?

1. Open mero jenkins [link](http://eos-jenkins.colo.seagate.com/job/Release_Engineering/job/InnerSource/job/EOS-Core/job/mero-vm-test/).
2. Login with Username as `eos-core` and password as `eos-core`.
3. Click on `Build With Parameters` option.
4. You need to enter the `GIT_REFSPEC` and `label`, follow the steps as mentioned below.
  * To get `GIT_REFSPEC` goto your gerrit server branch where you pushed your changes and click on`Download` section. The highlighted part `refs/changes/31/19331/4` in `checkout` section as shown below is your `GIT_REFSPEC`. Copy and paste it on `GIT_REFSPEC` section of jenkins.
  
  
<p align="center"><img src="../../assets/images/mero_gerritsnap_jenkins_GIT_REFSPEC_detail.JPG?raw=true"></p>

  * `label` field could be anything which is easy to recognize. It isn't mandatory field though.
5. Press on `Build` button to start building the code. 
6. All the currently running builds can be monitored from `Build History` section on left side pane. Specific build can be monitored by clicking on build number/lable from `Build History` section.

### Wanna test specific mero commit with s3?

* If s3server setup is not done yet then kindly visit [s3serverQuickstart](https://github.com/Seagate/cortx/edit/master/doc/S3ServerQuickStart.md) and get your setup ready.
* If s3 is already setup then check out detailed steps [here](S3ServerQuickStart.md#Testing-specific-MERO-version-with-S3Server).

### You're all set & You're awesome

In case of any query feel free to write to our [SUPPORT](SUPPORT.md).

Let's start without a delay to contribute to seagate's open source initiative and join this movement with us keeping a common goal of making data storage better, more efficient and more accessible.

Seagate welcomes You! :relaxed:

