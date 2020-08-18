# Cortx-Motr QuickStart guide
This is a step by step guide to get CORTX ready for you on your system.
Before cloning, however, you need to have a SSC / Cloud VM or a local VM setup in either VMWare Fusion or Oracle VirtualBox [LocalVMSetup](LocalVMSetup.md).  If you prefer video, here is a [link to a video](https://seagatetechnology.sharepoint.com/:v:/s/gteamdrv1/tdrive1224/EZbJ5AUWe79DksiRctCtsnUB9sILRr5DqHeBzdrwzNNg6w?e=Xamvex) produced by Seagate engineer Puja Mudaliar following these instructions.

## Accessing the source code right way
(For phase 1) Latest code which is getting evolved, advancing and contributed is on the current github server.
Seagate contributor will be referencing, cloning and committing code to/from this [Github](https://github.com/Seagate/cortx/).

Following steps as sudo user(sudo -s) will make your access to server hassel free.

1. Create SSH Public Key
  * [SSH generation](https://git-scm.com/book/en/v2/Git-on-the-Server-Generating-Your-SSH-Public-Key) will make your key generation super easy. follow the instructions throughly.
  
2. Add New SSH Public Key on [Github](https://github.com/settings/keys) and Enable SSO.
 
WoW! :sparkles:
You are all set to fetch cortx-motr repo now. 

## Cloning CORTX source code
Getting the main CORTX source code on your system is straightforward.


1. `$ git clone --recursive git@github.com:Seagate/cortx-motr.git -b main` (It has been assumed that "git" is preinstalled. if not then follow git installation specific steps provided [here](ContributingToMotr.md/#getting-git--gerit-to-work). Recommended git version is 2.x.x . Check your git version using `$ git --version` command.)
                                                                                                                                                                                           
2. `$ cd cortx-motr`

3. `$ gitdir=$(git rev-parse --git-dir)`

4. Enable some pre-commit hooks required before pushing your changes to remote 
   * Run this command from the parent dir of cortx-motr source
   Note: (This step needs to be validated)
   
     #`$ scp -p -P 29418 g${GID}@gerrit.mero.colo.seagate.com:hooks/commit-msg ${gitdir}/hooks/commit-msg`

## Building the CORTX source code
     
1. Build and install necessaries dependencies
   * To install all dependent packages like lustre, pip, etc.
  
    `$ sudo ./scripts/install-build-deps` 
    
   * Troubleshooting steps:
   * In case pip installation failure using scripts do following
   
     `$ python -m pip uninstall pip setuptools`
     
     `$ sudo ./scripts/install-build-deps`
    
   * If fails with dependency of pip3 , install pip3 using following
    
     `$ sudo yum install -y python34-setuptools`
    
     `$ sudo easy_install-3.4 pip`
    
   * If fails for 'ply' dependency, install ply using following
   
     `$ pip3 install ply`
  
2. Reboot
  * After reboot, check if Lustre network is working
  
     `$ sudo modprobe lnet`
  
     `$ sudo lctl list_nids`

3. Compiling cortx-motr (Commands assumes that user is already into it's main source directory i.e. `$cd cortx-motr`)
   * Run following command
   
   `$ sudo ./scripts/m0 make` or `$sudo ./scripts/m0 rebuild`
   
    Note: `./scripts/m0 rebuild` command includes make along with some clean up.
 
## Running Tests

1. Running Unit Tests (UTs)
 * `$ sudo ./scripts/m0 run-ut` (This may take a long time, i.e. aprx. 20-30 min) 
    > You can also explore other options of this run-ut command. Try : `$ sudo ./scripts/m0 run-ut -help`
    
2. For kernel space UTs
  * `$ sudo ./scripts/m0 run-kut`
  
3. Running a system test  

  To list all ST's 
  * `$ sudo ./scripts/m0 run-st -l`
  
   As an example for clovis module system test can be run using following command :
  * `$ sudo ./scripts/m0 run-st 52mero-singlenode-sanity`
   
   To run all the ST's,
  * `$ sudo ./scripts/m0 run-st`
  
All done! You're now CORTX-Motr-ready.
  
## Running Jenkins / System tests

TODO

### You're all set & You're awesome

In case of any query feel free to write to our [SUPPORT](SUPPORT.md).

Let's start without a delay to contribute to seagate's open source initiative and join this movement with us keeping a common goal of making data storage better, more efficient and more accessible.

Seagate welcomes You! :relaxed:

