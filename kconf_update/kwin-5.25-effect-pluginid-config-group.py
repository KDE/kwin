#!/usr/bin/env python3

import fileinput

def migrate_group(old, new, line):
    if not "[Effect-{}".format(old) in line:
        return
    print("# DELETE Effect-{}".format(old))
    print("[Effect-{}]\n".format(new))

for line in fileinput.input():
    if not "[Effect-" in line:
        print(line)
        continue
    migrate_group("Blur", "blur", line)
    migrate_group("DesktopGrid", "desktopgrid", line)
    migrate_group("DimInactive", "diminactive", line)
    migrate_group("FallApart", "fallapart", line)
    migrate_group("Glide", "glide", line)
    migrate_group("Kscreen", "kscreen", line)
    migrate_group("LookingGlass", "lookingglass", line)
    migrate_group("MagicLamp", "magiclamp", line)
    migrate_group("Magnifier", "magnifier", line)
    migrate_group("MouseClick", "mouseclick", line)
    migrate_group("MouseMark", "mousemark", line)
    migrate_group("Overview", "overview", line)
    migrate_group("PresentWindows", "presentwindows", line)
    migrate_group("Sheet", "sheet", line)
    migrate_group("ShowFps", "showfps", line)
    migrate_group("Slide", "slide", line)
    migrate_group("SlidingPopups", "slidingpopups", line)
    migrate_group("ThumbnailAside", "thumbnailaside", line)
    migrate_group("TrackMouse", "trackmouse", line)
    migrate_group("Wobbly", "wobblywindows", line)
    migrate_group("Zoom", "zoom", line)
