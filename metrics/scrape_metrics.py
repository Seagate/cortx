#! /usr/bin/env python3

import argparse
import copy
import json
import pickle
import cortx_community
import os
import re
import signal
import sys
import time
from requests.packages.urllib3.util import Retry
from github import Github  # https://pygithub.readthedocs.io/
import github
from datetime import datetime

def remove_string_from_set(Set, String):
  newset = set()
  for item in Set:
    if String not in item.lower():
      newset.add(item)
  return newset

def avoid_rate_limiting(gh):
  cortx_community.avoid_rate_limiting(gh)

# this function takes a NamedUsed (https://pygithub.readthedocs.io/en/latest/github_objects/NamedUser.html) and returns info about them
# it seems this function uses the github API to query some of this stuff and that kills the rate limit (and probably performance also)
# so we use a local pickle'd dictionary
# returns (company, email, login, True/False) <- last value is whether we already knew about them
def author_info(people,author,org_name):
  l = author.login
  if people.includes(l):
    c = people.get_company(l)
    e = people.get_email(l)
    known = True
  else:
    c = author.company
    e = author.email
    print("New person discovered in community!  %s %s %s" % (l, e, c))
    people.add_person(login=l,company=c,email=e,linkedin=None,org_name=org_name)
    people.persist()
    known = False
  return(c,e,l,known)

# pull the set of domains from the email addresses
# but don't count seagate ones
def domains_from_emails(emails):
  domains = set()
  for email in emails:
    try:
      domain = email.split('@')[1]
      if 'seagate' not in domain.lower():
        domains.add(domain)
    except IndexError:
      print("WTF: Couldn't split domain from email %s" % email)
  return domains

def persist_author_activity(author_activity):
  author_activity.persist()

def scrape_comment(people,gh,rname,comment,Type,local_stats,author_activity,org_name):
  key="%s_comment.%d" % (Type,comment.id)
  try:
    (login,url,created_at) = author_activity.get_activity(key)
  except KeyError:
    login = comment.user.login
    url = comment.html_url
    created_at = comment.created_at
    author_activity.add_activity(key,login,url,created_at)
  scrape_author(people,gh,rname,comment.user,local_stats,False,author_activity,org_name)
  local_stats['comments'] += 1
  if people.is_external(login):
    local_stats['external_comments'] += 1

def new_average(old_average,old_count,new_value):
  return (old_average * old_count + new_value) / (old_count+1)
    
def scrape_issue_or_pr(people,gh,rname,item,local_stats,author_activity,stats_name,commit,org_name):
  key = "%s.%d" % (stats_name, item.id)
  try:
    (login,url,created_at) = author_activity.get_activity(key)
  except KeyError:
    login = item.user.login
    url = item.html_url
    created_at = item.created_at
    author_activity.add_activity(key,login,url,created_at)
  scrape_author(people,gh,rname,item.user,local_stats,commit,author_activity,org_name)

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

def scrape_commit(people,gh,rname,commit,local_stats,author_activity,org_name):
  key = "commit.%s" % commit.sha
  try:
    (login,url,created_at) = author_activity.get_activity(key)
  except KeyError:
    login = commit.author.login
    url = commit.html_url
    created_at = commit.commit.author.date
    author_activity.add_activity(key,login,url,created_at)
  scrape_author(people,gh,rname,commit.author,local_stats,True,author_activity,org_name)
  local_stats['commits'] += 1
    
# the commit parameter is True if the author did a commit, False otherwise (e.g. commented on an Issue for example)
def scrape_author(people,gh,repo,author,repo_stats,commit,author_activity,org_name):
  avoid_rate_limiting(gh)
  (company, email, login, previously_known) = author_info(people,author,org_name)

  Type = people.get_type(login)
  while(True):
    if Type is None:
      Type = 'unknown'
    Type = Type.replace(' ','_')
    key = "%s_%s" % (Type.lower(), 'committers' if commit else 'participants')
    repo_stats[key].add(login)
    # We used to loop if someone was a Hackathon person so that they would also be counted as external.
    # But this is confusing and potentially results in double-counting.
    # Instead, let's just count them once and take sums when we do reporting
    #if Type == 'Hackathon': 
    #  Type = 'External'  # loop so that we count hackathon participants also as external
    #else:
    #  break
    break

  if not previously_known:
    repo_stats['new_logins'].add(login)
  
  # don't add None values to these sets
  if company:
    repo_stats['companies'].add(company)
    if commit:
      repo_stats['companies_contributing'].add(company)
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

def add_star_watch_fork(key,url,item,stats,people,author,author_activity,Type,gh,repo,org_name):
    try:
      (login,url,created_at) = author_activity.get_activity(key)
    except KeyError:
      try:
        login = item.owner.login # fork
        created_at = item.created_at
      except AttributeError:
        try:
          login = item.login  # watcher
          created_at = None
        except AttributeError:
          try:
            login = item.user.login # stargazer
          except AttributeError:
            print("WTF: what we think is a stargazer isn't:", item)
            print("Cowardly no longer attempting to add this item")
            return
          created_at = item.starred_at
      author_activity.add_activity(key=key,login=login,url=url,created_at=created_at)
    scrape_author(people,gh,repo,author,stats,False,author_activity,org_name)
    stats[Type].add((login,created_at))
    if people.is_external(login):
      stats['%s_external' % Type].add((login,created_at))
    
# this function assumes that all initial values are empty or 0
# however, if we are running in update mode, the values will be pre-initialized
# that is fine for the values that are over-written but problematic for the values that are incremented
# therefore, just in case we are running in update mode, explicitly reset incremented values to 0 before incrementing
def get_top_level_repo_info(stats,repo,people,author_activity,gh,org_name):
  print("Getting top level info from %s" % repo)
  stats['branches']              = len(list(repo.get_branches()))
  try:
    stats['clones_unique_14_days'] = repo.get_clones_traffic()['uniques']
    stats['clones_count_14_days']  = repo.get_clones_traffic()['count']
    stats['views_unique_14_days']  = repo.get_views_traffic()['uniques']
    stats['views_count_14_days']   = repo.get_views_traffic()['count']
  except github.GithubException as e:
    # sadly need push access in order to see traffic.  :(
    print("Can't get traffic: GithubException %s" % e.data)

  for f in repo.get_forks():
    key = 'fork -> %s' % (f.full_name)
    url = key
    add_star_watch_fork(key=key,url=url,item=f,stats=stats,people=people,author=f.owner,author_activity=author_activity,Type='forks',gh=gh,repo=repo,org_name=org_name)

  for sg in repo.get_stargazers_with_dates():
    key = 'starred -> %s:%s' % (repo,sg.user.login)
    url = 'starred -> %s' % repo
    add_star_watch_fork(key=key,url=url,item=sg,stats=stats,people=people,author=sg.user,author_activity=author_activity,Type='stars',gh=gh,repo=repo,org_name=org_name)

  for w in repo.get_watchers():
    key = 'watched -> %s:%s' % (repo,w.login)
    url = 'watched -> %s' % repo
    add_star_watch_fork(key=key,url=url,item=w,stats=stats,people=people,author=w,author_activity=author_activity,Type='watchers',gh=gh,repo=repo,org_name=org_name)

  # get referrers
  # this is just quick and dirty and seagate-specific top-referrers
  # should also figure out how to make a better general solution
  try:
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

    top_paths = []
    for tp in list(repo.get_top_paths())[0:3]:
      avoid_rate_limiting(gh)
      top_path = { 'count' : tp.count, 'uniques' : tp.uniques, 'path' : tp.path }
      top_paths.append(top_path)
    stats['top_paths'] = top_paths
  except github.GithubException as e:
    print("Can't get referrers: GithubException %s" % e.data)

  stats['downloads_releases'] = 0 # needed if we are running in update mode
  stats['downloads_vms'] = 0 # needed if we are running in update mode
  for r in repo.get_releases():
    for a in r.get_assets():
      avoid_rate_limiting(gh)
      stats['downloads_releases'] += a.download_count
      if 'VA' in a.browser_download_url:
        stats['downloads_vms'] += a.download_count


def get_commits(rname,repo,local_stats,people,author_activity,gh,org_name):
  print("Scraping commits from %s" % rname)
  avoid_rate_limiting(gh)
  try:
    commits = repo.get_commits()
  except Exception as e:
    print("WTF: get_commits failed?", e)
    commits = () 
  avoid_rate_limiting(gh)
  for c in commits:
    avoid_rate_limiting(gh)
    if c.author:
      scrape_commit(people,gh,rname,c,local_stats,author_activity,org_name)
    if c.commit and c.commit.message:
      email_addresses = re.findall(r'[\w\.-]+@[\w\.-]+', c.commit.message) # search for email addresses in commit message; might be some due to DCO
      #print("Adding Commit info %s and %s" % (c.author.company, c.author.email))
      local_stats['email_addresses'] = local_stats['email_addresses'].union(email_addresses)
    for comment in c.get_comments():
      scrape_comment(people,gh,rname,comment,"commit",local_stats,author_activity,org_name)

