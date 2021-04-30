# CORTX OVA 1.0.4

This release notes includes new features and bug fixes added to the CORTX OVA 1.0.4.

# Features

 1. On the About page, the serial number is displaying.
 2. The hostname displays on the UI.
 3. Added the FRU level while generating the support bundle.
 4. Created an ISO bundle to allow deployment in the isolated environment.
 5. Passwords are generating dynamically during the deployment.
 6. Prevented the duplication of the cluster_ids to avoid double encryption.
 7. Integrated the LDAP configuration scripts with the Provisioner.
 8. Added the S3 Services through CA-signed certificates on EES.
 9. Updated the ADDB logs path in the var/motr/s3server directory.
 10. Updated the S3 Working directory to /var/log/seagate/motr.
 11. Removed the access of the S3 account to create CSM users from the backend.
 12. Encrypted the passwords saved in the /etc/csm/csm.conf files.
 13. Only the S3 account user can delete the buckets from their account using CSM-CLI.
 14. A CSM user with a monitor role can view the list of available commands for which the user has access using the help (-h) command.
 15. A user can un-acknowledge an alert using the rest request.
 16. Updated the disk usage alert generation, APIs, and test cases.
 17. Users can start a node resource using the system start [resource_name] command.
 18. Debug level logs are available for all the implemented components after the successful installation.
 19. Info level logs are available for all the implemented components after the configuration.
 20. Root users can use CSM-CLI to change other user passwords and roles by specifying old_password.
 21. CSM users with monitor roles cannot perform list, update, delete, and create operations on iam_users using the CSM-CLI.
 22. Implemented the dynamic password fetching for LDAP and RAS.
 23. A CSM user with a role manager cannot perform any REST API request on an IAM user.
 24. Configure the Dev hardware client to test the DI automated test.
 25. API response of audit logs for the S3 component contains the specified parameters information in the specified format.
 26. Log-in error message is not displaying any confidential information.
 27. A non-root user cannot change its password by specifying old_password and password through CSM-CLI.
 28. Print the Motr ret code in the S3 log file for easier debugging.
 29. Upgraded the netty jar in the AUTH server.
 30. Provided the metadata extraction script.
 31. The rest request with default arguments returns appropriate records.
 32. Added the support for put-copy-object.
 33. The bucket policy denying access to S3 IAM users when allows permission present.
 34. Bucket policy allows access when both deny and allow permission is present.
 35. Added the Serial Number generated during the manufacturing process in the manifest support bundle.
 36. The code refactoring to the Motr wrapper file to have member functions defined in a separate C++ file.
 37. The GET API returns with 400 response code if the value of from param is greater than the value of to param.
 38. For no param interval and total_sample in the request, the GET API returns with 200 as a response code and appropriate with JSON response.

# Bug Fixes:

