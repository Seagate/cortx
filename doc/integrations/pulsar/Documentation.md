
# Documentation

The plan of implementation is this:
1. We setup Cortx on a VM.
2. We use aws s3 cli to check the bucket data on Cortx S3.
3. We setup Pulsar to use Cortx as tiered storage backend.
4. We make some hacky adjustments to make sure Cortx accepts the payload sizes sent by Pulsar. See issues.
5. We publish messages on a pulsar topic.
6. We offload the topic messages(older ones anyway) to Cortx s3.
7. We prove that the messages are offloaded based on internal stats of the topic via pulsar-admin.
8. We cross check in Cortx s3, by checking that the S3 bucket has message ledgers with index data.
9. We replay the data from the earliest message in the topic(which would be on Cortx S3 by now). We are now able to replay data with CortxS3 as source.

[Here](https://www.youtube.com/watch?v=-JPrpL1_8Mg) is a walkthrough/demo video to follow.

## Prerequisites 
For the reproduction of demo, we need the following:
1. python3, pip3
2. Cortx VM setup(Demo is on VirtualBox)


# Contents
The following is an index of files in this folder:
1. `pulsar-producer.py` - Writes messages to locally hosted pulsar on topic with name `test`. 
2. `pulsar-consumer.py` - Listens to the topic `test` on locally hosted pulsar. It gets only new messages to the topic and not older messages.
3. `pulsar-reader.py` - Connects to locally hosted pulsar and reads from the earliest message on topic.
4. `setup` - This folder has all the files needed to configure pulsar.
5. `setup/pulsar-env.sh` - This has template env variables which pulsar needs to offload data to Cortx S3. Source this file before running pulsar executable.
6. `setup/conf` - The configuration files needed in pulsar. Copy these into pulsar's conf folder before running the executable.

# Steps to Reproduce
We may need 6 terminals(to avoid confusion) to see the integration and monitor its working.
1. For SSH into Cortx VM
2. For running pulsar
3. For aws cli and pulsar-admin
4. For pulsar-producer to publish messages to topic
5. For pulsar-consumer to receive current messages on topic.
6. For pulsar-reader to read messages on topic from earliest message.


# Setting up Cortx
1. Install cortx on a VM following the steps [here](https://github.com/Seagate/cortx/blob/main/doc/OVA/1.0.4/CORTX_on_Open_Virtual_Appliance.rst)
2. Make sure you set the date in VM to current date and set `/etc/hosts` mapping(based on VM IPs) for s3.seagate.com and management.seagate.com in your machine so that the s3 server is accessible from Pulsar. Ping the hostnames to check they are working fine. To get SSH to the VM working, set mapping. See Section 11 of [here](https://github.com/Seagate/cortx/blob/main/doc/OVA/1.0.4/CORTX_on_Open_Virtual_Appliance.rst) to get IPs of the VM. The `/etc/hosts` file should have a couple of lines like this.
```
192.168.0.140 local.seagate.com s3.seagate.com sts.seagate.com iam.seagate.com sts.cloud.seagate.com
192.168.0.138 management.seagate.com
```
3. Try the sanity test for s3 server to make sure everything works.
4. In the VM, open the file `/opt/seagate/cortx/s3/conf/s3config.yaml` and change S3_MOTR_LAYOUT_ID to 1(`S3_MOTR_LAYOUT_ID: 1`)
5. Restart cortx server as per instructions [here](https://github.com/Seagate/cortx/blob/main/doc/OVA/1.0.4/CORTX_on_Open_Virtual_Appliance.rst).
6. Change the system date to correct value if necessary.
7. Configure the [Cortx GUI](https://github.com/Seagate/cortx/blob/main/doc/Preboarding_and_Onboarding.rst)
8. Go to https://management.seagate.com:28100/ and after onboarding, in the Manage menu, create an S3 account and download the access key and secret key.

# Setting up AWS CLI
1. First ensure that s3 server of cortx is working and then setup awscli and awscli-plugin-endpoint:
2. To install the AWS client, use: $ pip3 install awscli
3. To install the AWS plugin, use: $ pip3 install awscli-plugin-endpoint
4. Add the following to your `~/.aws/config`(or create a new file with the path and add the following)
```
[plugins]
endpoint = awscli_plugin_endpoint

[profile sg]
output = text
region = US
s3 =
    endpoint_url = http://s3.seagate.com
s3api =
    endpoint_url = http://s3.seagate.com
```
5. Add the following to your `~/.aws/credentials`(or create a new file with the path) and replace the AWS access key and secret key values with those generated from Cortx GUI.
```
[sg]
aws_access_key_id=AKIAw5WobgsyTh2rQ2ljuZmweA
aws_secret_access_key=sTNG8gMpkG/AOj+gTi6ayBkdr5aw5Xw5s7R252sF
```
Explainer: The above instructions are an extract of Section 1.4 along with prerequisites from [Cortx S3 Server guide](https://github.com/Seagate/cortx-s3server/blob/main/docs/CORTX-S3%20Server%20Quick%20Start%20Guide.md)
6. Test the cli by listing buckets from Cortx S3:
```
aws --profile=sg s3 ls 
```

# Set up s3 bucket in cortx for offloading topic messages
1. Create the bucket `pulsar-topic-test` in Cortx S3.
```
aws --profile=sg s3 mb s3://pulsar-topic-test
```
2. We will use this bucket to offload older messages on pulsar topics.

# Setting up Apache Pulsar:
1. Download pulsar binary tar along with offloaders
  https://pulsar.apache.org/download/
2. Untar pulsar binary tar and the offloaders tar
3. cd into the pulsar directory(apache-pulsar-2.7.1). Lets call this PULSAR_HOME and all our work is done from here.
4. copy offloaders folder from the untared offloaders directory into PULSAR_HOME
5. Copy pulsar-env.sh from setup folder in this repo into PULSAR_HOME. Change the s3 access key and secret key to the ones created from Cortx GUI.
6. Replace conf/standalone.conf and conf/broker.conf in PULSAR_HOME with the files in setup folder in this repo.
7. Run pulsar:
```
. pulsar-env.sh
./bin/pulsar standalone
```
8. You should now have pulsar running.
9. In another terminal window, from PULSAR_HOME, to view stats of test topic, run:
```
./bin/pulsar-admin --admin-url http://localhost:8088 persistent stats-internal public/default/test
```
10. Now set the offload settings for the public/default namespace:
```
./bin/pulsar-admin --admin-url http://localhost:8088 namespaces set-offload-policies -d aws-s3 -b pulsar-topic-test -e http://s3.seagate.com  -mbs 8388608 public/default -oat 8388608
```
We are indicating that Pulsar should offload topic data to Cortx after a threshold of 8MB and with a maximum block size of 8MB. These settings have already been done in config and this cli command is for showing how policies can be changed.
11. You can verify that the offload settings here:
```
./bin/pulsar-admin --admin-url http://localhost:8088 namespaces get-offload-policies public/default
```
12. Keep this terminal open for later admin actions.



# Setting up pulsar subscriber, reader and producer for test topic:
0. Prerequisites: python3.8, pip3
1. Install pulsar client
```
pip3 install pulsar-client
```
2. From this(repo's) folder, in one terminal window run:
```
python3 pulsar-subscriber.py
```
3. From this(repo's) folder, in another terminal window run:
```
python3 pulsar-producer.py 10
```
4. Pulsar subscriber should be printing 10 messages it received from the producer.
5. Now try publishing 100,000 messages
```
python3 pulsar-producer.py 100000
python3 pulsar-producer.py 100000
```
6. Publish as many as you wish and see that messages are received.
7. From the terminal we set aside for admin actions, check the topic's internal stats:
```
./bin/pulsar-admin --admin-url http://localhost:8088 persistent stats-internal public/default/test
```
8. You should see stats like this, indicating that the ledgers for the topic are not offloaded yet(watch for `offloaded: false`):
```
{
  "entriesAddedCounter" : 100010,
  "numberOfEntries" : 100010,
  "totalSize" : 17474068,
  "currentLedgerEntries" : 10495,
  "currentLedgerSize" : 1836625,
  "lastLedgerCreatedTimestamp" : "2021-05-02T04:08:45.235+05:30",
  "waitingCursorsCount" : 1,
  "pendingAddEntriesCount" : 0,
  "lastConfirmedEntry" : "38:10494",
  "state" : "LedgerOpened",
  "ledgers" : [ {
    "ledgerId" : 10,
    "entries" : 89515,
    "size" : 15637443,
    "offloaded" : false
  }, {
    "ledgerId" : 38,
    "entries" : 0,
    "size" : 0,
    "offloaded" : false
  } ],
  "cursors" : {
    "my-subscription1" : {
      "markDeletePosition" : "38:10494",
      "readPosition" : "38:10495",
      "waitingReadOp" : true,
      "pendingReadOps" : 0,
      "messagesConsumedCounter" : 100010,
      "cursorLedger" : 11,
      "cursorLedgerLastEntry" : 57,
      "individuallyDeletedMessages" : "[]",
      "lastLedgerSwitchTimestamp" : "2021-05-02T03:58:44.662+05:30",
      "state" : "Open",
      "numberOfEntriesSinceFirstNotAckedMessage" : 1,
      "totalNonContiguousDeletedMessagesRange" : 0,
      "properties" : { }
    }
  },
  "schemaLedgers" : [ ],
  "compactedLedger" : {
    "ledgerId" : -1,
    "entries" : -1,
    "size" : -1,
    "offloaded" : false
  }
}
```
7. From the terminal we are using for pulsar-admin, run manual offload:
```
./bin/pulsar-admin --admin-url http://localhost:8088 topics offload -s 1M persistent://public/default/test
```
8. Now check the internal stats and see that the offloaded flag is set true for some ledgers.
9. To verify that the topic's content is offloaded to cortx's s3, use aws cli to ls the bucket. You will notice ledgers with corresponding index objects showing in the s3 bucket.
```
> aws --profile=sg s3 ls pulsar-topic-test
2021-04-29 07:08:08   24576233 1151097a-f869-46db-a65b-0274856f7328-ledger-10
2021-04-29 07:08:08        247 1151097a-f869-46db-a65b-0274856f7328-ledger-10-index
2021-04-29 07:25:47   56018627 56dde8c1-1f3d-457d-acac-2ed565da3709-ledger-97
2021-04-29 07:25:48        327 56dde8c1-1f3d-457d-acac-2ed565da3709-ledger-97-index
2021-04-29 07:51:39   37345792 9f9eda83-92ac-47dc-bb9e-74ec7d50704f-ledger-123
2021-04-29 07:51:40        287 9f9eda83-92ac-47dc-bb9e-74ec7d50704f-ledger-123-index
```
10. Another way to demo the achievement is by listening to the pulsar topic and reading the topic.
a. Using the pulsar-subscriber which will receive latest messages on the topic.
b. Using the pulsar-reader.py which reads messages from the beginning of the topic.
Pulsar-reader reading messages shows that the topic messages are safely stored on low cost cortx store, to be used later even after years. This is useful for recommendation engines and trace simulation.

# Common Issues

## Cortx VM is inaccessible from local machine
Related to [this issue](https://github.com/Seagate/cortx/issues/985).
After poweroff and restart of VM, ssh into it fails:
```
ssh cortx@local.seagate.com
ssh: Could not resolve hostname local.seagate.com: nodename nor servname provided, or not known
```


### Workaround
Find all three IPs exposed by VM(See Section 11 [here](https://github.com/Seagate/cortx/blob/main/doc/OVA/1.0.4/CORTX_on_Open_Virtual_Appliance.rst)) and try each of IPs against the hostname.

In `/etc/hosts`, in the below lines, try each of the IPs of the VM:
```
192.168.0.140 local.seagate.com s3.seagate.com sts.seagate.com iam.seagate.com sts.cloud.seagate.com
192.168.0.138 management.seagate.com
```

## Management GUI of Cortex is not accessible
Related to [this issue](https://github.com/Seagate/cortx/issues/985).
The issue is similar to the above issue with ssh access. This time the IP mapping of `management.seagate.com` is the issue. Every poweroff and restart of VM can potentially create this issue.

## Clock skew
If the VM time is too off from the correct time/time of the client making a request, http requests fail with a clock skew error.

This can happen with aws cli command(which uses http request underneath):
```
aws --profile=sg s3 mb s3://seagatebucket2
make_bucket failed: s3://seagatebucket2 An error occurred (RequestTimeTooSkewed) when calling the CreateBucket operation: The difference between request time and current time is too large
```

It can also happen in Pulsar requests to offload topic data to S3. 

### Fix
Login to Cortx VM and set date to correct value or offset the time as below based on the difference between current VM time and correct time:
```
sudo su -
date -s "-xhours -yminutes +zseconds"
```

## AWS Credentials issues

s3 login fails or Pulsar could not write data to S3 due to credentials issue.

### Fix
1. Remember to replace the AWS access key and secret key in ~/.aws/credentials with the ones generated with Cortx GUI.
2. Remember to replace the AWS access key and secret key in pulsar-env.sh and source the file before running Pulsar.

## Cortx GUI is sometimes too slow
Sometimes management GUI can be ridiculously slow with a most of the requests failing(This can be observed in Network section of browser's developer tools). This causes modal messages saying waiting for server and s3 account creation fails.

### Workaround
Give the server a long time to relax between requests, until s3 access key pair is generated. Waiting for 30 seconds improved success rate of requests.

## Cort Multipart Uploads Alignment issues
Related to [this issue](https://github.com/Seagate/cortx-s3server/issues/876).

```
11:59:04.691 [offloader-OrderedScheduler-1-0] ERROR org.jclouds.http.internal.JavaUrlHttpCommandExecutorService - error after writing 65626/3283684 bytes to http://s3.seagate.com/pulsar-topic-test/70cec628-d533-4b0e-b1fa-da3cb132cfc6-ledger-121?partNumber=1&uploadId=cfe51210-144e-4274-953b-5cf2ab77c595
```

Cause: Forgetting to set LAYOUT_ID in Cortx config. See Section 4 in the `Setting Up Cortex` section above.


### Fix

In the VM, open the file `/opt/seagate/cortx/s3/conf/s3config.yaml` and change S3_MOTR_LAYOUT_ID to 1(`S3_MOTR_LAYOUT_ID: 1`) and restart Cortx as per instructions in [documentation](https://github.com/Seagate/cortx/blob/main/doc/OVA/1.0.4/CORTX_on_Open_Virtual_Appliance.rst).
