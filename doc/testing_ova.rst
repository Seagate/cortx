***********
Performing IO to a CORTX system and observing activity in the CORTX GUI
***********

If you wish to use the provided virtual machine image to test CORTX and observe activity in its GUI, 
you should have already followed `the instructions <CORTX_on_Open_Virtual_Appliance.rst>`_
to install and configure the image.  If you have not yet done so, please do that
now before following the rest of the instructions on this page.

#. If you do not already have one, download an S3 client.  These instructions use `CyberDuck <https://cyberduck.io/download/>`_.

#. If you have not already created an S3 user, then navigate to you your CORTX GUI, select 'Manage', then select the 'S3 Account' tab, then click on 'Add new account.'

   .. image:: images/add_s3_user.png
   
#. After adding the new account, make sure to copy the 'Access Key' and the 'Secret Key'.  Note that they will also be available in the CSV which is downloaded when you add the new account.

#. Return to your S3 client, and open a connection using the data IP you determined when you `installed and configured CORTX <CORTX_on_Open_Virtual_Appliance.rst>`_, and the access key and the secret key you recorded in the previous step.  Make sure also to set the protocol to 'Amazon S3'.  Here is how it looks in CyberDuck:

   .. image:: images/open_cyberduck_connection.png
   
   * If you see a Certificate error when you open the connection, you can ignore this error.  To suppress this error, you can add an SSL certificate in the Settings tab of the CORTX GUI.

#. After opening the connection in CyberDuck, create a new folder and then upload a large file into that folder.

#. Navigate to the Dashboard tab in your CORTX GUI, change 'Metric 1' to 'throughput_write' and you should see activity in the dashboard as so:

   .. image:: images/dashboard_traffic.png
   
#. BOOM.  You're all done and you're AWESOME.  Thanks for checking out the CORTX system; we hope you liked it.  Hopefully you'll stick around and participate in our community and help make it even better.
