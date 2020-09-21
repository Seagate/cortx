==========================
Preboarding and Onboarding
==========================

The preboarding process must be the first process that must be completed after configugring CORTX on OVA. The preboarding procedure must be performed only once.

.. raw:: html

    <details>
   <summary><a>Click here to complete the preboarding process.</a></summary>


1. Navigate to the *https://<management IP>:28100/#/preboarding/welcome*.

2. Accept the End User License Agree (EULA).

3. Create a user with admin privileges.

   - Enter the username, password, and email address in the relevant fields. Also, select the checkbox if want to receive the notifications.

   **Note**: Do not create an user with one of the following usernames.

   - **root**

   - **csm**
   
**Note**: The above mentioned procedure can be performed only once.
   
.. raw:: html
   
   </details>
     
.. raw:: html

    <details>
   <summary><a>Click here to complete the onboarding process.</a></summary>


1. Navigate to *https://<management IP>:28100/#/preboarding/login* or from the **Settings** page on the CSM GUI.

2. After being prompted, login again with the username and password that you had provided earlier.

3. Enter a name for your appliance in the **Appliance Name** field.

4. Upload the SSL certificate. If you do not upload a certificate, a default self-signed certificate provided by Seagate will be used.

5. Enter information about the DNS servers and search domains in relevant fields. The entries must be separated by comma, if either one or both the conditions mentioned below are applicable.

   - Number of DNS Servers > 1

   - Number of search domains > 1

6. Provide information about the NTP servers in the relevant field. The entries must be separated by comma, if the below mentioned condition is applicable.

   - Number of NTP Servers > 1

7. Configure the notifications by selecting the checkbox. If you do not want to receive notifications, select **Skip this step**, and click **Next**.

8. Click **Finish**. CORTX is now ready for use. CSM GUI can be accessed by navigating to *https://<management IP>:28100/#/login*

**Note**: The below mentioned steps can be performed multiple times.

.. raw:: html
   
   </details>
