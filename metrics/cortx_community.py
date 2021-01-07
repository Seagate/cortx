#! /usr/bin/env python3

import argparse
import json
import os
import pickle
import requests
import sys
import time
import xlrd
from github import Github

# put GH_OATH in your .bashrc
# the following are a bunch of useful curl commands to run from the command line
# > curl -i -u "johnbent:$GH_OATH" -X GET -d '' "https://api.github.com/repos/seagate/cortx/collaborators"
# will print the list of collaborators to the cortx repo as JSON
# > curl -i -u "johnbent:$GH_OATH" -X GET -d '' "https://api.github.com/orgs/seagate" 
# does something
# > curl -s -u "johnbent:$GH_OATH" -X GET "https://api.github.com/orgs/Seagate/teams/seagate-cortx-innersource/members?per_page=100"
# > curl -s -u "johnbent:$GH_OATH" -X GET "https://api.github.com/orgs/Seagate/teams/cortx-innersource/members?per_page=100"
# lists all team members !!!!!
# > curl -u "johnbent:$GH_OATH" -X GET "https://api.github.com/users/amolkongre"
# will show the email for the user; we might use this in the future to make sure only @seagate people have access
# > curl -s -u "johnbent:$GH_OATH" -X GET https://api.github.com/orgs/Seagate/teams/seagate-cortx-innersource/invitations?per_page=100
# shows the pending invitations!
# > curl -u "johnbent:$GH_OATH" -X PUT "https://api.github.com/orgs/seagate/teams/seagate-cortx-team/memberships/shalakadharap"
# will add someone to a team
# > curl -i -s -u "johnbent:$GH_OATH" -X GET -d '' "https://api.github.com/repos/seagate/cortx/releases" | grep download_count | awk '{print $2}' | sed 's/,//' |  paste -sd+ | bc
# will sum all releases
# > curl -i -s -u "johnbent:$GH_OATH" -X GET "https://api.github.com/search/issues?q=repo:Seagate/cortx+label:hacktoberfest+is:open"


PICKLE_DIR='pickles'

# this pickle saves actions done by logins
AUTHOR_ACTIVITY_PICKLE='%s/author_activity.pickle' % PICKLE_DIR

# this pickle saves actions hashed by uniquifier
ACTIVITY_HASH_PICKLE='%s/activity_hashes.pickle' % PICKLE_DIR

# this pickle saves the people in the community
COMMUNITY_PICKLE='%s/cortx_community.pickle' % PICKLE_DIR

# this pickle saves the per-repo and global stats
STATS_PICKLE='%s/persistent_stats.pickle' % PICKLE_DIR

# a simple class that is a dict of {repo : {date : stats }
# TODO: right now, scrape_metrics.py tries to create a GLOBAL view but it might not work so well
# also, print_metrics.py creates a global view which might work better but still isn't optimal
# I think what this class needs to do is create the GLOBAL view on demand whenever anyone asks for a repo matching 'GLOBAL'
class PersistentStats:
  def __init__(self):
    try:
      with open(STATS_PICKLE,'rb') as f:
        self.stats = pickle.load(f)
    except FileNotFoundError:
        self.stats = {}

  def get_repos(self):
    return sorted(self.stats.keys())

  def get_latest(self,repo):
    dates = self.get_dates(repo)
    latest = sorted(dates)[-1]
    return (self.stats[repo][latest],latest)

  def get_dates(self,repo):
    dates = self.stats[repo].keys()
    return sorted(dates)

  # some values are numbers, some are dicts, some are sets
  # this function will return them all as numbers (either the number itself or the size of the collection)
  def get_values_as_numbers(self,repo,key,dates=None):
    Values = self.get_values(repo,key,dates)
    Numbers = []
    for v in Values:
      try:
        Numbers.append(len(v))
      except TypeError:
        Numbers.append(v)
    return Numbers

  def get_values(self,repo,key,dates=None):
    Values=[]
    if not dates:
      dates = self.get_dates(repo)
    for date in dates:
      try:
        Value = self.stats[repo][date][key]
        Values.append(Value)
      except KeyError:
        # if we add new metrics, they won't be at the older dates
        Values.append(None)
    return Values

  def add_stats(self, date, repo, stats):
    if repo not in self.stats:
      self.stats[repo] = {}
    self.stats[repo][date] = stats
    self.persist()

  def add_stat(self, date, repo, stat, value):
    self.stats[repo][date][stat]=value
    self.persist()

  def persist(self):
    with open(STATS_PICKLE,'wb') as f:
      pickle.dump(self.stats,f)
    
  # note that this function doesn't use self
  # there might be some python way to mark this function accordingly 
  # but I don't know how to do that
  def print_repo(self,rname,stats,date,verbose,csv):
    print("Statistics in %s as of %s" % (rname,date))
    for k,v in sorted(stats.items()):
      try:
        short_value = len(v)   # the v is either int or set/list
      except TypeError:
        short_value = int(v)
      verbosity = ''
      if verbose:
        try:
          verbosity = list(v)
        except:
          pass  # nothing verbose to do for int fields
      if csv:
        print("%s,%s-%s,%s" % (date,rname,k,short_value))
      else:
        print("%s %s -> %d" % (rname,k,short_value),verbosity)


