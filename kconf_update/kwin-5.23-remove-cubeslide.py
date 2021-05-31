#!/usr/bin/env python3

import fileinput

for line in fileinput.input():
    if not line.startswith("cubeslideEnabled="):
        continue
    value = line[len("cubeslideEnabled="):].strip()
    if value == "true":
        print("# DELETE cubeslideEnabled")
        print("# DELETE slideEnabled") # make slide effect enabled, it's off now
