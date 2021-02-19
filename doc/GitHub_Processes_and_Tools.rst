===========================================================
Guidelines and Instructions for GitHub Processes and Tools
===========================================================

.. contents:: :local:
 
*******
GitHub
*******
GitHub brings together the world's largest community of developers to discover, share, and build better software.

Account Creation
================

.. raw:: html 

  <details> 
  <summary><a>Click here to view the procedure.</a></summary> 

To create a GitHub account, perform the procedure below.

1. Navigate to the GitHub by clicking `https://github.com/ <https://github.com/>`_. The login page appears.

2. Enter relevant information in the following fields:

   * **Username** – Your Username

   * **Email** - Your email address

   * **Password** - Ensure that the password meets either of the following two conditions:

     - The word must include at least 15 characters
                    
     - The word must include at least 8 characters. One numeric value and a   letter in lower case is mandatory.

3. Under **Email preferences**, select the **Send me occasional product updates, announcements, and offers** check box. This is an optional step.

4. Under the **Verify your account** section, click **Verify**.

   1. Using the arrows, rotate the image to achieve accuracy.
   2. Click **Done** after you ensure that the image is accurately positioned.

5. Click **Create Account**. An email with a link to verify the created account would be sent to you.

   - Click the link to complete the verification process.
   
.. raw:: html
   
   </details>

Personal Access Token (PAT)
===========================
Personal Access Tokens (PATs) are an alternative to using passwords for authentication to GitHub when using the GitHub API or the command line. 

.. raw:: html 

  <details> 
  <summary><a>Click here to view the procedure.</a></summary> 

1. Sign in to GitHub.

2. In the upper-right corner of any page, click your profile photo, then click **Settings**. The **Profile** page is displayed.

3. In the left sidebar, click **Developer settings**. The **GitHub Apps** page is displayed.

4. In the left sidebar, click **Personal access tokens**. The **Personal access tokens** page is displayed.

5. Click **Generate new token**. The New personal access token page is displayed.

6. Provide a name for the token in the relevant field.

7. Select the scopes or permissions to which you want to grant this token. To use your token to access repositories from the command line, select **repo**.

8. Click **Generate token**. A token is generated accordingly.

9. Copy the token to the clipboard.

.. raw:: html
   
   </details>

Authorizing PAT
---------------
.. raw:: html 

  <details> 
  <summary><a>Click here to view the procedure.</a></summary>
  
1. Sign in to your GitHub account.

2. In the upper-right corner of any page, click your profile photo, then click **Settings**. The **Profile** page is displayed.

3. In the left sidebar, click **Developer settings**. The **GitHub Apps** page is displayed.

4. In the left sidebar, click **Personal access tokens**. The **Personal access tokens** page is displayed.

.. raw:: html
   
   </details>

Branching Information
======================

.. raw:: html 

  <details> 
  <summary><a>Click here to expand</a></summary> 

- As per the global naming convention, we've renamed Master to Main branch.

- The Main branch represents the official history, and it must be deployable at any point of time. For every new feature that is being developed, the developer creates a new branch.

- At times, a single branch would be used to deliver a large feature, or prepare for a release.

- Before creating a branch, make sure that all the upstream changes from the main branch is maintained.

- Make sure that you are in the right branch before pulling the commits.

- The checked-out branch must have a “*” as a prefix to the name. If the returned value is not main, then switch to main.

- A new Git branch can be created from the current branch.

.. raw:: html
   
   </details>
   

Git Workflow
============

.. raw:: html 

  <details> 
  <summary><a>Click here to expand</a></summary>

In Git, the following two aspects are applicable:

- Gitflow

- Dev Branches

**1. Setting up the Git Config**

To perform the Git configuration, use the following:

- **$ git config --global user.name ‘Your Name’**

- **$ git config --global user.email ‘Your.Name@yourdomain.com’**

**2. Cloning a Repository**

To clone a repository, perform the procedure below.

1. Login to your GitHub account, and navigate to the above created fork.

2. Above the list of files, click the following tab.

   .. image:: images/code-button.png

3. To clone the repository using HTTPS, under the Clone with HTTPS section, click the clipboard. To clone the repository using a SSH key, including a certificate issued by your organization's SSH certificate authority, click **Use SSH**, then click the clipboard.

4. Open Git Bash.

5. Change the current working directory to the location where you want the cloned directory.

6. Type **git clone**, and then paste the URL you copied earlier. It will look like this, with your GitHub username instead of **YOUR-USERNAME**.

   ::
  
    $ git clone https://github.com/YOUR-USERNAME/repository name

7. Press **Enter**. Your local clone will be created. A local copy of your fork of the repository is created.

**3. Pushing your Branch**

To push the new dev branch to the remote repo, perform the following:

1. Configure Git to always push using the current branch.

   ::
   
    $ git config --global push.default current

