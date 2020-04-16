"""
This class act as object recovery processor which consumes
the rabbitmq message queue.
"""
#!/usr/bin/python3.6

import os
import traceback
import logging
import datetime
from logging import handlers

from s3backgrounddelete.object_recovery_queue import ObjectRecoveryRabbitMq
from s3backgrounddelete.eos_core_config import EOSCoreConfig


class ObjectRecoveryProcessor(object):
    """Provides consumer for object recovery"""

    def __init__(self):
        """Initialise Server, config and create logger."""
        self.server = None
        self.config = EOSCoreConfig()
        self.create_logger_directory()
        self.create_logger()
        self.logger.info("Initialising the Object Recovery Processor")

    def consume(self):
        """Consume the objects from object recovery queue."""
        self.server = None
        try:
            self.server = ObjectRecoveryRabbitMq(
                self.config,
                self.config.get_rabbitmq_username(),
                self.config.get_rabbitmq_password(),
                self.config.get_rabbitmq_host(),
                self.config.get_rabbitmq_exchange(),
                self.config.get_rabbitmq_queue_name(),
                self.config.get_rabbitmq_mode(),
                self.config.get_rabbitmq_durable(),
                self.logger)
            self.logger.info("Consumer started at " +
                             str(datetime.datetime.now()))
            self.server.receive_data()
        except BaseException:
            if self.server:
                self.server.close()
            self.logger.error("main except:" + str(traceback.format_exc()))

    def create_logger(self):
        """Create logger, file handler, formatter."""
        # Create logger with "object_recovery_processor"
        self.logger = logging.getLogger(
            self.config.get_processor_logger_name())
        self.logger.setLevel(self.config.get_file_log_level())
        # create file handler which logs even debug messages
        fhandler = logging.handlers.RotatingFileHandler(self.config.get_processor_logger_file(), mode='a',
                                                        maxBytes = self.config.get_max_bytes(),
                                                        backupCount = self.config.get_backup_count(), encoding=None,
                                                        delay=False )
        fhandler.setLevel(self.config.get_file_log_level())
        # create console handler with a higher log level
        chandler = logging.StreamHandler()
        chandler.setLevel(self.config.get_console_log_level())
        # create formatter and add it to the handlers
        formatter = logging.Formatter(self.config.get_log_format())
        fhandler.setFormatter(formatter)
        chandler.setFormatter(formatter)
        # add the handlers to the logger
        self.logger.addHandler(fhandler)
        self.logger.addHandler(chandler)

    def close(self):
        """Stop processor and close rabbitmq connection."""
        self.logger.info("Stopping the processor and rabbitmq connection")
        self.server.close()
        # perform an orderly shutdown by flushing and closing all handlers
        logging.shutdown()

    def create_logger_directory(self):
        """Create log directory if not exsists."""
        self._logger_directory = os.path.join(self.config.get_logger_directory())
        if not os.path.isdir(self._logger_directory):
            try:
                os.mkdir(self._logger_directory)
            except BaseException:
                self.logger.error(
                    "Unable to create log directory at " + self._logger_directory)

if __name__ == "__main__":
    PROCESSOR = ObjectRecoveryProcessor()
    PROCESSOR.consume()
