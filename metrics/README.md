### CORTX Community Scripts

This folder contains a bunch of scripts used by track CORTX Community Metrics.  They are intended to be run using crontab and piped into mail.
They also persist data into pickles and json files as found in the pickles/ child folder.  Those pickles should also be automatically updated
using a crontab job.

The main scripts in this folder are the following:
1. **get_github_community_activity.py**

    a. This is the main worker program that crawls github and puts a ton of into into pickles
    
    b. It creates two main pickles: one for actions and one for people
    
2. **get_activity.py**

    a. This program will print all known activity by a community member
    
3. **update_cortx_community.py**

    a. This program allows making modifications to the people pickle
    
    b. This allows us to identify when various community members are external or not
    
4. **print_activity.py**

    a. This program reads the pickles created by get_github_community_activity.py and prints the data from the latest report
