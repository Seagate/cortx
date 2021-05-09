<img src="https://github.com/Seagate/cortx/blob/main/doc/images/cortx-logo.png">

# CORTX S3 - Restic Integration

### CORTX integration with Restic is a fast and secure backup program, Restic. All your buckets are belong to us. 

### Change the way the world does by connecting CORTX™— Seagate’s open-source object storage software — with the tools and platforms that underpin the data revolution.

Storing and managing data had never been easy and with flourish of AI, deep learning we have generated paramounts of data called Big Data.

The ideal big data storage system would allow storage of a virtually unlimited amount of data, 
cope both with high rates of random write and read access, flexibly and efficiently deal with a range of different data models, 
support both structured and unstructured data, and for privacy reasons, only work on encrypted data. Obviously, all these needs cannot be fully satisfied.

## Background / Overview of the project
<img src="https://github.com/kyroninja/cortx/blob/main/doc/integrations/restic/logo.png">

## The Problem

### What we want to solve?
Creating backups can be an annoyance yet a necessity. Everyday the amount of data being stored and processed is growing. It is not surprising that a backup in today's age can easily take hundreds of gigs of storage. Whilst we do have cloud storage available from various providers, a local solution would be preferred with quick and optomized access to large volumes of data. Enter CORTX!

### Hypothesis
Integrating, S3 services for result storing and data retrieval. Restic is compatible with cloud services in it's latest version, so it is good and compatible to integrate Cortx S3 with it since Cortx is a subset of Amazon S3.

<b> What is CORTX? CORTX is a distributed object storage system designed for great efficiency, massive capacity, and high HDD-utilization. CORTX is 100% Open Source. </b>

## What our integration does?

Our integration is simple, it allows you to backup data to your S3 bucket and once you are done with backups, your data is available for easy retrieval. Multiple buckets can be created depending on backup type for example


### Why this integration is important?

It used to be said that the value of an effective backup system only becomes clear once you’ve lost data. When failures or losses occur, you need to have a fast and easy way to get your files and information back. If an organization loses data, the implications are severe. It could even jeopardize business continuity. Even in a domestic setting, data loss can be a painful experience. Backups are the only sensible safeguards. 

The program should be fast. You don’t want to wait all day for a backup or restore to complete. Some programs store a base backup image and then store the differences between the base image and the source machine for each subsequent backup. This speeds up the backup process considerably. It also uses less space for your backups. restic does all of this. It is free, open-source, licensed under the 2-Clause BSD License, and under active development

## Integration walkthrough
Step 0: Initial Setup
Follow this guide from Cortex Team:
<a href="https://github.com/Seagate/cortx/wiki/CORTX-Cloudshare-Setup-for-April-Hackathon-2021">CORTX-Cloudshare-Setup-for-April-Hackathon-2021</a>

Step 1: Download requirements
-Restic is available in most linux repos, in our case using Ubuntu which is a Debian derivative, allows us to simply run the following (switch to the Linux VM on CloudShare first) login with the details provided by CloudShare

- username: sysadmin
- password: Gq45qA0g17

Lets update the repos and system first
-  $sudo apt update
-  $sudo apt full-upgrade

Lets install Restic
-  $sudo apt install restic

<img src="https://github.com/kyroninja/cortx/blob/main/doc/integrations/restic/install_restic.png">

Step 2: Create a bucket to store the data with Restic
We first need to export some variables before restic can work
- export AWS_ACCESS_KEY_ID=AKIAtEpiGWUcQIelPRlD1Pi6xQ
- export AWS_SECRET_ACCESS_KEY=YNV6xS8lXnCTGSy1x2vGkmGnmdJbZSapNXaSaRhK

<img src="https://github.com/kyroninja/cortx/blob/main/doc/integrations/restic/export_s3_cred.png">

Lets create this bucket (192.168.2.102 is the address of the CORTX VM)
- restic -r s3:http://192.168.2.102/cortxrules init

<img src="https://github.com/kyroninja/cortx/blob/main/doc/integrations/restic/creatae_bucket_repo.png">

Note the http used otherwise there is x509 certificate error

The output is as follows
enter password for new repository: (password for the bucket)
enter password again:
created restic repository eefee03bbd at s3:http://192.168.2.102/cortxrules
Please note that knowledge of your password is required to access the repository. Losing your password means that your data is irrecoverably lost.

<img src="https://github.com/kyroninja/cortx/blob/main/doc/integrations/restic/cyber_duck.png">

Step 3: Lets backup some data

In the Linux VM there is a file called FSx.jpg so lets use that, run the following
- restic -r s3:http://192.168.2.102/cortxrules --verbose backup FSx.jpg

<img src="https://github.com/kyroninja/cortx/blob/main/doc/integrations/restic/list_backup_file.png">
Output is as follows:

open repository
enter password for repository:
password is correct
lock repository
load index files
start scan
start backup
scan finished in 1.837s
processed 200 KiB in 0:12
Files:        1 new,     0 changed,     0 unmodified
Dirs:         1 new,     0 changed,     0 unmodified
Added:      200 KiB
snapshot 69A0c125 saved

Step 4: Lets restore some data
- restic -r s3:http://192.168.2.102/cortxrules restore 69A0c125 --target /home/sysadmin/

<img src="https://github.com/kyroninja/cortx/blob/main/doc/integrations/restic/delete_restore_file.png">

Output is as follows:
enter password for repository:
restoring <Snapshot of [/home/sysadmin] at 2021-04-27 21:40:19.884408621 +0200 CEST> to /home/sysadmin

Video Link
<a href="https://youtu.be/E-OSJ-_PQ78">Integration Video</a>

Authors
- Pratish "kyroninja" Neerputh
- Shraddha Rajcoomar