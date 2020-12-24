#! /usr/bin/env python3

import argparse
import copy
import json
import pickle
import cortx_community
import os
import re
import time
from github import Github  # https://pygithub.readthedocs.io/
from datetime import datetime

def remove_string_from_set(Set, String):
  newset = set()
  for item in Set:
    if String not in item.lower():
      newset.add(item)
  return newset

def avoid_rate_limiting(gh):
  (remaining,total) = gh.rate_limiting
  if remaining < 10:
    reset = gh.rate_limiting_resettime
    sleep = reset - time.time()
    if(sleep > 0):
      sleep = int(sleep) + 5 # sleep a bit long to be extra safe
      print("Need to sleep %d seconds until %d" % (int(sleep),reset))
      time.sleep(sleep)

# this function takes a NamedUsed (https://pygithub.readthedocs.io/en/latest/github_objects/NamedUser.html) and returns info about them
# it seems this function uses the github API to query some of this stuff and that kills the rate limit (and probably performance also)
# so we use a local pickle'd dictionary
# returns (company, email, login, True/False) <- last value is whether we already knew about them
def author_info(people,author):
  l = author.login
  if people.includes(l):
    c = people.get_company(l)
    e = people.get_email(l)
    known = True
  else:
    c = author.company
    e = author.email
    print("New person discovered in community!  %s %s %s" % (l, e, c))
    people.add_person(login=l,company=c,email=e)
    people.persist()
    known = False
  return(c,e,l,known)

# pull the set of domains from the email addresses
# but don't count seagate ones
def domains_from_emails(emails):
  domains = set()
  for email in emails:
    domain = email.split('@')[1]
    if 'seagate' not in domain.lower():
      domains.add(domain)
  return domains

def persist_author_activity(author_activity):
  author_activity.persist()

def scrape_comment(people,gh,rname,comment,Type,local_stats,author_activity):
  key="%s_comment.%d" % (Type,comment.id)
  try:
    (login,url,created_at) = author_activity.get_activity(key)
  except KeyError:
    login = comment.user.login
    url = comment.html_url
    created_at = comment.created_at
    author_activity.add_activity(key,login,url,created_at)
  scrape_author(people,gh,rname,comment.user,login,local_stats,False,author_activity)
  local_stats['comments'] += 1
  if people.is_external(login):
    local_stats['external_comments'] += 1

def new_average(old_average,old_count,new_value):
  return (old_average * old_count + new_value) / (old_count+1)
    
def scrape_issue_or_pr(people,gh,rname,item,local_stats,author_activity,stats_name,commit):
  key = "%s.%d" % (stats_name, item.id)
  try:
    (login,url,created_at) = author_activity.get_activity(key)
  except KeyError:
    login = item.user.login
    url = item.html_url
    created_at = item.created_at
    author_activity.add_activity(key,login,url,created_at)
  scrape_author(people,gh,rname,item.user,login,local_stats,commit,author_activity)

  external = people.is_external(login)
  for _ in range(2):  # ok, we have to repeat all this: once for all items, once again to measure separately for external or internal
    if item.state == 'open':
      age_in_seconds = (datetime.utcnow() - item.created_at).total_seconds()
      local_stats['%s_open_ave_age_in_s' % stats_name] = new_average(local_stats['%s_open_ave_age_in_s' % stats_name], local_stats['%s_open' % stats_name], age_in_seconds)
      local_stats['%s_open' % stats_name] += 1
    else:
      age_in_seconds = (item.closed_at - item.created_at).total_seconds()
      local_stats['%s_closed_ave_age_in_s' % stats_name] = new_average(local_stats['%s_closed_ave_age_in_s' % stats_name], local_stats['%s_closed' % stats_name], age_in_seconds)
      local_stats['%s_closed' % stats_name] += 1
      try:
        if item.merged:
          local_stats['%s_merged' % stats_name] += 1
      except AttributeError:
        pass
    local_stats[stats_name] += 1
    stats_name='%s_%s' % (stats_name,"external" if external else "internal") 

