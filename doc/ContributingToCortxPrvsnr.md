# Code reviews and commits  
## Git setup on development box

`$ yum -y install git`

*  Setup the git config options
  `$ git config --global user.name ‘<First_Name> <Last_Name>’`  
  `$ git config --global user.email ‘<Your_Name>@<email_domain>.com’`  
  To work on a feature and submit review to GitHub  

*  Clone cortx-prvsnr  

  Each contributor needs to do 'fork' to create their own private cortx-motr and motr-galois repository.  
  Go to homepage of cortx-motr repository on GitHub, there you will see 'fork' at top right corner.  
  `$ git clone https://github.com/Seagate/cortx-prvsnr.git`  

*  Ensure you have checkout “main” branch  
  `$ git checkout -B main origin/main`  

  Now checkout your new branch for saving your code Example git checkout -B / Username = name or initials, example “John” or just “JB”  
  `$ git checkout -B 'username-your-local-branch-name' origin/main`  

*  Make code changes  
  Add files to be pushed to git to staged area  
  `$ git add foo/somefile.py`  
  Add all such files  

  Make sure build passes locally using these steps & commit your changes  
  `$ git commit --signoff -m "Provisioner-component : Appropriate Feature/Change Description"`

  Check git log to see your commit, verify the author name  
  `$ git log`

  If author name is not set properly then set using following command  
  `$ git commit --amend --author="Author Name < email@address.com >" --signoff`

  Make sure your git configs options are set properly.  
  **Note here the commit hook should add a ChangeID, something like**
  ```
  commit 918c33f0cdf4c3c8aae505157434646cfed93a6c  
  Author: <FirstName> <LastName> <firstname.lastname@email_domain.com>  
  Date:   Thu Aug 22 07:58:01 2017 +0530  

    <Cortx-Prvsnr>: This parameter not supported by systemd in our hardware
  ```

*  Push your changes to GitHub
  `$ git push origin 'username-your-local-branch-name'`

  **Before pushing, TAKE A PAUSE. Have you rebased your branch? If not then doing right now to avoid merge conflicts on github.**  
  `$ git pull --rebase`  

*  Open pull request for review  
  *  Open the URL given in the output of 'git push' command above.  
  *  Select base:branch as 'main' from the dropdown.  
  *  Click 'Create pull request' to create the pull request.  
  *  Add reviewers to get feedback on your changes.
  *  Running Jenkins / System tests
  *  Jenkins job will get trigerred automatically and results about static analysis and build health will get reflect in you PR dashboard.

*  How to rebase your local branch on latest main branch code?  
  `$ git checkout main`  
  `$ git fetch origin`  
  `$ git submodule update --init --recursive`  
  `$ git checkout -B 'username-your-local-branch-name' origin/username-your-local-branch-name`  
  `$ git rebase origin/main`  

You're all set & You're awesome
In case of any query feel free to write to our SUPPORT.

Let's start without a delay to contribute to seagate's open source initiative and join this movement with us keeping a common goal of making data storage better, more efficient and more accessible.

Seagate welcomes You! ☺️
