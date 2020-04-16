#!/bin/sh
# Script to generate potential object leak due to parallel/concurrent PUT requests

aws s3 rb s3://mybucket --force
rm -rf ./paralleltest/*
echo Creating new bucket "mybucket"
aws s3 mb s3://mybucket
filecnt=12
paralel=$((filecnt / 4))
mkdir -p ./paralleltest
# Create $filecnt files, each of 1MB size
for value in $(seq -f "file%.0f" $filecnt)
do
dd if=/dev/urandom of=./paralleltest/$value bs=1M count=1 iflag=fullblock
done
echo "Created $filecnt files"

# Write ${filecnt} files/objects to bucket 'mybucket', in parallel ($paralel at a time)
seq -f "file%.0f" $filecnt| xargs -n 1 -t -P $paralel -I {} aws s3 cp `pwd`/paralleltest/{} s3://mybucket/POCKey

# Cleanup
rm -rf ./paralleltest/*
rm -rf ./paralleltest
