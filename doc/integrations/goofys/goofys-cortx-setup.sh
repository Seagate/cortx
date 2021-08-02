#!/bin/bash

sudo apt-get update && sudo apt-get upgrade -y
sudo apt-get install golang awscli -y

aws configure set aws_access_key_id $3 --profile default
aws configure set aws_secret_access_key $4 --profile default
aws configure set region us-east-1 --profile default

mkdir -p /root/go/src/github.com/kahing/
cd /root/go/src/github.com/kahing/
git clone https://github.com/kahing/goofys.git

export GOPATH=/root/go
export GOOFYS_HOME=/root/go/src/github.com/kahing/goofys/

cd /root/go/src/github.com/kahing/goofys
git submodule init
git submodule update

go install /root/go/src/github.com/kahing/goofys
PATH=$PATH:/root/go/bin; export PATH

mkdir -p /root/shared
goofys --endpoint http://$1 -o allow_other $2 /root/shared