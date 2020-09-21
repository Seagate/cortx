==========================
Preboarding and Onboarding
==========================

Preboarding
===========

The preboarding process must be the first process that must be completed after configugring `CORTX on OVA <https://github.com/Seagate/cortx/blob/main/doc/CORTX_on_Open_Virtual_Appliance.rst>`_. The preboarding procedure must be performed only once.

.. raw:: html

    <details>
   <summary><a>Click here to read the preboarding procedure.</a></summary>

1. Navigate to the *https://<management IP>:28100/#/preboarding/welcome*.

2. Accept the End User License Agree (EULA).

3. Create a user with admin privileges.

   - Enter the username, password, and email address in the relevant fields. Also, select the checkbox if want to receive the notifications.

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

2. Enter a name for your appliance in the **System Name** field and click **Continue**.

3. If you have a SSL certificate, you can upload it. Else, a default self-signed certificate provided by Seagate will be used.

4. Enter information about the DNS servers and search domains in relevant fields. The entries must be separated by comma, if either one or both the conditions mentioned below are applicable.

   - Number of DNS Servers > 1

   - Number of search domains > 1

5. Provide information about the NTP servers in the relevant field. The entries must be separated by comma, if the below mentioned condition is applicable.

   - Number of NTP Servers > 1

6. Configure the notifications by selecting the checkbox. If you do not want to receive notifications, select **Skip this step**, and click **Next**.

7. Click **Finish**. CORTX is now ready for use. CSM GUI can be accessed by navigating to *https://<management IP>:28100/#/login*

.. raw:: html
   
   </details>

