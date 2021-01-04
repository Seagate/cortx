#! /bin/bash -x

since='2020-10-01'
until='2021-01-01'

# get the detailed point report for innersource
./get_personal_activity.py Innersource -s $since -u $until -d

# make sure there aren't any unknowns who need to be reclassified as innersource
echo "The following are unknown contributions during the time period:"
./get_personal_activity.py Unknown -s $since -u $until | egrep -v 'starred|fork|actions'
