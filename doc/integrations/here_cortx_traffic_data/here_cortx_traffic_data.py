#!/usr/bin/env python
# coding: utf-8
import requests
import json
import simplejson
import re
import boto3
from datetime import date
from datetime import datetime, timedelta
import smtplib

# Get route data
url = "https://data.traffic.hereapi.com/v7/incidents?in=circle:53.347019,-6.258394;r=2000&locationReferencing=shape&lang=en-US&inc22=0&details=true&apiKey=BRu0LRXmYLe-Im_GKb8zBltxJjGVlXeWDS8LOCNPJLA"
page = requests.get(url)
string_ = page.text
json_string = page.json()

# parse data
todays_data = dict()
for i in json_string['results']:
    loc = i['incidentDetails']['description']['value']

    if loc in todays_data:
        todays_data[loc] += 1
    else:
        todays_data[loc] = 1


print(todays_data)

# create bucket named the current date
ACCESS_KEY = 'sgiamadmin'
SECRET_ACCESS_KEY = 'ldapadmin'
END_POINT_URL = 'http://192.168.1.16:31949'

s3_client = boto3.client('s3', endpoint_url=END_POINT_URL,
                         aws_access_key_id=ACCESS_KEY,
                         aws_secret_access_key=SECRET_ACCESS_KEY,
                         verify=False)

s3_resource = boto3.resource('s3', endpoint_url=END_POINT_URL,
                             aws_access_key_id=ACCESS_KEY,
                             aws_secret_access_key=SECRET_ACCESS_KEY,
                             region_name='None',
                             verify=False)

today = date.today()

bucket_name = str(today)

#s3_client.create_bucket(Bucket=bucket_name)

# clean locations dict for writing to cortx
locations_data = []
for i in todays_data:
    d = str(i) + ', ' + str(todays_data[i])
    locations_data.append(d)

for loc in locations_data:
    loc = loc.split(', ')
    print(loc[0]+ ', ' + str(loc[1]) + '\n')

f = open("data.txt", "w")
for loc in locations_data:
    loc = loc.split(', ')
    f.write(loc[0]+ ', ' + str(loc[1]) + '\n')
f.close()

s3_resource.Bucket(str(today)).upload_file('data.txt', 'data.txt')

gmail_user = 'dylanthegander@gmail.com'
gmail_app_password = 'drilqouaaojpfrme'

sent_from = gmail_user
sent_to = ['dylanthegander@gmail.com']
sent_subject = "Travel Updates!"
sent_body = ''

for loc in todays_data:
    sent_body += str(loc) + '\n'
email_text = """From: %s
To: %s
Subject: %s

%s
""" % (sent_from, ", ".join(sent_to), sent_subject, sent_body)

try:
    server = smtplib.SMTP_SSL('smtp.gmail.com', 465)
    server.ehlo()
    server.login(gmail_user, gmail_app_password)
    server.sendmail(sent_from, sent_to, email_text)
    server.close()
    print('Email sent!')
except Exception as exception:
    print("Error: %s!\n\n" % exception)

# get yesterday's date
yesterday = today - timedelta(days=1)

# read yesterday's data from s3
s3_resource = boto3.resource('s3', endpoint_url=END_POINT_URL,
                             aws_access_key_id=ACCESS_KEY,
                             aws_secret_access_key=SECRET_ACCESS_KEY,
                             region_name='None',
                             verify=False)

s3_resource.Bucket(str(yesterday)).download_file('data.txt', 'data.txt')

f = open("data.txt", "r")

data = f.readlines()
f.close()

for i in range(len(data)):
    data[i] = data[i].replace('\n', '')
    data[i] = data[i].split(', ')[0]
    print(data[i])

# append today's data with yesterday's data
location_count = dict()

for d in data:
    if d in location_count:
        location_count[d] += 1
    else:
        location_count[d] = 1

for loc in todays_data:
    if loc in location_count:
        location_count[loc] += 1
    else:
        location_count[loc] = 1

for i in location_count:
    print(location_count[i])
    f = open("data.txt", "w")

for loc in location_count:
    f.write(loc+ ', ' + str(location_count[loc]) + '\n')
f.close()

# upload data with today and yesterday's data combined
s3_resource.Bucket(str(today)).upload_file('data.txt', 'data.txt')