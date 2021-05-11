# CORTX S3 Slack Bot

<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/logo750x500.png"  />

Repository link [https://github.com/sarthakarora1208/cortx-s3-slack-bot](https://github.com/sarthakarora1208/cortx-s3-slack-bot)
<br/>

Devpost link [https://devpost.com/software/cortx-s3-slack-bot](https://devpost.com/software/cortx-s3-slack-bot)
<br/>

Video link [https://youtu.be/G_Pu86H5nSg](https://youtu.be/G_Pu86H5nSg)
<br/>

## What does the CORTX S3 SLACK BOT do?

### File Syncing and Data Backup inside Slack

Cortx S3 Slack Bot enables users to access files in your S3 bucket directly from Slack using _Slash commands_. By using simple commands like `/cortx-s3-get filename` and `/cortx-s3-delete filename` we can find or delete files. Whenever a new file is shared on any public channel it is automatically added to the Cortx S3 test bucket, ensuring that all your slack files are safe in case a teammate accidently deletes a file that you need.

### File Searching

Most of the time we don't know the exact name of the file we are looking for. We also need to check if the file is actually present in the S3 bucket. Pooling the bucket over and over again to find a file or check for its existence is a computationally expensive and slow operation. To enable faster indexing of all the files on the S3 bucket, there is a layer of Elasticsearch between the Slack Bot and the S3 bucket. A user can find any file using the `/cortx-s3-search` command which opens a file search dialog. Elasticsearch's autocomplete functionality helps in navigating or guiding the user by prompting them with likely completions and alternatives to the filenames as they are typing it.

### Employee/Intern Onboarding

Whenever a new employee/intern joins the `#cortx-s3-test` channel he/she is greeted by our Cortx bot and is asked to upload his/her resume. After uploading their resume, they notify the slack bot with the `/cortx-s3-upload-resume resume.pdf` command. The bot processes the file extracts Personally Identifiable Information (PII) like name, email and phone number from the document updates of the csv file.
The administrators can get all the details of the employees within slack using `/cortx-s3-resume-data` slash command.

## In App Screenshots

<img src="https://user-images.githubusercontent.com/64438459/116312022-8a91ad00-a7c9-11eb-8be9-b41846ca2dfb.png" width="600px" heigth="600px"/>

<img src="https://user-images.githubusercontent.com/64438459/116312295-e1978200-a7c9-11eb-8f0c-5c26b2be4834.png" width="600px" heigth="600px"/>

<img src="https://user-images.githubusercontent.com/64438459/116312400-04299b00-a7ca-11eb-886c-3b28451a13d9.png" width="600px" heigth="600px"/>

<img src="https://user-images.githubusercontent.com/64438459/116312445-10155d00-a7ca-11eb-9048-8a08f38419db.png" width="600px" heigth="600px"/>

<img src="https://user-images.githubusercontent.com/64438459/116312359-f5db7f00-a7c9-11eb-892a-48c0465f05c8.png" width="600px" heigth="600px"/>

## How we built it

This integration has 5 components

<ol>
    <li>Slack Bot</li>
    <li>Cortx S3 Server</li>
    <li>Elasticsearch</li>
    <li>AWS Comprehend</li>
    <li>AWS Textract</li>
</ol>

The Project is set up to work in a python3 virtual environment. The Slack app is built using <a href="https://slack.dev/bolt-python/concepts">Bolt for Python</a> framework. For connecting to the CortxS3 Server, AWS Comprehend and AWS Textract we use their respective boto3 clients. We connect to Elasticsearch using the <a href="https://elasticsearch-py.readthedocs.io/en/v7.12.0/"> Python Elasticsearch Client</a>.

The Slack app listens to all sorts of events happening around your workspace — messages being posted, files being shared, users joining the team, and more. To listen for events, the slack app uses the Events API. To enable custom interactivity like the search modal we use the Blocks Kit.

Slash commands perform a very simple task: they take whatever text you enter after the command itself (along with some other predefined values), send it to a URL, then accept whatever the script returns and posts it as a Slackbot message to the person who issued the command or in a public channel. Here are the 5 slash commands we use to interact with the Cortx S3 bucket.

### File Sync

Whenever a new file is shared in any public slack channel the <a href="https://api.slack.com/events/file_shared#:~:text=The%20file_shared%20event%20is%20sent,the%20files.info%20API%20method."> <em>file_share event</em></a> is sent to the Slack app. The file is first indexed into Elasticsearch and then added to the Cortx S3 bucket with a key as file name.

### Slash Commands

<ul>
    <li> /cortx-s3-get</li>
    <li> /cortx-s3-search</li>
    <li> /cortx-s3-delete</li>
    <li> /cortx-s3-upload-resume</li>
    <li> /cortx-s3-resume-data</li>
</ul>

<img src="https://user-images.githubusercontent.com/42542489/116268666-2657f480-a79b-11eb-9ee6-c6823ac9aa33.png" width="750">

#### /corx-s3-get filename

After fetching the filename from the `command['text']` parameter we check if a the file exists using the `es.exists`(es = Elasticsearch client) function. If the file is found, we return the file back to the user as a direct message.

#### /corx-s3-search

This command opens up a modal inside of slack with a search bar, the user is suggested the file names depending on whatever text is written in.

#### /corx-s3-delete filename

After fetching the filename from the `command['text']` parameter we check if a the file exists using the `es.exists`(es = Elasticsearch client) function. If the file is found, we confirm if the user wants to permanently delete the file from the S3 bucket. If the user clicks yes, the file is permanently deleted.

<br>

<img src="https://user-images.githubusercontent.com/42542489/116237128-7758f000-a77d-11eb-9c7a-05cefa8a838f.png" width="750">

#### /corx-s3-upload-resume resume.pdf

When the command is invoked, we get the name of the file from the `command[text]` parameter. The slack app searches for the file on the S3 bucket and downloads it for local processing. The text is extracted from the .jpeg or .pdf resume file using AWS textract using OCR (Optical Character Recognition). The text is passed onto AWS Comprehend which identifies Personally Identifiable Information (PII) of the employee like name, email and phone number from the document. This data is appended in the resume-data.csv file.

#### /corx-s3-resume-data

Upon invocation we get the names and email addresses of the employees inside a table in slack populated with the data from resume-data.csv file.

![resume-analsyis](https://user-images.githubusercontent.com/42542489/116318250-1b6c8680-a7d2-11eb-9b95-861e187b20a6.gif)

# CORTX-S3 Slack Bot Installation Instructions

### Requirements

- [Python 3.6+](#Python-3.6+)
- [CORTX S3 Server](#CORTX-S3-Server)
- [ngrok](#ngrok)
- [AWS Account](#AWS-Account)
- [Elasticsearch 7.12.0](#Elasticsearch-7.12.0)
- [Slack](#Slack)

### Getting Started

---

<br/>
<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/python/Python_logo_and_wordmark.svg?raw=true" height="100" />

### Python 3.6+

To test the integration you need to have python installed on your computer. You can get a suitable release from [here](https://www.python.org/downloads/). You can check your python version by the following command.
<br>
<br>
<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/python/python.png">

We recommend using a virtual environment for development. [Read about it here](https://pypi.org/project/virtualenv/).

Follow the following steps to create a virtual environment, clone the repository and install all the packages.

### Cloning the repo

```bash
# Python 3.6+ required
git clone https://github.com/sarthakarora1208/cortx-s3-slack-bot
cd cortx-s3-slack-bot
python3 -m venv env
source env/bin/activate
pip3 install -r requirements.txt
```

<br/>

---

<br/>
<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/cortx-s3/cortx.png" height="100" />

### Cortx S3 Server

To successfully connect to a Cortx Server you need to set the _endpoint_url_ , _aws_access_key_id_ and _aws_secret_access_key_ in the [.env](./.env) file. If you are using a Cloudshare environment and followed the instructions from [https://raw.githubusercontent.com/Seagate/cortx/wiki/CORTX-Cloudshare-Setup-for-April-Hackathon-2021](https://raw.githubusercontent.com/Seagate/cortx/wiki/CORTX-Cloudshare-Setup-for-April-Hackathon-2021) you can simply copy the Server URL (CORTX endpoint) from the Connection Details section -> External address on your cortx-va-1.03 VM and paste in the [.env](./env) file

```bash
ENDPOINT_URL=""
AWS_ACCESS_KEY_ID="AKIAtEpiGWUcQIelPRlD1Pi6xQ"
AWS_SECRET_ACCESS_KEY="YNV6xS8lXnCTGSy1x2vGkmGnmdJbZSapNXaSaRhK"
```

You need to have a bucket with the name 'testbucket' inorder for the code to work

<br/>

---

<br/>
<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/ngrok/ngrok.png" height="100" />

### ngrok

##### Using ngrok as a local proxy

To develop locally we'll be using ngrok, which allows you to expose a public endpoint that Slack can use to send your app events. If you haven't already, [install ngrok from their website](https://ngrok.com/download) .

[Read more about ngrok](https://api.slack.com/tutorials/tunneling-with-ngrok)
<br/>

---

<br/>
<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/aws/aws.png" height="100">

### AWS Account

You need a verified aws account to test the [process_resume.py](./process_resume.py)

You can get your credentials file at ~/.aws/credentials (C:\Users\USER_NAME\.aws\credentials for Windows users) and copy the following lines in the [.env](./.env) file.

```bash
AMAZON_AWS_ACCESS_KEY_ID="YOUR_ACCESS_KEY_ID"
AMAZON_AWS_SECRET_ACCESS_KEY="YOUR_SECRET_ACCESS_KEY

```

<br/>

---

<br/>
<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/elasticsearch/elasticsearch_logo.png" height="100">

### Elasticsearch 7.12.0

The slack bot uses elastic search to index files on the S3 bucket.

To download Elasticsearch from their [website.](https://www.elastic.co/guide/en/elasticsearch/reference/current/install-elasticsearch.html)

You can run Elasticsearch on your own hardware, or use our hosted Elasticsearch Service on Elastic Cloud.

You can change the config variables in the [.env](./.env) file if you choose a hosted option

```bash
ELASTIC_DOMAIN='http://localhost'
ELASTIC_PORT=9200
```

You can test the elasticsearch client by running [elasticsearch_connector.py](./elasticsearch_connector.py).

```bash
python3 elastic_connector.py
```

A successful connection will yield:

<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/elasticsearch/elasticsearch_running.png">

<br>

---

<br>
<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/slack/Slack_Technologies_Logo.svg" height="100">

### Slack

You need to have slack installed on your computer. If you don't have Slack you get it from here for [Windows](https://slack.com/intl/en-in/downloads/windows) or [Mac](https://slack.com/intl/en-in/downloads/mac). Login to your account, if you don't have an account you can make one [here](https://slack.com/get-started#/create).

If you are an existing user you need to make a new channel #cortx-s3-test and you must be able to add new apps.

`You need to create a new workspace (https://slack.com/create) and add a new channel #cortx-s3-test`

 <img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/slack/create-new-channel.png" width="400">

To get started, you'll need to create a new Slack app, go to:
[https://api.slack.com/apps](https://api.slack.com/apps)

Bolt is a foundational framework that makes it easier to build Slack apps with the platform's latest features. We will be using this make our slack bot

1. Click on `Create an App` button
2. Give the app name as cortx-bot and choose the development workspace

   <img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/slack/slack-1.png" width="400">

   <img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/slack/slack-2.png" width="400">

3. Requesting scopes - [Scopes](https://api.slack.com/scopes) give your app permission to do things (for example, post messages) in your development workspace. You can select the scopes to add to your app by navigating over to the _OAuth & Permissions_ sidebar.

4. Add the following scopes the _Bot Token Scopes_ by clicking on the `Add an OAuth Scope ` button
<br>
<table>
    <thead>
        <tr>
            <th>OAuth Scope</th>
            <th>Description</th>
        </tr>
    </thead>
    <tbody>
        <tr>
            <td>
                <a href="https://api.slack.com/scopes/channels:history">channels:history</a>
            </td>
            <td>
                View messages and other content in public channels that cortx-bot has been added to
            </td>
        </tr>
        <tr>
            <td>
                <a href="https://api.slack.com/scopes/channels:join">channels:join</a>
            </td>
            <td>Join public channels in a workspace</td>
        </tr>
        <tr>
            <td>
                <a href="https://api.slack.com/scopes/channels:read">channels:read</a>
            </td>
            <td>View basic information about public channels in a workspace</td>
        </tr>
        <tr>
            <td>
                <a href="https://api.slack.com/scopes/chat:write">chat:write</a>
            </td>
            <td>Send messages as @cortxbot</td>
        </tr>
        <tr>
            <td>
                <a href="https://api.slack.com/scopes/chat:write.customize">chat:write.customize</a>
            </td>
            <td>
            Send messages as @cortxbot with a customized username and avatar
            </td>
        </tr>
        <tr>
            <td>
                <a href="https://api.slack.com/scopes/chat:write.public">chat:write.public</a>
            </td>
            <td>Send messages to channels @cortxbot isn't a member of</td>
        </tr>
        <tr>
            <td>
                <a href="https://api.slack.com/scopes/commands">commands</a>
            </td>
            <td>Add shortcuts and/or slash commands that people can use</td>
        </tr>
        <tr>
            <td>
                <a href="https://api.slack.com/scopes/files:read">files:read</a>
            </td>
            <td>View files shared in channels and conversations that cortx-bot has been added to</td>
        </tr>
        <tr>
            <td>
                <a href="https://api.slack.com/scopes/files:write">files:write</a>
            </td>
            <td>Upload, edit, and delete files as cortx-bot</td>
        </tr>
    </tbody>
</table>
<br/>

<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/slack/bot-token-scopes.png" width="400">

<br/>

5. Add the following scopes the the _User Token Scopes_ by clicking on the `Add an OAuth Scope ` button

<table>
    <thead>
        <tr>
            <th>OAuth Scope</th>
            <th>Description</th>
        </tr>
    </thead>
    <tbody>
        <tr>
            <td>
                <a href="https://api.slack.com/scopes/channels:history">channels:history</a>
            </td>
            <td>
                View messages and other content in public channels that cortx-bot has been added to
            </td>
        </tr>
        <tr>
            <td>
                <a href="https://api.slack.com/scopes/files:read">files:read</a>
            </td>
            <td>View files shared in channels and conversations that cortx-bot has been added to</td>
        </tr>
    </tbody>
</table>
<br/>

<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/slack/user-token-scopes.png" width="400">

<br/>

6. Install your own app by selecting the `Install App` button at the top of the OAuth & Permissions page, or from the sidebar.

7. After clicking through one more green `Install App To Workspace` button, you'll be sent through the Slack OAuth UI.

8. After installation, you'll land back in the _OAuth & Permissions_ page and find a _Bot User OAuth Access Token._ and a _User OAuth Token_. Click on the copy button for each of them. These tokens need to be added to the [.env](./.env) file. (The bot token starts with xoxb whereas the user token is longer and starts with xoxp)

```bash
SLACK_USER_TOKEN=xoxp-your-user-token
SLACK_BOT_TOKEN=xoxb-your-bot-token
```

<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/slack/tokens.png" width="400">

9. In addition to the access token, you'll need a signing secret. Your app's signing secret verifies that incoming requests are coming from Slack. Navigate to the _Basic Information_ page from your [app management page](https://api.slack.com/apps). Under App Credentials, copy the value for _Signing Secret_ and add it to the [.env](./env) file.

```bash
SLACK_SIGNING_SECRET=your-signing-secret
```

<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/slack/app-creds.png" width="400">

10. Make sure you have followed the steps in [Cloning the repo](#Cloning-the-repo). To start the bolt app. The HTTP server is using a built-in development adapter, which is responsible for handling and parsing incoming events from Slack on port 3000

```bash
python3 app.py
```

<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/python/python3-app.png">

Open a new terminal and ensure that you've installed [ngrok](#ngrok), go ahead and tell ngrok to use port 3000 (which Bolt for Python uses by default):

```bash
ngrok http 3000
```

<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/ngrok/ngrok_running.gif">

For local slack development, we'll use your ngrok URL from above, so copy it your clipboard

```bash
For example: https://your-own-url.ngrok.io (copy to clipboard)
```

11. Subscribing to events - Your app can listen to all sorts of events happening around your workspace — messages being posted, files being shared, and more. On your app configuration page, select the _Event Subscriptions_ sidebar. You'll be presented with an input box to enter a `Request URL`, which is where Slack sends the events your app is subscribed to. _Hit the save button_

By default Bolt for Python listens for all incoming requests at the /slack/events route, so for the Request URL you can enter your ngrok URL appended with /slack/events.

```bash
Request URL: https://your-own-url.ngrok.io/slack/events
```

If the challenge was successful you will get a verified right next to the Request URL.

<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/slack/event-subscriptions.png" width="400">

On the same page click on the `Subscribe to bot events` menu on the bottom of the page. Click on the `Add Bot User Event` .

Similary click on the `Subscribe to events on behalf of user`. Click on the `Add Workspace Event`.

Add the following scopes

<table>
    <thead>
        <tr>
            <th>EventName</th>
            <th>Description</th>
            <th>Required Scope</th>
        </tr>
    </thead>
    <tbody>
        <tr>
            <td>
                <a href="">file_share</a>
            </td>
            <td>
            A file was shared
            </td>
            <td>
               files:read
            </td>
        </tr>
        <tr>
            <td>
                <a href="">message.channels</a>
            </td>
            <td>
                A message was posted to a channel
            </td>
            <td>
                channesls:history
            </td>
        </tr>
    </tbody>
</table>

<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/slack/Bot-and-User-Events.png">

<br/>
<br/>

12. Next up select the _Interactivity & Shortcuts_ sidebar and toggle the switch as on. Again for the Request URL enter your ngrok URL appended with /slack/events

```bash
Request URL: https://your-own-url.ngrok.io/slack/events
```

<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/slack/interactivity.png" width="400">

13. Scroll down to the _Select Menus_ section, in the Options Load URL, enter your ngork URL appended with /slack/events

```bash
Options Load URL: https://your-own-url.ngrok.io/slack/events
```

<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/slack/select-menus.png" width="400">

14. Finally we come to the slash commands. Slack's custom slash commands perform a very simple task: they take whatever text you enter after the command itself (along with some other predefined values), send it to a URL, then accept whatever the script returns and posts it as a Slackbot message to the person who issued the command. We have 5 slash commands to be added in the workspace.

Head over to the _Slash Commands_ sidebar and click on the `Create New Command` button to head over the Create New Command page.
Add the Command, Request URL,Short Description and Usage hint, according to the table provided below.

Click on Save to return to the _Slash Commands_

<table>
    <thead>
        <tr>
            <th>Command</th>
            <th>Request URL</th>
            <th>Short Description</th>
            <th>Usage Hint</th>
        </tr>
    </thead>
    <tbody>
        <tr>
            <td>
            /cortx-s3-get
            </td>
            <td>
            https://your-own-url.ngrok.io/slack/events
            </td>
            <td>
            Get a file from s3 bucket
            </td>
            <td>
            filename
            </td>
        </tr>
        <tr>
            <td>
            /cortx-s3-search
            </td>
            <td>
            https://your-own-url.ngrok.io/slack/events
            </td>
            <td>
            Search for a file in S3
            </td>
            <td>
            </td>
        </tr>
        <tr>
            <td>
            /cortx-s3-delete
            </td>
            <td>
            https://your-own-url.ngrok.io/slack/events
            </td>
            <td>
            Deletes the given file from the s3 bucket
            </td>
            <td>
            filename
            </td>
        </tr>
        <tr>
            <td>
            /cortx-s3-upload-resume
            </td>
            <td>
            https://your-own-url.ngrok.io/slack/events
            </td>
            <td>
            Upload resume to database
            </td>
            <td>
            resume.pdf
            </td>
        </tr>
        <tr>
            <td>
            /cortx-s3-resume-data
            </td>
            <td>
            https://your-own-url.ngrok.io/slack/events
            </td>
            <td>
            Get resume data from s3
            </td>
            <td>
            </td>
        </tr>
    </tbody>
</table>

<br/>
<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/slack/cortx-s3-get.png" width="400">
<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/slack/cortx-s3-search.png" width="400">
<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/slack/cortx-s3-delete.png" width="400">
<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/slack/cortx-s3-upload-resume.png" width="400">
<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/slack/cortx-s3-resume-data.png" width="400">
<img src="https://raw.githubusercontent.com/sarthakarora1208/cortx-s3-slack-bot/master/images/slack/slash-commands.png" width="400">

14. Watch the [video](https://youtu.be/G_Pu86H5nSg) to know more about using these slack commands

15. Open the slack channel and upload a file in any channel, note the file name

16. Then type the `/cortx-s3-search` and search for your file

![ezgif-7-2d48a9abea31](https://user-images.githubusercontent.com/44650484/116314744-21139d80-a7cd-11eb-9b4c-abd674b17fd5.gif)

## ISSUES

### Elasticsearch Error

<img src="https://user-images.githubusercontent.com/44650484/116310729-01c64180-a7c8-11eb-9e0e-f16a2e302891.png" width="600px" heigth="600px"/>

### Solution - Starting Elasticsearch first it is possible that elasticsearch is not working

<img src="https://user-images.githubusercontent.com/44650484/116311210-9e88df00-a7c8-11eb-8761-4bb84bf52770.png" width="600px" heigth="600px"/>

### Endpoint URL error

<img src="https://user-images.githubusercontent.com/44650484/116311210-9e88df00-a7c8-11eb-8761-4bb84bf52770.png" width="600px" heigth="600px"/>

### Solution - add your endpoint url
