==========================
Preboarding and Onboarding
==========================

Preboarding
===========

The preboarding process must be the first process that must be completed after configuring CORTX. The preboarding procedure must be performed only once.

.. raw:: html

    <details>
   <summary><a>Click here to expand the preboarding procedure.</a></summary>


1. Open a browser and navigate to the *https://<management IP>:28100/#/preboarding/welcome*. The management IP must be fetched from the step 6 of `CORTX_on_Open_Virtual_Appliance <https://github.com/Seagate/cortx/blob/Changes-to-VA/doc/CORTX_on_Open_Virtual_Appliance.rst>`_.

   - You may see a message about your connection not being private. Ignore the message.

2. Click **Start**. A page that requests you to accept the End User License Agreement (EULA) appears.

   .. image:: images/Start1.PNG

3. Click **Get Started**. A window showcasing the End User License Agreement (EULA) appears.

   .. image:: images/Get.PNG

3. Click **Accept** to accept the EULA.

   .. image:: images/EULA1.PNG

4. Create a user with admin privileges by entering the username, password, and email address in the relevant fields, and click **Apply and Continue**.

   - If you want to receive email notifications, select the **Subscribe to email notifications** checkbox.
   
   .. image:: images/Adminu.PNG

   **Note**: Do not create an user with one of the following usernames.

   - **root**

   - **csm**
  
.. raw:: html
   
   </details>
   
Onboarding
===========

The onboarding procedure must be performed after completing the preboarding procedure by accessing *https://<management IP>:28100/#/preboarding/login*. The onboarding procedure can be performed multiple times.
     
.. raw:: html

    <details>
   <summary><a>Click here to expand the onboarding procedure.</a></summary>

#. If prompted, login again with the username and password that you had provided earlier.


   .. image:: images/login.PNG

2. Enter a name for your application in the **System Name** field, and click **Continue**.

   .. image:: images/Systemname.PNG

3. If you have a SSL certificate, you can upload it. Else, a default self-signed certificate provided by Seagate will be used. Click **Continue**.

   .. image:: images/SSL.PNG
   
   **Note**: This is an optional step.

4. Enter information about the **DNS servers** and **Search domains** in relevant fields, and click **Continue**. 
   The entries must be separated by comma, if either one or both the conditions mentioned below are applicable.
   
   - Number of DNS Servers > 1
   
     - If you don't know your DNS servers, 8.8.8.8 will usually work.
      
   - Number of search domains > 1
   
     - If you don't know your search domains, use cortx.test.
  
   
   .. image:: images/DNS.PNG
   
   
5. Enter the NTP server address in the text box and select the NTP time zone offset from the drop-down menu. Then, click **Continue**.
   The entries must be separated by comma, if the below mentioned condition is applicable.

   - Number of NTP Servers > 1
   
     - If you don't know your NTP server, ntp-b.nist.gov will usually work.
   
   .. image:: images/NTP.PNG

6. Configure the email notifications by selecting the **Email** checkbox, and providing the relevant information. After configuring, click **Continue**.

   .. image:: images/Email.PNG

7. Click **Continue** after reviewing the summary. A dialog box is displayed indicating the success of the onboarding the process.

8. Click **Go to dashboard**. CORTX is now ready for use. 

   .. image:: images/DB.PNG
   
   **Note**: CSM GUI can be accessed by navigating to *https://<management IP>:28100/#/login*


.. raw:: html
   
   </details>

