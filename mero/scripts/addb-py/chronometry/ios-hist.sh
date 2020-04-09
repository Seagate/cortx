#!/usr/bin/env bash
set -e

python3 hist.py -fpng -o write.png -v -u ms -p fom_req \
	"[[auth,zero-copy-initiate],[zero-copy-initiate,tx_open],[tx_open,stobio-launch],[stobio-launch,network-buffer-release],[tx_commit, finish]]"
python3 hist.py -fpng -o read.png -v -u ms -p fom_req_r \
	"[[network-buffer-acquire,stobio-launch],[stobio-launch,stobio-finish],[stobio-launch,zero-copy-initiate],[zero-copy-initiate,success]]"
