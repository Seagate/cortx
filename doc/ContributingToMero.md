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

### To work on a feature and submit review to gerrit
Ensure you have checkout “innersource” branch

> $ git checkout innersource

Now checkout your new branch for saving your code
Example git checkout -b <username>/<feature>
Username = name or initials, example “John” or just “JB”

> $ git checkout -b JB/mero_sync

Make code changes

Add files to be pushed to git to staged area

> $ git add foo/somefile.cc

Add all such files

Make sure build passes locally using [these steps](#compilation-and-running-unit-test) & commit your changes

> $ git commit -m "GID - Appropriate Feature/Change Description"

Check git log to see your commit, verify the author name

> $ git log 

If author name is not set properly then set using following command

> git commit --amend --author="Author Name < email@address.com >"

 * Make sure your [git configs options](#getting-git--gerit-to-work) are set properly.

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

* Before pushing, TAKE A PAUSE. Have you [rebased your branch](#How-to-rebase)? if not then doing right now to avoide merge conflicts on gerrit.

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

> $ git pull origin innersource

Update local JB/mero_sync branch
> $ git checkout JB/mero_sync

> $ git pull origin JB/mero_sync

Start the rebase to pull innersource in currently checked out dev/kd/myfeature
> $ git rebase innersource

This might raise merge conflicts. fix all the merge conflicts cautiously.

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

* If s3server setup is not done yet then kindly visit [s3serverQuickstart](S3ServerQuickStart.md) and get your setup ready.
* If s3 is already setup then check out detailed steps [here](S3ServerQuickStart.md#Testing-specific-MERO-version-with-S3Server).

### You're all set & You're awesome

In case of any query feel free to write to our [SUPPORT](SUPPORT.md).

Let's start without a delay to contribute to seagate's open source initiative and join this movement with us keeping a common goal of making data storage better, more efficient and more accessible.

Seagate welcomes You! :relaxed:
