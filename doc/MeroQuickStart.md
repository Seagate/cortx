# Mero QuickStart guide
This is a step by step guide to get CORTX ready for you on your system.
Before cloning, however, you have your VMs setup with specifications mentioned in [Virtual Machine](VIRTUAL_MACHINE.md).

## Accessing the code right way
(For phase 1) Latest code which is getting evolved, advancing and contributed is on the gerrit server.
Seagate contributor will be refernecing, cloning and committing code to/from this [Gerrit server](http://gerrit.mero.colo.seagate.com:8080).

Following steps will make your access to server hassel free.

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
1. $ cd path/to/your/dev/directory
2. $ export GID=<your_seagate_GID> # this will make subsequent sets easy to copy-paste :)
3. $ git clone --recursive ssh://g${GID}@gerrit.mero.colo.seagate.com:29418/mero (It has been assumed that "git" is preinstalled. Recommended git version is 2.x.x . Check your git version using `$ git --version` command.)
4. $ cd mero
5. Enable some pre-commit hooks required before pushing your changes to remote (command to be run from the parent dir of Mero source).
  * $ scp -p -P 29418 g${GID}@gerrit.mero.colo.seagate.com:hooks/commit-msg .git/hooks
    * if permission denied, then do following 
    > $ chmod 600 /root/.ssh/id_rsa 
  
6. Build necessaries dependencies
  * $ sudo ./scripts/install-build-deps
   * To install all dependent packages like lustre, pip, etc.
   * In case pip installation failure using scripts do following
    
    > $ python -m pip uninstall pip setuptools
    > $ Run script ./scripts/install-build-deps
    
   If fails with dependency of pip3 , install pip3 using following
    
    > $ sudo yum install python34-setuptools
    > $ sudo easy_install- 3.4 pip
    
   If fails for 'ply' dependency, install ply using following
   
    > $ pip3 install ply
   
  
## Compiliation and Running Unit Test
All following commands assumes that user is already into it's main source directory.
1. building mero
  * ./scripts/m0 make
2. Running Unit Tests (UTs)
  * sudo ./scripts/m0 run-ut
    > Feel free to expore other options of this run-ut command. Try : sudo run-ut --help
    
    > sudo access is necessary as inserts relevant kernel modules beforehand.
3. For kernel space UTs
  * sudo ./scripts/m0 run-kut
4. Running a system test
    
   As an example for clovis module system test can be run using following command :
  * sudo ./clovis/st/utils/clovis_sync_replication_st.sh
  
KABOOM!!!
  
## Running Jenkins / System tests

TODO

## Code reviews and commits

### Getting Git / Gerit to Work
Update Git to the latest version
With older git version, you might be getting errors with commit hook, like this one:


> $ git commit
> git: 'interpret-trailers' is not a git command. See 'git --help'.
cannot insert change-id line in .git/COMMIT_EDITMSG

Fix (for CentOS 7.x):

 > $ yum remove git
 
 > $ yum -y install  https://centos7.iuscommunity.org/ius-release.rpm
 
 > $ yum -y install  git2u-all
 
 > $ yum-config-manager --disable ius/x86_64 # prevent accidental updates from this repo

Installing clang-format

 > $ mkdir -p ~/Downloads/clang ~/bin
 
 > $ cd ~/Downloads/clang
 
 > $ wget http://llvm.org/releases/3.8.0/clang+llvm-3.8.0-linux-x86_64-centos6.tar.xz
 
 > $ tar -xvJf clang+llvm-3.8.0-linux-x86_64-centos6.tar.xz
 
 > $ ln -s ~/Downloads/clang/clang+llvm-3.8.0-linux-x86_64-centos6/bin/git-clang-format ~/bin/git-clang-format
 
Setup the git config options

 > $ git config --global user.name ‘Your Name’
 
 > $ git config --global user.email ‘Your.Name@seagate.com’

### To work on a feature and save your code to git
Ensure you have checkout out ‘master’ branch

> $ git checkout master

Now checkout your new branch for saving your code
Example git checkout -b dev/<username>/<feature>
Username = name or initials, example “John” or just “JB”

> $ git checkout -b dev/s3_sync

Make Changes

Add files to be pushed to git to staged area

> $ git add server/somefile.c
> $ git add ut/someotherfile.c

Add all such files

Now commit your changes

> $ git commit -m ‘S3 - Some change’

Check git log to see your commit, verify the author name

> $ git log 

Once your changes are committed locally, it's time to push up to server
push to your branch [Use this only if you want to backup your code, else prefer gerrit steps below]

> $ git push origin dev/s3_sync

### To work on a feature and submit review to gerrit
Ensure you have checkout “master” branch

> $ git checkout master

> $ git checkout -b dev/s3_sync

Make code changes

Add files to be pushed to git to staged area

> $ git add server/somefile.cc

Add all such files

Now commit your changes

> $ git commit -m ‘S3 - Some description of change’

Check git log to see your commit, verify the author name

> $ git log 

Note here the commit hook should add a ChangeID, something like 
~~~
commit a41a6d839148026cc0c3b838352529e10898e5dc
Author: Rajesh Nambiar <rajesh.nambiar@seagate.com>
Date:   Thu Apr 16 00:55:01 2020 -0600

    EOS-7148: This parameter not supported by systemd in our hardware

    Change-Id: I1ce3d04e74d56c11645a95b1d523e72b0cc01e17

~~~

Once your changes are committed locally, it's time to push up the review to gerrit
push to ‘master’ branch
> $ git push origin HEAD:refs/for/master

If you want to make more changes, perform locally and use amend, so that last commit is updated with new changes and gerrit treats this as new patchset on the same review associated with the same changeid created earlier.
> $ git commit --amend

### How to rebase?
Let’s say you want to rebase dev/s3_sync with latest changes in master branch.
Here are the steps:
Ensure there are no local changes, if yes take a backup and git stash so local is clean
> $ git stash

Update local master branch
> $ git checkout master
> $ git pull origin master  (alternatively git pull --rebase)

Update local dev/s3_sync branch
> $ git checkout dev/s3_sync
> $ git pull origin dev/s3_sync  (alternatively git pull --rebase)

Start the rebase to pull master in currently checked out dev/kd/myfeature
> $ git rebase master

Test your local rebase and push upstream using step
> $ git push origin HEAD:refs/for/master

### You're all set & You're awesome

In case of any query feel free to write to our [SUPPORT](support.md).

Let's start without a delay to contribute to seagate's open source initiative and join this movement with us keeping a common goal of making data storage better, more efficient and more accessible.

Seagate welcomes You! :relaxed:

