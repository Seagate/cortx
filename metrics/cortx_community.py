#! /usr/bin/env python3

import argparse
import datetime
import dateutil.parser as dateparser
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

AUTHOR_ACTIVITY_PICKLE='%s/author_activity.pickle' % PICKLE_DIR       # actions done by logins
ACTIVITY_HASH_PICKLE='%s/activity_hashes.pickle' % PICKLE_DIR         # actions hashed by uniquifier
COMMUNITY_PICKLE='%s/cortx_community.pickle' % PICKLE_DIR             # the people in the community
SLACK_COMMUNITY_PICKLE='%s/cortx_slack_community.pickle' % PICKLE_DIR # the people in the community
STATS_PICKLE='%s/persistent_stats.pickle' % PICKLE_DIR                # the per-repo and global stats
COMPARE_PROJECTS_PICKLE='%s/compare_projects.pickle' % PICKLE_DIR     # the historical star and fork counts for all known open source projects

# a map of projects mapping to (org, repo_prefix)
projects={'Ceph'  : ('Ceph',None),
          'MinIO' : ('MinIO',None),
          'DAOS'  : ('daos-stack',None), 
          'CORTX' : ('Seagate','cortx'),
          'Swift' : ('openstack','swift'),
          'OpenIO': ('open-io','oio'),
          'ECS'   : ('EMCECS', 'ecs' )}

# map of orgs mapping to the companies which run those orgs
# used to identify which people are external because they don't belong to any of the known companies
org_company_map = {
  'daos-stack' : ('intel'),
  'Ceph' : ('redhat','suse'),
  'MinIO' : ('minio'),
  'Seagate' : ('seagate','dsr','calsoft', 'codacy-badger'),
  'openstack' : ('swiftstack', 'nvidia'),
  'EMCECS' : ( 'dell', 'emc', 'vmware' ),
  'open-io' : ('openio'),
}

# what is currently ugly is that we assume that the org strings are the same in the projects and org_company_map maps
# let's at least just confirm that here
for org in [v[0] for v in projects.values()]:
  if org not in org_company_map.keys():
    raise KeyError('org %s is unknown org' % org)
if len(org_company_map) != len(projects):
  raise KeyError('the two org maps are not consistent')

# a simple helper to get the github connection
def get_gh():
  gh = Github(os.environ.get('GH_OATH'))
  return gh

# a simple helper function to check the github rate
def rate_check(gh=None):
  if not gh:
    gh = Github(os.environ.get('GH_OATH'))
  print("Remaining is %d, reset in %.2f" % 
    (gh.get_rate_limit().core.remaining,
    (gh.get_rate_limit().core.reset - datetime.datetime.utcnow()).total_seconds()/60))  


class ProjectComparisons:
  def __init__(self,org_name=None,stats=None):
    self.pname = get_pickle_name(COMPARE_PROJECTS_PICKLE,org_name)
    if stats:
      self.stats = stats
    else:
      try:
        with open(self.pname,'rb') as f:
          self.stats = pickle.load(f)
      except FileNotFoundError:
          self.stats = {}

  def set_stats(self,stats):
    self.stats=stats
    self.persist()

  def get_stats(self):
    return self.stats

  def persist(self):
    with open(self.pname, 'wb') as f:
      pickle.dump(self.stats, f)

# a simple class that is a dict of {repo : {date : stats }
# TODO: right now, scrape_metrics.py tries to create a GLOBAL view and it works OK 
# In fact, print_metrics.py just uses it as do the Jupyter notebooks.
# I think what this class might want to do instead is create the GLOBAL view on demand whenever anyone asks for a repo matching 'GLOBAL'
class PersistentStats:
  def __init__(self,org_name=None):
    self.pname = get_pickle_name(STATS_PICKLE,org_name)
    try:
      with open(self.pname,'rb') as f:
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

  def get_keys(self,repo,date):
    if not date:
      date=self.get_latest(repo)[1]
    return self.stats[repo][date].keys()

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
    with open(self.pname,'wb') as f:
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
        try:
          short_value = int(v)
        except TypeError:   # could be a None snuck in due to manual manipulation
          continue
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

