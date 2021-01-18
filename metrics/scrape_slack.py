#!/usr/bin/env python3.6
# -*- coding: utf-8 -*-
import argparse
import cortx_community as cc
import datetime
import json
import os
import slack

from time import sleep
from pprint import pprint

# import requests

# https://python-slackclient.readthedocs.io/en/latest/basic_usage.html
# https://api.slack.com/methods/users.getPresence is a member active

MESSAGES_PER_PAGE = 200
MAX_MESSAGES = 1000
GLOBAL='GLOBAL'

# make a helper function to consolidate all calls to the api 
def call_api(client,method,data):
  response = client.api_call(api_method=method,data=data)
  sleep(1.1)
  if not response['ok']:
    print("WTF: call to %s failed" % method)
    assert response['ok']
    return None
  else:
    return response

# pass an optional limit to only get a few channels; useful for fast development
def get_channels(client,limit=None):
  channels={}
  response = call_api(client=client,method="conversations.list",data={'types' : 'public_channel', 'exclude_archived' : 'true'})
  for c in response.data['channels']:
    channels[c['id']] = c['name']
    if limit and len(channels) >= limit:
      break 
  return channels

def join_channels(client,channels):
  for c in channels.keys():
    response = call_api(client=client,method="conversations.join",data={'channel' : c})
    if response.status_code != 200:
      print("Couldn't join channel %s: %d" % (c, response.status_code))

def get_member_count(client,channels):
  members={}
  global_count=0
  for cid,cname in channels.items():
    response = call_api(client=client,method="conversations.info",data={'channel' : cid, 'include_num_members' : 'true'})
    num_members = response.data['channel']['num_members']
    print("%s has %d members" % (cname,num_members))
    members[cname]=num_members
    global_count=max(global_count,num_members)
  members[GLOBAL]=global_count
  return members

    
# note that when our members exceeds 1000, we will have to start making multiple api calls to get the full list
# can borrow the code from get_conversations
def get_members(client,all_people,slack_people):
  members=[]
  active_members=[]
  request = call_api(client=client,method="users.list",data=None)
  for m in request['members']:
    # get presence    
    response = call_api(client=client,method="users.getPresence",data={'user' : m['id']})
    presence=response.data['presence']

    # get profile
    glogin=None
    if slack_people.find_person(m['id']):
      print("Person %s is already in our slack pickle" % m['name'])
      glogin=slack_people.get_github(m['id'])
    else:
      response = call_api(client=client,method="users.profile.get",data={'user' : m['id']})
      try:
        email = response.data['profile']['email']
        real_name = response.data['profile']['real_name']
        found = all_people.find_person(email)
        if found:
          glogin = found.get_login()
        else:
          glogin = "GID_UNK_%s" % m['name']
          all_people.add_person(login=glogin,company=None,email=email,linkedin=None,org_name=None)
        print("Person %s [%s] (%s, %s) %s already in our people pickle" % (real_name,presence,m['name'],email, 'is' if found else 'is not'),found)
        slack_people.add_person(slack_id=m['id'], github=glogin, email=email, name=m['name'])
        all_people.add_note(glogin,{'slack_id' : m['id']})
      except KeyError:
        print("WTF: couldn't get meaningful info for profile of %s" % m['name'])

    if glogin:
      members.append(glogin)
      if presence == 'active':
        active_members.append(glogin)
  slack_people.persist()
  all_people.persist()
  return (members,active_members)

def get_channel_conversations(client,channel,cname):
    # get first page
    page = 1
    messages=[]
    print("Getting conversations for %s" % cname)
    while True:
      cursor=None
      if page > 1:
        cursor = response['response_metadata']['next_cursor']
      response = call_api(client=client,method='conversations.history',data={'channel' : channel, 'limit' : MESSAGES_PER_PAGE, 'cursor' : cursor})
      messages += response['messages']
      page += 1

      if len(messages) >= MAX_MESSAGES or not response['has_more']:
        break

    return messages