# a simple class that is a dict of {login , set(url)} that can be queried to see what someone has been doing in the activity
class CortxActivity:
  def __init__(self):
    try:
      with open(AUTHOR_ACTIVITY_PICKLE,'rb') as f:
        self.activity = pickle.load(f)
    except FileNotFoundError:
      self.activity = {}
    try:
      with open(ACTIVITY_HASH_PICKLE, 'rb') as f:
        self.hashes = pickle.load(f)
    except FileNotFoundError:
      self.hashes = {}
    self.new_activities = set()

  # a cache of activities to try to avoid overusing github API
  def get_activity(self,uniquifier):
    return self.hashes[uniquifier]

  def add_activity(self,key,login,url,created_at):
    if login not in self.activity:
      self.activity[login] = set()
    self.activity[login].add((url,created_at))
    self.hashes[key]=(login,url,created_at)
    self.new_activities.add((login,url,created_at))

  # this class maintains a list of new activities detected see add_activity above
  # the user of this class will insert a ton of activity
  # presumably most of it has been previously seen
  # but new stuff should be remembered and potentially used to send alerts, etc
  def get_new_activities(self):
    return self.new_activities

  def persist(self):
    with open(AUTHOR_ACTIVITY_PICKLE,'wb') as f:
      pickle.dump(self.activity,f)
    with open(ACTIVITY_HASH_PICKLE,'wb') as f:
      pickle.dump(self.hashes,f)

  def get_activity(self,login):
    return self.activity[login]

class CortxPerson:
  def __init__(self,login,company,email,linkedin):
    self.login = login
    self.company = company
    self.email = email
    self.type = None
    self.note = None
    self.linked = None
    if self.login.endswith('-bot'):
      self.type = 'Bot'
    if company and not ('seagate' in company.lower() or 'dsr' in company.lower() or 'calsoft' in company.lower() or 'codacy-badger' in login):
      self.type = 'External'

  def add_note(self,note):
    try:
      self.note += '\n%s' % note
    except:
      self.note = note

  def update_email(self, email):
    self.email = email

  def update_type(self, Type):
    self.type = Type
  
  def update_company(self, company):
    self.company = company

  def update_linkedin(self, linkedin):
    self.linked = linkedin

  def get_company(self):
    return self.company

  def get_email(self):
    return self.email

  def get_type(self):
    return self.type

  def get_login(self):
    return self.login

  def __str__(self):
    return("%s at company %s email %s type %s linkedin %s %s" % (self.login, self.company, self.email, self.type, self.linked, "\nNotes: %s" if self.note else ""))


class CortxCommunity:
  pickle_file = COMMUNITY_PICKLE 
  allowed_types = set(['Bot', 'CORTX Team', 'Innersource', 'Hackathon', 'External','Mannequin','EU R&D'])

  def __init__(self):
    try:
      f = open(self.pickle_file, 'rb')
      self.people = pickle.load(f)
      f.close()
    except FileNotFoundError:
      self.people = {} 

  def is_external(self,login):
    Type = self.get_type(login)
    return Type in set(['External','Innersource','Hackathon','EU R&D'])

  def get_types(self):
    return self.allowed_types

  def __str__(self):
    string = "CORTX Community Members: %d total" % len(self.people)
    for person in sorted(self.people.keys()):
      string += "\nPerson %s" % self.people[person].__str__()
    return string

  def __iter__(self):
    return self.people.__iter__()

  def __next__(self):
    return self.people.__next__()

  def items(self):
    return self.people.items()

  def get_email(self,login):
    return self.people[login].get_email()

  def get_company(self,login):
    return self.people[login].get_company()

  def get_login(self,login):
    return self.people[login].get_login()

  def get_type(self,login):
    return self.people[login].get_type()

  def values(self):
    return self.people.values()

  def items(self):
    return self.people.items()

  def persist(self):
    with open(self.pickle_file, 'wb') as f:
      pickle.dump(self.people, f)

  def includes(self, login):
    return login in self.people

  def add_person(self, login, company, email,linkedin=None):
    person = CortxPerson(login,company,email,linkedin)
    self.people[login] = person

  def add_note(self, login, note):
    self.people[login].add_note(note)

  def update_type(self, login, Type):
    if Type not in self.allowed_types:
      raise TypeError("%s is not in the allowed types: " % Type, self.allowed_types)
    self.people[login].update_type(Type)

  def update_email(self, login, email):
    self.people[login].update_email(email)

  def update_company(self, login, company):
    self.people[login].update_company(company)

  def update_linkedin(self, login, linkedin):
    self.people[login].update_linkedin(linkedin)


def get_auth():
  uid  = os.environ.get('USER')
  oath = os.environ.get('GH_OATH')
  return(uid,oath)

