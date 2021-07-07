This release notes includes new features, enhancements, and bug fixes added to the CORTX OVA 2.0.0.


# New Features

This section provides information on all the newly added features in the CORTX OVA R2 release.

1. #### **Erasure Coding:** 
  
    From this release, CORTX OVA adds support for erasure coding. Erasure coding is a data protection method. The user data is  split into multiple small data units of pre-defined unit sizes. These units are encoded with the redundant (parity) data units and stored across different storage devices in the cluster. The erasure coding provides the redundancy to the CORTX to endure the failures at disk or node level. In CORTX, we now support erasure coding with configurabledata units (N) + parity (K) + spare (S); the default is 4+2+2.  Please note that currently CORTX only supports N+K+S where K and S are equal but this will be relaxed in future releases. As part of future releases failure, the notifications and handling for drives/nodes will be done and data can be read in the degraded mode if failure occur.
    
2. #### **Cluster: Bootstrap Management Service** 
  
    The CORTX HA provides a framework to manage all the CORTX services and facilitates configuration of all the CORTX services for HA.  After all the required CORTX services are deployed and configured, this feature provides cluster configuration and ability to start the stack. 
  
3. #### **Cluster: Bootstrap IO Service** 
  
    After all the CORTX services are deployed, this feature allows you to initialize the S3 IO services in the single node CORTX VM.
  
4. #### **Copy Objects** 
  
    The CORTX now supports the copy object feature. This feature allows you to create a copy of object stored in the CORTX Cloud. You can copy the object stored in the same bucket or different bucket. 
    
    Demo: [Copy Objects](https://www.youtube.com/watch?v=eoAAY6gU8lI&list=PLOLUar3XSz2P_4MxY4z0ut9-dMGDnpFPp&index=3).

5. #### **S3 IAM User Management** 
  
    For the S3 IAM user serval enhancement and new features are added in this release:
      -	As an S3 account user, you wil be able to manage IAM user information.
      -	S3 account users are able to create, view, update, and delete the IAM users. You can also change your S3 account password.
      -	Using the CLI, the S3 account owner can view the access key and secrete key while creating the IAM user account.
      -	The S3 account owner can regenerate and delete the access key.
    
    Demo: [S3 IAM User Management](https://www.youtube.com/watch?v=7Tyyj6jDi4c&list=PLOLUar3XSz2P_4MxY4z0ut9-dMGDnpFPp&index=5)

6. #### **CSM Audit Logs** 
  
    From this release of CORTX OVA, you can download and view the CSM Audit logs in the CORTX CSM GUI. You can view the CSM logs from the Maintenance > Audit Logs option in the left pane. 
  
7. #### **CORTX OVA Deployment – Single Node Cluster** 
  
    From the R2 release, you can create a single node cluser using CORTX OVA image. This will allow users to perform the CORTX cluser operation in a single node VM.