# a helper function to get the name of a pickle file now that we are trying to use these scripts to scrape data for other repos
# if org is None or is 'Seagate', then just use the original pickle name to be consistent with our historical naming
# but if org is something else, then insert org into the pickle name
def get_pickle_name(pickle,org):
  pname = pickle
  if org and 'Seagate' not in org: 
    pname = pname.replace('.pickle','.%s.pickle' % org) 
  return pname


# a simple class that is a dict of {login , set(url)} that can be queried to see what someone has been doing in the activity
class CortxActivity:
  def __init__(self,org_name=None):
    self.activity_file = get_pickle_name(AUTHOR_ACTIVITY_PICKLE,org_name)
    self.hash_file = get_pickle_name(ACTIVITY_HASH_PICKLE,org_name)
    try:
      with open(self.activity_file,'rb') as f:
        self.activity = pickle.load(f)
    except FileNotFoundError:
      self.activity = {}
    try:
      with open(self.hash_file, 'rb') as f:
        self.hashes = pickle.load(f)
    except FileNotFoundError:
      self.hashes = {}
    self.new_activities = set()

  # a cache of activities to try to avoid overusing github API
  # I think this function is not used since there is a second
  # function with the same name which is currently used....
  # TOD: change the name of this function to something garbage and make sure everything still works, then delete it
  # actually, we need this one.  It's super important in scrape_metric to avoid using github API too much.
  # but now we broke it by overriding it.  So now we need to restore it by changing the name of the other one.
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
    with open(self.activity_file,'wb') as f:
      pickle.dump(self.activity,f)
    with open(self.hash_file,'wb') as f:
      pickle.dump(self.hashes,f)

  def get_activities(self,login):
    return self.activity[login]

class CortxPerson:
  def __init__(self,login,company,email,linkedin,org_name):
    self.login = login
    self.company = company
    self.email = email
    self.type = None
    self.note = None
    self.linked = None
    if self.login.endswith('-bot'):
      self.type = 'Bot'
    elif company:
      companies = org_company_map[org_name]
      external = True
      for c in companies:
        if c.lower() in company.lower():
          external = False
      if external:
        self.type = 'External'

  def get_note(self):
    return self.note

  # starting now, note will be a dict.  Hope this works without breaking pickles
  def add_note(self,note):
    assert self.note is None or isinstance(self.note,dict)
    assert isinstance(note,dict)
    try:
      # this won't work.  We'll have to rewrite this to be a merger of dicts
      self.note.update(note) 
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

  def get_linkedin(self):
    return self.linked

  def get_email(self):
    return self.email

  def get_type(self):
    return self.type

  def get_login(self):
    return self.login

  def __str__(self):
    return("%s at company %s email %s type %s linkedin %s %s" % (self.login, self.company, self.email, self.type, self.linked, " Notes: %s" % self.note if self.note else ""))

class SlackCommunity():
  def __init__(self,org_name=None):
    self.pickle_file = get_pickle_name(SLACK_COMMUNITY_PICKLE,org_name)
    try:
      f = open(self.pickle_file, 'rb')
      self.people = pickle.load(f)
      f.close()
    except FileNotFoundError:
      self.people = {} 

  def persist(self):
    with open(self.pickle_file, 'wb') as f:
      pickle.dump(self.people, f)

  def find_email(self,email):
    for sid,person in self.people.items():
      if person['email'] == email:
        return sid
    return None

  def find_person(self,slack_id):
    try:
      return self.people[slack_id]
    except KeyError:
      return None

  def set_github(self,slack_id,github):
    person = self.find_person(slack_id)
    person['github']=github

  def print_person(self,slack_id):
    person = self.people[slack_id]
    print("Person %s github:%s email:%s" % (person['name'], person['github'], person['email']))

  def get_github(self,slack_id):
    person = self.find_person(slack_id)
    return person['github']

  def get_email(self,slack_id):
    person = self.find_person(slack_id)
    return person['email']

  def find_login(self,login):
    for sid,person in self.people.items():
      if person['github'] == login:
        return sid
    print("No person in slack pickle with name of %s" % login)
    return None

  def add_person(self,slack_id,github,email,name):
    self.people[slack_id] = { 'github' : github, 'email' : email, 'name' : name }

  def __str__(self):
    header = "CORTX Slack Community Members: %d total" % len(self.people)
    string = header + '\n'
    strings = []
    for sid,person in self.people.items():
      strings.append("Person %s [%s %s %s]" % (person['name'], person['github'], person['email'],sid))
    string += ('\n'.join(sorted(strings)) + '\n' + header)
    return string

