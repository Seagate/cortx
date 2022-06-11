import boto3
from celery import Celery

import config

app = Celery('tasks', broker='redis://localhost//', backend = 'redis://localhost/')
app.conf.task_routes = {'s3_logs.tasks.*': {'queue': 'logs'}}

import s3_logs.tasks
import s3_events.tasks

