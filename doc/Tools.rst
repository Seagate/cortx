====================
Tools and Procedures
====================
*******
GitHub
*******
GitHub brings together the world's largest community of developers to discover, share, and build better software.

Account Creation
================
To create a GitHub account, perform the procedure below.

1. Navigate to the GitHub by clicking `https://github.com/ <https://github.com/>`_. The login page appears.

2. Enter relevant information in the following fields:

   * **Username** – Your Username

   * **Email** - Your email address

   * **Password** - Ensure that the password meets either of the following two conditions:

                    * The word must include at least 15 characters
                    
                    * The word must include at least 8 characters. One numeric value and a   letter in lower case is mandatory.

3. Under **Email preferences**, select the **Send me occasional product updates, announcements, and offers** check box. This is an optional step.

4. Under the **Verify your account** section, click **Verify**.

   1. Using the arrows, rotate the image to achieve accuracy.
   2. Click **Done** after you ensure that the image is accurately positioned.

5. Click **Create Account**. An email with a link to verify the created account would be sent to you.

   - Click the link to complete the verification process.

Personal Access Token (PAT)
===========================
Personal Access Tokens (PATs) are an alternative to using passwords for authentication to GitHub when using the GitHub API or the command line. To generate a PAT, perform the procedure below.

1. Sign in to GitHub.

2. In the upper-right corner of any page, click your profile photo, then click **Settings**. The **Profile** page is displayed.

3. In the left sidebar, click **Developer settings**. The **GitHub Apps** page is displayed.

4. In the left sidebar, click **Personal access tokens**. The **Personal access tokens** page is displayed.

5. Click **Generate new token**. The New personal access token page is displayed.

6. Provide a name for the token in the relevant field.

7. Select the scopes or permissions to which you want to grant this token. To use your token to access repositories from the command line, select **repo**.

8. Click **Generate token**. A token is generated accordingly.

9. Copy the token to the clipboard.

10. To use the token to authenticate, authorize the token with a SAML single-sign-on. The procedure to authorize a token is mentioned below.

Authorizing PAT
---------------
To authorize a PAT, perform the procedure mentioned below.

1. Sign in to your GitHub account.

2. In the upper-right corner of any page, click your profile photo, then click **Settings**. The **Profile** page is displayed.

3. In the left sidebar, click **Developer settings**. The **GitHub Apps** page is displayed.

4. In the left sidebar, click **Personal access tokens**. The **Personal access tokens** page is displayed.

5. Navigate to the token you want to authorize, and select **Enable SSO**.

6. Click **Authorize**. The token is authorized with the SAML Single Sign-On.

Git Workflow
============
In Git, the following two aspects are applicable:

- Gitflow

- Dev Branches

Setting up the Git Config
-------------------------
To perform the Git configuration, use the following:

- **$ git config --global user.name ‘Your Name’**

- **$ git config --global user.email ‘Your.Name@yourdomain.com’**

Forking a Repository
--------------------
To fork a repository, perform the procedure mentioned below.

1. Login to the GitHub account.

2. Navigate to the relevant repository.

3. In the top-right corner of the page, click **Fork**. A fork of the required repository is created successfully.

Cloning a Repository
--------------------
To clone a repository, perform the procedure below.

1. Login to your GitHub account, and navigate to the above created fork.

2. Above the list of files, click the following tab.

   .. image:: images/code-button.png

3. To clone the repository using HTTPS, under the Clone with HTTPS section, click the clipboard. To clone the repository using a SSH key, including a certificate issued by your organization's SSH certificate authority, click **Use SSH**, then click the clipboard.

4. Open Git Bash.

5. Change the current working directory to the location where you want the cloned directory.

6. Type **git clone**, and then paste the URL you copied earlier. It will look like this, with your GitHub username instead of **YOUR-USERNAME**.

  - **$ git clone https://github.com/YOUR-USERNAME/repository name**

7. Press **Enter**. Your local clone will be created. A local copy of your fork of the repository is created.

Syncing the Fork with Repository
--------------------------------
To configure Git to sync with the fork, perform the following:

1. Open Git Bash.

2. Change directories to the location of the fork you cloned in the earlier procedure.

   - To navigate to your home directory, type **cd**.

   - To list the files and folders in your current directory, type **ls**.

   - To go into one of your listed directories, type **cd your_listed_directory**.

   - To go up one directory, type **cd** ..

3. Type **git remote –v**, and press **Enter**. The configured remote repository for your fork is visible.

4. Type **git remote add upstream**, and then paste the URL you had copied. Then, click **Enter**.

   - **$ git remote add upstream <<URL>>**

5. To verify the new upstream repository that you had specified for your fork, type **git remote -v** again. You should see the URL for your fork as **origin**, and the URL for the original repository as **upstream**.

Branching Information
---------------------
Please note the below mentioned points.

- As per the global naming convention, the Master branch is the Main branch.

- The Main branch represents the official history, and it must be deployable at any point of time. For every new feature that is being developed, the developer creates a new branch.

