# Microsoft BizTalk Server 2020 Installation and Configuration Guide  
**Install BizTalk Server 2020**

Follow these steps to install Microsoft BizTalk Server 2020 (any version).  
**[*NOTE: This guide assumes you have a SQL Server Environment to install BizTalk.
If you need to set up a SQL Server environment please refer to the accompanying
document: [Microsoft SQL Server 2019 Installation Guide](./Microsoft%20SQL%20Server%202019%20Installation%20Guide.md).*]  
  
**[*NOTE: Before you begin, ensure you are logged into the Windows Server with
local Administrator permissions.*]

1.  Download the BizTalk Server 2020 media as an ISO from your MSDN account.

2.  Double click the ISO file to mount it and then run **Setup.exe** in the
    \\**BizTalkServer** directory:  
    
    ![](media/65847419612eeeb7b82632c0e5a193e1.png)

3.  Click the **Install Microsoft BizTalk Server 2020** option:  
    
    ![](media/1a6a6edbba807ef38907820d84e09d94.png)

4.  Enter your customer information and press **Next**:  
    
    ![](media/cf4e299768db0ec203edb66723be72d8.png)

5.  Accept the License Agreement and click **Next**:  
    
    ![](media/0a4c84b27a70c6d8fc50dedc9464846e.png)

6.  Choose your Customer Experiment Improvement involvement and click **Next**: 
    
    
    ![](media/da88225d035a8d38692862e0e6dc2ff8.png)

7.  Choose your installation directory and any optional components (the defaults
    are sufficient) and click Next:  
    
    ![](media/5b96e6b315d2c94d2cd328545c1c641f.png)

8.  Select **Install Latest BizTalk Server Cumulative Update. (Recommend)** and
    then click **Install**:  
    
    ![](media/f82271ff81ca1bbceddd8ceeae09b9eb.png)

9.  Click **Next** to install the **Microsoft BizTalk Server 2020 Cumulative
    Update 1**:  
    
    ![](media/0b8e11cd2bd1bb9d42961c2e9de1fa67.png)

1.  Accept the license agreement and click **Next**:  
    
    ![](media/fcafee6a7bad6990446652a5bf866e49.png)

2.  Choose your level of participation in the Customer Experience Improvement
    Program and click **Next**:  
    
    ![](media/21efd0be3e09e5a8d1f8154b4b638d89.png)

3.  Proceed with the installation…  
    
    ![](media/07442e571151ede4399c66d5bcd64b0e.png)

4.  Click **Next** to install the cumulative update:  
    
    ![](media/7e950ff54b53cdbb2e97633c347a84d9.png)

1.  Click **Yes** to confirm the restarting of necessary services:  
    
    ![](media/892d9fba59ea2afc438af351f86a874e.png)

2.  Wait for the Cumulative Update to complete and click **Finish**:  
    
    ![](media/16a4e72287ab25702667b3b0cf199d6a.png)

3.  Choose whether to use Microsoft Update to check for updates and click
    **Next**:  
    
    ![](media/7711fdbd02bfa273d51115b8d7961ae2.png)

4.  Click **Finish** to complete the BizTalk Server implementation and commence
    configuration of the server:  
    ![](media/8a21e5842b2120020b335a2c0336a899.png)

## Configure BizTalk Server 2020

Follow these steps to *configure* Microsoft BizTalk Server 2020.

1.  Click **the Windows icon** in the bottom left corner and then click
    **Control Panel**:  
    
    ![](media/62ea6ce70e8f40637f92ae3abb5002b2.png)

2.  Click **Administrative Tools**:  
    
    ![](media/c9501d71ab053d8d9eace3560dbe9d4e.png)

3.  Double Click **Computer Management**:  
    
    ![](media/c5e8a12b58d7702fe33ed4801a633eeb.png)

4.  Expand **Local Users and Groups** and then Right-click Users and select
    **New User…  
    **  
    ![](media/4c054c20cc53b175bb7bd2a711c03938.png)

5.  Create a new user named **BTServiceControl** which will be the local account
    the BizTalk Server service runs as. Then provide a password of your choosing
    that you will remember. Finally clear the **User must change password at
    next logon** setting, and **select User cannot change password** and
    **Password never expires**, and then click **Create** followed by **Close**:
    
    
    ![](media/659f7e3d4f8c3c63adcd97a305d6c522.png)

1.  If the **Microsoft BizTalk Server Configuration** program was not already
    launched at the completion of the previous installation section, search for
    it now and run it. It should pick up the default SQL Server instance that
    was installed previously, otherwise enter the SQL Server instance you wish
    BizTalk to install to:  
    
    ![](media/b7c75e779952895fb82726be14a98a75.png)

1.  With the **Custom configuration** option selected, enter the BizTalk Service
    account you created in step 5. above along with its password, and then click
    **Configure**:  
    
    ![](media/300a909fa527eb9be08005ebe49da45e.png)

2.  Select Enterprise SSO on the left and then check the Enable Enterprise
    Single Sign-On on this computer:  
    
    ![](media/b171924fd999b9767e5b720e02a47652.png)

3.  Select **Enterprise SSO Secret Backup** on the left, and then enter the same
    password as in Step 5. above for both **Secret backup password** and
    **Confirm password**, along with a suitable **Password reminder** and make
    note of the location where the SSO Secret Backup is being placed:  
    
    ![](media/4dc82e0f61ba6e59255ad9404bc3cf7b.png)

4.  Select **Group** on the left (be patient asit can take several seconds for
    the screen to respond) and then click (again be patient as it can take
    several seconds for the screen to respond) **Enable BizTalk Server Group on
    this computer**:  
    
    ![](media/19d44261534f1bdbfdba08d629a48039.png)

5.  Click **BizTalk Runtime** on the left, and then click (be patient as it can
    take several seconds for the screen to respond) **Register the BizTalk
    Server runtime components** and then click **Apply Configuration** on the
    toolbar:  
    
    ![](media/b20eef5678e35c6e2ba2522330e92f3d.png)

6.  Click **Next** to commence the configuration process for a basic BizTalk
    Server environment:  
    
    ![](media/456b5bc9e41e74779b5b35208854c61f.png)

7.  Click Finish when the process completes:  
    
    ![](media/3351ae820786ea5b5482404f43dd94d0.png)

8.  Review the summary page and then **Close** the Microsoft BizTalk Server
    Configuration program:  
    
    ![](media/4587823301ff268364e37305b136ab69.png)

9.  Search for **BizTalk** and click the **BizTalk Server Administration**
    application (Tip: You may wish to pin this to your Task Bar for easy
    access):  
    
    ![](media/c8a181e5b22d3b8554335967caa4c13f.png)

10. Congratulations, you have completed the installation and configuration of
    **BizTalk Server 2020**!  
    
    ![](media/af8c81202ac6a69effa78154cec907b1.png)