1. Added the prerequisite requirement for Node replacement.
2. Fixed the STONITH issue.
3. Fixed the issue of the SAS HBA Alerts (Fault/Fault resolved) that goes missing after component replacement SAS HBA.
4. Fixed the issue of the cluster going into the unusable state when Node 2 is powering off.
5. Updated all the health resources.
6. Updated the StatsD plug-in.
7. Updated the CSM setup to fix the CSM configuration issues.
8. Fixed the file naming convention issue.
9. Updated the path to collect the motr logs.
10. Fixed the tar file creation issue. 
11. Updated the cron timeout to fix the system shutdown issue. 
12. Fixed the time-zone issue.
13. Fixed the issue of private network fault alert on UI.
14. The Capacity details are displaying in the CSM GUI. 
15. Fixed the issue of deployment is failed on the Intel servers.
16. After the UDS RPMs are installed the UDS service file will be updated.
17. Fixed the issue of salt-minion is in failed state on a primary node after the software update.
18. Fixed the issue of Health map for node-1 and Enclosure went missing when Node-1 is in standby mode. 
19. Fixed the issue of the Kernel Mismatch error on the CentOS 7.8 cluster setup. 
20. Fixed the issue of the Health view page is displaying the old hostname.
21. Fixed the issue of deployment is failing due to the cryptography python package absent. 
22. Fixed the issue when the SSPL installation is getting failed with requisites were not found an error. 
23. Fixed the issue of deploy replacement is getting failed as the serial number is already assigned in stage 1. 
24. Fixed the issue of cortxub user password is not expiring at the first login. 
25. Fixed the issue of the CSM UI giving a success message for a software update when the software update is failed using CSM UI. 
26. Fixed the issue of the beta build deployment fails on the dell systems as the mpath fails to set different priorities on LUNs. 
27. Fixed the issue of the cortxub user's first login with credentials failed with permission denied. 
28. Fixed the issue when the unboxing fails with ERROR - Salt client command failed and static network configuration. 
29. Fixed the issue where the unboxing script fails to update the SSH file. 
30. Fixed the issue where post successful build deployment, the stonith resource is missing in PCS status as BMC IP for primary node failed to update in the cluster pillar file. 
31. Fixed the issue of the primary node is inaccessible from the secondary node and PCS status shows all services up. 
32. Moved the cluster out of maintenance state after the CSM config stage. 
33. Fixed the issue of the Node replacement failed in deploy replacement stage 2 for the NodeJS after boxing-unboxing. 
34. Fixed the issue of unboxing with the static network configuration. 
35. Updated the fallback mechanism in the glusterfs. 
36. Fixed the issue where the NodeJS installation fails during the node replacement due to missing entries in the "salt-call pillar.items commons". 
37. Fixed the issue of the unboxing is hung while updating management VIP in the pillar. 
38. Updated the HA Proxy for the DHCP environment. 
39. Fixed the issue where the IP validation fails for the public data network. 
40. Blocked the Motr remote access on the management and data networks. 
41. Fixed the issue of unboxing fails when hostname and IP are changed during unboxing at the volume creation phase. 
42. Fixed the issue of the VIPs failed to update while unboxing in the DHCP environment.
43. Generating the support bundle for the current state of system health view dynamically. 
44. Fixed the consul error for HCTL Status. 
45. Added the alerts for the FAN insertion and removal. 
46. Fixed the PC cluster stop command issue for Node reboot. 
47. Fixed the deployment for 1TB, 3TB, and 5TB volume size. 
48. Fixed the Node replacement issue. 
49. Fixed the CSM GUI update issue. 
50. Fixed the issue of unboxing failure. 
51. Fixed the issue of the HA installation caused the deployment to fail due to consul_watcher. Also, removed the VM-related configuration. 
52. Changed the owner of the SSPL directory to the sspl-ll user to access the "/var/cortx/sspl" directory in SSPL code. 
53. Added the lshw package as a required package in sspl-ll.spec file. 
54. Fixed the issue that was causing multiple "RAID integrity" alerts generation. 
55. Updated the resource health view script to add a node disk dictionary in the node data OS section. 
56. Added the change to retry consul if there is an internal server error and to store the message in consul if there is any type of error happens while publishing the message to the RabbitMQ Queue.
57. Updated the SSPL resource stop logic. 
58. Stored minion ID, consul host, and port in conf file during sspl_config, to start SSPL service in minimum time on a replaced node after node replacement. 
59. Used the show configuration API to fetch the correct chassis serial number. 
60. Fixed the Invalid Session key handle error.
61. Updated the JSON messages. 
62. Updated the pyinstaller version. 
63. Added the cortx-prvsnr installation as part of the cortx-csm build process. 
64. Removed the uninstalled packages from the CORTX component list. 
65. Improve the performance of the S3 Server. 
66. Fixed the issue of illegal characters generated as part of the S3 access key. 
67. Fix the S3 sanity cleanup issues. 
68. Fixed the issue where the NextMarker is not getting properly set in list-objects using boto3 when the bucket contains more than 1000 objects. 
69. Fixed the issue where object listing with prefix specified continues key enumeration even after prefix match stops. 
70. Fixed the bucket policy allows access when both deny and allow permission is present. 
71. The s3_bundle_generate.sh script displays the error "Repository 'csm_uploads': Error parsing config: Error parsing "baseurl = '/3rd_party'": URL must be http, ftp, file or https not ""
72. Fixed the issue of handle probable delete index entry delete while s3server failure during object writes.
73. Added the fsync call to the put_keyval operation.
74. CSM CLI/GUI list 500 users.
75. [Splunk] Fixed the Ceph S3 regression in list object API. 
76. [Splunk] Fixed the issue of failure for s3tests.functional.test_s3.test_bucket_acls_changes_persistent. 
77. [Splunk] Fixed the issue of s3tests.functional.test_s3.test_bucket_notexist. 
78. [Splunk] Fixed the issue of bad_amz_date_epoch test cases on the bucket and object creation. 
79. [Splunk] Fixed the issue of s3tests.functional.test_s3:test_bucket_create_naming_bad_punctuation fails.
80. [Splunk] Fixed the issue of s3server crash in s3tests.functional.test_s3.test_multipart_upload_empty. 
81. [Splunk] Fixed the s3tests.functional.test_s3:test_bucket_create_naming_bad_punctuation issue in S3 Auth Server. 
82. [Splunk] Fixed the test_object_requestid_matchs_header_on_error issue. 
83. Fixed the issue of calling the UploadPart operation the Service is unavailable. 
84. [Splunk] Fixed the s3tests.functional.test_s3.test_multipart_upload fails issue when it ran against main branch. 
85. [Splunk] Fixed the error code mismatch for aws4 bad_ua and bad_auth date cases issue. 
86. Updated the HA Proxy.
87.	Fixed the issue where the Motr HA interface using the wrong tag.
88.	Fixed the issue of consul state is not updating for process failure.
89.	Added the license header for Motr data recovery (motr_data_recovery.sh).
90.	Update the BE error message.
91.	Fixed the issue where the Motr Kernel service is not starting on the remote node.
92.	Updated the message description for data inconsistent.
93.	Updated the IEM alert message.
94.	Optimized the object listing performance.
95.	Updated the SSPL support bundle temporary directory path.
96.	Fixed the issue of URL decode 'continuation-token' during List Object V2 request validation.
97.	Fixed the OVA for the host file and set the salt-master default configuration in case of a single node.
98.	Fixed the issue where the unboxing script is failing when the static IP network configuration option is selected.
99.	Fixed the issue where the S3 CLI requests fail with "No route to host" error intermittently.
100.	Updated the SSL certificates.
101.	Fixed the issue where SFTP session returns with non-zero exit code.
102.	Updated the glusterfs service relation with the salt-master.
103.	Updated the JSON structures in the Manifest file.
104.	Fixed the issue of dangling web-socket connection.
105.	Replaced the UDX with LDP and added the expansion to the summary page.
106.	Added the post-update command for CSM set-up.
107.	Fixed the issue where the shutdown uses the redundant ssh to local host.
108.	Added a new SSL certification.
109.	Updated the CORTX build locations.
