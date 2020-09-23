## Working with git for CORTX

1. Generate an SSH key on your development box using:

     ```shell
     $ ssh-keygen -o -t rsa -b 4096 -C "Email-address"
     ```
  2. Add the SSH key to your GitHub Account:
    1. Copy the public key: `id_rsa.pub`. By default, your public key is located at `/root/.ssh/id_rsa.pub`
    2. Navigate to [GitHub SSH key settings](https://github.com/settings/keys) on your GitHub account.
      
    :page_with_curl:**Note:** Ensure that you've set your Email ID as the Primary Email Address associated with your GitHub Account. 
    
    3. Paste the SSH key you generated in Step 1 and click **Add SSH key**.
    5. [Create a Personal Access Token or PAT](https://help.github.com/en/github/authenticating-to-github/creating-a-personal-access-token).
      
 - Update Git to the latest version. If you're on an older version, you'll see errors in your commit hooks that look like this:

    `$ git commit`

     **Sample Output**
  
    ```shell

    git: 'interpret-trailers' is not a git command.
    See 'git --help'
    cannot insert change-id line in .git/COMMIT_EDITMSG
    ```

- Install Fix for CentOS 7.x by using: `$ yum remove git`
  * Download the [RPM file from here](https://packages.endpoint.com/rhel/7/os/x86_64/endpoint-repo-1.7-1.x86_64.rpm) and run the following commands:
  
    ```shell
       $ yum -y install
       $ yum -y install git
    ```

   </p>
    </details>
