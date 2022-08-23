#! /usr/bin/env python3 

from atlassian import Confluence

import argparse
import os
import PyPDF2


token=os.getenv('JI_OATH')

conf=Confluence(url='https://seagate-systems.atlassian.net',
                api_version='cloud',
                username='john.bent@seagate.com',
                password=token)

def get_children(parent,depth=0):
  children=conf.get_page_child_by_type(parent, type='page', start=None, limit=None, expand='history')
  for child in sorted(children, key = lambda i: i['title']):
   indent=" " * depth
   print(indent,parent,child['history']['createdBy']['email'],child['title'])
   get_children(child['id'],depth+1) # recurse

def main():
  parser = argparse.ArgumentParser(description='Collect and print info about all descendants of a confluence page', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('-s', '--space', help='Which space', default='PRIVATECOR') 
  parser.add_argument('title', type=str, help='The title of the page to query.')
  args = parser.parse_args()

  parent = conf.get_page_id(args.space,args.title)
  get_children(parent)


if __name__ == "__main__":
    main()
