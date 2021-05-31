#!/usr/bin/env python3

import fileinput

for line in fileinput.input():
    if not line.startswith("LayoutName="):
        continue
    value = line[len("LayoutName="):].strip()
    if value != "coverswitch":
        continue
    print("# DELETE LayoutName") # will use the default layout
