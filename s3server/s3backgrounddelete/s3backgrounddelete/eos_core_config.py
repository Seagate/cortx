"""This class stores the configuration required for s3 background delete."""

import sys
import os
import shutil
import logging
import yaml


class EOSCoreConfig(object):
    """Configuration for s3 background delete."""
    _config = None
    _conf_file = None

    def __init__(self):
        """Initialise logger and configuration."""
        self.logger = logging.getLogger(__name__ + "EOSCoreConfig")
        self._load_and_fetch_config()

    @staticmethod
    def get_conf_dir():
        """Return configuration directory location."""
        return os.path.join(os.path.dirname(__file__), 'config')

    def _load_and_fetch_config(self):
        """Populate configuration data."""
        conf_home_dir = os.path.join(
            '/', 'opt', 'seagate', 's3', 's3backgrounddelete')
        self._conf_file = os.path.join(conf_home_dir, 'config.yaml')
        if not os.path.isfile(self._conf_file):
            try:
                os.stat(conf_home_dir)
            except BaseException:
                os.mkdir(conf_home_dir)
            shutil.copy(
                os.path.join(
                    self.get_conf_dir(),
                    's3_background_delete_config.yaml'),
                self._conf_file)

        if not os.access(self._conf_file, os.R_OK):
            self.logger.error(
                "Failed to read " +
                self._conf_file +
                " it doesn't have read access")
            sys.exit()
        with open(self._conf_file, 'r') as file_config:
            self._config = yaml.safe_load(file_config)

    def get_logger_directory(self):
        """Return logger directory path for background delete from config file or KeyError."""
        if 'logconfig' in self._config and self._config['logconfig']['logger_directory']:
            return self._config['logconfig']['logger_directory']
        else:
            raise KeyError(
                "Could not parse logger directory path from config file " +
                self._conf_file)

    def get_scheduler_logger_name(self):
        """Return logger name for scheduler from config file or KeyError."""
        if 'logconfig' in self._config and self._config['logconfig']['scheduler_logger_name']:
            return self._config['logconfig']['scheduler_logger_name']
        else:
            raise KeyError(
                "Could not parse scheduler loggername from config file " +
                self._conf_file)

    def get_processor_logger_name(self):
        """Return logger name for processor from config file or KeyError."""
        if 'logconfig' in self._config and self._config['logconfig']['processor_logger_name']:
            return self._config['logconfig']['processor_logger_name']
        else:
            raise KeyError(
                "Could not parse processor loggername from config file " +
                self._conf_file)

    def get_scheduler_logger_file(self):
        """Return logger file for scheduler from config file or KeyError."""
        if 'logconfig' in self._config and self._config['logconfig']['scheduler_log_file']:
            return self._config['logconfig']['scheduler_log_file']
        else:
            raise KeyError(
                "Could not parse scheduler logfile from config file " +
                self._conf_file)

    def get_processor_logger_file(self):
        """Return logger file for processor from config file or KeyError."""
        if 'logconfig' in self._config and self._config['logconfig']['processor_log_file']:
            return self._config['logconfig']['processor_log_file']
        else:
            raise KeyError(
                "Could not parse processor loggerfile from config file " +
                self._conf_file)

    def get_file_log_level(self):
        """Return file log level from config file or KeyError."""
        if 'logconfig' in self._config and self._config['logconfig']['file_log_level']:
            return self._config['logconfig']['file_log_level']
        else:
            raise KeyError(
                "Could not parse file loglevel from config file " +
                self._conf_file)

    def get_console_log_level(self):
        """Return console log level from config file or KeyError."""
        if 'logconfig' in self._config and self._config['logconfig']['console_log_level']:
            return self._config['logconfig']['console_log_level']
        else:
            raise KeyError(
                "Could not parse console loglevel from config file " +
                self._conf_file)

    def get_log_format(self):
        """Return log format from config file or KeyError."""
        if 'logconfig' in self._config and self._config['logconfig']['log_format']:
            return self._config['logconfig']['log_format']
        else:
            raise KeyError(
                "Could not parse log format from config file " +
                self._conf_file)

    def get_eos_core_endpoint(self):
        """Return endpoint from config file or KeyError."""
        if 'eos_core' in self._config and self._config['eos_core']['endpoint']:
            return self._config['eos_core']['endpoint']
        else:
            raise KeyError(
                "Could not find eos_core endpoint from config file " +
                self._conf_file)

    def get_eos_core_service(self):
        """Return service from config file or KeyError."""
        if 'eos_core' in self._config and self._config['eos_core']['service']:
            return self._config['eos_core']['service']
        else:
            raise KeyError(
                "Could not find eos_core service from config file " +
                self._conf_file)

    def get_eos_core_region(self):
        """Return region from config file or KeyError."""
        if 'eos_core' in self._config and self._config['eos_core']['default_region']:
            return self._config['eos_core']['default_region']
        else:
            raise KeyError(
                "Could not find eos_core default_region from config file " +
                self._conf_file)

    def get_eos_core_access_key(self):
        """Return access_key from config file or KeyError."""
        if 'eos_core' in self._config and self._config['eos_core']['access_key']:
            return self._config['eos_core']['access_key']
        else:
            raise KeyError(
                "Could not find eos_core access_key from config file " +
                self._conf_file)

    def get_eos_core_secret_key(self):
        """Return secret_key from config file or KeyError."""
        if 'eos_core' in self._config and self._config['eos_core']['secret_key']:
            return self._config['eos_core']['secret_key']
        else:
            raise KeyError(
                "Could not find eos_core secret_key from config file " +
                self._conf_file)

    def get_daemon_mode(self):
        """Return daemon_mode flag value for scheduler from config file\
           else it should return default as "True"."""
        if 'eos_core' in self._config and self._config['eos_core']['daemon_mode']:
            return self._config['eos_core']['daemon_mode']
        else:
            #Return default mode as daemon mode i.e. "True"
            return "True"

    def get_rabbitmq_username(self):
        """Return username of rabbitmq from config file or KeyError."""
        if 'rabbitmq' in self._config and self._config['rabbitmq']['username']:
            return self._config['rabbitmq']['username']
        else:
            raise KeyError(
                "Could not parse rabbitmq username from config file " +
                self._conf_file)

    def get_rabbitmq_password(self):
        """Return password of rabbitmq from config file or KeyError."""
        if 'rabbitmq' in self._config and self._config['rabbitmq']['password']:
            return self._config['rabbitmq']['password']
        else:
            raise KeyError(
                "Could not parse rabbitmq password from config file " +
                self._conf_file)

    def get_rabbitmq_host(self):
        """Return host of rabbitmq from config file or KeyError."""
        if 'rabbitmq' in self._config and self._config['rabbitmq']['host']:
            return self._config['rabbitmq']['host']
        else:
            raise KeyError(
                "Could not parse rabbitmq host from config file " +
                self._conf_file)

    def get_rabbitmq_queue_name(self):
        """Return queue name of rabbitmq from config file or KeyError."""
        if 'rabbitmq' in self._config and self._config['rabbitmq']['queue']:
            return self._config['rabbitmq']['queue']
        else:
            raise KeyError(
                "Could not parse rabbitmq queue from config file " +
                self._conf_file)

    def get_rabbitmq_exchange(self):
        """
        Return exchange name of rabbitmq from config file.
        The exchange parameter is the name of the exchange.
        The empty string denotes the default or nameless exchange messages are
        routed to the queue with the name specified by routing_key,if it exists
        """
        return self._config['rabbitmq']['exchange']

    def get_rabbitmq_exchange_type(self):
        """Return exchange type of rabbitmq from config file or KeyError."""
        if 'rabbitmq' in self._config and self._config['rabbitmq']['exchange_type']:
            return self._config['rabbitmq']['exchange_type']
        else:
            raise KeyError(
                "Could not parse rabbitmq exchange_type from config file " +
                self._conf_file)

    def get_rabbitmq_mode(self):
        """Return mode of rabbitmq from config file or KeyError."""
        if 'rabbitmq' in self._config and self._config['rabbitmq']['mode']:
            return self._config['rabbitmq']['mode']
        else:
            raise KeyError(
                "Could not parse rabbitmq mode from config file " +
                self._conf_file)

    def get_rabbitmq_durable(self):
        """Return durable of rabbitmq from config file or KeyError."""
        if 'rabbitmq' in self._config and self._config['rabbitmq']['durable']:
            return self._config['rabbitmq']['durable']
        else:
            raise KeyError(
                "Could not parse rabbitmq durable from config file " +
                self._conf_file)

    def get_schedule_interval(self):
        """Return schedule interval of rabbitmq from config file or KeyError."""
        if 'rabbitmq' in self._config and self._config['rabbitmq']['schedule_interval_secs']:
            return self._config['rabbitmq']['schedule_interval_secs']
        else:
            raise KeyError(
                "Could not parse rabbitmq schedule interval from config file " +
                self._conf_file)

    def get_probable_delete_index_id(self):
        """Return probable delete index-id from config file or KeyError."""
        if 'indexid' in self._config and self._config['indexid']['probable_delete_index_id']:
            return self._config['indexid']['probable_delete_index_id']
        else:
            raise KeyError(
                "Could not parse probable delete index-id from config file " +
                self._conf_file)

    def get_max_keys(self):
        """Return maximum number of keys from config file or KeyError."""
        if 'indexid' in self._config and self._config['indexid']['max_keys']:
            return self._config['indexid']['max_keys']
        else:
            return 1000

    def get_global_instance_index_id(self):
        """Return probable delete index-id from config file or KeyError."""
        if 'indexid' in self._config and self._config['indexid']['global_instance_index_id']:
            return self._config['indexid']['global_instance_index_id']
        else:
            raise KeyError(
                "Could not parse global instance index-id from config file " +
                self._conf_file)

    def get_max_bytes(self):
        """Return maximum bytes for a log file"""
        if 'logconfig' in self._config and self._config['logconfig']['max_bytes']:
            return self._config['logconfig']['max_bytes']
        else:
            raise KeyError(
                "Could not parse maxBytes from config file " +
                self._conf_file)

    def get_backup_count(self):
        """Return count of log files"""
        if 'logconfig' in self._config and self._config['logconfig']['backup_count']:
            return self._config['logconfig']['backup_count']
        else:
            raise KeyError(
                "Could not parse backupcount from config file " +
                self._conf_file)

    def get_leak_processing_delay_in_mins(self):
        """Return 'leak_processing_delay_in_mins' from 'leakconfig' section """
        if 'leakconfig' in self._config and self._config['leakconfig']['leak_processing_delay_in_mins']:
            return self._config['leakconfig']['leak_processing_delay_in_mins']
        else:
            # default delay is 15mins
            return 15

    def get_version_processing_delay_in_mins(self):
        """Return 'version_processing_delay_in_mins' from 'leakconfig' section """
        if 'leakconfig' in self._config and self._config['leakconfig']['version_processing_delay_in_mins']:
            return self._config['leakconfig']['version_processing_delay_in_mins']
        else:
            # default delay is 15mins
            return 15

