#!/bin/bash
cd /data/halon
hctl mero status
./scripts/h0 fini
sleep 10
./scripts/h0 init
sleep 10
hctl mero bootstrap
hctl mero status 
sleep 180
hctl mero status 

