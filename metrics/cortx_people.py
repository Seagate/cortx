#! /usr/bin/env python3

from cortx_community import get_logins,CortxCommunity,get_teams

import argparse
import json
import os
import re
import sys
import time
from github import Github

def Debug(msg):
  print(msg)

def person_match(person,string):
  match = False
  try:
    if string in person.login.lower():
      match = True
  except:
    pass
  try:
    if string in person.company.lower():
      match = True
  except:
    pass
  try:
    if string in person.email.lower():
      match = True
  except:
    pass
  return match

def main():
  parser = argparse.ArgumentParser(description='Update or print info in our cortx community pickle.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('--individual', '-i', help='Update or print just one single person.')
  parser.add_argument('--type', '-t', help="When you update an individual, what type is she")
  parser.add_argument('--email', '-e', help="When you update an individual, add her email")
  parser.add_argument('--company', '-c', help="When you update an individual, what company is she")
  parser.add_argument('--linkedin', '-l', help="When you update an individual, add her linkedin profile")
  parser.add_argument('--unknowns', '-u', help="Dump the unknowns and quit", action="store_true")
  parser.add_argument('--dump', '-d', help="Dump entire community and quit", action="store_true")
  args = parser.parse_args()

  people = CortxCommunity()
  gh = Github(os.environ.get('GH_OATH'))

  if (args.individual):
    updated=False
    if people.includes(args.individual):
      if args.type:
        updated = True
        people.update_type(args.individual,args.type)
      if args.company:
        updated = True
        people.update_company(args.individual,args.company)
      if args.linkedin:
        updated = True
        people.update_linkedin(args.individual,args.linkedin)
      if args.email:
        updated = True
        people.update_email(args.individual,args.email)
      print(people.people[args.individual])
    else:
      print("Person %s not in the known community" % args.individual)
    if updated:
      people.persist()
    sys.exit(0)

  if args.unknowns:
    unknowns = 0
    for person in people.values():
      if person.type is None: 
        print(person)
        unknowns += 1
        try:
          if person_match(person,'seagate') or person_match(person,'dsr') or person_match(person,'calsoft'):
            print("%s login matches seagate or contractor; adding to CORTX Team" % person.login)
            people.update_company(person.login,'Seagate')
            people.update_type(person.login,'CORTX Team')
            people.persist()
        except AttributeError:
          pass
        try:
            # if they are unknown, look them up and see if they are part of CORTX organization
          user = gh.get_user(login=person.login)
          user.get_organization_membership('Seagate')
          print("%s is in Seagate org; adding to CORTX Team"%person.login)
          people.update_company(person.login,'Seagate')
          people.update_type(person.login,'CORTX Team')
          people.persist()
        except:
          pass

    print("%d total unknowns in community" % unknowns)
    sys.exit(0)
    
  if args.dump:
    print(people)
    types = {}
    for person in people.values():
      if person.type not in types:
        types[person.type] = 0
      types[person.type] += 1
    print(types)
    sys.exit()

  # if no args are passed, the program reaches here
  # let's add a linkedin field for everyone
  # we just used this once to change the pickle to add a field
  #for login,person in people.items():
  #  people.add_person(login,person.company,person.email,None)
  #  if person.type:
  #    people.update_type(login,person.type)
  

  # if no args are passed, the program reaches here
  # if the program reaches here, then it will attempt to auto-update info about the community in the people pickle
  # fetch the innersource folks from the innersource json

  # this code is dangerous.  We needed it once but we don't probably want to run it again
  print("Cowardly doing nothing")
  sys.exit()
  with open('pickles/innersource.json','r') as f:
    innersource = json.load(f)
  Debug("Loaded innersource:")
  print(innersource)

  allteams = get_teams('https://api.github.com/orgs/Seagate/teams/cortx-community/teams')
  team_members = set()
  for team in allteams:
    if 'innersource' not in team:
      Debug("Fetching from %s" % team)
      gids = get_logins('members', 'https://api.github.com/orgs/seagate/teams/%s' % team)
      team_members |= gids

  for person in people:
    if person in team_members:
      people.update_type(person,"CORTX Team")
      people.update_company(person,"Seagate")
    if person in innersource:
      if people.get_type(person) is None:
        people.update_type(person,"Innersource") 
      people.update_company(person,"Seagate")
      people.update_email(person,innersource[person])
    else:
      Type = people.get_type(person)
      if not Type:
        match = re.match('[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}',person)
        if match:
          print("Updating %s to be a mannequin" % person)
          people.update_type(person,"Mannequin")
        else:
          print("Person %s is unknown" % person)

  people.persist()
  

if __name__ == "__main__":
    main()

