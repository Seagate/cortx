# Setup a CentOS Local VM
This is a guide which lists the steps required to get a local VM and use it for S3 and/or Mero development.

## 1. Download the VM
Download both .ovf and .vmdk files from the following link into a directory on your computer: https://seagatetechnology-my.sharepoint.com/personal/basavaraj_kirunge_seagate_com/_layouts/15/onedrive.aspx?id=%2Fpersonal%2Fbasavaraj%5Fkirunge%5Fseagate%5Fcom%2FDocuments%2FInnersource&originalPath=aHR0cHM6Ly9zZWFnYXRldGVjaG5vbG9neS1teS5zaGFyZXBvaW50LmNvbS86ZjovZy9wZXJzb25hbC9iYXNhdmFyYWpfa2lydW5nZV9zZWFnYXRlX2NvbS9FZ2tNaHFtVWJJZEZzOFRyOU4tYkhlY0J0TUR3bTAyUWJHMFQ4dlM3VExPZFVnP3J0aW1lPWVWc2UtVFhvMTBn

## 2. Add the VM in VMWare Fusion or Oracle VirtualBox
You need to have either VMWare Fusion or Oracle VirtualBox to use the VM. In case you have VirtualBox installed, select the "Import Appliance" option and select the .ovf file downloaded in the previous step.  Note: if you are a Seagate employee and do not already have access to either VMWare Fusion or Oracle VirtualBox, IT requires you to submit an Exception request since these are not officially supported applications.

## 3. Using the VM
Start the VM, you will be presented with a login prompt after sometime. You need to enter the following credentials to login:
Username: root
Password: seagate

## 4. Configuring the networking interface
At the command prompt type `# ip a`. You will see some output like this:
<p align="center"><img src="../../assets/images/ip_a_op.png?raw=true"></p>
Note down the interface name (other than the loopback interface "lo"). In this case it is *ens33*. This will be required in Step 6. If the interface is down use the following command to bring it up:<br/>
`# ifup ens33`

## 5. Clone S3 or Mero source
Now you are ready to clone either S3 or Mero. Please refer to the Mero or S3 quick start documents to do so.

## 6. Check for LNet
Type the following command:
`# lctl list_nids`
you should get the following output:
<p align="center"><img src="../../assets/images/lctl_list_nids_op.png?raw=true"></p>

If you don't get this output, LNet is not up. Perform the following steps:  
`# systemctl start lnet`  
`# lnetctl net add --net tcp0 --if ens33`  

And check again. Now you should be able to see a similar output as shown above.

## 7. Verify system time
Verify the system time is current, if not set it to the current time using the `timedatectl` utility.

## You are now all set to use this VM for either Mero or S3 development.