def scrape_commit(people,gh,rname,commit,local_stats,author_activity):
  key = "commit.%s" % commit.sha
  try:
    (login,url,created_at) = author_activity.get_activity(key)
  except KeyError:
    login = commit.author.login
    url = commit.html_url
    created_at = commit.commit.author.date
    author_activity.add_activity(key,login,url,created_at)
  scrape_author(people,gh,rname,commit.author,login,local_stats,True,author_activity)
  local_stats['commits'] += 1
    
# the commit parameter is True if the author did a commit, False otherwise (e.g. commented on an Issue for example)
def scrape_author(people,gh,repo,author,login,repo_stats,commit,author_activity):
  avoid_rate_limiting(gh)
  (company, email, login, previously_known) = author_info(people,author)

  Type = people.get_type(login)
  while(True):
    if Type is None:
      Type = 'unknown'
    Type = Type.replace(' ','_')
    key = "%s_%s" % (Type.lower(), 'committers' if commit else 'participants')
    repo_stats[key].add(login)
    if Type == 'Hackathon':
      Type = 'External'  # loop so that we count hackathon participants also as external
    else:
      break

  if not previously_known:
    repo_stats['new_logins'].add(login)
  
  # don't add None values to these sets
  if company:
    repo_stats['companies'].add(company)
  if email:
    repo_stats['email_addresses'].add(email)
  if login:
    repo_stats['logins'].add(login)

def get_external_activities(people,activities):
  newset = set()
  for activity in activities:
    (login,url,created_at) = activity
    if people.is_external(login):
      newset.add(activity)
  return newset

def clean_domains(domains):
  new_d = set()
  for item in domains:
    if 'ssc-vm' not in item and re.search('smc?\d+-[mr]\d+',item) is None and 'github' not in item and 'segate' not in item:
      new_d.add(item)
  return new_d

def get_top_level_repo_info(stats,repo,gh):
  stats['forks']    = repo.forks_count
  stats['stars']    = repo.stargazers_count
  stats['watchers'] = repo.subscribers_count
  stats['clones_unique_14_days'] = repo.get_clones_traffic()['uniques']
  stats['clones_count_14_days']  = repo.get_clones_traffic()['count']
  stats['views_unique_14_days']  = repo.get_views_traffic()['uniques']
  stats['views_count_14_days']   = repo.get_views_traffic()['count']

  # get referrers
  # this is just quick and dirty and seagate-specific top-referrers
  # should also figure out how to make a better general solution
  tr = repo.get_top_referrers()
  top_referrers = []
  for r in tr: 
    avoid_rate_limiting(gh)
    if r.referrer == 'seagate.com':
      stats['seagate_referrer_count']   = r.count
      stats['seagate_referrer_uniques'] = r.uniques
    elif r.referrer == 'blog.seagate.com':
      stats['seagate_blog_referrer_count']   = r.count
      stats['seagate_blog_referrer_uniques'] = r.uniques
    top_referrers.append(r)
  stats['top_referrers'] = top_referrers

  # get releases
  for r in repo.get_releases():
    for a in r.get_assets():
      avoid_rate_limiting(gh)
      stats['downloads_releases'] += a.download_count
      if 'VA' in a.browser_download_url:
        stats['downloads_vms'] += a.download_count

  # get top paths
  top_paths = []
  for tp in list(repo.get_top_paths())[0:3]:
    avoid_rate_limiting(gh)
    top_path = { 'count' : tp.count, 'uniques' : tp.uniques, 'path' : tp.path }
    top_paths.append(top_path)
  stats['top_paths'] = top_paths

