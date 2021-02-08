#! /usr/bin/env python3

from cortx_community import get_logins,CortxCommunity,get_teams,SlackCommunity,CortxActivity

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

def get_mergable_email(source,target):
  se = source.get_email()
  te = target.get_email()
  assert se or te, "Could not find a mergable email"
  if se and te:
    if '@seagate.com' in se:
      return se
    elif '@seagate.com' in te:
      return te
    else:
      assert se == te, "Can't merge different non-seagate emails %s != %s" % (se,te)
  if se:
    return se
  else:
    return te

def get_activities(login,activity):
  activities={}
  for (url,created_at) in activity.get_activities(login):
    if created_at is not None:  # just don't count watch events since they don't have a date
      activities[created_at] = url
  return activities


# function to merge two different people into one
# this usually happens when we find someone in slack and can't automatically find their github login
def merge(target_login,source_login,people):
  slack_people=SlackCommunity() 
  activity=CortxActivity()
  # what do we need to do here?
  # 1. find all activity belonging to merge and add it to individual - DONE
  # 2. find merge in slack people and change github login to individual - DONE
  # 3. remove merge from cortx people
  # 4. copy the slack id from merge into target
  assert target_login, "Can't merge without specifying the individual into whom to merge" 
  #print("need to merge %s into %s" % (source_login, target_login)) 
  try:
    activities=get_activities(source_login,activity)
  except KeyError: # this person has no activities
    activities={}
  target=people.get_person(target_login)
  source=people.get_person(source_login)
  email=get_mergable_email(source,target)
  print("need to merge %s into %s using %s" % (source, target, email)) 
  sperson=slack_people.find_login(source_login)
  assert sperson, "Couldn't find %s in slack pickle" % email
  slack_people.set_github(sperson,target_login)
  print("Also need to clean up slack person %s" % sperson)
  for date,url in activities.items():
    #def add_activity(self,key,login,url,created_at):
    key="migrated_event.%s.%s.%s.%s" % (url,date,target_login,source_login)
    try:
      (login,url,created_at) = activity.get_activity(key) # already exists
    except:
      print("Not yet migrated: Migrating %s %s" % (date,url))
      activity.add_activity(key,target_login,url,date)

  # copy over company, type, linkedin, and email; merge notes
  if source.get_company() and not target.get_company():
    print("Trying to transfer company %s" % source.get_company())
    people.set_company(target_login,source.get_company())
  if source.get_type() and not target.get_type():
    print("Trying to transfer type %s" % source.get_type())
    people.set_type(target_login,source.get_type())
  if source.get_linkedin() and not target.get_linkedin():
    print("Trying to transfer type %s" % source.get_linkedin())
    people.set_linkedin(target_login,source.get_linkedin())
  if source.get_note():
    print("Trying to transfer note %s" % source.get_note())
    people.add_note(target_login,source.get_note())
  people.update_email(target_login,email)

  if 'GID_UNK' in source_login:
    people.remove_person(source_login)
  else:
    print("Cowardly refusing to remove non GID_UNK login")

  activity.persist()
  slack_people.persist()
  people.persist()

def main():
  parser = argparse.ArgumentParser(description='Update or print info in our cortx community pickle.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('--individual',  '-i', help='Update or print just one single person.')
  parser.add_argument('--type',        '-t', help="When you update an individual, what type is she")
  parser.add_argument('--email',       '-e', help="When you update an individual, add her email")
  parser.add_argument('--company',     '-c', help="When you update an individual, what company is she")
  parser.add_argument('--linkedin',    '-l', help="When you update an individual, add her linkedin profile")
  parser.add_argument('--note',        '-n', help="When you update an individual, add a note (must be formatted as python dict)")
  parser.add_argument('--unknowns',    '-u', help="Dump the unknowns and quit", action="store_true")
  parser.add_argument('--dump',        '-d', help="Dump entire community and quit", action="store_true")
  parser.add_argument('--slack',       '-s', help="Operate on the slack people", action="store_true")
  parser.add_argument('--merge',       '-m', help="Merge one person into another")
  parser.add_argument('--org',         '-o', help='Print the latest statistics for a different org', default='Seagate')
  parser.add_argument('--github',      '-g', help='Change the github login for a slack person', action="store")
  args = parser.parse_args()

  if args.slack:
    people = SlackCommunity(org_name=args.org)
  else:
    people = CortxCommunity(org_name=args.org)
  gh = Github(os.environ.get('GH_OATH'))

  if args.merge:
    merge(target_login=args.individual,source_login=args.merge,people=people)

  if (args.individual):
    updated=False
    if not args.slack:
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
        if args.note:
          updated = True
          note = json.loads(args.note) 
          people.add_note(args.individual,note)
        print(people.people[args.individual])
      else:
        print("Person %s not in the known community" % args.individual)
    else:
      if args.github:
        gpeople=CortxCommunity()
        gperson = None
        sperson = None
        try:
          gperson=gpeople.get_person(args.github)
        except KeyError:
          print("Error: %s is unknown github ID" % args.github)
        try:
          sperson=people.get_github(args.individual)
        except TypeError:
          print("Error: %s is unknown slack ID" % args.individual)
        assert gperson and sperson, "Can't operate unless both args are valid"
        people.set_github(args.individual,args.github)
        updated=True
      people.print_person(args.individual)
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
    if not args.slack:
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

