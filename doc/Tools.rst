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

 - **Username** – Your Username

 - **Email** - Your email address

 - **Password** - Ensure that the password meets either of the following two conditions:

                      - The word must include at least 15 characters
                      - The word must include at least 8 characters. One numeric value and a   letter in lower case is mandatory.

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

2. Above the list of files, click

   .. image:: images/code-button.png
   :width: 275
   :align: center

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
