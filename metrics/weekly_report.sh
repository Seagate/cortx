#! /bin/bash

# this is currently running on Windows Subsystem Linux and sometimes mail is flakey
# sudo service postfix status may be needed 

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd $DIR

mail_subj_prefix="Weekly CORTX Community Report"
email="john.bent@seagate.com"

# scrape the metrics and mail the raw dump
tfile=$(mktemp /tmp/cortx_community.XXXXXXXXX)
./scrape_metrics.py > $tfile
echo "Please see attached" | mail -s "$mail_subj_prefix : Scraper Output" -r $email $email -A $tfile 

# mail the metrics as a CSV 
ts=`date +%Y-%m-%d`
tfile="/tmp/cortx_community_stats.$ts.csv"
./print_metrics.py -c -a -s | grep -v '^Statistics' > $tfile
echo "Please see attached" | mail -s "$mail_subj_prefix : Summary as Attached CSV" -r $email $email -A $tfile 

# mail innersource and external activity reports
tfile=$(mktemp /tmp/cortx_community.XXXXXXXXX)
for group in Innersource External
do
  ./get_personal_activity.py $group -l > $tfile
  mail -s "$mail_subj_prefix : $group Activity" -r $email $email < $tfile
done

# mail the team report
./get_personal_activity.py 'VenkyOS,johnbent,justinzw,Saumya-Sunder,novium258' -l > $tfile
mail -s "$mail_subj_prefix : Open Source Team Activity" -r $email $email < $tfile

# commit the pickles because they were updated in the scrape
./commit_pickles.sh | mail -s "Weekly Pickle Commit for CORTX Community" -r $email $email
