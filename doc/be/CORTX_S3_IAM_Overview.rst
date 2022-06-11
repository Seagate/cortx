.. vim: syntax=rst

|image0|

=======
Agenda
=======

● CORTX-S3 Auth Server

● Authentication

● Authorization

● Identity and Access Management

● Access Control List (ACL)

● Bucket Policy

● Access Evaluation method

● s3iamcli

● Demo CORTX-S3 ACL with CloudBerry CORTX-S3 Explorer

===============
CORTX-S3 Auth Server
===============

● Enable access control to resources (Bucket, Objects)

● Developed in Java using Java netty io framework.

● One instance of Auth server runs on each LDRr1 node

● Uses OpenLdap as backend to store IAM data.

● OpenLdap is clustered, IAM data gets replicated across nodes.

● Provides REST API interface, AWS IAM API

● Every CORTX-S3 request generates authentication and authorization request.

===============
CORTX-S3 Auth Server
===============

|image1|

**CORTX-S3 IAM endpoint:** *iam.seagate.com:9443*

**SSL ca cert:** */etc/ssl/stx-s3-clients/s3auth/s3authserver.crt*  

===============
CORTX-S3 Auth Server
===============

**Authentication : “Verify Identity of a User”**

**Authorization : “Permission to perform operation”**

====================================
CORTX-S3 Authentication and Authorization
====================================

|image2|

Note: since 3/4/2020, in most cases Authorization and Authentication are now
done as part of a single "combined" API call (to improve performance).  See here
for more details: `Combined Auth Call Design
<https://github.com/Seagate/cortx-s3server/blob/main/docs/design/Combined-Auth-Call.md>`_.

=======================   
Authentication methods
=======================


**Authentication steps**
#########################

• **Parse Http Header**

• **Validate Date Header**

• **Check access key in OpenLdap**

• **Calculate Signature**

• **Compare Signature**

• **Return response xml**

|image3|

**CORTX-S3 Auth server supports both AWS V4 and AWS V2**

**Access Id** - This is a unique id generated for each user.

**Secret Key-** The secret key is used by CORTX-S3-clients to 
sign all the requests to the CORTX-S3-server.

**- No exchange of secret key**
   
   
===============================================
Authentication : Management/Web Login Use Case
===============================================

|image4|

..

=====================================
Identity and Access Management (IAM)
=====================================

|image5|

• CORTX-S3 Account

• IAM User

• Static groups – AllUsers, AuthenticatedUser

=================================
CORTX-S3 User Schema (DIT in OpenLdap)
=================================
   
|image6|

   
======================
Authorization methods
======================


**Authorization - Steps**

○ Parse input http header

○ Parse Bucket Policy Json

○ Evaluate input request

○ Openldap call to retrieve user details

○ Match permissions in Policy

○ Parse ACL xml

○ Match permissions

CORTX-S3 Account - level Access Management

* CORTX-S3 ACL

IAM User and Account - level Access Management

* CORTX-S3 Bucket Policy

================   
CORTX-S3 ACL Overview
================

|image7|

..

=============
Resource ACL
=============

+---------------------+----------------------+----------------------+
|    **Permission**   |    **When granted on |    **When granted on |
|                     |    a bucket**        |    an object**       |
+=====================+======================+======================+
|    **READ**         |    Allows grantee to |    Allows grantee to |
|                     |    list the objects  |    read the object   |
|                     |    in the bucket     |    data and its      |
|                     |                      |    metadata          |
+---------------------+----------------------+----------------------+
|    **WRITE**        |    Allows grantee to |    Not applicable    |
|                     |    create,           |                      |
|                     |    overwrite, and    |                      |
|                     |    delete any object |                      |
|                     |    in the bucket     |                      |
+---------------------+----------------------+----------------------+
|    **READ_ACP**     |    Allows grantee to |    Allows grantee to |
|                     |    read the bucket   |    read the object   |
|                     |    ACL               |                      |
|                     |                      |    ACL               |
+---------------------+----------------------+----------------------+
|    **WRITE_ACP**    |    Allows grantee to |    Allows grantee to |
|                     |    write the ACL for |    write the ACL for |
|                     |    the applicable    |    the applicable    |
|                     |    bucket            |    object            |
+---------------------+----------------------+----------------------+
|    **FULL_CONTROL** |    Allows grantee    |    Allows grantee    |
|                     |    the READ, WRITE,  |    the READ,         |
|                     |    READ_ACP, and     |    READ_ACP, and     |
|                     |    WRITE_ACP         |    WRITE_ACP         |
|                     |    permissions on    |    permissions on    |
|                     |    the bucket        |    the object        |
+---------------------+----------------------+----------------------+



