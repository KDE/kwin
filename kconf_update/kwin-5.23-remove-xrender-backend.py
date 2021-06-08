#!/usr/bin/env python3

import fileinput

for line in fileinput.input():
    if not line.startswith("Backend="):
        continue
    value = line[len("Backend="):].strip()
    if value != "XRender":
        continue
    print("# DELETE Backend") # will use the default compositing type
