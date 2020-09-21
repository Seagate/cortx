==========================
Preboarding and Onboarding
==========================

Preboarding
===========

The preboarding process must be the first process that must be completed after configuring `CORTX on OVA <https://github.com/Seagate/cortx/blob/main/doc/CORTX_on_Open_Virtual_Appliance.rst>`_. The preboarding procedure must be performed only once.

.. raw:: html

    <details>
   <summary><a>Click here to expand the preboarding procedure.</a></summary>

#. Open a browser and navigate to *https://<management IP>:28100/#/preboarding/welcome*.

   * You may see a message about your connection not being private; if so, just proceed past this message.
 
#. Accept the terms and conditions.

#. Create a user with admin privileges.

   - Enter the username, password, and email address in the relevant fields. Also, select the checkbox if want to receive the notifications.

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

#. Enter a name for your system in the **System Name** field.  Any name is fine.

#. Optional: upload an SSL certificate. If you do not upload a certificate, a default self-signed certificate provided by Seagate will be used.

#. Enter information about the DNS servers and search domains in relevant fields. Multiple entries must be separated by comma.

   - If you don't know your DNS servers, 8.8.8.8 will usually work.

   - If you don't know your search domains, use cortx.test will usually work.

#. Provide information about the NTP servers in the relevant field. Multiple entries must be separated by commas.

   - If you don't know your NTP server, ntp-b.nist.gov will usually work.

#. Configure email notifications if desired. If you do not want to receive notifications, select **Skip this step**, and click **Next**.

#. Click **Finish**. CORTX is now ready for use. The CORTX GUI can be accessed by navigating to *https://<management IP>:28100/#/login*

.. raw:: html
   
   </details>

