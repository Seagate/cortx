==========================
Configuring the CORTX GUI
==========================

This guide provides step-by-step instructions to configure the CORTX GUI. You can configure the CORTX GUI after your CORTX cluster is up and running using one of the following guides:

- `Building the CORTX Environment for Single Node <https://github.com/Seagate/cortx/blob/main/doc/community-build/Building-CORTX-From-Source-for-SingleNode.md>`_

The configuration of CORTX GUI gets completed in two-step:

- `Preboarding <#Preboarding>`_
- `Onboarding <#Onboarding>`_

Preboarding
===========

The preboarding procedure must be performed only once. On the preboarding process, you need to create an admin user for CORTX GUI.

.. raw:: html

    <details>
   <summary><a>Click here to expand the preboarding procedure.</a></summary>


1. Open a browser and navigate to the *https://<management IP>/#/preboarding/welcome*

   - If you see a message about your connection not being private, it is safe to ignore this message.
   
2. Click **Start**. A page that requests you to accept the End User License Agreement (EULA) appears.

   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/Start1.PNG

3. Click **Get Started**. A **CORTX Terms and Conditions** showcasing the EULA appears.

   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/Get.PNG

4. Click **Accept** to accept the **CORTX Terms and Conditions**.

   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/EULA1.PNG
   
Onboarding
===========

The onboarding procedure must be performed after completing the preboarding procedure. You should be brought automatically to the correct page after completing the preboarding:

- If you are not or you want to redo the onboarding, navigate to *https://<management IP>/#/onboarding* 
- If you logged out navigate to: *https://<management IP>/#/preboarding/login*

You can also perform the onboarding tasks from the **Settings** page.

.. raw:: html

    <details>
   <summary><a>Click here to expand the onboarding procedure.</a></summary>

#. If prompted, log in with default username and password i.e. cortxadmin/Cortxadmin@123

   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/login.PNG

#. Enter a memorable name in the **System Name** field, and click **Continue**.

   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/Systemname.PNG

#. Optional: If you have an SSL certificate, you can upload it. Otherwise, a default self-signed certificate provided by Seagate will be used. Click **Continue**.

   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/image-2021-09-16-18-32-26-155.png
   
#. Enter information about the **DNS servers** and **Search domains** in relevant fields, and click **Continue**. 
   Multiple entries must be separated by commas.
   
   - If you don't know your DNS servers, 8.8.8.8 will usually work.
      
   - If you don't know your search domains, please use cortx.test.
  
   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/DNS.png
   
   
#. Enter the NTP server address in the text box and select the NTP time zone offset from the drop-down menu. Then, click **Continue**.
   Multiple entries must be separated by a comma.

   - If you don't know your NTP server, ntp-b.nist.gov will usually work. To use another NTP server, visit `https://tf.nist.gov/tf-cgi/servers.cgi <https://tf.nist.gov/tf-cgi/servers.cgi>`_
   
   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/NTP.PNG

#. Optional: If you want to receive email notifications, select the **Subscribe to email notifications** checkbox. The **Notification settings: Email** page is displayed.
   
   #. Fill in the fields appropriately.
       
   #. After filling in the information in all the fields, click **Send test mail**. If configured correctly, you should receive a test mail from CORTX.
       
   #. Click **Apply**. The process of configuring emails is completed.

   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/Email.PNG

#. Click **Continue** after reviewing the summary. A dialog box is displayed indicating the success of the onboarding process.

#. Click **Go to dashboard**. CORTX is now ready for use. 

   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/DB.PNG
   
   **Note**: CSM GUI can now be accessed by navigating to *https://<management IP>/#/login*

.. raw:: html
   
   </details>
