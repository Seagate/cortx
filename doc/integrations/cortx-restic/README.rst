
############
CORTX-Restic
############

Link to Vimeo: https://vimeo.com/582403890

Integrating CORTX with Restic
*****************************

.. image:: ./logo.png
    :alt: logo



#####################
Step 1 - Installation
#####################

Installation
************


Alpine Linux
============

.. code-block:: console

    $ apk add restic

Arch Linux
==========

.. code-block:: console

    $ pacman -S restic

Debian
======

.. code-block:: console

    $ apt-get install restic


Fedora
======

.. code-block:: console

    $ dnf install restic

macOS
=====


.. code-block:: console

    $ brew install restic

Nix & NixOS
===========

.. code-block:: console

    $ nix-env --install restic

OpenBSD
=======

.. code-block:: console

    # pkg_add restic

FreeBSD
=======

.. code-block:: console

    # pkg install restic

openSUSE
========

.. code-block:: console

    # zypper install restic

RHEL & CentOS
=============

.. code-block:: console

    $ yum install yum-plugin-copr
    $ yum copr enable copart/restic
    $ yum install restic

Solus
=====

.. code-block:: console

    $ eopkg install restic

Windows
=======

.. code-block:: console

    scoop install restic




#########################
Step 2 - CORTX Connection
#########################
    

CORTX Server
************

`CORTX <https://github.com/Seagate/cortx>`__ is an Open Source Object Storage,
uniquely optimized for mass capacity and compatible with AWS S3 API.

-  Make sure you have an existing CORTX server running
-  You can also refer to https://github.com/Seagate/cortx for step by step guidance
   on installation

You must first setup the following environment variables with the
credentials of your CORTX server.

.. code-block:: console

    $ export AWS_ACCESS_KEY_ID=<YOUR-CORTX-ACCESS-KEY-ID>
    $ export AWS_SECRET_ACCESS_KEY= <YOUR-CORTX-SECRET-ACCESS-KEY>

Now you can easily initialize restic to use CORTX server as a backend with
this command.

.. code-block:: console

    $ ./restic -r s3:<YOUR-CORTX-ENDPOINT-URL>/<BUCKET-NAME> init
    enter password for new repository:
    enter password again:
    created restic repository 6ad29560f5 at s3:<YOUR-CORTX-ENDPOINT-URL>/<BUCKET-NAME>
    Please note that knowledge of your password is required to access
    the repository. Losing your password means that your data is irrecoverably lost.

If you use CORTX development server as S3 server, and encounter this error 
"x509: cannot validate certificate for 192.168.1.111 because it doesn't contain any IP SANs",
you can add ``--insecure-tls`` to avoid this error. 

.. code-block:: console

    $ ./restic -r s3:<YOUR-CORTX-ENDPOINT-URL>/<BUCKET-NAME> init --insecure-tls

Note: At the time of this update, restic's latest stable release 0.12.1 does not have this ``--insecure-tls`` option. But it will be included in future releases.
Alternatively, you can get the latest restic souce and build it, and get this option enabled.

.. code-block:: console

    $ git clone https://github.com/restic/restic
    $ cd restic
    $ go run build.go

###################
Step 3 - Backing Up
###################


You can even backup individual files in the same repository (not passing
``--verbose`` means less output):

.. code-block:: console

    $ restic -r s3:<YOUR-CORTX-ENDPOINT-URL>/<BUCKET-NAME> backup ~/work.txt
    enter password for repository:
    password is correct
    snapshot 249d0210 saved

If you're interested in what restic does, pass ``--verbose`` twice (or
``--verbose=2``) to display detailed information about each file and directory
restic encounters:

.. code-block:: console

    $ echo 'more data foo bar' >> ~/work.txt

    $ restic -r s3:<YOUR-CORTX-ENDPOINT-URL>/<BUCKET-NAME> backup --verbose --verbose ~/work.txt
    open repository
    enter password for repository:
    password is correct
    lock repository
    load index files
    using parent snapshot f3f8d56b
    start scan
    start backup
    scan finished in 2.115s
    modified  /home/user/work.txt, saved in 0.007s (22 B added)
    modified  /home/user/, saved in 0.008s (0 B added, 378 B metadata)
    modified  /home/, saved in 0.009s (0 B added, 375 B metadata)
    processed 22 B in 0:02
    Files:           0 new,     1 changed,     0 unmodified
    Dirs:            0 new,     2 changed,     0 unmodified
    Data Blobs:      1 new
    Tree Blobs:      3 new
    Added:      1.116 KiB
    snapshot 8dc503fc saved

In fact several hosts may use the same repository to backup directories
and files leading to a greater de-duplication.

Now is a good time to run ``restic check`` to verify that all data
is properly stored in the repository. You should run this command regularly
to make sure the internal structure of the repository is free of errors.

#####################
Tested by:
#####################

- Dec 10 2021: Harrison Seow (harrison.seow@seagate.com) using Cortx OVA 1.0.3 on CloudShare VM.
- Nov 21 2021: Bo Wei (bo.b.wei@seagate.com) using Cortx OVA 2.0.0 as S3 Server.