Resource ACL Example:


.. code-block:: xml


		<?xml version="1.0" encoding="UTF-8" standalone="no"?><AccessControlPolicy xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
		
		 <Owner>
		 
		  <ID>b17f937d52db45ae9ddccc89da9da0f2e03f96492bf54720a51fad0fef9c55e6</ID>
		  
		  <DisplayName>usertest</DisplayName>
		  
		 </Owner>
		 
		 <AccessControlList>
		 
		  <Grant>
		  
		   <Grantee xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:type="CanonicalUser">
		   
			<ID>b17f937d52db45ae9ddccc89da9da0f2e03f96492bf54720a51fad0fef9c55e6</ID>
			
			<DisplayName>usertest</DisplayName>
			
		   </Grantee>
		   
		   <Permission>FULL_CONTROL</Permission>
		   
		  </Grant>
		  
		 </AccessControlList>
		 
		</AccessControlPolicy>
		

..

=================
CORTX-S3 Bucket Policy
=================

  
● Bucket policy is a resource based access management policy.

● Using Bucket policy, we can grant access to other CORTX-S3 accounts or
IAM users.

● Bucket policy is associated with individual bucket and stored as
part of bucket metadata.

● By default no bucket policy is created unlike Bucket ACL

.. code-block:: python

	{

		"Version": "2012-10-17", 
		"Id": "ExamplePolicy01", 
		"Statement": [

			{
				"Sid": "ExampleStatement01", 
				"Effect": "Allow", 
				"Principal": {

				"AWS": "arn:aws:iam::Account- ID:user/Dave"
			},

			"Action": [ 
			"s3:GetObject", 
			"s3:GetBucketLocation", 
			"s3:ListBucket"
				],
			"Resource": [ 
			"arn:aws:s3:::examplebucket/*",
			"arn:aws:s3:::examplebucket"
					]
				}
			]
		}
..

==================
Access Evaluation
==================


|image9|

..

=========== 
s3iamcli
===========

**API’s /Operations available**


-  CORTX-S3 Account

   - CreateAccountLoginProfile
   
   - GetAccountLoginProfile
   
   - UpdateAccountLoginProfile
   
   

-  CORTX-S3 User
   
   - CreateUserLoginProfile
   
   - GetUserLoginProfile
   
   - UpdateUserLoginProfile
   
   

-  CORTX-S3 Access Key/Secret Key
   
   - ChangePassword
   
   - GetTempAuthCredentials
   
   
..

===============  
**Questions?**
===============   

*Reach out to:*


* *CORTX Community Slack channel* - `<https://cortxcommunity.slack.com>`_

   
==============
**Thank you**
==============


.. |image0| image:: images/1_Data_is_Potential.png
   
.. |image1| image:: images/2_S3_Auth_Server.png
  
.. |image2| image:: images/3_S3_Authentication_and_Authorization.png
  
.. |image3| image:: images/4_Authentication_Methods.jpeg
 
.. |image4| image:: images/5_Authentication_Management.png

.. |image5| image:: images/6_Identity_and_Access_Management.png
  
.. |image6| image:: images/7_S3_User_Schema.png
 
.. |image7| image:: images/8_S3_ACL_Overview.png
 
.. |image9| image:: images/9_Access_Evaluation.png