- At times, a single branch would be used to deliver a large feature, or prepare for a release.

- Before creating a branch, make sure that all the upstream changes from the main branch is maintained.

- Make sure that you are in the right branch before pulling the commits.

- The checked-out branch must have a “*” as a prefix to the name. If the returned value is not main, then switch to main.

  .. image:: images/Writer1.png
  
- A new Git branch can be created from the current branch.

  .. image:: images/Writer2.png
  
  
Code Changes and GIT Commands
-----------------------------

- Make your code changes, and commit.

 - When main is the branch, and is ready to pull the updates:

  - **$ git pull origin main**

 - You may have to run the following:

  - **$ git pull origin/feature_x** 
 
 .. image:: images/Writer3.png
 
- The Git pull command merges the git fetch and git merge commands.

- With each commit, there would be additions and deletions. The following command provides an updated list of files.

 - **$ git status**

- Run the following command from root of the project to add files individually or in bulk.

 - **$ git add**

- Run the following command to address additions and deletions.

 - **$ git add –all**

- When the updates are presented differently, under the heading of Changes to be committed, run the following:

 - **$ git commit -m "<type>(<scope>): <subject>"**
 
 .. image:: images/Writer4.png
 
Pushing your Branch
-------------------

To push the new dev branch to the remote repo, perform the following:

1. Configure Git to always push using the current branch.

   - **$ git config --global push.default current**

2. Push a local branch to a different remote branch.

   - **$ git push origin <local_branch>:<remote_branch>**
   
    .. image:: images/Writer5.png
 
 
Pull Request
------------
To create a pull request on GitHub, navigate to the main page of the respective repository, and perform the following:

1. Select the appropriate branch from the **Branch** drop-down menu.
2. Click **Compare & Pull Request**.
3. Type a title and description for your pull request.
4. Select the reviewers using the menu on the right-side of the window.
5. Click **Create Pull Request**. The pull request is raised successfully.


Rebasing
--------
Rebasing ensures that you have the latest version of main. The procedure is detailed below.

1. Consume the commits from your dev branch.
2. Temporarily unset them
3. Move to the newest head of the main branch
4. Commit them again

**Note**: If there are no issues, conflicts would not occur.

To rebase your local dev branch on the latest version of main: 

•	**$ git checkout main             /* ensure you are on the main branch**
•	**$ git pull                                   /* pull the latest from the remote**
•	**$ git push origin PM/cortx-re-testbranch  /* update your copy in the repo**
•	**$ git rebase main                 /* rebase on the main branch**    
•	**$ git push origin PM/cortx-re-testbranch --force   /* force update the remote** 

******
Codacy
******

Codacy is an automated code analysis or quality tool that enables a developer to deliver effective software in a faster and seem less manner.

******************
Working of Codacy
******************
You can use Codacy by performing the below mentioned procedure.

1. Login to your GitHub account, and navigate to the relevant repository.
2. Scroll down the page until you reach the **README** section.
3. Under the **README** section, click the **code quality** tab. The **Dashboard** of the Codacy portal is displayed. You can view the following information:

   - Graphical representation of the repository certification
   - Commits made in your repository
   - Issues reported in your repository
   - Files associated with your repository
   - Pull requests raised in your repository
   - Security status of different parameters
 
********************************
Developer Certificate of Origin
********************************

The Developer Certificate of Origin (DCO) is a way through which you certify that you wrote the code, or you have the right to submit the same. The DCO text is mentioned below.

By making a contribution to this project, I certify that:

-  The contribution was created in whole or in part by me and I
   have the right to submit it under the open source license
   indicated in the file; or

-  The contribution is based upon previous work that, to the best
   of my knowledge, is covered under an appropriate open source
   license and I have the right under that license to submit that
   work with modifications, whether created in whole or in part
   by me, under the same open source license (unless I am
   permitted to submit under a different license), as indicated
   in the file; or

-  The contribution was provided directly to me by some other
   person who certified (a), (b) or (c) and I have not modified
   it.

-  I understand and agree that this project and the contribution
   are public and that a record of the contribution (including all
   personal information I submit with it, including my sign-off) is
   maintained indefinitely and may be redistributed consistent with
   this project or the open source license(s) involved.

You must sign off that you adhere to the above requirements, by pre-fixing **Signed-off-by** to the commit messages.

- **Signed-off-by**: Random J Developer `random@developer.example.org <mailto:random@developer.example.org>`_

In Command Line Interface (CLI), you can add **–s** to append automatically.

- **$ git commit -s -m 'This is my commit message'** 

Usage of hooks will help if you have the tendency to forget adding **-s**. To know more about this, refer `https://lubomir.github.io/en/2016-05-04-signoff-hooks.html <https://lubomir.github.io/en/2016-05-04-signoff-hooks.html>`_
 
To know more about DCO and CLA, refer `DCO and CLA <https://github.com/Seagate/cortx/blob/main/doc/dco_cla.md>`_.
