## Code commits and reviews

### GitHub setup

#### Generate SSH key on your development box (Needed if you wish to use SSH for clone)
> $ $ ssh-keygen -o -t rsa -b 4096 -C "seagate-email-address"

#### Add SSH key to GitHub Account
 1. Copy public key i.e. id_rsa.pub (default location: /root/.ssh/id_rsa.pub)
 2. Go to GitHub SSH key settings: https://github.com/settings/keys of your account
 3. Check if your GitHub-id is associated with Seagate email address as Primary email else SSO will not work.
 4. Add the new SSH key and then select Enable SSO option and Authorize for this key. 

#### Token (personal access) for command line (Required for submodule clone process)
 - Create a personal access token and use it in place of a password when performing Git operations over HTTPS with Git on the command line or the API.
 - A personal access token is required to authenticate to GitHub in the following situations:
   - When you're using two-factor authentication
   - To access protected content in an organization that uses SAML single sign-on (SSO). Tokens used with organizations that use SAML SSO must be authorized.
   - Reference Article, [Creating a personal access token for the command line](https://help.github.com/en/github/authenticating-to-github/creating-a-personal-access-token)
   - In this new token, please make sure that you have enabled SSO.

### Git setup on development box
1. Update Git to the latest version
    - with older git version, you might be getting errors with commit hook, like this one:

> $ git commit
> git: 'interpret-trailers' is not a git command. See 'git --help'.
cannot insert change-id line in .git/COMMIT_EDITMSG

Fix (for CentOS 7.x):

 > $ yum remove git
 
 > $ yum -y install  https://packages.endpoint.com/rhel/7/os/x86_64/endpoint-repo-1.7-1.x86_64.rpm
 
 > $ yum -y install git
  
2. Install git-clang-format

> $ yum install git-clang-format

3. Setup the git config options

 > $ git config --global user.name ‘Your Name’
 
 > $ git config --global user.email ‘Your.Name@seagate.com’
 
 > $ git config --global color.ui auto
 
 > $ git config --global credential.helper cache (Required for submodule clone process)
 
### To work on a feature and submit review to GitHub

#### Clone cortx-s3server
- Each contributor needs to do 'fork' to create their own private cortx-s3server repository.
> $ git clone git@github.com:<GitHub-ID>/cortx-s3server.git
 
- Ensure you have checked out “Dev” branch

   > $ git checkout dev

   > $ git checkout -b <your-local-branch-name>

- Make code changes

- Add files to be pushed to git to staged area

   > $ git add foo/somefile.cc

- Add all such files

- Now commit your changes

   > $ git commit -m ‘GID - Appropriate Feature/Change Description’

- Check git log to see your commit, verify the author name

   > $ git log 

- If author name is not set properly then set using following command

- Push your changes to GitHub
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
To github.com:<your-GitHub-Id>/cortx-s3server.git
 * [new branch]        <your-local-branch-name> -> <your-local-branch-name>
~~~

### Open pull request for review
- Open the URL given in the output of 'git push'.
- Select base:branch as 'dev' from the dropdown.
<p align="center"><img src=""></p>
- Add reviewers and click "Create pull request"

### Running Jenkins / System tests
- Get commit-id of your change
<p align="center"><img src=""></p>
- Start pre-merge jobs using commit id and label(Optional) [Jenkins Job](http://eos-jenkins.mero.colo.seagate.com/job/S3server/job/s3-github-test/)

### How to rebase your local branch on latest master?
> $ git checkout master
> $ git pull origin master
> $ git submodule update --init --recursive
> $ git checkout <your-local-branch>
> $ git rebase origin/master
- If you get conflicts, follow the steps mentioned in the error message from git. 

* To get familiar with jenkins please visit [here](https://en.wikipedia.org/wiki/Jenkins_(software)).

### You're all set & You're awesome

In case of any query feel free to write to our [SUPPORT](SUPPORT.md).

Let's start without a delay to contribute to seagate's open source initiative and join this movement with us keeping a common goal of making data storage better, more efficient and more accessible.

Seagate welcomes You! :relaxed:
