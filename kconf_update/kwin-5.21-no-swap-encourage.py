#!/usr/bin/env python3

import fileinput

for line in fileinput.input():
    if not line.startswith("GLPreferBufferSwap="):
        continue
    value = line[len("GLPreferBufferSwap="):].strip()
    if value != "n":
        continue
    print("# DELETE GLPreferBufferSwap") # will use the default swap strategy