def summarize_consolidate(local_stats,global_stats,people,author_activity,ave_age_str):
  summary_stats = {}
  local_stats['domains']                  = clean_domains(domains_from_emails(local_stats['email_addresses']))
  local_stats['external_email_addresses'] = remove_string_from_set(local_stats['email_addresses'],'seagate')
  local_stats['new_external_activities']  = get_external_activities(people,author_activity.get_new_activities())

  # remove some bullshit companies
  for bs_company in ('Seagate', 'Codacy', 'Dependabot'):
    for key in [ 'companies', 'companies_contributing' ]:
      try:
        local_stats[key].remove(bs_company)
      except KeyError:
        pass

  # now merge each value from local_stats into global stats
  for key in sorted(local_stats.keys()):
    try:                        # for values that are ints
      if 'unique' in key:
        global_stats[key] = max(global_stats[key],local_stats[key]) 
      elif ave_age_str in key:
        # treat averages differently, put the total value in here, down below adust by count to get actual average 
        item  = key[0:len(key)-len(ave_age_str)] # get the substring of this key that corresponds to the count
        count = local_stats[item]
        global_stats[key] += (local_stats[key] * count)
      else:
        global_stats[key] += local_stats[key]
    except TypeError:           # other values should be sets
      global_stats[key] = global_stats[key].union(local_stats[key])

# issues includes pr's.  Very confusing!  Thanks github!
def get_issues_and_prs(rname,repo,local_stats,people,author_activity,gh,org_name):
  print("Scraping issues and prs from %s" % rname)
  avoid_rate_limiting(gh)
  try:
    issues = repo.get_issues(state='all')
  except GithubException as e:
    print("WTF: get_issues failed? will recurse and try again", e)
    return get_issues_and_prs(rname,repo,local_stats,people,author_activity,gh,org_name) # recurse

  for issue in issues:
    if issue.pull_request is None:
      Type = 'issues'
      commit = False
    else:
      issue = issue.as_pull_request()
      Type = 'pull_requests'
      commit = True
    scrape_issue_or_pr(people,gh,rname,issue,local_stats,author_activity,Type,commit,org_name)
    for comment in issue.get_comments():
      avoid_rate_limiting(gh)
      scrape_comment(people,gh,rname,comment,"issue",local_stats,author_activity,org_name)

def get_contributors(rname,repo,local_stats,people,gh,org_name):
  # collect info from contributors
  print("Scraping contributors from %s" % rname)
  avoid_rate_limiting(gh)
  try:
    contributors = repo.get_contributors()
  except github.GithubException as e:
    print("Ugh, githubexception %s " % e.data )
    print("while trying to get contributors for %s. will recurse and try again" % (rname))
    return get_contributors(rname,repo,local_stats,people,gh,org_name)  # try to fix by recursing.

  try:
    for c in contributors:
      scrape_author(people,gh,rname,c,local_stats,True,None,org_name)
      local_stats['contributors'].add(c.login)
  except github.GithubException as e:
    print("Ugh, githubexception %s " % e.data )
    print("while trying to get contributors for %s. cowardly failing" % (rname))

# little helper function for putting empty k-v pairs into stats structure
def load_actors(D,people):
  actors = [ k.replace(' ','_').lower() for k in people.get_types() ] 
  actors.append('unknown')
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

# the referrers set is a bit weird so consolidate the global view of it a bit differently
# hmm, it actually seems a bit quixotic to attempt to handle global completely in this script
# because if we don't scrape all repos, then global won't be correct anyway
def consolidate_referrers(referrers):
  nr = {}
  for r in referrers:
    if r.referrer not in nr:
      nr[r.referrer] = { 'count' : 0, 'uniques' : 0 } 
    nr[r.referrer]['count'] += r.count
    nr[r.referrer]['uniques'] = max(nr[r.referrer]['uniques'],r.uniques)
  return nr

