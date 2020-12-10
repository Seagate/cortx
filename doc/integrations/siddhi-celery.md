# CORTX Event-Driven Processing Concept for  Data Ingestion Pipelines
Our integration idea is to add event-driven processing capabilities on top of CORTX in order to allow customers to define complex data-based rules on S3 operations and trigger workers accordingly.

This concept is based on AWS Lambda and other serverless solutions, but we are adding an advanced rule engine (Siddhi) which makes the routing and rule definition much more flexible.

One use case can be automotive systems development where a very large amount of data is ingested on a daily basis. Once that data is put inside the CORTX S3, our rule engine would decide which artifacts can be processed locally and which should be copied to the cloud, activating the proper workers that would do the actual job. We can also trigger pre-processing workers that could compress or reduce the data size so that the cloud upload would be faster.
Other use cases : automatic virus scans, media files optimization, sensitive information detection, etc.

We initially intended to use an event-driven approach through a pub-sub mechanism listening on CORTX events, but it seems that there is no such mechanism available at this time. Instead, we went for a polling approach using S3 list buckets and list objects commands. The events are fed into Siddhi which is an advanced rule engine from WSO2. When the rules we defined were triggered, a corresponding job was sent to the Celery job queue and executed by a worker from the workers pool.
Siddhi allows for an extremely flexible rules (or "queries") that can detect almost any pattern within a time-series data stream. for example "event A happened 5 times within 3 minutes" or "event A happened, but event B didn't follow it within 2 minutes". Those rules can be added and remove at run time and once triggered invoke a callback which in our case triggers a job.

![overview](/doc/images/siddhi-celery-pipeline.png)

links: 

[siddhi](https://siddhi.io/)

[celery](https://docs.celeryproject.org/en/stable/)

This is how it's done:

1. clone the repo

2. install java 8 and higher and make sure JAVA_HOME is set in the environment variables

3. download and install siddhi from https://github.com/siddhi-io/siddhi-sdk/releases/download/v5.1.2/siddhi-sdk-5.1.2.zip and copy it into the siddhi-celery/ directory

4. run "sudo apt install python3-venv"

5. run ". ./env"

6. goto siddhi-celery/scripts and run the celery job queue and the siddhi rule engine:
![screensho1](/doc/images/siddhi1.png)

7. copy a log file (file extension should be *.log) into the bucket named "logs":
![screensho2](/doc/images/siddhi2.png)

8. within a few seconds, the file would be compressed and the original file would be deleted:
![screensho3](/doc/images/siddhi3.png)
![screensho3](/doc/images/siddhi4.png)