def check_rate_limit():
  u="https://api.github.com/rate_limit"
  (uid,oath) = get_auth()
  r = requests.get(u, auth=(uid,oath))
  js = r.json()
  return js

def ensure_rate_limit(r):
  if int(r.headers['X-RateLimit-Remaining']) <= 2:  # try early just to give some buffer
    print(r.headers)
    reset = int(r.headers['X-RateLimit-Reset'])
    sleep = reset - time.time()
    if(sleep > 0):
      sleep = int(sleep) + 5 # sleep for some extra time to make sure there is no problem
      print("Need to sleep %d seconds until %d" % (int(sleep),reset))
      time.sleep(sleep)

def pages_to_json(url):
  items=[]
  (uid,oath) = get_auth()

  # the incoming url might already have some arguments in it.
  # if it does, don't repeat the '?' character
  first_char="?"
  if "?" in url:
    first_char="&"

  full_json = None

  page=1
  while True:
    u="%s%sper_page=100&page=%d"%(url,first_char,page)
    print(u)
    page+=1
    r = requests.get(u, auth=(uid,oath))
    ensure_rate_limit(r)
    js = r.json()
    try:
      full_json = full_json + js
    except TypeError:
      full_json = js
    items.extend(js)
    if len(js)<100:
      break;
  return (items,full_json)

def get_teams(url):
  items = pages_to_json(url)[0]
  teams = set([])
  for item in items:
    team = item['name']
    teams.add(team)
  return sorted(teams)

def get_repos():
  gh = Github(os.environ.get('GH_OATH'))
  stx = gh.get_organization('Seagate')
  srepos = stx.get_repos()
  repos = set([])
  for repo in srepos:
    if "cortx" in repo.name and 'old' not in repo.name and 'backup' not in repo.name:
      repos.add(repo.name)
  return sorted(repos)


def search_repo(repo,user,Type,daterange):
  url = "https://api.github.com/search/issues?q=repo:%s+%s:%s+created:%s" % (repo,Type,user,daterange)
  #(uid,oath) = get_auth()
  #r = requests.get(url, auth=(uid,oath))
  #js = r.json()
  (items,json) = pages_to_json(url)
  return json

def get_logins(Type,team_url):
  (uid,oath)=get_auth()
  page=1
  gids = set([])
  while True:
    url="%s/%s?per_page=100&page=%d" % (team_url, Type,page)
    r = requests.get(url, auth=(uid, oath))
    js = r.json()
    try:
      for member in js:
        gid = member['login'] 
        gids.add(gid)
    except TypeError:
      print( "ERROR", team_url, js )
      sys.exit(0)
    if len(js) < 100:
      break
    else:
      page += 1
  #print("%d %s %s: " % (len(gids), team_url, Type), sorted(gids))
  return gids

def get_registrants(excel,gcolumn,ecolumn,sheet=0,firstrow=1):
  wb = xlrd.open_workbook(excel) 
  sheet = wb.sheet_by_index(sheet) 
  sheet.cell_value(0, ecolumn) 
    
  gids = {}
  successes = set([])
  for i in range(firstrow,sheet.nrows): 
    gid = sheet.cell_value(i,gcolumn)
    email = sheet.cell_value(i,ecolumn)
    gids[gid] = email
  return gids

# the function which reads the spreadsheet and just returns the gids which are not yet registered
def get_new_registered_gids(excel,team_url,team):

  already_registered = get_logins('members',team_url) 
  pending = get_logins('invitations',team_url) 

  if team == 'innersource':
    ecolumn=3
    gcolumn=6
    firstrow=201
  else:
    print("Team is %s" % team)
    gcolumn=1
    ecolumn=0
    firstrow=0

  registrants = get_registrants(excel,gcolumn,ecolumn,0,firstrow)
    
  gids = {}
  successes = set([])
  for (gid,email) in sorted(registrants.items()): 
    if  gid:
      if gid in already_registered:
        #print( "%25s [%35s] is already registered" % (gid,email) )
        successes.add(email)
      elif gid in pending:
        print( "%25s [%35s] is pending" % (gid,email) )
        successes.add(email)
      else:
        gids[gid] = email
    else:
      print("Unexpected error: gid %s, email %s" % (gid, email) )
  return (gids,successes)

# gids is a dict with login:email, successes is a set of emails that are either registered or pending
def add_gids(gids,successes,team_url):
  (uid,oath)=get_auth()
  for gid,email in sorted(gids.items()):
    if email not in successes:
      # try with both the 'members' and 'memberships' URLs....
      r = requests.put("%s/memberships/%s" % (team_url,gid), auth=(uid,oath))
      print("%25s [%35s] added to memberships %s: %s" % (gid, email, team_url, r.status_code == requests.codes.ok))
      # this one always fails so just do the memberships one; not sure what the difference is.
      #r = requests.put("%s/members/%s" % (team_url,gid), auth=(uid,oath))
      #print("%25s [%35s] added to members     %s" % (gid, email, r.status_code == requests.codes.ok))
    else:
      pass
      #print("%25s [%35s] not attempting to reinvite" % (gid, email))
