#! /bin/sh
was=
while read line; do
    echo LINE:$line >>/tmp/qwe.txt
    if echo "$line" | grep '^IgnoreFocusStealingClasses=' >/dev/null 2>/dev/null; then
        echo REPLACE >>/tmp/qwe.txt
        echo "$line" | sed 's/\(^IgnoreFocusStealingClasses=.*$\)/\1,kded,kio_uiserver,kget/'
        echo "$line" | sed 's/\(^IgnoreFocusStealingClasses=.*$\)/\1,kded,kio_uiserver,kget/' >>/tmp/qwe.txt
        was=1
    else
        echo "$line"
    fi
done
if test -z "$was"; then
    echo APPEND >>/tmp/qwe.txt
    echo "IgnoreFocusStealingClasses=kded,kio_uiserver,kget"
fi
