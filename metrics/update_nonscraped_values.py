#! /usr/bin/env python3

import cortx_community as cc

import argparse
import dateutil.parser as dateparser
import datetime
import os

def get_all_keys(ps,repo):
  all_keys=set()
  dates = ps.get_dates(repo)
  for d in dates:
    keys=ps.get_keys(repo=repo,date=d)
    all_keys |= keys
  return all_keys

def main():
  parser = argparse.ArgumentParser(description='Update the pickles after scraping for any values which were not scraped.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  args = parser.parse_args()

  ps=cc.PersistentStats()
  repos=ps.get_repos()

  for r in repos:
    keys = get_all_keys(ps,r)
    (latest_stats,latest_date) = ps.get_latest(r)
    dates = ps.get_dates(r)
    for k in keys:
      if k not in latest_stats:
        Values=ps.get_values(r,k,reversed(dates))
        print("Latest on %s for %s is missing %s. Will check %d values" % (latest_date,r,k,len(Values)))
        for Value in Values:
          if Value:
            print("Need to update %s:%s on %s with" % (r,k,latest_date),Value) 
            ps.add_stat(date=latest_date,repo=r,stat=k,value=Value)
            break
          else:
            print("Can't use",Value)
          

    

if __name__ == "__main__":
    main()

