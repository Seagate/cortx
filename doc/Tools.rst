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
