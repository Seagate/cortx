#!/usr/bin/python
# -*- coding: utf-8 -*-
import os
import slack
import argparse
import json
from time import sleep

# import requests

CHANNEL = 'C01952XCLCF'
MESSAGES_PER_PAGE = 200
MAX_MESSAGES = 1000


def get_args():

    # Create the parser
    parser = argparse.ArgumentParser()

    # Add the arguments
    parser.add_argument('--file', '-f', required=True)
    parser.add_argument('--token', '-t', required=True)

    # Execute the parse_args() method and return
    return parser.parse_args()


def main():

    # init web client
    client = slack.WebClient(token=os.environ['SLACK_TOKEN'])

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

