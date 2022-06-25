import logging
import time

from colorama import Fore, Style

from PySiddhi.DataTypes.LongType import LongType
from PySiddhi.core.SiddhiManager import SiddhiManager
from PySiddhi.core.query.output.callback.QueryCallback import QueryCallback
from PySiddhi.core.util.EventPrinter import PrintEvent

from .monitor import S3Monitor
from .s3_bucket import tasks as s3_bucket_tasks
#import s3_bucket.tasks


logger = logging.getLogger(__name__)


INPUT_STREAM_NAME = "cortxEventStream"
#LOG_INPUT_STREAM_NAME = "cortxEventStream"
LOG_QUERY_NAME = "cortxEventQuery"

#BUCKET_INPUT_STREAM_NAME = "cortxBucketStream"
BUCKET_QUERY_NAME = "cortxBucketQuery"

OBJECT_QUERY_NAME = "cortxObjectQuery"


# Siddhi Query to filter events with volume less than 150 as output
SIDDHI_APP = """\
define stream {stream} (
    event_code string,
    bucket_name string,
    object_name string
);

@info(name = '{log_query}')
from {stream}[event_code == "LOG_CREATED" or event_code == "LOG_DELETED"]#window.lengthBatch(5)
select event_code,bucket_name,object_name
insert into outputStream;

@info(name = '{bucket_query}')
from {stream}[event_code == "BUCKET_CREATED" or event_code == "BUCKET_DELETED"]#window.timeBatch(10 sec)
select event_code,bucket_name,object_name
insert into outputStream;

@info(name = '{object_query}')
from {stream}[(event_code == "OBJECT_CREATED" or event_code == "OBJECT_DELETED") and regex:matches('.+\.log', object_name)]#window.timeBatch(10 sec)
select event_code,bucket_name,object_name
insert into outputStream;"
""".format(stream=INPUT_STREAM_NAME, log_query=LOG_QUERY_NAME, bucket_query=BUCKET_QUERY_NAME, object_query=OBJECT_QUERY_NAME)
#from {stream}[volume < 150]


def run(args):
    logger.info('Bootstrapping:')
    siddhi_manager = SiddhiManager()
    logger.info('Manager up...')
    runtime = siddhi_manager.createSiddhiAppRuntime(SIDDHI_APP)
    logger.info('Runtime up...')

    # Add listener to capture output events
    class LogQueryCallbackImpl(QueryCallback):
        def receive(self, timestamp, inEvents, outEvents):
            #PrintEvent(timestamp, inEvents, outEvents)
            log_filenames = [event.getData(1) for event in inEvents]
            logger.info('%sCompressing log file %s%s%s', Fore.WHITE, Fore.GREEN, ', '.join(log_filenames), Style.RESET_ALL)

    class BucketQueryCallbackImpl(QueryCallback):
        def receive(self, timestamp, inEvents, outEvents):
            #PrintEvent(timestamp, inEvents, outEvents)
            log_filenames = [event.getData(1) for event in inEvents]
            logger.info('%sCompressing log file %s%s%s', Fore.WHITE, Fore.GREEN, ', '.join(log_filenames), Style.RESET_ALL)

    class ObjectQueryCallbackImpl(QueryCallback):
        def receive(self, timestamp, inEvents, outEvents):
            #PrintEvent(timestamp, inEvents, outEvents)
            for event in inEvents:
                logger.info('%sCompressing log file %s%s/%s%s', Fore.WHITE, Fore.GREEN, event.getData(1), event.getData(2), Style.RESET_ALL)
                async_task = s3_bucket_tasks.compress_log.delay(event.getData(1), event.getData(2))
                logger.info('%sAsync task: %s%r%s', Fore.WHITE, Fore.YELLOW, async_task, Style.RESET_ALL)

    runtime.addCallback(LOG_QUERY_NAME, LogQueryCallbackImpl())
    runtime.addCallback(BUCKET_QUERY_NAME, BucketQueryCallbackImpl())
    runtime.addCallback(OBJECT_QUERY_NAME, ObjectQueryCallbackImpl())

    # Retrieving input handler to push events into Siddhi
    input_handler = runtime.getInputHandler(INPUT_STREAM_NAME)

    # Starting event processing
    logger.info('Starting runtime...')
    runtime.start()

    try:
        monitor = S3Monitor()
        monitor.monitor_buckets(input_handler)
        # dummy_log_events(input_handler)
        logger.info('Waiting for any residual events')
        time.sleep(10)
    except Exception as e:  # pylint: disable=broad-except
        logger.error('UNEXPECTED ERROR: %s', e)
        raise
    finally:
        logger.info('Shutting down...')
        siddhi_manager.shutdown()
        logger.info('Goodbye')


def dummy_log_events(input_handler):
    # Sending events to Siddhi
    logger.info('Sending events')
    for n in range(1, 31):
        event_data = ["LOG_CREATED", "foo.log.%d" % n, '']
        logger.info('Sending event %s', event_data)
        input_handler.send(event_data)
        if n % 3 == 0:
            time.sleep(5)

