#!/usr/bin/env python3

import fileinput

speed_map = [0, 0.25, 0.5, 1, 2, 4, 20]

for line in fileinput.input():
    if not line.startswith("AnimationSpeed="):
        continue
    speed = int(line[len("AnimationSpeed="):])
    if speed < 0 or speed >= len(speed_map):
        continue
    print("AnimationDurationFactor=%f" % speed_map[speed])
    print("# DELETE AnimationSpeed") #special kconf syntax to remove the old key
