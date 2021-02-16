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
from dotenv import load_dotenv
from cryptography.fernet import Fernet


def get_args():

    # Create the parser
    parser = argparse.ArgumentParser()

    # Add the arguments
    parser.add_argument('--token', '-t', required=True)

    # Execute the parse_args() method and return
    return parser.parse_args()

def decrypt(filename, key):
    """
    Given a filename (str) and key (bytes), it decrypts the file and write it
    """
    f = Fernet(key)
    with open(filename, "rb") as file:
        # read the encrypted data
        encrypted_data = file.read()
    # decrypt data
    decrypted_data = f.decrypt(encrypted_data)
    # write the original file
    with open(".env", "wb") as file:
        file.write(decrypted_data)

def load_key():
    """
    Loads the key from the current directory named `key.key`
    """
    cwd = os.getcwd()
    key_path = os.path.join(cwd, "slack.key")
    return open(key_path, "rb").read()

#mykey = os.getenv('SLACK_KEY')
args = get_args()
mykey = args.token
#mykey = "xJ4f4pNM-mRcBKGW-bSYM-iqSnMfuDEKM_s0dfjbI5g="
#mykey = load_key()
print ("Mykey", mykey)

cwd = os.getcwd()
file_path = os.path.join(cwd, "slack.env")
decrypt(file_path, mykey)

load_dotenv()
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


def main():
    workspaces = ["MINIO", "OPENIO", "DAOS", "CEPH", "OPENSTACK"]
    workspace_list = {}
    for workspace in workspaces:
        result = api_process_post_method(workspace)
        print("Processing %s..." % (workspace))
        workspace_list[workspace] = result
        # print(json.dumps(result, indent=4, sort_keys=True))
    # Its important to use binary mode
    dbfile = open('../pickles/slack_users_stats.pickle', 'ab')
    pickle.dump(workspace_list, dbfile)
    print ("List", workspace_list)

    # todo
    # download_csv('overview', '30d')
    # download_csv('users', '30d')
    # download_csv('channels', '30d')


if __name__ == '__main__':
    main()
