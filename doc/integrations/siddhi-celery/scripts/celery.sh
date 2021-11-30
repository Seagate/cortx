#!/bin/bash

$(dirname $0)/../venv/bin/celery -A cortx_siddhi.app worker --loglevel=INFO

