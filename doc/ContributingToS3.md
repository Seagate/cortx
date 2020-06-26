## Code commits and reviews

### GitHub setup

# Generate SSH key on your development box (Needed if you wish to use SSH for clone)
> $ $ ssh-keygen -o -t rsa -b 4096 -C "<seagate-email-address>"

# Add SSH key to GitHub Account
 Copy public key i.e. id_rsa.pub (default location: /root/.ssh/id_rsa.pub)
 Go to GitHub SSH key settings: https://github.com/settings/keys of your account
 Check if your GitHub-id is associated with Seagate email address as Primary email else SSO will not work.
 Add the new SSH key and then select Enable SSO option and Authorize for this key. 

# Token (personal access) for command line (Required for submodule clone process)
 Create a personal access token and use it in place of a password when performing Git operations over HTTPS with Git on the command line or the API.
 A personal access token is required to authenticate to GitHub in the following situations:
   . When you're using two-factor authentication
   . To access protected content in an organization that uses SAML single sign-on (SSO). Tokens used with organizations that use SAML SSO must be authorized.
   . Reference Article, Creating a personal access token for the command line
   . In this new token, please make sure that you have enabled SSO.

### Git setup on development box
Update Git to the latest version
with older git version, you might be getting errors with commit hook, like this one:

> $ git commit
> git: 'interpret-trailers' is not a git command. See 'git --help'.
cannot insert change-id line in .git/COMMIT_EDITMSG

Fix (for CentOS 7.x):

 > $ yum remove git
 
 > $ yum -y install  https://packages.endpoint.com/rhel/7/os/x86_64/endpoint-repo-1.7-1.x86_64.rpm
 
 > $ yum -y install git
  
Install git-clang-format

> $ yum install git-clang-format

Setup the git config options

 > $ git config --global user.name ‘Your Name’
 
 > $ git config --global user.email ‘Your.Name@seagate.com’
 
 > $ git config --global color.ui auto
 
 > $ git config --global credential.helper cache (Required for submodule clone process)
 
### To work on a feature and submit review to GitHub

# Clone cortx-s3server
Each contributor needs to do 'fork' to create their own private cortx-s3server repository.
> $ git clone git@github.com:<GitHub-ID>/cortx-s3server.git
 
Ensure you have checked out “Dev” branch

> $ git checkout dev

> $ git checkout -b <your-local-branch-name>

Make code changes

Add files to be pushed to git to staged area

> $ git add foo/somefile.cc

Add all such files

Now commit your changes

> $ git commit -m ‘GID - Appropriate Feature/Change Description’

Check git log to see your commit, verify the author name

> $ git log 

If author name is not set properly then set using following command

Push your changes to GitHub
> $ git push origin <your-local-branch-name>

Example output: 
~~~
Enumerating objects: 4, done.
Counting objects: 100% (4/4), done.
Delta compression using up to 2 threads
Compressing objects: 100% (2/2), done.
Writing objects: 100% (3/3), 332 bytes | 332.00 KiB/s, done.
Total 3 (delta 1), reused 0 (delta 0)
remote: Resolving deltas: 100% (1/1), completed with 1 local object.
remote:
remote: Create a pull request for 'dev/ak/test-innersource' on GitHub by visiting:
remote:      https://github.com/<your-GitHub-Id>/cortx-s3server/pull/new/<your-local-branch-name>
remote:
To github.com:amitwac2608/cortx-s3server.git
 * [new branch]        <your-local-branch-name> -> <your-local-branch-name>
~~~

### Open pull request for review
Open the URL given in the output of

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
