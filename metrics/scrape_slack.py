#!/usr/bin/env python3.6
# -*- coding: utf-8 -*-
import os
import slack
import argparse
import json
from time import sleep

# import requests

# https://python-slackclient.readthedocs.io/en/latest/basic_usage.html
# https://api.slack.com/methods/users.getPresence is a member active

CHANNEL = 'C01952XCLCF'
MESSAGES_PER_PAGE = 200
MAX_MESSAGES = 1000

def get_channels(client):
  channels={}
  response = client.conversations_list(types="public_channel",exclude_archived=1)
  for c in response.data['channels']:
    channels[c['id']] = c['name']
  return channels

def join_channels(client,channels):
  for c in channels.keys():
    response = client.conversations_join(channel=c)  
    if response.status_code != 200:
      print("Couldn't join channel %s: %d" % (c, response.status_code))

def get_member_count(client,channels):
  for cid,cname in channels.items():
    response = client.conversations_info(channel=cid,include_num_members=True)
    members = response.data['channel']['num_members']
    print("%s has %d members" % (cname,members))
    

def get_members(client):
  members=[]
  request = client.api_call("users.list")
  for m in request['members']:
    members.append(m['name'])
  return members

def main():

    # init web client
    client = slack.WebClient(token=os.environ['SLACK_OATH'])

    # get all public channels
    channels=get_channels(client)

    # join all public channels
    join_channels(client,channels)

    get_member_count(client,channels)

    members=get_members(client)
    print("%d members" % len(members), members)

    return 

    # get first page
    page = 1
    print ('Retrieving page {}'.format(page))
    response = client.conversations_history(channel=CHANNEL,
            limit=MESSAGES_PER_PAGE)
    assert response['ok']
    messages_all = response['messages']

    # get additional pages if below max message and if they are any
    while len(messages_all) + MESSAGES_PER_PAGE <= MAX_MESSAGES \
        and response['has_more']:
        page += 1
        print ('Retrieving page {}'.format(page))
        sleep(1)  # need to wait 1 sec before next call due to rate limits
        response = client.conversations_history(channel=CHANNEL,
                limit=MESSAGES_PER_PAGE,
                cursor=response['response_metadata']['next_cursor'])
        assert response['ok']
        messages = response['messages']
        messages_all = messages_all + messages

    print ('Fetched a total of {} messages from channel {}'.format(len(messages_all),
            CHANNEL))

    # write the result to a file
    with open('messages.json', 'w', encoding='utf-8') as f:
        json.dump(messages_all, f, sort_keys=True, indent=4,
                  ensure_ascii=False)


if __name__ == '__main__':
    main()

