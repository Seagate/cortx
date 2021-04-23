# Using the CORTX BizTalk Adapter

## Overview

## Goals

## Notes  
Part 1 - Send Data to CORTX Using the BizTalk CORTX Adapter

1.  Create a new folder in your computer’s file system, then create two
    subfolders under it named “IN” and “OUT”.

2.  Right click on the root file and click “Properties \> Security\> Edit”

1.  Type “BTServiceControl” into the text box, then click “Add”. In the next
    pane, click “Full Control”, then click “Ok” to give BizTalk full permission
    to access these new file folders you have created.

1.  Create a .txt file in notepad, enter some data, then save it to the computer
    desktop.

1.  Go to S3 Browser and click “New Bucket” in the top left-hand corner. Name
    your bucket “cortxdemo” (must be all lower-case) and click “Create new
    bucket”. Your bucket should appear in the tree-view.

1.  In BizTalk Administration Console, open up the default BizTalk Application
    “BizTalk Application 1” and right click on Send Ports. Click “New \>Static
    One-Way Send Port..”

1.  Give the Send Port a name, then click the Dropdown menu beside “Type:”.
    Select WCF-Custom from the list of options.

1.  Enter “cortx://uvo1u026p25wjnv7485.vm.cld.sr:80/cortxdemo” into the URL bar.
    Write “CORTXAdapter” into the SOAP text box.

1.  Click the “Bindings” tab at the top of the window. Click the dropdown menu
    beside “Binding Type:” and click “CORTXAdapterBinding”. Enter your
    “AccessKeyID” and “SecretAccessKey”, then enter “%SourceFileName%” in the
    “objectName” box.

1.  Click the “Messages” tab at the top of the window. Click the “Template”
    radio button, then enter “\<bts-msg-body
    xmlns="http://www.microsoft.com/schemas/bts2007" encoding="base64"/\>” in
    the XML text box. Afterwards, click “Ok” and create the Send Port.

1.  Under “BizTalk Application 1” right click on the “Receive Ports” tab and
    click “New\>One Way Receive Port…”

1.  Give the Receive Port a name, then click “OK” to create it.

1.  Under “BizTalk Application 1” right click on the “Receive Locations” tab and
    click “New\>One Way Receive Location…”

1.  Select the Receive Port you created in the previous steps when prompted.
    Give the Receive Location the same name as the Receive Port, then click the
    dropdown menu beside “Type:”. Select “FILE” from the list of options, then
    click the “Configure” button.

1.  Click “Browse”, then find and select the “IN” folder you created in your
    computer’s file system at the beginning of the tutorial. Change the “File
    mask:” property to “\*.txt”. This tells the BizTalk ReceivePort/Location to
    consume any data placed in that folder with the extension “.txt.” Click “Ok”
    to create the Receive Location.

1.  Right click on “BizTalk Application 1” and click “Start”. This
    starts/enables all of the ports you’ve just created.

2.  Go back to the Send Ports tab under the BizTalk Application, right click the
    Port you created previously and then click “Properties”. Click “Filters” on
    the left-hand side, and in the “Property” dropdown menu find and select
    “BTS.ReceivePortName”. In the “Value” text box, paste the name of the
    Receive Port you previously created. This tells BizTalk that any message
    coming in that specified Receive Port will be redirected to this Send Port.
    This concept is called subscription in the context of BizTalk.

1.  The solution is now complete and ready to test. Copy the .txt file you
    created on the desktop in earlier steps, then paste it into your “IN”
    folder. You should see the message vanish from the folder as BizTalk
    consumes it.

2.  Go back to S3 Browser and refresh your “cortexdemo” bucket. You should see
    that BizTalk has taken the file from the “IN” directory in your computer’s
    file system and deposited it into your S3 bucket.

**Sending Data From CORTX Using BizTalk CORTX Adapter**

1.  In BizTalk Administration Server (under BizTalk Application 1), right click
    on Receive Ports, then click “New\>One Way Receive Port”. Give the port a
    name, then click “Ok” to create it.

![C:\\Users\\BTAdmin\\AppData\\Local\\Microsoft\\Windows\\INetCache\\Content.Word\\13.png](media/ac12f0d81d3531b5b11fdeb92d090595.png)

1.  Still under BizTalk Application 1, right click on the Receive Locations tab,
    then click “New\>One Way Receive Location”. Name the Receive Location the
    same as your previously created Receive Port. Click the dropdown menu beside
    “Type:”, and find and select “WCF-Custom” from the list of options. After,
    click the “Configure” button beside the dropdown.

![C:\\Users\\BTAdmin\\AppData\\Local\\Microsoft\\Windows\\INetCache\\Content.Word\\15.png](media/e216605c362e88fed4ae5ae4d0b93c5c.png)

1.  Enter “cortx://uvo1u026p25wjnv7485.vm.cld.sr:80/cortxdemo” into the URL bar.

1.  Click the “Bindings” tab at the top of the window. Fill in your
    “AccessKeyID” and “SecretAccessKey”, then clear the text box beside
    “objectName”.

1.  Click the “Messges” tab at the top of the window. Click the “Path” radio
    button, and paste “/\*[local-name()='base64Binary']” into the “Body path
    expression:” text box. Lastly, select “Base64” from the dropdown menu beside
    “Node encoding:”. Click “Ok” to finish creating the Receive Location.

1.  Under BizTalk Application 1, right click on Send Ports. Click “New \>Static
    One-Way Send Port..”

    ![C:\\Users\\BTAdmin\\AppData\\Local\\Microsoft\\Windows\\INetCache\\Content.Word\\8.png](media/a24643f6ac036e4bdc65df0f57012dd2.png)

2.  Give the Send Port a name, and in the dropdown menu beside “Type:”, find and
    select “FILE” from the list of options. After, click the “Configure” button
    beside the dropdown.

1.  Click the “Browse” button. In the tree-view of your computer’s file system,
    find and select the “OUT” folder you created at the start of the tutorial.

1.  Click “Ok” to save this configuration. On the left-hand side of the Send
    Port window, click “Filters”. Under the “Property” dropdown menu, find and
    select “BTS.ReceivePortName. In the “Value” text box, paste the name of the
    Receive Port you created in step 1). After, click “Ok” to finish creating
    the Send Port.

1.  The solution is now configured and ready to test. Upload your .txt file to
    the “cortxdemo” bucket in S3 Browser. After a few seconds, refresh the
    bucket list and you should see the file vanish as BizTalk consumes it. Go to
    your computer’s file system, and open your “OUT” folder. You should see that
    BizTalk has deposited the file from S3 into your file folder.