def get_commits(rname,repo,local_stats,people,author_activity,gh):
  print("Scraping commits from %s" % rname)
  avoid_rate_limiting(gh)
  try:
    commits = repo.get_commits()
  except Exception as e:
    print("WTF: get_commits failed?", e)
    commits = () 
  for c in commits:
    avoid_rate_limiting(gh)
    if c.author:
      scrape_commit(people,gh,rname,c,local_stats,author_activity)
    if c.commit and c.commit.message:
      email_addresses = re.findall(r'[\w\.-]+@[\w\.-]+', c.commit.message) # search for email addresses in commit message; might be some due to DCO
      #print("Adding Commit info %s and %s" % (c.author.company, c.author.email))
      local_stats['email_addresses'] = local_stats['email_addresses'].union(email_addresses)
    comments = c.get_comments()
    for comment in comments:
      avoid_rate_limiting(gh)
      scrape_comment(people,gh,rname,comment,"commit",local_stats,author_activity)

def summarize_consolidate(local_stats,global_stats,people,author_activity):
  summary_stats = {}
  local_stats['domains']                  = clean_domains(domains_from_emails(local_stats['email_addresses']))
  local_stats['external_email_addresses'] = remove_string_from_set(local_stats['email_addresses'],'seagate')
  local_stats['new_external_activities']  = get_external_activities(people,author_activity.get_new_activities())
  for key in sorted(local_stats.keys()):
    try:                        # for values that are ints
      if 'unique' in key:
        global_stats[key] = max(global_stats[key],local_stats[key]) 
      else:
        global_stats[key] += local_stats[key]
    except TypeError:           # other values should be sets
      global_stats[key] = global_stats[key].union(local_stats[key])

# issues includes pr's.  Very confusing!  Thanks github!
def get_issues_and_prs(rname,repo,local_stats,people,author_activity,gh):
  print("Scraping issues and prs from %s" % rname)
  avoid_rate_limiting(gh)
  try:
    issues = repo.get_issues(state='all')
  except Exception as e:
    print("WTF: get_issues failed?", e)
    issues = () 
  for issue in issues:
    if issue.pull_request is None:
      Type = 'issues'
      commit = False
    else:
      issue = issue.as_pull_request()
      Type = 'pull_requests'
      commit = True
    scrape_issue_or_pr(people,gh,rname,issue,local_stats,author_activity,Type,commit)
    for comment in issue.get_comments():
      avoid_rate_limiting(gh)
      scrape_comment(people,gh,rname,comment,"issue",local_stats,author_activity)

def get_contributors(rname,repo,local_stats,people,gh):
  # collect info from contributors
  print("Scraping contributors from %s" % rname)
  contributors = repo.get_contributors()
  for c in contributors:
    scrape_author(people,gh,rname,c,c.login,local_stats,True,None)
    local_stats['contributors'].add(c.login)

# little helper function for putting empty k-v pairs into stats structure
def load_actors(D,actors):
  for actor in actors:
    D['%s_participants' % actor] = set()
    D['%s_committers' % actor] = set()

# little helper function for putting empty k-v pairs into stats structure
def load_items(D,first,second,third):
  for f in first:
    for s in second:
      for t in third:
        key = "%s%s%s" % (f,s,t)
        D[key] = 0

