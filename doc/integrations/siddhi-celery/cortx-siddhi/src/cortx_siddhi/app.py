import boto3
from celery import Celery

from cortx_siddhi import config

app = Celery('tasks', broker='redis://localhost//', backend = 'redis://localhost/')
app.conf.task_routes = {'cortx_siddhi.s3_logs.tasks.*': {'queue': 'logs'}}

import cortx_siddhi.s3_logs.tasks
import cortx_siddhi.s3_events.tasks
import cortx_siddhi.s3_bucket.tasks

