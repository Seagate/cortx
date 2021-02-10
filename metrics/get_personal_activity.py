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

def get_company(company,people):
  def get_lower(tag):
    try:
      return tag.lower()
    except AttributeError:
      return ''
  group = set()
  for login,person in people.items():
    if company.lower() in get_lower(person.get_email()) or company.lower() in get_lower(person.get_company()):
      group.add(login)
  return group

def get_group(Type,people):
  group = set()
  for login,person in people.items():
    if person.get_type() == Type or Type == 'All':
      group.add(login)
  return group

def get_githubteam():
  url='https://api.github.com/orgs/seagate/teams/cortx-community'
  logins = cortx_community.get_logins('members',url)
  return logins

def get_logins(CSV,people,company):
  logins = set()
  for login in CSV.split(','):
    if login == 'External' or login == 'EU R&D' or login == 'Innersource' or login == 'All' or login == 'Unknown' or login == 'Hackathon':
      if login == 'Unknown':
        login = None  # set to None so it will match correctly in get_group
      logins |= get_group(Type=login,people=people)
    elif company:
      logins |= get_company(login,people)
    elif login == 'githubteam':
      logins |= get_githubteam()
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
  msg = ''
  if 'slack' or '#' in url:
    points = POINTS_COMMENTS
  elif 'starred' in url:
    points = POINTS_STAR
  elif 'fork' in url:
    points = POINTS_FORK
  else:
    tokens = url.split('/')
    action = tokens[-2]
    if action == 'pull':
      repo = stx.get_repo(tokens[-3])
      pr = repo.get_pull(int(tokens[-1]))
      a = pr.additions
      d = pr.deletions
      f = pr.changed_files
      points = get_score(action,a+d)
      msg = ( "%d additions, %d deletions, %d files changed" % (a,d,f))
    elif action == 'commit':
      repo = stx.get_repo(tokens[-3])
      co = repo.get_commit(tokens[-1])
      a = co.stats.additions
      d = co.stats.deletions
      points = get_score(action,a+d)
      msg = ( "%d additions, %d deletions" % (a,d))
    elif action == 'issues':
      points = get_score(action,0)  # this should be an issue
    else:
      raise Exception("unknown contribution; can't score: %s" % (url))
  return (points,msg) 

def get_activities(logins,company,people):
  activity = cortx_community.CortxActivity()
  activities = {}
  logins = get_logins(logins,people,company)
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
  return (activities,logins)

def filter_activities(activities,since,until,limit):
  # using the new data structure, filter by since and until 
  filtered_activities = {}
  if since or until:
    for login,actions in sorted(activities.items()):
      filtered_activities[login]={}
      for d,u in sorted(actions.items()):
        if since and d < since:
          continue
        if until and d > until:
          continue
        filtered_activities[login][d]=u
  else:
    filtered_activities = activities

  # optionally filter by limit
  if limit:
    new_filtered = {}
    for login,actions in sorted(filtered_activities.items()):
      if len(actions) >= limit:
        new_filtered[login]=actions
    filtered_activities = new_filtered
  return filtered_activities

def get_info(login,people):
  try:
    email=people.get_email(login)
  except KeyError:
    email = None
  try:
    Type=people.get_type(login)
  except KeyError:
    Type = None
  try:
    Company=people.get_company(login)
  except KeyError:
    Company = None
  return(email,Type,Company)

def print_activities(filtered_activities,logins,details,zero,people,since,until):
  if details:
    gh = Github(os.environ.get('GH_OATH'))
    stx = gh.get_organization('Seagate')

  daterange = "since %s" % (since.strftime('%Y-%m-%d') if since else "inception")
  if until:
    daterange += " until %s" % until.strftime('%Y-%m-%d')

  if len(logins) > 1:
    print("Getting activities from %d logins: %s" % (len(logins),sorted(logins)))

  # now print from the filtered list as sorted by the quantitied of actions per login
  total_actions = 0
  for k in sorted(filtered_activities, key=lambda k: len(filtered_activities[k]), reverse=True):
    login = k
    actions = filtered_activities[k]
    (email,Type,Company)=get_info(login,people)
    total_score   = 0
    if len(actions) > 0 or args.zero:
      print("%d actions for %s [email %s, company %s, Type %s] %s" % (len(actions),login, email, Company, Type, daterange))
      total_actions += len(actions)
    for d,u in sorted(actions.items()):
      if details:
        (points,Details) = get_details(u,stx)
        total_score += points
      print("\t-- %s %s %s %s" % (login,d,u, Details if details else ''))
    if len(actions) > 0 and details:
      print("\t%4.1f POINTS for %s" % (total_score,login))
      
  print("SUMMARY: %d total observed actions from %s %s" % (total_actions, logins, daterange))


def main():
  parser = argparse.ArgumentParser(description='Retrieve all activity done by a particular user.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('login', metavar='LOGIN', type=str, help="Comma-separate lists of logins [can use External,Hackathon,EU R&D,Innersource,All,Unknown as wildcards]")
  parser.add_argument('-s', '--since', type=str, help="Only show activity since yyyy-mm-dd")
  parser.add_argument('-u', '--until', type=str, help="Only show activity until yyyy-mm-dd")
  parser.add_argument('-w', '--last_week', action='store_true', help="Only show activity in the last seven days")
  parser.add_argument('-m', '--last_month', action='store_true', help="Only show activity in the last 30 days")
  parser.add_argument('-d', '--details', action='store_true', help="Print stats for pulls and commits, also reports a total score")
  parser.add_argument('-c', '--company', action='store_true', help="Instead of looking up an individual, look up all folks from a particular company")
  parser.add_argument('-l', '--limit', type=int, help="Only show actions if gte to limit")
  parser.add_argument('-z', '--zero', action='store_true', help="Show folks even if they have no actions")
  args = parser.parse_args()

  people=cortx_community.CortxCommunity()
  (activities,logins) = get_activities(logins=args.login,company=args.company,people=people)

  if args.since:
    args.since = dateparser.parse(args.since)
  if args.until:
    args.until = dateparser.parse(args.until)
  if args.last_week:
    args.since = datetime.datetime.today() - datetime.timedelta(days=7)
  if args.last_month:
    args.since = datetime.datetime.today() - datetime.timedelta(days=30)

  filtered_activities=filter_activities(activities=activities,since=args.since,until=args.until,limit=args.limit)

  print_activities(filtered_activities=filtered_activities,logins=logins,details=args.details,zero=args.zero,people=people,since=args.since,until=args.until)

if __name__ == "__main__":
    main()

