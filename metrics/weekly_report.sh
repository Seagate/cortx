#! /bin/bash

# this is currently running on Windows Subsystem Linux and sometimes mail is flakey
# sudo service postfix status may be needed 
# migrated it to run in ssc-vm but now it looks like cron doesn't load the env var I need

# this is probably not the right way to do this but manually source the .bashrc
. ~/.bashrc


# this only works for some dumb reason if you're calling the script with the full path
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd $DIR

mail_subj_prefix="Weekly CORTX Community Report"
email="john.bent@seagate.com"

# start with a git pull in case the pickles were updated elsewhere
git pull

# scrape the metrics and mail the raw dump
tfile=$(mktemp /tmp/cortx_community.XXXXXXXXX.txt)
./scrape_metrics.py > $tfile
echo "Please see attached" | mail -s "$mail_subj_prefix : Scraper Output" -r $email -a $tfile $email 

# mail the metrics as a CSV 
ts=`date +%Y-%m-%d`
tfile="/tmp/cortx_community_stats.$ts.csv"
./print_metrics.py -c -a -s | grep -v '^Statistics' > $tfile
./print_metrics.py | mail -s "$mail_subj_prefix : Summary with Attached CSV" -r $email -a $tfile $email 

# mail innersource and external activity reports
tfile=$(mktemp /tmp/cortx_community.XXXXXXXXX)
for group in Innersource External Unknown
do
  ./get_personal_activity.py $group -l > $tfile
  mail -s "$mail_subj_prefix : $group Activity" -r $email $email < $tfile
done

# mail the team report
./get_personal_activity.py 'VenkyOS,johnbent,justinzw,Saumya-Sunder,novium258' -l > $tfile
mail -s "$mail_subj_prefix : Open Source Team Activity" -r $email $email < $tfile

# commit the pickles because they were updated in the scrape
./commit_pickles.sh | mail -s "Weekly Pickle Commit for CORTX Community" -r $email $email

# make the executive report
exec_report=CORTX_Metrics_Topline_Report
jupyter nbconvert --to pdf --output-dir=/tmp --no-input --output $exec_report.$ts $exec_report.ipynb
echo "Please see attached" | mail -s "$mail_subj_prefix : Metrics Executive Report" -r $email -a /tmp/$exec_report.$ts.pdf $email 

# make the bulk conversion of all metrics into graphs report
bulk_report=CORTX_Metrics_Graphs
jupyter nbconvert --to pdf --output-dir=/tmp --no-input --output $bulk_report.$ts $bulk_report.ipynb
echo "Please see attached" | mail -s "$mail_subj_prefix : Metrics Bulk Report" -r $email -a /tmp/$bulk_report.$ts.pdf $email 
