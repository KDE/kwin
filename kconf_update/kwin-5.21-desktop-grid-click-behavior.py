#!/usr/bin/env python3

import sys

VALUE_MAP = {
    'true': 0,
    'false': 1,
}

if __name__ == '__main__':
    for line in sys.stdin:
        line = line.strip()
        if line.startswith('PresentWindows='):
            _, value = line.split('=', 1)
            print("# DELETE PresentWindows")
            print("ClickBehavior=%d" % VALUE_MAP[value])
