#! /bin/sh
# remove <number>=<shortcuts> lines for per-window shortcuts that are not supposed to be saved
while read line; do
    if echo "$line" | grep '^[0-9]\+=' >/dev/null 2>/dev/null; then
        key=`echo "$line" | sed 's/^\([0-9]\+\)=.*$/\1/'`
        echo '# DELETE [kwin]'$key
    else
        echo "$line"
    fi
done
