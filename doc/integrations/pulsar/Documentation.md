

# Setting up Cortx
1. Install cortx on a VM following the steps [here](https://github.com/Seagate/cortx/blob/main/doc/CORTX_on_Open_Virtual_Appliance.rst)
2. Make sure you set the date to current date and set `/etc/hosts` mapping for s3.seagate.com and management.seagate.com so that the s3 server is accessible from your machine.
3. Try the sanity test for s3 server to make sure everything works.
4. In the VM, open the file /opt/seagate/cortx/s3/conf/s3config.yaml and change S3_MOTR_LAYOUT_ID to 1(`S3_MOTR_LAYOUT_ID: 1`)
5. Restart cortx server as per instructions [here](https://github.com/Seagate/cortx/blob/main/doc/CORTX_on_Open_Virtual_Appliance.rst).
6. Change the system date if necessary.
7. Configure the [Cortx GUI](https://github.com/Seagate/cortx/blob/main/doc/Preboarding_and_Onboarding.rst)
8. Go to https://management.seagate.com:28100/ and after onboarding, in the Manage menu, create and S3 account and download the access key and secret key.

# Setting up AWS CLI
1. First ensure that s3 server of cortx is working and then setup awscli and awscli-plugin-endpoint:
2. To install the AWS client, use: $ pip3 install awscli
3. To install the AWS plugin, use: $ pip3 install awscli-plugin-endpoint
4. Add the following to your `~/.aws/config`
```
[sg]
  output = text
  region = US
  s3 = endpoint_url = http://s3.seagate.com
  s3api = endpoint_url = http://s3.seagate.com
  [plugins]
  endpoint = awscli_plugin_endpoint
```
5. Add the following to your `~/.aws/credentials` and replace the AWS access key and secret key with those generated from Cortx GUI.
```
[sg]
aws_access_key_id=AKIAw5WobgsyTh2rQ2ljuZmweA
aws_secret_access_key=sTNG8gMpkG/AOj+gTi6ayBkdr5aw5Xw5s7R252sF
```
Explainer: The above instructions are an extract of Section 1.4 along with prerequisites from here:
https://github.com/Seagate/cortx-s3server/blob/main/docs/CORTX-S3%20Server%20Quick%20Start%20Guide.md
6. Test the cli by listing buckets:
```
aws --profile=sg s3 ls 
```

# Set up s3 bucket in cortx for offloading topic messages
1. Create the bucket `pulsar-topic-test`.
```
aws --profile=sg s3 mb s3://pulsar-topic-test
```
2. We will use this bucket to push older messages on pulsar topics.

# Setting up Apache Pulsar:
1. Download pulsar binary tar along with offloaders
  https://pulsar.apache.org/download/
2. Untar pulsar binary tar and the offloaders tar
3. cd into the pulsar directory(apache-pulsar-2.7.1). Lets call this PULSAR_HOME and all our work is done from here.
4. copy offloaders folder from the untared offloaders directory into PULSAR_HOME
5. Copy pulsar-env.sh from pulsar folder in this repo into PULSAR_HOME. Change the s3 access key and secret key to the ones created from Cortx GUI.
6. Replace conf/standalone.conf and conf/broker.conf in PULSAR_HOME with the files in pulsar folder in this repo.
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
11. You can verify the offload settings:
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
2. From this repo's folder, in one terminal window run:
```
python3 pulsar-subscriber.py
```
3. From this repo's folder, in another terminal window run:
```
python3 pulsar-producer.py 10
```
4. Pulsar subscriber should be printing 10 messages it received from the producer.
5. Now try publishing 100,000 messages
```
python3 pulsar-producer.py 100000
python3 pulsar-producer.py 100000
```
6. Publish as many and see that messages are received.
7. From the terminal we set aside for admin actions, check the topic's internal stats:
```
./bin/pulsar-admin --admin-url http://localhost:8088 persistent stats-internal public/default/test
```
8. You should see stats like this, indicating that the ledgers for the topic are not offloaded yet(watch for `offloaded: false`):
```
{
  "entriesAddedCounter" : 700000,
  "numberOfEntries" : 831716,
  "totalSize" : 145301702,
  "currentLedgerEntries" : 199999,
  "currentLedgerSize" : 34944587,
  "lastLedgerCreatedTimestamp" : "2021-04-29T07:36:50.012+05:30",
  "waitingCursorsCount" : 1,
  "pendingAddEntriesCount" : 0,
  "lastConfirmedEntry" : "123:199998",
  "state" : "LedgerOpened",
  "ledgers" : [{
    "ledgerId" : 123,
    "entries" : 0,
    "size" : 0,
    "offloaded" : false
  } ],
  "cursors" : {
    "my-subscription2" : {
      "markDeletePosition" : "123:199998",
      "readPosition" : "123:199999",
      "waitingReadOp" : true,
      "pendingReadOps" : 0,
      "messagesConsumedCounter" : 700000,
      "cursorLedger" : 72,
      "cursorLedgerLastEntry" : 135,
      "individuallyDeletedMessages" : "[]",
      "lastLedgerSwitchTimestamp" : "2021-04-29T07:04:13.397+05:30",
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
8. Now check the internal stats and see that the offloaded flag is set true.
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
10. Another way to demo the achievement is by listening to the pulsar topic in two ways.
a. Using the pulsar-subscriber
b. Using the pulsar-reader.py which reads messages from the beginning of the topic.
Pulsar-reader reading messages shows that the topic messages are safely stored on low cost cortx store, to be used later even after years. This is useful for recommendation engines and trace simulation.
