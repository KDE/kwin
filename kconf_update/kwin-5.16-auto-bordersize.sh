#!/bin/sh

HAS_SIZE=''
HAS_AUTO=''

while read -r line
do
  # substring from beginning to equal sign
  ENTRY="${line%%=*}"
  if [ "$ENTRY" = "BorderSize" ]; then
    HAS_SIZE=1
  fi
  if [ "$ENTRY" = "BorderSizeAuto" ]; then
    HAS_AUTO=1
  fi
  echo "$line"
done

if [ -n "$HAS_SIZE" -a -z "$HAS_AUTO" ]; then
  # unset auto borders if user has set a border
  # size in the past (for good measure make
  # also sure auto borders are not yet set)
  echo "BorderSizeAuto=false"
fi
