#!/usr/bin/env python3.6
# -*- coding: utf-8 -*-
import argparse
import cortx_community as cc
import datetime
import difflib
import json
import os

from time import sleep
from pprint import pprint

UNK_STR='GID_UNK'

def find_closest(gpeople,sname,Verbose=False):
  closest_score=0
  closest_login=None
  for login,gperson in gpeople.items():
    if UNK_STR in login:
      continue
    this_score=difflib.SequenceMatcher(None, login, sname).ratio()
    if this_score>closest_score:
      closest_score=this_score
      closest_login=login
    if Verbose:
      print("%s =?= %s -> %.2f" % (login,sname,this_score))
  return(closest_score,closest_login)

def get_unknowns(gpeople,speople):
  unknowns={}
  for login,person in gpeople.items():
    if UNK_STR in person.get_login():
      # try to find the corresponding person from the slack pickle
      notes=person.get_note()
      slack_id=notes['slack_id']
      sperson=speople.find_person(slack_id)
      unknowns[login]=(person,sperson)
  return unknowns

def main():
  parser = argparse.ArgumentParser(description='Try to find matches for slack people who are not yet matched to github logins.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('-t', '--threshold', action='store', type=float, help='The threshold for a "close" match.', default=0.7)
  args = parser.parse_args()

  gpeople=cc.CortxCommunity()
  speople=cc.SlackCommunity()

  unknowns=get_unknowns(gpeople,speople)
  Verbose=False
  for login,(gperson,sperson) in unknowns.items():
    (score,who)=find_closest(gpeople,sperson['name'],Verbose=Verbose)
    if score > args.threshold:
      print("Sperson %s has %.2f similarity to gperson %s" % (sperson['name'], score, who))

if __name__ == '__main__':
  main()

