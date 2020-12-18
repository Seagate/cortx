==========================
Preboarding and Onboarding
==========================

Preboarding
===========

The preboarding process is the first process that must be completed after configuring CORTX. The preboarding procedure must be performed only once.

.. raw:: html

    <details>
   <summary><a>Click here to expand the preboarding procedure.</a></summary>


1. Open a browser and navigate to the *https://<management IP>:28100/#/preboarding/welcome*.

   - If you see a message about your connection not being private, it is safe to ignore this message.

2. Click **Start**. A page that requests you to accept the End User License Agreement (EULA) appears.

   .. image:: images/Start1.PNG

3. Click **Get Started**. A **CORTX Terms and Conditions** showcasing the EULA appears.

   .. image:: images/Get.PNG

4. Click **Accept** to accept the **CORTX Terms and Conditions**.

   .. image:: images/EULA1.PNG

5. Create a user with admin privileges by entering the username, password, and email address in the relevant fields, and click **Apply and Continue**.  The usernames *root*, *cortx* and *csm* are reserved so please do not use them.
    
   .. image:: images/preboard_create_admin.PNG

  
.. raw:: html
   
   </details>
   
Onboarding
===========

The onboarding procedure must be performed after completing the preboarding procedure.  You should be brought automatically to the correct page after completing the preboarding.  If you are not, or if you subsequently want to redo the onboarding, you can navigate to *https://<management IP>:28100/#/onboarding* or, if logged out, through: *https://<management IP>:28100/#/preboarding/login* . You can also perform the onboarding tasks from the **Settings** page.

.. raw:: html

    <details>
   <summary><a>Click here to expand the onboarding procedure.</a></summary>

#. If prompted, login again with the username and password that you had provided earlier.

   .. image:: images/login.PNG

#. Enter a memorable name in the **System Name** field, and click **Continue**.

   .. image:: images/Systemname.PNG

#. Optional: If you have a SSL certificate, you can upload it. Otherwise, a default self-signed certificate provided by Seagate will be used. Click **Continue**.

   .. image:: images/SSL.PNG
   
#. Enter information about the **DNS servers** and **Search domains** in relevant fields, and click **Continue**. 
   Multiple entries must be separated by commas.
   
   - If you don't know your DNS servers, 8.8.8.8 will usually work.
      
   - If you don't know your search domains, please use cortx.test.
  
   .. image:: images/DNS.png
   
   
#. Enter the NTP server address in the text box and select the NTP time zone offset from the drop-down menu. Then, click **Continue**.
   Multiple entries must be separated by comma.

   - If you don't know your NTP server, ntp-b.nist.gov will usually work.
   
   .. image:: images/NTP.PNG

#. Optional: If you want to receive email notifications, select the **Subscribe to email notifications** checkbox. The **Notification settings: Email** page is displayed.
   
   #. Fill in the fields appropriately.
       
   #. After filling information in all the fields, click **Send test mail**. If configured correctly, you should receive a test mail from CORTX.
       
   #. Click **Apply**. The process of configuring emails is completed.

   .. image:: images/Email.PNG

#. Click **Continue** after reviewing the summary. A dialog box is displayed indicating the success of the onboarding process.

#. Click **Go to dashboard**. CORTX is now ready for use. 

   .. image:: images/DB.PNG
   
   **Note**: CSM GUI can now be accessed by navigating to *https://<management IP>:28100/#/login*

.. raw:: html
   
   </details>
   
**Important**: To open the 28100 port, run the below mentioned commands.

  ::
  
   salt '*' cmd.run "firewall-cmd --zone=public-data-zone --add-port=28100/tcp --permanent"
   
   salt '*' cmd.run "firewall-cmd --reload"
   
 

