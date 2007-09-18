#!/bin/sh

while read line; do
	echo "$line" | sed 's@=\s*on\s*$@=true@g' | sed 's@=\s*off\s*$@=false@g'
done