# if update is true, it loads an existing pickle instead of creating a new one
# this is useful when new fields are added 
def collect_stats(update):
  gh = Github(os.environ.get('GH_OATH'))
  avoid_rate_limiting(gh)
  stx = gh.get_organization('Seagate')
  today = datetime.today().strftime('%Y-%m-%d')

  # the shared structure that we use for collecting stats
  global_stats = { 'email_addresses'               : set(), 
                   'external_email_addresses'      : set(),
                   'companies'                     : set(), 
                   'domains'                       : set(), 
                   'logins'                        : set(), 
                   'new_logins'                    : set(),
                   'new_external_activities'       : set(),
                   'top_paths'                     : [], 
                   'top_referrers'                 : [],
                   'downloads_releases'            : 0,
                   'downloads_vms'                 : 0,
                   'commits'                       : 0, 
                   'comments'                      : 0,
                   'external_comments'             : 0,
                   'pull_requests_merged'          : 0,
                   'pull_requests_external_merged' : 0,
                   'pull_requests_internal_merged' : 0,
                   'clones_unique_14_days'         : 0,
                   'clones_count_14_days'          : 0,
                   'views_unique_14_days'          : 0,
                   'views_count_14_days'           : 0,
                   'forks'                         : 0,
                   'stars'                         : 0,
                   'seagate_referrer_count'        : 0,
                   'seagate_referrer_uniques'      : 0,
                   'seagate_blog_referrer_count'   : 0,
                   'seagate_blog_referrer_uniques' : 0,
                   'watchers'                      : 0,
                   'contributors'                  : set() }
  load_actors(global_stats,('mannequin','innersource','external','hackathon','bot','cortx_team','unknown'))
  load_items(global_stats,('issues','pull_requests'),('_external','_internal',''),('','_open','_closed','_open_ave_age_in_s','_closed_ave_age_in_s'))
  global_stats['pull_requests_external_merged'] = 0
  local_stats_template = copy.deepcopy(global_stats)    # save an empty copy of the stats struct to copy for each repo
  author_activity = cortx_community.CortxActivity()     # load up the author activity pickle 
  people = cortx_community.CortxCommunity()             # load up the people pickle
  persistent_stats = cortx_community.PersistentStats()  # load up all the stats

  for repo in stx.get_repos():
    rname = repo.name # put this in a variable just in case it is a github API to fetch this
    if 'cortx' not in rname or rname.endswith('.old') or rname.endswith('-old') or repo.private:
      continue

    local_stats = copy.deepcopy(local_stats_template) # get an empty copy of the stats structure

    # Use this update if you just want to add some new data and don't want to wait for the very slow time
    # to scrape all activity.  Once you have finished the update, migrate the code out of the update block.
    # Typically we don't use update; only during development 
    # Note that update doesn't work for values that are incremented . . . 
    if update:
      (cached_local_stats,timestamp) = persistent_stats.get_latest(rname)  # load the cached version
      print("Fetched %s data for %s" % (timestamp, repo))
      for k,v in cached_local_stats.items():
        local_stats[k] = v
    else:
      get_issues_and_prs(rname,repo,local_stats,people=people,author_activity=author_activity,gh=gh)
      get_commits(rname,repo,local_stats,people=people,author_activity=author_activity,gh=gh)
      get_top_level_repo_info(local_stats,repo,gh=gh)
      get_contributors(rname,repo,local_stats,people=people,gh=gh)

    # what we need to do is query when the last time this ran and then pass 'since' to get_commits

    # summarize info for this repo and persist the data structures
    summarize_consolidate(local_stats,global_stats,people=people,author_activity=author_activity)
    persist_author_activity(author_activity)
    persistent_stats.add_stats(date=today,repo=rname,stats=local_stats)
    persistent_stats.print_repo(rname,local_stats,date=today,verbose=False)

  # print and persist the global consolidated stats
  persistent_stats.print_repo('GLOBAL',global_stats,date=today,verbose=False)
  persistent_stats.add_stats(date=today,repo='GLOBAL',stats=global_stats)

def dump_stats(stats,Type):
  def print_list_as_csv(data,first_item):
    data.insert(0,first_item)
    res = ",".join([str(i) for i in data]) 
    print(res)

  ts = stats[TIMES]
  print_list_as_csv(stats[TIMES],'')
  for repo,data in sorted(stats[Type].items()):
    print_list_as_csv(data,repo)

def main():
  parser = argparse.ArgumentParser(description='Collect and print info about all cortx activity in public repos.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('-u', '--update', help='Load last stats and update it instead of creating a new one.', action='store_true')
  #parser.add_argument('--dump', '-d', help="Dump currents stats [either '%s', '%s', or '%s'" % (PNAME,INAME,TNAME), required=False)
  #parser.add_argument('--collect', '-c', help='Collect new stats', action='store_true')
  args = parser.parse_args()

  collect_stats(args.update)

if __name__ == "__main__":
    main()

