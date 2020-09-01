# Contribute to the CORTX Project

Have you ever wanted to solve the world's storage issues? We'll help you understand [CORTX—the coolest project on storage solution](https://github.com/Seagate/cortx), how it's organized, and the best places to get started with contributing. 

After reading this guide, you'll be able to pick up topics and issues to contribute, submit your codes, and get your work reviewd and merged. Feel free to browse through the [list of contrubutions](https://github.com/Seagate/cortx/blob/main/doc/SuggestedContributions.md), and [view and submit issues](https://github.com/Seagate/cortx/issues). 

We welcome all feedback and contributions!

## Contribution Guide

- [**Code of Conduct**](#Code-of-Conduct)
- [**Community Guidelines**](#Community-Guidelines)
- [**Contribution Process**](Contribution_Process)
- [**Submitting issues**](#Submitting_Issues)
- [**Contributing to Documentation**](#Contributing_to_Documentation)

## Code of Conduct

We excited for your interest in CORTX and hope you will join us. We take community very seriously and we are committed to creating a community built on respectful interactions and inclusivity as documented in our [Contributor Covenant Code of Conduct](https://github.com/Seagate/cortx/blob/main/CODE_OF_CONDUCT.md).

## Community Guidelines

To know more about the CORTX community, refer to the [CORTX Community Brief](https://github.com/Seagate/cortx/blob/main/doc/SB510_1-2004US_Seagate_CORTX_Community-Brief_R7.2.pdf)

### CORTX Community Values

The CORTX community is strongly driven by the following Community Values:

<details>
<summary>Click to expand!</summary>
<p>

**Inclusive** - Our ambitions are global. The CORTX community is, too. The perspectives and skills necessary to achieve our goals are wide and varied; we believe in creating a community and a project that is inclusive, accessible, and welcoming to everyone.

**Open** - We are dedicated to remaining open and transparent. We believe in keeping CORTX Community code freely and fully available to be viewed, modified, and used without vendor lock in or other in-built limitations.

**Inspired** - CORTX is all about the challenge. Our goals are not small: we want to build the world’s best scalable mass-capacity object storage system, one that can work with any hardware and interoperate with all workloads. CORTX is built on hard work, ingenuity and an engineering mindset. We embrace hard problems and find inspired solutions.

**Evolving** – CORTX is continuously growing and adapting. As a community project, there is no limit to its development. We continuously make room for improvement and welcome the opportunities offered by the ever-evolving nature of community projects.

</p>
</details>

## Contribution Process

<details>
<summary>Prerequisites</summary>
<p>

- Please read our [Code Style Guide](https://github.com/Seagate/cortx/blob/main/doc/CodeStyle.md).

- Before you set up your GitHub, you'll need to

  1. Generate the SSH key on your development box using:

      ```shell

       $ ssh-keygen -o -t rsa -b 4096 -C "seagate-email-address"
      ```
  2. Add the SSH key to your GitHub Account:
    1. Copy the public key: `id_rsa.pub`. By default, your public key is located at `/root/.ssh/id_rsa.pub`
    2. Navigate to [GitHub SSH key settings](https://github.com/settings/keys) on your GitHub account.
      
      :page_with_curl:**Note:** Ensure that you've set your Seagate Email ID as the Primary Email Address associated with your GitHub Account. SSO will not work if you do not set  your Seagate Email ID as your Primary Email Address.
    3. Paste the SSH key you generated in Step 1 and select *Enable SSO*.
    4. Click **Authorize** to authorize SSO for your SSH key.
    5. [Create a Personal Access Token or PAT](https://help.github.com/en/github/authenticating-to-github/creating-a-personal-access-token).

      :page_with_curl:**Note:** Ensure that you have enabled SSO for your PAT.
      
 - Update Git to the latest version. If you're on an older version, you'll see errors in your commit hooks that look like this:

    `$ git commit`

  Sample Output
  
    ```shell

    git: 'interpret-trailers' is not a git command.
    See 'git --help'
    cannot insert change-id line in .git/COMMIT_EDITMSG
    ```

- Install Fix for CentOS 7.x by using: `$ yum remove git`

    Download the [RPM file from here](https://packages.endpoint.com/rhel/7/os/x86_64/endpoint-repo-1.7-1.x86_64.rpm) and run the following commands:

    ```shell

    $ yum -y install
    $ yum -y install git
    ```

   </p>
    </details>

Contributing to the CORTX repository is a four-step process where you'll need to:

1. Set up Git on your Development Box
2. Clone the CORTX repository
3. Commit your Code
4. Create a Pull Request
5. Run Jenkins and System Tests

### 1. Setup Git on your Development Box

Once you've installed the prerequisites, follow these steps to set up Git on your Development Box:

1. Install git-clang-format using: `$ yum install git-clang-format`

2. Set up git config options using:

   ```shell

   $ git config --global user.name ‘Your Name’
   $ git config --global user.email ‘Your.Name@domain_name’
   $ git config --global color.ui auto
   $ git config --global credential.helper cache
   ```

### 2. Clone the CORTX repository

<details>
<summary>Click to expand!</summary>
<p>
Before you can work on a GitHub feature, you'll need to clone the repository you're working on. You'll need to **Fork** the repository to clone it into your private GitHub repository and follow these steps:

1. Navigate to the repository homepage on GitHub.
2. Click **Fork**
3. Run the following commands in Shell:

   `$ git clone git@github.com:"your-github-id"/repo-name.git`

4.  You'll need to setup the upstream repository in the remote list. This is a one-time activity.

    1. Run the following command to view the configured remote repository for your fork.
    
       `$ git remote -v`  

      Sample Output:
    
      ```shell
    
      origin git@github.com:<gitgub-id>/cortx-sspl.git (fetch)
      origin git@github.com:<github-id>/cortx-sspl.git (push)
      ```

   2. Set up the upstream repository in the remote list using:
   
      `$ git remote add upstream git@github.com:Seagate/reponame.git

      `$ git remote -v`

      Sample Output:
    
      ```shell
    
      origin git@github.com:<gitgub-id>/cortx-sspl.git (fetch)
      origin git@github.com:<github-id>/cortx-sspl.git (push)
      upstream git@github.com:Seagate/cortx-sspl.git (fetch)
      upstream git@github.com:Seagate/cortx-sspl.git (push)
      ```
    
5. Check out to your branch using:

   `$ git checkout <branchname>`

   `$ git checkout -b 'your-local-branch-name`
   
   :page_with_curl: **Note:** By default, you'll need to contribute to the main branch. However, this may differ for some repositories. 

</p>
</details>

### 3. Commit your Code 

<details>
<summary>Click to expand!</summary>
<p>

:page_with_curl: **Note:** At any point in time to rebase your local branch to the latest main branch, follow these steps:

  ```shell

  $ git pull origin master
  $ git submodule update --init --recursive
  $ git checkout 'your-local-branch'
  $ git pull origin 'your-remote-branch-name'
  $ git submodule update --init --recursive
  $ git rebase origin/master
  ```
  
You can make changes to the code and save them in your files.

1. Use the command below to add files that need to be pushed to the git staging area:

- `$ git add foo/somefile.cc`

2. To commit your code changes use:

   `$ git commit -m ‘comment’` - Enter your GitHub Account ID and an appropriate Feature or Change description in comment.

3. Check out your git log to view the details of your commit and verify the author name using: `$ git log`

  :page_with_curl:**Note:** If you need to change the author name for your commit, refer to the GitHub article on [Changing author info](https://docs.github.com/en/github/using-git/changing-author-info).

4. To Push your changes to GitHub, use: `$ git push origin 'your-local-branch-name'`

   Your output will look like the Sample Output below:

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
   remote: To github.com:<your-GitHub-Id>/cortx-s3server.git
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
3. Select **main** from the Branches/Tags drop-down list.
4. Click **Create pull request** to create the pull request.
5. Add reviewers to your pull request to review and provide feedback on your changes.

</p>
</details>

### 5. Run Jenkins and System Tests

Creating a pull request automatically triggers Jenkins jobs and System tests. To familiarize yourself with jenkins, please visit the [Jenkins wiki page](https://en.wikipedia.org/wiki/Jenkins_(software)).

### CLA and DCO 

## Submitting Issues

## Contributing to Documentation

## Resources 

Refer to these Quickstart Guides to build and contribute to the CORTX project.

<details>
<summary>Click to expand!</summary>
<p>

- Provisioner
- Motr
- S3 Server
- CSM

</p>
</details>

## Communication Channels


- Join the CORTX Slack Channel [![Slack](https://img.shields.io/badge/chat-on%20Slack-blue")](https://join.slack.com/t/cortxcommunity/shared_invite/zt-femhm3zm-yiCs5V9NBxh89a_709FFXQ?) and chat with your fellow contributors 

- Mail Id [opensource@seagate.com](opensource@seagate.com)

- You can start a thread in the [Github Community](https://github.com/orgs/Seagate/teams/cortx-community/discussions) for any questions, suggestions, feedback, or discussions with your fellow community members. 

## Thank You!

We thank you for stopping by to check out the CORTX Community. We are fully dedicated to our mission to build open source technologies that help the world save unlimited data and solve challenging data problems. Join our mission to help reinvent a data-driven world.
