#! /usr/bin/env python3

import cortx_community as cc
from github import Github  # https://pygithub.readthedocs.io/
import os

VERBOSE=True
def Verbose(String,warning_msgs=None):
    if VERBOSE:
        print(String) 
    if warning_msgs:
        warning_msgs.append(String)

gh = Github(os.environ.get('GH_OATH'))
cc.avoid_rate_limiting(gh,10,False)

repos=[]
repos=cc.get_repos(None,'Seagate','cortx')
rnames = [r.name for r in repos]
Verbose("CORTX Repos: %s" % ", ".join(rnames))

def get_type(issue):
    if issue.pull_request is None:
        Type="issues"
    else:
        Type="pullrequests"
    return Type

issues={}
for r in repos:
    issues[r.name]=[]
    for i in r.get_issues(state='open'):
        if get_type(i) == "issues":
            issues[r.name].append(i)
    Verbose("Fetched %d open issues for %s" % (len(issues[r.name]), r.name))

for r in repos:
    for i in issues[r.name]:
        is_bug = False
        is_enhancement = False
        for l in i.get_labels():
            if l.name == "bug":
                is_bug = True
                break
            elif l.name == "enhancement":
                is_enhancement = True
                break
        if not is_bug and not is_enhancement:
            print("Issue %s is not labeled as either bug or enhancement" % i.html_url)
