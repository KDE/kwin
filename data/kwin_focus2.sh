#! /bin/sh
while read line; do
    if echo "$line" | grep '^IgnoreFocusStealingClasses=' >/dev/null 2>/dev/null; then
        echo "$line" | sed 's/,kded//' | sed 's/kded,//' | sed 's/,kget//' | sed 's/kget,//'
    else
        echo "$line"
    fi
done
