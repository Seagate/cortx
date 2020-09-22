==========================
Preboarding and Onboarding
==========================

Preboarding
===========

The preboarding process must be the first process that must be completed after configugring `CORTX_on_Open_Virtual_Appliance <https://github.com/Seagate/cortx/blob/main/doc/CORTX_on_Open_Virtual_Appliance.rst>`_. The preboarding procedure must be performed only once.

.. raw:: html

    <details>
   <summary><a>Click here to read the preboarding procedure.</a></summary>

1. Navigate to the *https://<management IP>:28100/#/preboarding/welcome*. The management IP that was got from step 6 of `CORTX_on_Open_Virtual_Appliance <https://github.com/Seagate/cortx/blob/Changes-to-VA/doc/CORTX_on_Open_Virtual_Appliance.rst>`_.

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

The onboarding procedure must be performed after completing the preboarding procedure by accessing *https://<management IP>:28100/#/preboarding/login* or from the **Settings** page on the CSM GUI. The onboarding procedure can be performed multiple times.

     
.. raw:: html

    <details>
   <summary><a>Click here to read the onboarding procedure.</a></summary>


1. After being prompted, login again with the username and password that you had provided earlier.

2. Enter a name for your application in the **System Name** field, and click **Continue**.

   .. image:: images/Systemname.PNG

3. If you have a SSL certificate, you can upload it. Else, a default self-signed certificate provided by Seagate will be used. Click **Continue**.

   .. image:: images/SSL.PNG

4. Enter information about the **DNS servers** and **Search domains** in relevant fields. Click **Continue**. The entries must be separated by comma, if either one or both the conditions mentioned below are applicable. 

   - Number of DNS Servers > 1

   - Number of search domains > 1
   
   
   .. image:: images/DNS.PNG
   
   
5. Provide information about the NTP servers in the relevant field. The entries must be separated by comma, if the below mentioned condition is applicable.

   - Number of NTP Servers > 1

6. Configure the notifications by selecting the checkbox. If you do not want to receive notifications, select **Skip this step**, and click **Next**.

7. Click **Finish**. CORTX is now ready for use. CSM GUI can be accessed by navigating to *https://<management IP>:28100/#/login*

.. raw:: html
   
   </details>

