#!/bin/bash

#echo -e "\n\n\nRUN mero unit test cases"
#cd /data/mero
#./scripts/m0 run-ut
#echo -e "\n\n\nRUN kernal mero unit test cases"
#cd /data/mero
#./scripts/m0 run-kut

echo -e "\n\nRUN halon st"
cd /data/halon
./scripts/h0 test run-st
echo -e "\n\nDONE !!!"


