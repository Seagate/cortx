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
    if login == 'External' or login == 'EU R&D' or login == 'Innersource' or login == 'All' or login == 'Unknown' or login == 'Hackathon':
      if people is None: # because we might come through this path multiple times, cache the pickle
        people=cortx_community.CortxCommunity()
      if login == 'Unknown':
        login = None  # set to None so it will match correctly in get_group
      logins |= get_group(Type=login,people=people)
    else:
      logins.add(login)
  return logins

# a function to pulls stats out of either a PR or a commit
def get_details(url,stx):
  POINTS_COMMENTS=1
  POINTS_ISSUE=3
  POINTS_PULL=5 # but bonus points if lots of lines changed
  POINTS_COMMIT=5 # reward for successfully getting the PR landed
  POINTS_STAR=.1
  POINTS_FORK=.2

  def get_score(Type,lines):
    if Type == 'issues':
      return POINTS_ISSUE
    elif Type == 'pull':
      return POINTS_PULL + int(lines / 10)
    elif Type == 'commit':
      return POINTS_COMMIT
    else:
      raise Exception("Unknown github activity type %s" % Type)
    
  # if it looks like this: https://github.com/Seagate/cortx/commit/946871ec7079647e4e012aa3ea9bd7b6e4539a03 -> then pull the sha and get the details
  # if it looks like this: https://github.com/Seagate/cortx/commit/4beeb0c9438b349561000c00c9c45c0ed37db145#commitcomment-41801950 -> then ignore it because it is a comment
  # if it looks like this: https://github.com/Seagate/cortx/pull/159 -> then pull the integer and get the details
  # if it looks like this:  https://github.com/Seagate/cortx/pull/127#discussion_r471936500 -> then ignore it because it is a comment
  # I was thinking about doing a regex and I actually had a pattern:  m = re.match('https:\/\/github.com\/Seagate\/([a-z-0-9]+)\/(pull|commit)\/([a-z0-9]+)', url)
  # But since we want to avoid the comment lines, I think it's probably just easier to do it with split
  tokens = url.split('/')
  msg = ''
  if '#' not in tokens[-1]:
    if tokens[-2] == 'pull':
      repo = stx.get_repo(tokens[-3])
      pr = repo.get_pull(int(tokens[-1]))
      a = pr.additions
      d = pr.deletions
      f = pr.changed_files
      points = get_score(tokens[-2],a+d)
      msg = ( "%d additions, %d deletions, %d files changed" % (a,d,f))
    elif tokens[-2] == 'commit':
      repo = stx.get_repo(tokens[-3])
      co = repo.get_commit(tokens[-1])
      a = co.stats.additions
      d = co.stats.deletions
      points = get_score(tokens[-2],a+d)
      msg = ( "%d additions, %d deletions" % (a,d))
    elif tokens[-2] == 'issues':
      points = get_score(tokens[-2],0)  # this should be an issue
    else:
      if 'starred' in url:
        points = POINTS_STAR
      elif 'fork' in url:
        points = POINTS_FORK
      else:
        raise Exception("unknown type %s in %s" % (tokens[-2],url))
  else: # this is a comment
    points = POINTS_COMMENTS
  return (points,msg) 

def main():
  parser = argparse.ArgumentParser(description='Retrieve all activity done by a particular user.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('login', metavar='LOGIN', type=str, help="Comma-separate lists of logins [can use External,Hackathon,EU R&D,Innersource,All,Unknown as wildcards]")
  parser.add_argument('-s', '--since', type=str, help="Only show activity since yyyy-mm-dd")
  parser.add_argument('-u', '--until', type=str, help="Only show activity until yyyy-mm-dd")
  parser.add_argument('-l', '--last_week', action='store_true', help="Only show activity in the last seven days")
  parser.add_argument('-d', '--details', action='store_true', help="Print stats for pulls and commits, also reports a total score")
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

  if args.details:
    gh = Github(os.environ.get('GH_OATH'))
    stx = gh.get_organization('Seagate')

  activities = {}
  logins = get_logins(args.login)
  for login in logins:
    activities[login] = {}
    try:
      # create a new structure to hold the data in an organization that is more easily sorted
      # go through the activity and save each into the new format
      # problem is that the watch event doesn't have a date for it . . . 
      for (url,created_at) in activity.get_activities(login):
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
    total_score   = 0
    if len(actions) > 0:
      print("%d actions for %s %s" % (len(actions),login, daterange))
      total_actions += len(actions)
    for d,u in sorted(actions.items()):
      if args.details:
        (points,details) = get_details(u,stx)
        total_score += points
      print("\t%s %s %s %s" % (login,d,u, details if args.details else ''))
    if len(actions) > 0 and args.details:
      print("\t%4.1f POINTS for %s" % (total_score,login))

      
  print("SUMMARY: %d total observed actions from %s %s" % (total_actions, args.login, daterange))

if __name__ == "__main__":
    main()

