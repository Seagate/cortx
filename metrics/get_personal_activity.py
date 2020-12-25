#! /usr/bin/env python3

import cortx_community 

import argparse
import dateutil.parser as dateparser
import datetime
import json
import os
import pickle
import pprint
import sys
import time
from github import Github

def get_group(Type,people):
  group = set()
  for login,person in people.items():
    if person.get_type() == Type or Type == 'All':
      group.add(login)
  return group

def get_logins(CSV):
  logins = set()
  people = None
  for login in CSV.split(','):
    if login == 'External' or login == 'Innersource' or login == 'All':
      if people is None:
        people=cortx_community.CortxCommunity()
      logins |= get_group(Type=login,people=people)
    else:
      logins.add(login)
  return logins

def main():
  parser = argparse.ArgumentParser(description='Retrieve all activity done by a particular user.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('login', metavar='LOGIN', type=str, help="Comma-separate lists of logins [can use External or Innersource or All as wildcards]")
  parser.add_argument('-s', '--since', type=str, help="Only show activity since yyyy-mm-dd")
  parser.add_argument('-u', '--until', type=str, help="Only show activity until yyyy-mm-dd")
  parser.add_argument('-l', '--last_week', action='store_true', help="Only show activity in the last seven days")
  args = parser.parse_args()

  activity = cortx_community.CortxActivity()

  if args.since:
    args.since = dateparser.parse(args.since)
  if args.until:
    args.until = dateparser.parse(args.until)
  if args.last_week:
    args.since = datetime.datetime.today() - datetime.timedelta(days=7)
  daterange = "since %s" % (args.since.strftime('%Y-%m-%d') if args.since else "inception")
  if args.until:
    daterange += " until %s" % args.until.strftime('%Y-%m-%d')

  activities = {}
  logins = get_logins(args.login)
  for login in logins:
    activities[login] = {}
    try:
      # create a new structure to hold the data in an organization that is more easily sorted
      # go through the activity and save each into the new format
      # problem is that the watch event doesn't have a date for it . . . 
      for (url,created_at) in activity.get_activity(login):
        if created_at is not None:  # just don't count watch events since they don't have a date
          activities[login][created_at] = url
    except KeyError: 
      pass
      #print("Login %s has no observed activity" % login)

  # using the new data structure, filter by since and until 
  filtered_activities = {}
  if args.since or args.until:
    for login,actions in sorted(activities.items()):
      filtered_activities[login]={}
      for d,u in sorted(actions.items()):
        if args.since and d < args.since:
          continue
        if args.until and d > args.until:
          continue
        filtered_activities[login][d]=u
  else:
    filtered_activities = activities

  # now print from the filtered list
  total_actions = 0
  for login,actions in sorted(filtered_activities.items()):
    if len(actions) > 0:
      print("%d actions for %s %s" % (len(actions),login, daterange))
      total_actions += len(actions)
    for d,u in sorted(actions.items()):
      print("\t%s %s %s" % (login,d,u))
      
  print("SUMMARY: %d total observed actions from %s %s" % (total_actions, args.login, daterange))

if __name__ == "__main__":
    main()

