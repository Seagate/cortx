#! /usr/bin/env python3

import cortx_community

import argparse

def Debug(msg):
  print(msg)

def main():
  parser = argparse.ArgumentParser(description='Set a value for CORTX Community.', add_help=False, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  required = parser.add_argument_group('required arguments')
  optional = parser.add_argument_group('optional_requirements')
  optional.add_argument(
      '-h',
      '--help',
      action='help',
      help='show this help message and exit'
  )
  required.add_argument('--key', '-k', type=str, help="Which key to set / query", required=True)
  optional.add_argument('--value', '-v', type=str, help="Which value to set", required=False)
  optional.add_argument('--date', '-d', type=str, help='Which date to set', required=False)
  optional.add_argument('--org',  '-o', help='Which org',  default='Seagate')
  optional.add_argument('--repo', '-r', help='Which repo', default='GLOBAL')
  args = parser.parse_args()

  repo = args.repo
  org  = args.org
  key  = args.key
  val  = args.value
  date = args.date

  ps = cortx_community.PersistentStats(org_name=args.org)
  dates=ps.get_dates(args.repo)

  if date is None:
    date = dates[-1]
    print("Defaulting to use last valid date %s" % date)

  if val is not None:
    ps.add_stat(date=date,repo=repo,stat=key,value=int(val))
    print("Changing %s on %s to be %s" % (repo,date,val))

  for d in dates:
    print( d, args.key, ps.get_values_as_numbers(args.repo,args.key,[d]))

if __name__ == "__main__":
    main()