class CortxCommunity:
  allowed_types  = set(['External','Innersource','Hackathon','EU R&D','Bot', 'CORTX Team', 'Mannequin'])
  external_types = set(['External','Innersource','Hackathon','EU R&D'])

  def __init__(self,org_name=None):
    self.pickle_file = get_pickle_name(COMMUNITY_PICKLE,org_name)
    try:
      f = open(self.pickle_file, 'rb')
      self.people = pickle.load(f)
      f.close()
    except FileNotFoundError:
      self.people = {} 

  def get_external_activity(self,since=None,until=None):
    if since:
      since = dateparser.parse(since)
    if until:
      until = dateparser.parse(until)
    activities={}
    ca=CortxActivity()
    for login,person in self.people.items():
      if self.external_type(person.get_type()):
        activities[login]={}
        try:
          for action,date in sorted(ca.get_activities(login)):
            try:
              if (since and date < since) or (until and date > until):
                continue
              activities[login][date]=action
            except TypeError:
              pass # some actions have no date (i.e. watches)
        except KeyError:
          pass # no activities ever recorded for this person
    return activities


  def get_person(self,login):
    return self.people[login]

  def get_external_types(self):
    return self.external_types

  def external_type(self,Type):
    return Type in self.external_types

  def is_external(self,login):
    Type = self.get_type(login)
    return self.external_type(Type)

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

  def get_linkedin(self,login):
    return self.people[login].get_linkedin()

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

  def add_person(self, login, company, email,linkedin=None,org_name=None):
    person = CortxPerson(login,company,email,linkedin,org_name)
    self.people[login] = person

  def remove_person(self,login):
    del self.people[login]

  def find_person(self, email):
    for login,person in self.people.items():
      if person.get_email() == email:
        return person
    return None

  def get_note(self,login):
    return self.people[login].get_note()

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


def avoid_rate_limiting(gh):
  THRESHOLD=50
  #(remaining,total) = gh.rate_limiting # weird, something changed and this no longer refreshes...
  rl=gh.get_rate_limit()
  remaining=rl.core.remaining
  if remaining < THRESHOLD:
    print("Approaching rate limit; only %d remaining" % remaining) 
    reset = gh.rate_limiting_resettime
    sleep = reset - time.time()
    if(sleep > 0):
      sleep = int(sleep) + 5 # sleep a bit long to be extra safe
      print("Need to sleep %d seconds until %d" % (int(sleep),reset))
      time.sleep(sleep)

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

# one thing to consider in the future is maybe this should check repo.parent
# some repo's are forked in an org and maybe we shouldn't scrape them
# that might be a way to do that
def get_repos(gh=None,org_name='Seagate',prefix='cortx'):
  if not gh:
    gh = Github(os.environ.get('GH_OATH'))
  org = gh.get_organization(org_name)
  orepos = org.get_repos()
  repos = [] 
  for repo in orepos:
    if (prefix and prefix not in repo.name) or repo.name.endswith('.old') or repo.name.endswith('-old') or repo.private:
      continue
    else:
      repos.append(repo)
  return repos


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
