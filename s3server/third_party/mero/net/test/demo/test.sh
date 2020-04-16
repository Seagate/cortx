#!/usr/bin/env bash
set -eux

x=1
for i in {1..100}; do
	let x++
done
echo $x
