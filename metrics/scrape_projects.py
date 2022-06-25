#! /usr/bin/env python3

import argparse
import os
import cortx_community as cc
from github import Github
import time

def get_all_repos(gh,orgs,Verbose=False):
  repos={}
  for project,org in orgs.items():
    org_name    = org[0]
    repo_prefix = org[1]
    repos[project] = cc.get_repos(gh=gh,org_name=org_name,prefix=repo_prefix)
  if Verbose:
    for p,r in repos.items():
      print(p, [k.name for k in r])
  return repos

# a function which returns a list of days from 0 to last day on which a repo got a star/fork
# and a list of the star/fork counts on each of those days
# also fills in the missing days on which no stars/forks were added
def get_stars_or_forks(gh,repos,what,org_name,Verbose=False):
    dates = {}
    # get all the data
    for r in repos:
        if Verbose:
            print("Fetching %s for %s : %s" % (what, org_name,r.name))
        if what is 'stars':
            items = r.get_stargazers_with_dates()
        elif what is 'forks':
            items = r.get_forks()
        else:
            items = r.get_watchers()
        cc.avoid_rate_limiting(gh)
        for sg in items:
            while True:
              cc.avoid_rate_limiting(gh)
              try:
                if what is 'stars':
                    d = sg.starred_at
                else:
                    d = sg.created_at
                if d not in dates:
                    dates[d]=0
                dates[d] += 1
              except:
                print("WTF.  Failed on item, ", sg, " sleeping for 30")
                time.sleep(30)
                continue
              break # break out of while loop
    # convert all the data
    first_day = None
    last_day = None
    count = 0
    counts={}
    for d in sorted(dates):
        if first_day is None:
            first_day = d
        day = (d - first_day).days
        count += dates[d]
        counts[day] = count
        last_day = day
    days = list(range(last_day+1))
    stars = []
    last_stars=0
    for i in range(last_day+1):
        try:
            last_stars = counts[i]
        except KeyError:
            pass
        stars.append(last_stars)
    return(days,stars)

def get_stars(gh,repos,org_name,Verbose=False):
    return get_stars_or_forks(gh,repos,'stars',org_name,Verbose)

def get_forks(gh,repos,org_name,Verbose=False):
    return get_stars_or_forks(gh,repos, 'forks',org_name,Verbose)

def get_watchers(gh,repos,org_name,Verbose=False):
    return get_stars_or_forks(gh,repos, 'watchers',org_name,Verbose)

def main():
  parser = argparse.ArgumentParser(description='Scrape fork and star data for all known projects.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('-v', '--verbose', action='store_true', help="Verbose")
  args = parser.parse_args()

  gh = Github(os.environ.get('GH_OATH'))
  orgs = cc.projects
  repos = get_all_repos(gh,orgs,Verbose=args.verbose)
  star_data={}
  fork_data={}
  watchers={}
  for p in orgs.keys():
    watchers[p] =get_watchers(gh,repos[p],p,Verbose=args.verbose)
    fork_data[p]=get_forks(gh,repos[p],p,Verbose=args.verbose)
    star_data[p]=get_stars(gh,repos[p],p,Verbose=args.verbose)
  projects_data = { 'forks' : fork_data, 'stars' : star_data, 'watchers' : watchers }

  pc=cc.ProjectComparisons(org_name=None,stats=projects_data)
  pc.persist()
  print("Finished successfully")

if __name__ == "__main__":
    main()

