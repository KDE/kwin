#! /bin/sh
was=
while read line; do
    if echo "$line" | grep '^IgnoreFocusStealingClasses=' >/dev/null 2>/dev/null; then
        echo "$line" | sed 's/\(^IgnoreFocusStealingClasses=.*$\)/\1,kio_uiserver/'
        was=1
    else
        echo "$line"
    fi
done
if test -z "$was"; then
    echo "IgnoreFocusStealingClasses=kio_uiserver"
fi
