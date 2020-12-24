#! /usr/bin/env python3

import cortx_community

import argparse
import copy
import json
import os
import pickle
import pprint
import sys
import time
from github import Github

def Debug(msg):
  print(msg)

def print_interesting_arrays(rname,stats):
  # new_external_activities  # once the pickle is established, we should start showing this one 
  print("\nInteresting fields")
  for k in ('companies', 'external_committers', 'external_participants', 
            'hackathon_committers', 'hackathon_participants', 'innersource_committers', 'innersource_participants', 
            'new_logins', 'unknown_committers', 'unknown_participants', 'domains', 'external_email_addresses',
            'top_paths', 'top_referrers'):
    print("%s %d %s " % (rname,len(stats[k]),k), stats[k], "\n")

def consolidate_referrers(referrers):
  nr = {}
  for r in referrers:
    if r.referrer not in nr:
      nr[r.referrer] = { 'count' : 0, 'uniques' : 0 } 
    nr[r.referrer]['count'] += r.count
    nr[r.referrer]['uniques'] = max(nr[r.referrer]['uniques'],r.uniques)
  return nr

def main():
  parser = argparse.ArgumentParser(description='Print the latest statistics for CORTX Community.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('-v', '--verbose', action='store_true', help="Show all data")
  parser.add_argument('-a', '--all', action='store_true', help="Show all repos (i.e. not just summary)")
  parser.add_argument('-s', '--suppress', action='store_true', help="Don't show interesting fields")
  parser.add_argument('-i', '--individual', type=str, help="Only show data for a single repo")
  parser.add_argument('-c', '--csv', action='store_true', help="Output in csv")
  args = parser.parse_args()

  ps = cortx_community.PersistentStats()
  repos = sorted([repo for repo in ps.get_repos() if repo != 'GLOBAL'])

  # first build the global stats
  gstats = {} 
  gstats['repo_count'] = len(repos)
  timestamp = None
  for repo in repos: 
    (rstats,timestamp) = ps.get_latest(repo)
    for k,v in rstats.items():
      if isinstance(v,int) or isinstance(v,float):
        if k not in gstats:
          gstats[k] = 0
        gstats[k] += v
      elif isinstance(v,set):
        if k not in gstats:
          gstats[k] = set()
        gstats[k] |= v
      elif isinstance(v,list):
        if k not in gstats:
          gstats[k] = []
        gstats[k] += v
      else:
        raise TypeError("%s has unknown type %s" % (k, type(v)))

  # top referrers is a bit weird so clean that one up specifically here
  gstats['top_referrers'] = consolidate_referrers(gstats['top_referrers'])

  # remove some bullshit companies
  for bs_company in ('Seagate', 'Codacy', 'Dependabot'):
    gstats['companies'].remove(bs_company)

  if args.individual:
    (repo,timestamp) = ps.get_latest(args.individual)
    ps.print_repo(args.individual,repo,timestamp,verbose=args.verbose,csv=args.csv)
    if not args.suppress:
      print_interesting_arrays(args.individual,repo)
  else:
    ps.print_repo('GLOBAL',gstats,timestamp,verbose=args.verbose,csv=args.csv)
    if not args.suppress:
      print_interesting_arrays('GLOBAL',gstats)


  if args.all:
    for repo in repos:
      (rstats,timestamp) = ps.get_latest(repo)
      ps.print_repo(repo,rstats,timestamp,verbose=args.verbose,csv=args.csv)
    

if __name__ == "__main__":
    main()

