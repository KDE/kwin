#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
# SPDX-FileCopyrightText: 2022 Alex Richardson <arichardson.kde@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# This little helper strips unnecessary information from builtin effect metadata files to
# reduce the size of kwin executables and json parsing runtime overhead.

import argparse
import json

def main():
    parser = argparse.ArgumentParser(prog="kwin-strip-effect-metadata")
    parser.add_argument("--source", help="input file", required=True)
    parser.add_argument("--output", help="output file", required=True)
    args = parser.parse_args()
    stripped_json = dict(KPlugin=dict())
    with open(args.source, "r") as src:
        original_json = json.load(src)
        stripped_json["KPlugin"]["Id"] = original_json["KPlugin"]["Id"]
        stripped_json["KPlugin"]["EnabledByDefault"] = original_json["KPlugin"]["EnabledByDefault"]

    with open(args.output, "w") as dst:
        json.dump(stripped_json, dst)


if __name__ == "__main__":
    main()