# if update is true, it loads an existing pickle instead of creating a new one
# this is useful when new fields are added 
def collect_stats(gh,org_name,update,prefix,top_only):
  avoid_rate_limiting(gh)
  today = datetime.today().strftime('%Y-%m-%d')

  # populate our persistent data structures from the pickles
  people = cortx_community.CortxCommunity(org_name)             
  author_activity = cortx_community.CortxActivity(org_name)     
  persistent_stats = cortx_community.PersistentStats(org_name)  

  # averages are weird so handle them differently
  ave_age_str='_ave_age_in_s'

  # the shared structure that we use for collecting stats
  global_stats = { 'branches'                      : 0, 
                   'clones_count_14_days'          : 0,
                   'clones_unique_14_days'         : 0,
                   'comments'                      : 0,
                   'commits'                       : 0, 
                   'companies_contributing'        : set(),
                   'companies'                     : set(), 
                   'contributors'                  : set(), 
                   'domains'                       : set(), 
                   'downloads_releases'            : 0,
                   'downloads_vms'                 : 0,
                   'email_addresses'               : set(), 
                   'external_comments'             : 0,
                   'external_email_addresses'      : set(),
                   'forks_external'                : set(),
                   'forks'                         : set(),
                   'logins'                        : set(), 
                   'new_external_activities'       : set(),
                   'new_logins'                    : set(),
                   'pull_requests_external_merged' : 0,
                   'pull_requests_internal_merged' : 0,
                   'pull_requests_merged'          : 0,
                   'seagate_blog_referrer_count'   : 0,
                   'seagate_blog_referrer_uniques' : 0,
                   'seagate_referrer_count'        : 0,
                   'seagate_referrer_uniques'      : 0,
                   'stars_external'                : set(),
                   'stars'                         : set(),
                   'top_paths'                     : [], 
                   'top_referrers'                 : [],
                   'views_count_14_days'           : 0,
                   'views_unique_14_days'          : 0,
                   'watchers_external'             : set(),
                   'watchers'                      : set(),
                    }
  load_actors(global_stats,people)
  load_items(global_stats,('issues','pull_requests'),('_external','_internal',''),('','_open','_closed','_open_ave_age_in_s','_closed_ave_age_in_s'))
  local_stats_template = copy.deepcopy(global_stats)    # save an empty copy of the stats struct to copy for each repo

  for repo in cortx_community.get_repos(org_name=org_name,prefix=prefix): 

    local_stats = copy.deepcopy(local_stats_template) # get an empty copy of the stats structure
    rname=repo.name # just in case this requires a github API call, fetch it once and reuse it

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
      get_top_level_repo_info(local_stats,repo,people=people,author_activity=author_activity,gh=gh,org_name=org_name)
      get_contributors(rname,repo,local_stats,people=people,gh=gh,org_name=org_name)
      if not top_only:
        get_issues_and_prs(rname,repo,local_stats,people=people,author_activity=author_activity,gh=gh,org_name=org_name)
        get_commits(rname,repo,local_stats,people=people,author_activity=author_activity,gh=gh,org_name=org_name)

    # what we need to do is query when the last time this ran and then pass 'since' to get_commits

    # summarize info for this repo and persist the data structures
    summarize_consolidate(local_stats,global_stats,people=people,author_activity=author_activity,ave_age_str=ave_age_str)
    persist_author_activity(author_activity)
    persistent_stats.add_stats(date=today,repo=rname,stats=local_stats)
    persistent_stats.print_repo(rname,local_stats,date=today,verbose=False,csv=False)

  # do a bit of cleaning on global stats
  # print and persist the global consolidated stats

  # treat the 'ave_age_in_s' fields differently 
  # all those fields have consistent names: 'x_ave_age_in_s'
  # also, there will always be a corresponding field x which is the count
  for ave_age in [key for key in global_stats.keys() if ave_age_str in key]:
    item  = ave_age[0:len(ave_age)-len(ave_age_str)]
    try:
      global_stats[ave_age] /= global_stats[item]
    except ZeroDivisionError:
      global_stats[ave_age] = 0

  global_stats['top_referrers'] = consolidate_referrers(global_stats['top_referrers'])

  persistent_stats.print_repo('GLOBAL',global_stats,date=today,verbose=False,csv=False)
  persistent_stats.add_stats(date=today,repo='GLOBAL',stats=global_stats)

def report_status(signalNumber, frame):
  js=cortx_community.check_rate_limit()
  rem = js['rate']['remaining']
  res = js['rate']['reset']
  interval = res - time.time()
  print("\n%d requests remaining, %.1f minutes until reset" % (rem,interval/60.0))

def main():
  parser = argparse.ArgumentParser(description='Collect and print info about all cortx activity in public repos.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('-u', '--update', help='Load last stats and update it instead of creating a new one.', action='store_true')
  parser.add_argument('-t', '--toponly', help='Only scrape top-level info for the repo', action='store_true')
  parser.add_argument('project', help='The project whose repos to scrape', action='store')  # one required arg for org
  #parser.add_argument('--dump', '-d', help="Dump currents stats [either '%s', '%s', or '%s'" % (PNAME,INAME,TNAME), required=False)
  #parser.add_argument('--collect', '-c', help='Collect new stats', action='store_true')
  args = parser.parse_args()

  # register a simple signal handler so the impatient can see what's happening
  signal.signal(signal.SIGINT, report_status)

  try:
    (org,prefix) = cortx_community.projects[args.project]
  except KeyError:
    print('%s is not a known project' % args.project)
    sys.exit(0)

  # now go off and do a ton of work. :)
  retry= Retry(total=10,status_forcelist=(500,502,504,403),backoff_factor=10) 
  per_page=100
  gh = Github(login_or_token=os.environ.get('GH_OATH'),per_page=per_page, retry=retry)
  collect_stats(gh=gh,org_name=org,update=args.update,prefix=prefix,top_only=args.toponly)

if __name__ == "__main__":
    main()

