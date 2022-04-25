#! /bin/bash 

since='2022-01-01'
until='2022-04-01'

# get the detailed point report for innersource
./get_personal_activity.py Innersource -s $since -u $until -d

# make sure there aren't any unknowns who need to be reclassified as innersource
echo "The following are unknown contributions during the time period:"
#echo "Temporarily disabled"
./get_personal_activity.py Unknown -s $since -u $until | egrep -v 'starred|fork|actions'
echo "Update spreadsheet : https://bit.ly/3vw4eDN" 
