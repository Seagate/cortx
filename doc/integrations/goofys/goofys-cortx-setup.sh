#!/bin/bash

sudo apt-get update && sudo apt-get upgrade -y
sudo apt-get install golang awscli -y

aws configure set aws_access_key_id $3 --profile default
aws configure set aws_secret_access_key $4 --profile default
aws configure set region us-east-1 --profile default

mkdir -p $HOME/go/src/github.com/kahing/
cd $HOME/go/src/github.com/kahing/
git clone https://github.com/kahing/goofys.git

export GOPATH=$HOME/go
export GOOFYS_HOME=$HOME/go/src/github.com/kahing/goofys/

cd $HOME/go/src/github.com/kahing/goofys
git submodule init
git submodule update

go install $HOME/go/src/github.com/kahing/goofys
PATH=$PATH:$HOME/go/bin; export PATH

mkdir -p $HOME/shared
goofys --endpoint http://$1 $2 $HOME/shared
