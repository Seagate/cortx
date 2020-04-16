## Steps to configure & run S3backgrounddelete process

----
1. Install & configure RabbbitMQ as per {s3 src}/s3backgrounddelete/rabbitmq_setup.md

2. Create s3backgrounddelete log folder
    >mkdir -p /var/log/seagate/s3/s3backgrounddelete

3. Start S3 and mero services after setting S3_SERVER_ENABLE_OBJECT_LEAK_TRACKING to true in
    /opt/seagate/s3/conf/s3config.yaml.

4. Create an account with account name s3-background-delete-svc and update
    access_key and secret_key in /opt/seagate/s3/s3backgrounddelete/config.yaml

5. Use following scripts to generate stale oids in s3server
    >{s3 src}/s3backgrounddelete/scripts/delete_object_leak.sh
    {s3 src}/s3backgrounddelete/scripts/update_object_leak.sh

6. Add proper schedule_interval_secs in
    /opt/seagate/s3/s3backgrounddelete/config.yaml for s3backgounddelete process. Default is 900 seconds.

7. Start S3backgrounddelete scheduler service

    >systemctl start s3backgroundproducer

8. Start S3backgrounddelete processor service

    >systemctl start s3backgroundconsumer

9.  Check scheduler and processor logs at
    >/var/log/seagate/s3/s3backgrounddelete/object_recovery_scheduler.log &
    >/var/log/seagate/s3/s3backgrounddelete/object_recovery_processor.log respectively.
----
