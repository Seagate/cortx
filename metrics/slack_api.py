#!/usr/bin/env python
# -*- coding: utf-8 -*-

import argparse
import datetime
from slack_sdk import WebClient
#import slack
import os
import json
from time import sleep

try:
    from configparser import ConfigParser
except ImportError:
    from ConfigParser import ConfigParser  # ver. < 3.0

def get_args():

    # Create the parser
    parser = argparse.ArgumentParser()

    # Add the arguments
    #parser.add_argument('--file', '-f', required=True)

    # Execute the parse_args() method and return
    return parser.parse_args()


def read_config():
   # instantiate
   config = ConfigParser()

   # parse existing file
   config.read('slack_config.ini')

   # read values from a section
   minio_api = config.get('minio', 'api')
   daos_api = config.get('daos', 'api')

   return minio_api, daos_api

def get_client():
  print (os.environ['HOME'])
  return WebClient(token=os.environ['SLACK_OATH'])

def get_stats():
    """Get the stats for a given slack channels

    If the argument isn't passed in, the default will be used

    Parameters
    ----------
    channel_name : str, require
	The channel of the slack

    """
    # init web client
    client = get_client()
    print (client)

def main():
    minio_api, daos_api = read_config()
    get_stats()

if __name__ == '__main__':
  main()
