#! /bin/bash 

since='2020-05-01'
until='2020-07-01'

# get the detailed point report for innersource
./get_personal_activity.py Innersource -s $since -u $until -d

# make sure there aren't any unknowns who need to be reclassified as innersource
echo "The following are unknown contributions during the time period:"
echo "Temporarily disabled"
#./get_personal_activity.py Unknown -s $since -u $until | egrep -v 'starred|fork|actions'
