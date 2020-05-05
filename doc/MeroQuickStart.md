# Mero QuickStart guide
This is a step by step guide to get CORTX ready for you on your system.
Before cloning, however, you need to have a SSC / Cloud VM or a local VM setup in either VMWare Fusion or Oracle VirtualBox [LocalVMSetup](LocalVMSetup.md).  If you prefer video, here is a [link to a video](https://seagatetechnology.sharepoint.com/:v:/s/gteamdrv1/tdrive1224/EZbJ5AUWe79DksiRctCtsnUB9sILRr5DqHeBzdrwzNNg6w?e=Xamvex) produced by Seagate engineer Puja Mudaliar following these instructions.

## Accessing the code right way
(For phase 1) Latest code which is getting evolved, advancing and contributed is on the gerrit server.
Seagate contributor will be referencing, cloning and committing code to/from this [Gerrit server](http://gerrit.mero.colo.seagate.com:8080).

Following steps as sudo user(sudo -s) will make your access to server hassel free.

1. Create SSH Public Key
  * [SSH generation](https://git-scm.com/book/en/v2/Git-on-the-Server-Generating-Your-SSH-Public-Key) will make your key generation super easy. follow the instructions throughly.
2. Add SSH Public Key on [Gerrit server](http://gerrit.mero.colo.seagate.com:8080).
  * Log into the gerrit server with your seagate gid based credentials.
  * On right top corner you will see your name, open drop down menu by clicking and choose settings.
  * In the menu on left, click SSH Public Keys, and add your public key (which is generated in step one) right there.

WoW! :sparkles:
You are all set to fetch mero repo now. 

## Cloning CORTX
Getting the main CORTX code on your system is straightforward.
1. `$ sudo -s`
2. `$ cd path/to/your/dev/directory`
3. `$ export GID=<your_seagate_GID>` # this will make subsequent sets easy to copy-paste :)
4. `$ git clone --recursive "ssh://${GID}@gerrit.mero.colo.seagate.com:29418/mero" -b innersource` (It has been assumed that "git" is preinstalled. if not then follow git installation specific steps provided [here](#getting-git--gerit-to-work). Recommended git version is 2.x.x . Check your git version using `$ git --version` command.)
(If "Permission denied (publickey). fatal: Could not read from remote repository" error occurs while using ssh in this step then use the following alternate command) `$ git clone --recursive "http://gerrit.mero.colo.seagate.com/mero" -b innersource`                                                                                                                                                                                           
5. `$ cd mero`
6. `$ gitdir=$(git rev-parse --git-dir)`
7. Enable some pre-commit hooks required before pushing your changes to remote (command to be run from the parent dir of Mero source).
  * `$ scp -p -P 29418 ${GID}@gerrit.mero.colo.seagate.com:hooks/commit-msg ${gitdir}/hooks/commit-msg`
8. Build necessaries dependencies
  * To install all dependent packages like lustre, pip, etc.
  
    `$ ./scripts/install-build-deps` 
   * Troubleshooting steps:
   * In case pip installation failure using scripts do following
   
     `$ python -m pip uninstall pip setuptools`
     
     `$ Run script ./scripts/install-build-deps`
    
   * If fails with dependency of pip3 , install pip3 using following
    
     `$ yum install python34-setuptools`
    
     `$ easy_install- 3.4 pip`
    
   * If fails for 'ply' dependency, install ply using following
   
     `$ pip3 install ply`
  
9. Reboot
  * After reboot, check if Lustre network is working
  
     `$ sudo modprobe lnet`
  
     `$ sudu lctl list_nids`

    
  
## Compilation and Running Unit Test

All following commands assumes that user is already into it's main source directory.

1. building mero
 * `$ sudo ./scripts/m0 make` or `$sudo ./scripts/m0 rebuild`
   * $ ./scripts/m0 rebuild command includes make along with some clean up.
  
2. Running Unit Tests (UTs)
 * `$ sudo ./scripts/m0 run-ut`
    > Feel free to expore other options of this run-ut command. Try : sudo run-ut --help
    
3. For kernel space UTs
  * `$ sudo ./scripts/m0 run-kut`
  
4. Running a system test  

  To list all ST's 
  * `$ sudo ./scripts/m0 run-st -l`
  
   As an example for clovis module system test can be run using following command :
  * `$ sudo ./scripts/m0 run-st 52mero-singlenode-sanity`
   
   To run all the ST's,
  * `$ sudo ./scripts/m0 run-st`
  
KABOOM!!!
  
## Running Jenkins / System tests

TODO

### You're all set & You're awesome

In case of any query feel free to write to our [SUPPORT](SUPPORT.md).

Let's start without a delay to contribute to seagate's open source initiative and join this movement with us keeping a common goal of making data storage better, more efficient and more accessible.

Seagate welcomes You! :relaxed:

