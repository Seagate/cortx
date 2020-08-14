## Code reviews and commits

### GitHub setup

#### Generate SSH key on your development box (Needed if you wish to use SSH for clone)
> `$ ssh-keygen -t rsa`

#### Add SSH key to GitHub Account and Enable SSO
  1. Copy public key i.e. id_rsa.pub (default location: /root/.ssh/id_rsa.pub)
  2. Go to GitHub SSH key settings: https://github.com/settings/keys of your account
  3. Select Enable SSO option and Authorize for this key

#### Token (personal access) for command line (Required for submodule clone process)
  - Create a personal access token and use it in place of a password when performing Git operations over HTTPS with Git on the command line or the API.
  - Reference Article, [Creating a personal access token for the command line](https://help.github.com/en/github/authenticating-to-github/creating-a-personal-access-token)
  - In this new token, please make sure that you have enabled SSO.


### Git setup on development box
- Update Git to the latest version.
With older git version, you might be getting errors with commit hook, like this one:

  > $ git commit
  > git: 'interpret-trailers' is not a git command. See 'git --help'.
cannot insert change-id line in .git/COMMIT_EDITMSG

  Fix (for CentOS 7.x):

  > $ yum remove git

  > $ yum -y install https://packages.endpoint.com/rhel/7/os/x86_64/endpoint-repo-1.7-1.x86_64.rpm

  > $ yum -y install git

- Setup the git config options

  > $ git config --global user.name ‘Your Name’

  > $ git config --global user.email ‘Your.Name@seagate.com’

  > $ git config --global color.ui auto


### To work on a feature and submit review to GitHub

#### Clone cortx-sspl
- Each contributor needs to do 'fork' to create their own private cortx-sspl repository.
  - Go to homepage of [cortx-sspl repository on GitHub](https://github.com/Seagate/cortx-sspl), there you will see 'fork' at top right corner.

  > $ git clone --recursive git@github.com:"your-github-id"/cortx-sspl.git

- Setup upstream repo in the remote list. (It's one-time activity)

  > $ git remote -v (See the current configured remote repository for your fork.)
      - origin git@github.com:<gitgub-id>/cortx-sspl.git (fetch)
      - origin git@github.com:<github-id>/cortx-sspl.git (push)

  > $ git remote add upstream git@github.com:Seagate/cortx-sspl.git

  > $ git remote -v (Upstream repo should be visible)
      - origin git@github.com:<gitgub-id>/cortx-sspl.git (fetch)
      - origin git@github.com:<github-id>/cortx-sspl.git (push)
      - upstream git@github.com:Seagate/cortx-sspl.git (fetch)
      - upstream git@github.com:Seagate/cortx-sspl.git (push)

- Ensure you have checked out “dev” branch

  > $ git checkout dev

  > $ git checkout -b 'your-local-branch-name'

- Make code changes

- Add files to be pushed to git to staged area

  > $ git add "File path which are modified"

- Add all such files

- Commit your changes

  > git commit -m "Appropriate Feature/Change Description"

- Check git log to see your commit, verify the author name

   > $ git log

- If author name is not set properly then set using following command

  > git commit --amend --author="Author Name < email@address.com >"

- Push your changes to GitHub
  * Before pushing, TAKE A PAUSE. Have you [rebased your branch](#How-to-rebase)? If not then doing right now to avoid merge conflicts on github.

  > $ git push origin 'your-local-branch-name'

  * Note: Sometimes, It fails to push local branch changes to origin after rebase. Use --force option to push it forcefully.

  > $ git push origin 'your-local-branch-name'

  > $ git push --force origin 'your-local-branch-name'


### Open pull request for review
- Open the URL given in the output of 'git push' command above.
- Select base:branch as 'dev' from the dropdown.

- click 'Create pull request' to create the pull request.
- Add reviewers to get feedback on your changes.


### Running Jenkins / System tests
- Jenkins job will get triggered automatically after every check-in in the dev branch of Seagate/sspl.


### How to rebase your local branch on latest master?

  > $ git checkout dev

  > $ git fetch upstream

  > $ git pull upstream dev

  > $ git checkout 'your-local-branch'

  > $ git rebase upstream/dev

  - If you get conflicts, follow the steps mentioned in the error message from git.

* To get familiar with jenkins please visit [here](https://en.wikipedia.org/wiki/Jenkins_(software)).

### You're all set & You're awesome

In case of any query feel free to write to our [SUPPORT](SUPPORT.md).

Let's start without a delay to contribute to seagate's open source initiative and join this movement with us keeping a common goal of making data storage better, more efficient and more accessible.

Seagate welcomes You! :relaxed:
