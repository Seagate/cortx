#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#######################################################################
# Author                : Venkatesh K
# Date                  : 20-01-2021
# Description           : Used to get total user count from slack
# Usage                 : python file_name.py
#######################################################################
import re
import argparse
import requests
import json
import os
import pickle
from cryptography.fernet import Fernet
from datetime import datetime

def get_args():

    # Create the parser
    parser = argparse.ArgumentParser()

    # Add the arguments
    parser.add_argument('--token', '-t', required=True)

    # Execute the parse_args() method and return
    return parser.parse_args()


token = os.getenv('SLACK_OATH')
url = 'https://cortxcommunity.slack.com/api/team.stats.export'

payload = {'token': token, 'offline': 'false'}
WORKSPACE_INFO = {
    "MINIO": {
        "workspace_id": "T3NG3BE8L",
        "channel_id": "C3NDUB8UA",
    },
    "CEPH": {
        "workspace_id": "T0HFAB7T3",
        "channel_id": "C0HF72CER"
    },
    "OPENIO": {
        "workspace_id": "T1DEE58R4",
        "channel_id": "C1DET3MK3"
    },
    "DAOS": {
        "workspace_id": "T4RUE2FDH",
        "channel_id": "C4SM0RZ54"
    },
    "OPENSTACK": {
        "workspace_id": "T0D0VLNG7",
        "channel_id": "C0D0Z7XFB"
    },
}


def clean(data):
    """ Clean the html tags and remove un-wanted white space
    Parameters
    ----------
    Data:
       Raw html data with tags & multiple whitespaces
    Returns:
       Remove spaces, taggs and return the enriched data
    """
    data = str(data)
    data = data.replace(',', '')
    data = re.sub('\s+', ' ', data)
    data = re.sub('<[^>]*?>', '', data)
    data = data.strip()
    return data


def api_process_post_method(workspace):
    """ Process the slack url to get user count
    If the argument isn't passed in, the function throw an error.
    Parameters
    ----------
    Workspace: MINIO, CEPH, OPENIO, etc..
    -------------------------------------

    Returns:
    --------
    Total user count for the given workspace

    """
    url = "https://edgeapi.slack.com/cache/%s/users/counts" % (WORKSPACE_INFO[workspace]["workspace_id"])
    channel_id = WORKSPACE_INFO[workspace]["channel_id"]

    try:
        payload = {
            "token": os.environ[workspace],
            "channel": channel_id,
            "as_admin": False
        }
        cookie = workspace + '_COOKIE'
        # print (cookie)
        headers = {'content-type': 'application/json', 'cookie': os.environ[cookie]}

        r = requests.post(url, data=json.dumps(payload), headers=headers)
        return r.json()
    except:
        pass


def download_csv(type, date_range):
    """ Download the csv file for the given type and save it.
    Parameters:
    ----------
    Type: overview, channels, users
       Date Range: 30d, 15d, 1d

    Returns:
       None.
    """
    print('Beginning %s file download with requests' % (type))
    try:
        payload.update({'type': type, 'date_range': date_range})
        response = requests.get(url, params=payload)
        filename = type + '.csv'
        with open(filename, 'wb') as f:
            f.write(response.content)
    except:
        pass
        # todo
    return


def open_pickle(path: str):
    """ 
    Open a pickle and return loaded pickle object.
    :type path: str
    
    :param : path: File path to pickle file to be opened.
    :rtype : object
    """
    try:
        with open(path, 'rb') as opened_pickle:
            try:
                return pickle.load(opened_pickle)
            except Exception as pickle_error:
                print (pickle_error)
                raise
    except FileNotFoundError as fnf_error:
        return dict()
    except IOError as io_err:
        print (io_err)
        raise
    except EOFError as eof_error:
        print (eof_error)
        raise
    except pickle.UnpicklingError as unp_error:
        print (unp_error)
        raise

def main():
    workspaces = ["MINIO", "OPENIO", "DAOS", "CEPH", "OPENSTACK"]
    pkl_obj_file = "../pickles/slack_users_stats.pickle"
    workspace_list = {}
    for workspace in workspaces:
        result = api_process_post_method(workspace)
        print("Processing %s..." % (workspace))
        workspace_list[workspace] = result
        # print(json.dumps(result, indent=4, sort_keys=True))

    pkl_dict = open_pickle(pkl_obj_file)
    date = datetime.today().strftime('%Y-%m-%d')

    pkl_dict[date] = workspace_list
    print ("Updating the slack user count to pickle object")
    print (json.dumps(workspace_list, indent=4, sort_keys=True))

    # Its important to use binary mode
    dbfile = open(pkl_obj_file, 'wb')
    pickle.dump(pkl_dict, dbfile)

    # todo
    # download_csv('overview', '30d')
    # download_csv('users', '30d')
    # download_csv('channels', '30d')


if __name__ == '__main__':
    main()
