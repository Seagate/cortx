## Working with Git for CORTX

Contributing to the CORTX project using Git is a four-step process as listed below.

### 1. Prerequisities

<details>
     <summary>Click to expand!</summary>
     <p>

 - Update Git to the latest version.
 - Generate [SSH](SSH_Public_Key.rst) and [PAT](Tools.rst#personal-access-token-pat) access for your GitHub Account.
 - Install Fix for CentOS 7.x by using: 
 
 `$ yum remove git`
 
  * Download the [RPM file from here](https://packages.endpoint.com/rhel/7/os/x86_64/endpoint-repo-1.7-1.x86_64.rpm) and run the following commands:
  
    ```shell
       $ yum -y install
       $ yum -y install git
    ```

   </p>
    </details>
    
### 2. Clone the CORTX Repository

Before you contribute to the CORTX project, you'll have to **Fork** the CORTX repository to clone it into your private GitHub repository.

<details>
     <summary>Click to expand</summary>
     <p>

1. Navigate to the repository homepage on GitHub.
2. Click **Fork**
3. Run the following commands in Shell:
   
   `$ git clone --recursive https://github.com/Seagate/<repository>.git`

4. You'll need to setup the upstream repository in the remote list. This is a one-time activity. Run the following command to view the configured remote repository for your fork.
    
   `$ git remote -v`  

    **Sample Output:**
    
    ```shell
    
     origin git@github.com:<gitgub-id>/cortx-sspl.git (fetch)
     origin git@github.com:<github-id>/cortx-sspl.git (push)
     ```

 5. Set up the upstream repository in the remote list using:
   
    `$ git remote add upstream https://github.com/Seagate/<repository>.git`
      
    `$ git remote -v`

     **Sample Output:**
    
     ```shell
    
     origin git@github.com:<gitgub-id>/cortx-sspl.git (fetch)
     origin git@github.com:<github-id>/cortx-sspl.git (push)
     upstream git@github.com:Seagate/cortx-sspl.git (fetch)
     upstream git@github.com:Seagate/cortx-sspl.git (push)
     ```
    
6. Check out to your branch using:

   `$ git checkout "branchname"`

   `$ git checkout -b 'your-local-branch-name`
   
    :page_with_curl: **Note:** By default, you'll need to contribute to the main branch. 

</p>
</details>

### 3. Commit your Code 

<details>
<summary>Click to expand!</summary>
<p>

:page_with_curl: **Note:** At any point in time to rebase your local branch to the latest main branch, follow these steps:

  ```shell

  $ git pull origin main
  $ git submodule update --init --recursive
  $ git checkout 'your-local-branch'
  $ git pull origin 'your-remote-branch-name'
  $ git submodule update --init --recursive
  $ git rebase origin/main
  ```
  
You can make changes to the code and save them in your files.

1. Use the command below to add files that need to be pushed to the git staging area:

    `$ git add foo/somefile.cc`

2. To commit your code changes use:

   `$ git commit -s -m ‘comment’` - enter your GitHub Account ID and an appropriate Feature or Change description in comment.

3. Check out your git log to view the details of your commit and verify the author name using: `$ git log`

   :page_with_curl:**Note:** If you need to change the author name for your commit, refer to the GitHub article on [Changing author info](https://docs.github.com/en/github/using-git/changing-author-info).

4. To Push your changes to GitHub, use: `$ git push origin 'your-local-branch-name'`

   **Sample Output**

   ```shell

   Enumerating objects: 4, done.
   Counting objects: 100% (4/4), done.
   Delta compression using up to 2 threads
   Compressing objects: 100% (2/2), done.
   Writing objects: 100% (3/3), 332 bytes | 332.00 KiB/s, done.
   Total 3 (delta 1), reused 0 (delta 0)
   remote: Resolving deltas: 100% (1/1), completed with 1 local object.
   remote:
   remote: Create a pull request for 'your-local-branch-name' on GitHub by visiting:
   remote: https://github.com/<your-GitHub-Id>/cortx-s3server/pull/new/<your-local-branch-name>
   remote: To github.com:<your-GitHub-Id>/reponame.git
   * [new branch] <your-local-branch-name> -> <your-local-branch-name>
   ```
</p>
</details>

### 4. Create a Pull Request 

<details>
<summary>Click to expand!</summary>
  <p>       
   
1. Once you Push changes to GitHub, the output will display a URL for creating a Pull Request, as shown in the sample code above.

   :page_with_curl:**Note:** To resolve conflicts, follow the troubleshooting steps mentioned in git error messages.

2. You'll be redirected to GitHib remote.
3. Select the relevant repository branch from the *Branches/Tags* drop-down list.
4. Click **Create pull request** to create the pull request.
5. Add reviewers to your pull request to review and provide feedback on your changes.

</p>
</details>

## Create an Issue 

Perform the below mentioned procedure to create an issue in GitHub:

1. Login to GitHub with your credentials.
2. Navigate to the CORTX repository or relevant submodule. Then, click Issues. List of issues are displayed.
3. If there are multiple issue types, click Get started next to the type of issue you'd like to open.
4. Click New Issue. A page requesting the Title and Description is displayed.
5. Enter a title and description for your issue, and click Submit new issue.

:page_with_curl: **Note:** Click Open a blank issue if the type of issue you want to open, is not included in the available different types of issues.


