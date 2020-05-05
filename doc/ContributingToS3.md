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

* Before pushing, TAKE A PAUSE. You can rebased your branch from [here](#How-to-rebase). If not then do it right now to avoide merge conflicts on gerrit.

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

> $ git pull origin innersource

Update local JB/S3_sync branch
> $ git checkout JB/S3_sync

Start the rebase to pull master in currently checked out dev branch
> $ git rebase innersource

This might raise merge conflicts. fix all the merge conflicts cautiously.

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
6. All the current builds can be monitored from `Build History` section on left side pane. Specific build can be monitored by clicking on build number/lable from `Build History` section.

### You're all set & You're awesome

In case of any query feel free to write to our [SUPPORT](SUPPORT.md).

Let's start without a delay to contribute to seagate's open source initiative and join this movement with us keeping a common goal of making data storage better, more efficient and more accessible.

Seagate welcomes You! :relaxed:
