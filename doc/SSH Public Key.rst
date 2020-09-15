Generating SSH Public Key
-------------------------
Perform the below mentioned procedure to generate the SSH public key.

1. Navigate to `SSH Generation <https://git-scm.com/book/en/v2/Git-on-the-Server-Generating-Your-SSH-Public-Key>`_. A page with the *4.3 Git on the Server - Generating Your SSH Public Key* section opens.

2. Run the commands mentioned below.
 
 - **$ cd ~/.ssh**

 - **$ ssh-keygen -o**

 The key is generated successfully.

3. Copy the generated key, and navigate to `SSH and GPG Keys <https://github.com/settings/keys>`_.

4. Click **New SSH Key**, and paste the key that you had copied.

5. Select **Enable SSO** from the  drop-down list.

   - Remember to click on **Authorize** at the dialog pop up to authorize the SSH key using Seagate SSO.