def add_activity(glogin,mdate,cname,author_activity):
  key="slack_comment.%s.%s.%s" % (cname,glogin,mdate)
  try:
    (login,url,created_at) = author_activity.get_activity(key)
  except KeyError:
    login = glogin 
    url = "slack_%s" % cname 
    created_at = mdate 
    author_activity.add_activity(key,login,url,created_at)

# conversation also has 'ts' value so we should be able to get only people talking in the last week
def get_conversations(client,channels,slack_people,author_activity):
  week_ago = datetime.date.today() - datetime.timedelta(days=7)
  all_talkers_alltime=set()
  talkers_alltime={}
  all_talkers_weekly=set()
  talkers_weekly={}
  for cid,cname in channels.items():
    talkers_alltime[cname]=set()
    talkers_weekly[cname]=set()
    conversations = get_channel_conversations(client,cid,cname)
    print("Channel %s has %d conversations" % (cname,len(conversations)))
    for c in conversations:
      try:
        glogin=slack_people.get_github(c['user'])
        talkers_alltime[cname].add(glogin)
        all_talkers_alltime.add(glogin)
        mdate=datetime.datetime.fromtimestamp(int(float(c['ts'])))
        if mdate.date() >= week_ago:
          talkers_weekly[cname].add(glogin)
          all_talkers_weekly.add(glogin)
        add_activity(glogin,mdate,cname,author_activity)
      except KeyError:
        pass
      except TypeError:
        print("WTF, user %s not in slack people" % c['user'])
  talkers_alltime[GLOBAL] = all_talkers_alltime
  talkers_weekly[GLOBAL] = all_talkers_weekly
  author_activity.persist()
  return (talkers_alltime,talkers_weekly)

def get_client():
  return slack.WebClient(token=os.environ['SLACK_OATH'])

# create a stats structure similar to what is produced in scrape_metrics.py
def get_stats():

  stats={}

  # load up our people pickle
  author_activity = cc.CortxActivity()     
  all_people = cc.CortxCommunity()
  slack_people = cc.SlackCommunity()

  # init web client
  client = get_client()

  print("Getting channels")
  channels=get_channels(client,limit=None)
  for cname in channels.values():
    stats[cname] = {}
  stats[GLOBAL] = {}
  print(stats)

  print("Joining channels")
  join_channels(client,channels)

  print("Getting member lists")
  (all_members,active_members)=get_members(client,all_people,slack_people)
  print("%d members, %d active members" % (len(all_members),len(active_members)))
  stats[GLOBAL]['slack_members'] = all_members
  stats[GLOBAL]['slack_active_members'] = active_members

  print("Getting talkers lists")
  (all_talkers,weekly_talkers) = get_conversations(client,channels,slack_people,author_activity)
  for cname in channels.values():
    stats[cname]['slack_participants'] = all_talkers[cname]
    stats[cname]['stack_weekly_participants'] = weekly_talkers[cname]
  stats[GLOBAL]['slack_participants'] = all_talkers[GLOBAL]
  stats[GLOBAL]['slack_weekly_participants'] = weekly_talkers[GLOBAL]
  print(stats)

  print("Getting member counts")
  member_counts = get_member_count(client,channels)
  print(member_counts)
  for cname in channels.values():
    stats[cname]['slack_member_count'] = member_counts[cname]
  stats[GLOBAL]['slack_member_count'] = member_counts[GLOBAL]

  return stats

def main():

  stats = get_stats()

  # ok this works pretty well so far.
  # now the thing we need to do is merge these stats into cc.PersistentStats
  # for each channel name, find the corresponding repo and add to latest date for it
  # might be off by a few days but not overly worrisome
  # what to do about the channels which don't have a correspoding repo?
  # general and getting-started should be merged into parent 'cortx'.  What about the rest?
  # maybe just merge all that don't have a 1:1 mapping.
  return

  # write the result to a file
  with open('messages.json', 'w', encoding='utf-8') as f:
    json.dump(messages_all, f, sort_keys=True, indent=4, ensure_ascii=False)


if __name__ == '__main__':
  main()

