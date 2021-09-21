#!/bin/sh

HAS_ENABLED_KEY=''
HAS_CUSTOM_CONFIG=''

kwinrcname=`qtpaths --locate-file GenericConfigLocation kwinrc`
if [ -f "$kwinrcname" ]; then
    if grep -q "\[Effect-kwin4_effect_translucency\]" "$kwinrcname"; then
        HAS_CUSTOM_CONFIG=1
    fi
fi

while read -r line; do
    KEY="${line%=*}"
    if [ "$KEY" = "kwin4_effect_translucencyEnabled" ]; then
        HAS_ENABLED_KEY=1
    fi
    echo "$line"
done

if [ -n "$HAS_CUSTOM_CONFIG" ] && [ -z "$HAS_ENABLED_KEY" ]; then
    echo "kwin4_effect_translucencyEnabled=true"
fi