2. Push a local branch to a different remote branch.

   ::
   
    $ git push origin <local_branch>:<remote_branch>
    
**4. Syncing the main Branch**

- Make your code changes, and commit.

  - When main is the branch, and is ready to pull the updates:

    ::
    
     $ git pull origin main
    
- With each commit, there would be additions and deletions. The following command provides an updated list of files.

  ::
 
   $ git status

- Run the following command to address additions and deletions.

  ::
  
   $ git add –all

- When the updates are presented differently, under the heading of Changes to be committed, run the following:

  ::
   
   $ git commit -m "<type>(<scope>): <subject>"
   

**5. Forking the Repository**
  
A fork is a copy of a repository. Forking a repository allows you to freely experiment with changes without affecting the original project i.e., creating a “fork” is producing a personal copy of some external contributor repository which act as a sort of bridge between the original repository and your personal copy.

.. image:: images/fork.PNG

Image Source: `Click here <https://www.toolsqa.com/git/git-fork/>`_

**5.1 How does Forking (Git Fork) work?**

A contributor can use forks to propose changes related to fixing a bug rather than raising an issue for the same so he that he can:

- Fork a repository

  ::
  
   curl -u $github_user_name
   
   https://api.github.com/repos/$upstream_repo/$upstream_repo_name/forks -d ''


To fork a repository, perform the procedure mentioned below.

1. Login to the GitHub account.

2. Navigate to the relevant repository.

3. In the top-right corner of the page, click **Fork**. A fork of the required repository is created successfully.

**5.2 Forking and Performing changes**

- Create a local clone of your fork by running the following command.

  ::
  
   git clone <URL of your fork>>
   
- Verify the new upstream repository you've specified for your fork by running the following command.

  ::
  
   git remote –v

- Pushing code changes to your fork.

- Send changes to Original Repository via Pull Request (PR).

  - You can contribute back to the original repository by sending a request to the original author to pull your fork into their repository by submitting a pull request.
  
.. image:: images/cent.PNG

Image Source: `Click here <https://www.toolsqa.com/git/git-fork/>`_

**Note**: Forking is allowed for public repositories without permission but if the repository is private, the contributor can only be able to fork if he/she has required permission from the owner/admin of the repository. 
    
**6. Advantages of Forking**

- Improving some other contributor's code 

- Reusing the code in a project 

- Reduce license cost consumed per user or contributor 

**7. Forking and Cloning**

- "forked" repositories and "forking" are not special operations. Forked repositories are created using the standard git clone command. Forked repositories are generally server-side clones.  

- There is no unique Git command to create forked repositories. A clone operation is essentially a copy of a repository and its history. 

- Upstream - Upstream branches are closely associated with remote branches and define the branch tracked on the remote repository by your local remote branch (also called as remote tracking branch)

.. image:: images/forkingcloning.PNG

**8. Syncing the Fork with Repository**
  
To configure Git to sync with the fork, perform the following:

1. Clone your project by running the following command.

   ::
   
    $ git clone https://github.com/YOUR-USERNAME/<repository name>

2. List the current configured remote repository for your fork by running the following command.

   ::

    $ git remote -v

3. Specify a new remote upstream repository that will be synced with the fork, by running the following command.

   ::
   
    $ git remote add upstream https://github.com/YOUR-USERNAME/<repository name>

4. Make you origin repository same as an upstream repository, by running the below mentioned command.

   ::
   
    $ git fetch upstream   

5. Now checkout to your main branch by running the below mentioned command, if you are already not checked out.

   ::
   
   $ git checkout main
   
6. Run the below mentioned command.

   ::
   
    $ git merge upstream/main
    
   Now your local repository is synced with the upstream repository and you can make changes to your local repository, and pull to the upstream repository.
     
**9. Pull Request**
  
To create a pull request on GitHub, navigate to the main page of the respective repository, and perform the following:

1. Select the appropriate branch from the **Branch** drop-down menu.

   .. image:: images/mergepatch.PNG
   
2. Click **Compare & Pull Request**.
3. Type a title and description for your pull request.

   .. image:: images/contributing.PNG
   
4. Select the reviewers using the menu on the right-side of the window.
5. Click **Create Pull Request**. The pull request is raised successfully.

   .. image:: images/cpr.PNG
   
.. raw:: html
   
   </details>


******
Codacy
******

Codacy is an automated code analysis or quality tool that enables a developer to deliver effective software in a faster and seamless manner.

.. raw:: html

    <details>
   <summary><a>Working of Codacy</a></summary>


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
   
.. raw:: html
   
   </details>
   
********************************************************************
Developer Certificate of Origin and Contributor License Agreement
********************************************************************
 
CORTX always requires DCO and may require CLA.  To learn more, please refer to `DCO and CLA <https://github.com/Seagate/cortx/blob/main/doc/dco_cla.md>`_.
