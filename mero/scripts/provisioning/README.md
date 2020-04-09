Build and test environment for Mero
===================================

Quick Start (MacOS)
-------------------

* Install
    - [Homebrew](https://brew.sh/)

      ```bash
      /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
      ```

    - `bash` 4.x

          brew install bash

    - GNU `readlink`

          brew install coreutils

    - [VirtualBox](https://www.virtualbox.org/wiki/Downloads)
    - [Vagrant](https://www.vagrantup.com/downloads.html)
    - Vagrant plugins

          vagrant plugin install vagrant-{env,hostmanager,scp,vbguest}

    - Ansible

          brew install ansible

* Configure
    - `m0vg` script (make sure you have `$HOME/bin` in the `$PATH`)

      ```bash
      MERO_SRC=~/src/mero  # use the actual Mero location on your host system
      ln -s $MERO_SRC/scripts/m0vg $HOME/bin/
      ```

    - VMs

      ```bash
      # open virtual cluster configuration file in default editor
      m0vg env edit
      ```

      paste the following template updating parameters as desired:

      ```bash
      # a host directory to share among all VMs,
      # by default it's a parent of $MERO_SRC dir
      #M0_VM_SHARED_DIR=~

      # default sharing mechanism is NFS, which is recommended,
      # but in case it doesn't work for some reason, a virtual
      # provider specific sharing mechanism can be used (e.g.
      # VirtualBox shared folder)
      #M0_VM_SHARE_TYPE=provider

      # a comma-separated list of additional packages to be installed
      # on each VM (they must be available in default `yum` repositories
      # or EPEL)
      #M0_VM_EXTRA_PKGS=python36,python36-pip

      # a script executed on CMU node after provisioning is finished,
      # a `sudo` can be used in the script to gain root privileges
      #M0_VM_PROVISION_SCRIPT=~/vagrant-postinstall.sh

      # amount of RAM available on CMU node
      #M0_VM_CMU_MEM_MB=4096

      # number of client VMs
      #M0_VM_CLIENT_NR=2
      # amount of RAM available on every client node
      #M0_VM_CLIENT_MEM_MB=3072

      # number of ssu VMs
      #M0_VM_SSU_NR=3
      # amount of RAM available on every ssu node
      #M0_VM_SSU_MEM_MB=2048
      # number of data drives on every ssu node
      #M0_VM_SSU_DISKS=12
      # size of each data drive on ssu node
      #M0_VM_SSU_DISK_SIZE_GB=8
      ```

      see `m0vg params` output for the full list of supported configuration
      parameters

* Run
    - check VMs state

          m0vg status

    - create _cmu_ VM (this can take ~30 minutes depending on the internet
      connection, CPU and system disk speed)

          m0vg up cmu

    - restart _cmu_ VM in order to activate shared folder

          m0vg reload cmu

    - logon on _cmu_ and check contents of `/data` dir

          m0vg tmux
          ls /data

    - create _ssu_ and _client_ VMs (can take about ~40 minutes depending on the
      number of configured _ssu_ and _client_ nodes)

          m0vg up /ssu/ /client/
          m0vg reload /ssu/ /client/

    - stop all nodes when they're not needed to be running

          m0vg halt

    - if a node hangs (e.g. Mero crash in kernel or deadlock) it can be forced
      to shutdown using `-f` option for `halt` command, for example:

          m0vg halt -f client1

Quick Start (Windows)
---------------------

* Install
    - [VirtualBox](https://www.virtualbox.org/wiki/Downloads)
    - [Vagrant](https://www.vagrantup.com/downloads.html)
    - Vagrant plugins

          vagrant plugin install vagrant-{env,hostmanager,scp,vbguest}

    - [Git for Windows](https://git-scm.com/download/win)
      During installation, when asked, choose the following options (keep other options to their default setting):

      - _Use Git and optional Unix tools from the Command Prompt_
      - _Checkout as-is, commit Unix-style line ending_
      - _Enable symbolic links_

* Configure

    - Open _Git Bash_ terminal, add CRLF configuration option to make sure that Mero/Halon scripts can work on VM

      ```bash
      git config --global core.autocrlf input
      ```

    - Clone Mero repository somewhere, just as an example let's say it's in `$HOME/src/mero`

    - Create a persistent alias for `m0vg` script:

      ```bash
      cat <<EOF >> $HOME/.bash_profile
      # use the actual Mero location on your host system
      export MERO_SRC=$HOME/src/mero
      alias m0vg="\$MERO_SRC/scripts/m0vg"
      EOF
      ```

      Exit and re-launch _Git Bash_ terminal. At this point the setup should be complete.

* Run

    - Follow the steps from _Run_ section under _Quick Start (MacOS)_ above.

      > *NOTE*: during `m0vg up cmu` command execution you may be asked to enter
      > your Windows username and password, and then grant permissions for
      > creating Windows shared directory - that is required to enable
      > passwordless ssh access from _cmu_ VM to other VMs, it will be asked
      > only once when creating _cmu_ VM for the first time.

Overview
--------

This directory contains scripts for quick deployment of a "devvm" virtual
machine (based on a stock _Centos7_ by default), prepared for Mero development
and testing on a local desktop or a laptop.

The virtual machine is automatically created from the [official _Centos7_ base
image](https://app.vagrantup.com/centos/boxes/7), which is downloaded from the
[vagrantcloud](https://vagrantcloud.com/search) repository. After provisioning
and installation of the required rpm packages, including build tools and latest
_Lustre_ from Whamcloud's repository, it takes about 2.5GB of extra disk space
per VM.

Besides main virtual machine, which can be used as a build node, additional
machines can be provisioned as well to provide a cluster-like environment for
debugging and testing mero on multiple nodes. The main machine is named _cmu_,
machines with attached disks are named _ssuN_, and "client" machines are named
_clientN_, where _N_ is a natural number.

All machines are accessible by names (with `.local` suffix) within their private
network, with password-less ssh access from the main node to other test nodes. A
directory containing mero source code is shared with each machine over _NFS_.
This should provide a short-enough "hack/build/test" cycle for efficient
development workflow.

Depending on the host OS, different virtualization providers are supported. On
_Linux_ those are _Libvirt/KVM_ and _VirtualBox_. On _Mac OS_ - _VirtualBox_
and _VMware Fusion_.

Requirements
------------

In order to run these scripts, additional tools have to be installed first. It's
assumed that either _Mac OS_ or _Linux_ is used as a host operating system. It
should work on a _Windows_ host as well, though some additional configuration
steps may be required.

* Minimum Host OS
    - 8GB of RAM
    - 10GB of free disk space
    - 2 CPU cores

* Additional Software/Tools:
    - [VirtualBox](https://www.virtualbox.org/wiki/Downloads) _OR_
      [VMware Fusion](https://www.vmware.com/products/fusion.html) +
      [Vagrant VMware plugin](https://www.vagrantup.com/vmware/index.html) (_Mac OS_ only)
    - `libvirt + qemu-kvm` (_Linux_ only)
    - [Vagrant](https://www.vagrantup.com/downloads.html)
    - [Ansible](https://github.com/ansible/ansible) (_Mac OS_ and _Linux_ only)
    - [Git for Windows](https://git-scm.com/download/win) (_Windows_ only)

On _Ubuntu Linux_ all of the above prerequisites can be installed with a single
command:

    sudo apt install qemu-kvm libvirt-bin vagrant ansible

Though, it's actually better to get a more up-to-day versions of _Vagrant_ and
_Ansible_ than those provided by a distribution. The procedure is same as
described below for _Mac OS_.

On _Mac OS_ the easiest way to install those tools is to download
_VirtualBox_/_VMware Fusion_  and _Vagrant_ packages from their official
web-sites (refer to the links above).

And install _Ansible_ using _Python_ package manager `pip`, which is
available on _Mac OS_ "out of the box":

```bash
# install for current user only
# make sure that '$HOME/.local/bin' is in your PATH
pip install --user ansible

# install system-wide
sudo pip install ansible
```

Another popular alternative is to use [MacPorts](https://www.macports.org/) or
[Homebrew](https://brew.sh/) package managers:

```bash
# install Ansible using MacPorts
sudo port install py36-ansible

# install Ansible using Homebrew
brew install ansible
```

After _Vagrant_ is installed, a couple of plugins need to be installed also. On
_Linux_ it is `vagrant-libvirt` (for _kvm_ support), and on _Mac OS_ it's
`vagrant-vbguest`, when using _VirtualBox_ and `vagrant-vmware-fusion`, when
using _VMware Fusion_:

```bash
# linux with Qemu/KVM
vagrant plugin install vagrant-libvirt

# macos with VirtualBox
vagrant plugin install vagrant-vbguest

# macos with VMware Fusion
vagrant plugin install vagrant-vmware-fusion
```

It's highly recommend to install a few more _Vagrant_ plugins for a better user
experience:

* `vagrant-env` -- for saving commonly used configuration variables in a config file
* `vagrant-hostmanager` -- for managing /etc/hosts file on guest machines
* `vagrant-scp` -- for easier file copying between the host and VM

DevVM provisioning
------------------

After installing required tools from the above section, all that remains is to
run `vagrant up` command in the directory containing this `README` file, that
will do rest of the work. But, there is a better way to achieve the same result
which is more convenient:

    ./scripts/m0vg up

The `m0vg` helper script is a wrapper around _Vagrant_ and _Ansible_ commands
that can be *symlinked* somewhere into the `PATH` and be called from any
directory. Check out `m0vg --help` for more info.

It will spawn a VM and configure it using _Ansible_ "playbook"
`scripts/provisioning/cmu.yml`, that specifies all _Mero_ dependencies which
should be installed in order to build and run _Mero_. It will install _Lustre_
2.10.4 from the [official Whamcloud's
repository](https://downloads.whamcloud.com/public/lustre/lustre-2.9.0/el7/client/).
During provisioning, _Vagrant_ might pause and ask for user password, this is
needed for _NFS_ auto-configuration (it will add a new entry in `/etc/exports`
and restart `nfsd` service).

By default, _Vagrant_ creates a `vagrant` user inside VM with password-less
`sudo` privileges. The user password is also `vagrant`.

When provisioning is finished it should be possible to login into the VM with
`./scripts/m0vg ssh` command. Please, refer to the _Vagrant basics_ section
below for the list of other useful _Vagrant_ commands.

If a cluster-like environment is needed, more machines can be provisioned:

    ./scripts/m0vg up cmu /ssu/ /client/

The additional parameters are also explained in the _Vagrant basics_ section
below.

It is possible to control different parameters of the `Vagrantfile` via
environment variables or `.env` file that should be placed alongside
`Vagrantfile`. For instance, the following two examples do the same thing but
with the latter there is no need to specify env variables every time while
executing a vagrant command, they will be loaded from the `.env` file:

```bash
# -1- using env variables
M0_SSU_NR=5 M0_CLIENT_NR=3 vagrant up

# -2- using env file
cat .env
M0_SSU_NR=5
M0_CLIENT_NR=3

vagrant up
```

By the way, there is no need to create `.env` file manually, `m0vg env edit`
helps with that. A complete list of supported variables is printed by `m0vg
params` command.

All additional nodes can be accessed from the main machine (_cmu_) by their name
in a `.local` domain. For example, here is how to execute a command on the
_ssu1_ from _cmu_:

    ssh ssu1.local <command>

The host directory containing mero sources directory will be mounted over _NFS_
on each VM under `/data`.

> *NOTE*: one important aspect of how _Vagrant_ works is that it creates a hidden
> `.vagrant` directory, alongside `Vagrantfile`, where it keeps all configuration
> data related to provisioned VMs. If that directory is lost the access to the VMs
> is lost as well. Which can happen unintentionally as a result of
> `git clean -dfx`. This is another reason to use `m0vg` script which takes care
> of it by moving `.vagrant` directory outside of mero source tree.

Building and running Mero
-------------------------

Normally, Mero sources should be accessible over _NFS_ (or native
VirtualBox/VMware shared folder) on each VM under `/data` directory:

```bash
# build Mero in source tree
cd /data/mero
./scripts/m0 make
```

If, for some reason, _Vagrant_ hasn't been able to configure the _NFS_ share it
is still possible to copy mero sources to VM with the help of `vagrant-scp`
plugin:

```bash
# on the host
tar -czf ~/mero.tar.gz $MERO_SRC
m0vg scp ~/mero.tar.gz :~

# on VM
cd ~
tar -xf mero.tar.gz
cd mero
./autogen.sh && ./configure && make rpms-notests
```

Resulting _rpm_ files will be available in `~/rpmbuild/RPMS/x86_64/` directory.
To verify them they can be installed with:

    sudo yum install rpmbuild/RPMS/x86_64/*

Vagrant basics
--------------

_Vagrant_ can be thought of as a scriptable unification API on top of various
virtualization providers, like _VirtualBox_, _VMware_, _KVM_ etc. From a user
perspective all virtual machine configuration is done in a single `Vagrantfile`,
which essentially is just a `ruby` script. It's processed every time `vagrant`
command is executed, which expects to find it in the current working directory.

Most common _Vagrant_ commands are:

```bash
# checking status of VM(s), e.g. running, halted, destroyed
vagrant status

# creating a VM if it doesn't exist or starting it if it's stopped;
# if there are provisioning steps, they are performed only once when VM is
# created/started for the first time
vagrant up

# stopping (powering off) a VM in graceful way
vagrant halt

# forcing a VM to power off, as if unplugging power cable
vagrant halt -f

# loggin into VM
vagrant ssh

# repeating provisioning steps (VM should be running)
vagrant provision

# repeating provisioning steps (VM should be stopped)
vagrant up --provision

# destroying VM and all associated data disks
vagrant destroy
```

Most of the _Vagrant_ commands accept VM name as argument if there are multiple
VMs configured in `Vagrantfile`. It is also possible to specify a regular
expression instead of a single name to operate on several VMs:

```bash
# start a 'cmu' VM
vagrant up cmu

# start all SSUs
vagrant up /ssu/

# start all VMs
vagrant up cmu /ssu/ /client/
```

Streamlining VMs creation and provisioning with snapshots
---------------------------------------------------------

It might be useful to save VM state just after provisioning for instant access
to a clean VM without re-doing a complete provisioning from scratch. Please
notice, that it's better when VMs are powered off when snapshot is made:

```bash
# poweroff all VMs
m0vg halt

# create snapshots, 'clean-vm' is just a name of the snapshot and can be
# changed to your liking
m0vg snapshot save cmu clean-vm
m0vg snapshot save ssu1 clean-vm
m0vg snapshot save ssu2 clean-vm
m0vg snapshot save client1 clean-vm
```

Then later, in order to discard the current state and restore a clean VM one may
do:

    m0vg snapshot restore --no-provision cmu clean-vm

If `--no-provision` option is omitted, the _Ansible_ provisioning will be
repeated after the restore phase. It may come in handy for getting latest
security updates for the VM since snapshot creation.

Managing multiple VM sets with workspaces
-----------------------------------------

Workspaces is a handy little feature of `m0vg` script, it exploits the fact that
_Vagrant_ keeps all configuration data related to `Vagrantfile` in a single
directory. If that special directory is replaced with another one, _Vagrant_
will use it instead. So it's possible to keep around multiple of those
directories and switch between them, thus having multiple virtual clusters.

The `m0vg` supports following actions on workspaces:

    m0vg workspace list
    m0vg workspace add    <NAME>
    m0vg workspace switch <NAME>

The `workspace` sub-command can be shortened as just `ws`.

`m0vg` also maintains a dedicated `.env` file for each workspace so when
switching between workspaces each keeps it's own set of environment variables.

Executing Ansible commands manually
-----------------------------------

In some rare cases it can be useful to run _Ansible_ commands against _Vagrant_
VMs manually. For this purpose `m0vg` script supports `ansible` command. Here
are just a few examples:

```bash
# list all hosts present in the cluster
m0vg ansible cluster.yml --list-hosts

# list all the tasks that would be performed for 'cmu' machine
m0vg ansible cmu.yml --list-tasks
```

VirtualBox / VMware / Libvirt specifics
---------------------------------------------------------

[Is it possible to bring up a VM with vagrant but then manage it from VMWare?](https://stackoverflow.com/questions/43548645)

On the other hand it can be achieved by starting a VM with _GUI_ enabled:

  M0_VM_ENABLE_GUI=yes m0vg up cmu

This needs to be done only once and VM will appear in VMware's VM Library.
