#!/usr/bin/env python

from __future__ import print_function

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
