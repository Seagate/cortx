#!/usr/bin/env python
# -*- coding: utf-8 -*-
#######################################################################
# Author		: Venkatesh K
# Date			: 20-01-2021
# Description		: Used to get total user count  from slack
# Usage			: python file_name.py
#######################################################################
import re
import requests
import json
import os
import sys
import html
#from dateutil import parser
import datetime

def clean(data):
    data = str(data)
    data = data.replace(',', '')
    data = re.sub('\s+', ' ', data)
    data = re.sub('<[^>]*?>', '', data)
    data = data.strip()
    return data


def api_process(url):
   """ Process the slack url to get user count
    If the argument isn't passed in, the function throw an error.
    Parameters
    ----------
    Url : str, require
	The url for minio, ceph and daos
    Returns:
        The user count of given url.
    """
    try:
        response = requests.get(url)
        data = html.unescape(response.text)
        count_obj = re.search(r'\"total\">(\d+)<\/b>\s*users', data)
        total_count = clean(count_obj.group(1))
    except:
        pass
        #todo
    return total_count

def main():
    result = api_process("https://slack.openio.io/")
    print ("Minio total count %s" %(result))
    result = api_process("http://slackin-ceph-public.herokuapp.com/")
    print ("Ceph total count %s" %(result))

if __name__ == '__main__':
  main()

