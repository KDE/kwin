#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2024 Natalie Clarius <natalie.clarius@kde.org>
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import io
import subprocess

# renames the "NightColor" group into to "NightLight" 

proc = subprocess.Popen(
    [
        "qtpaths",
        "--locate-file",
        "ConfigLocation",
        "kwinrc",
    ],
    stdout=subprocess.PIPE,
)
for line in io.TextIOWrapper(proc.stdout, encoding="utf-8"):
    kwinrcPath = line.removesuffix("\n")

if len(kwinrcPath) == 0 or not kwinrcPath.endswith("kwinrc"):
    # something is wrong
    exit()

with open(kwinrcPath, "r+") as kwinrc:
    inputLines = kwinrc.readlines()
    outputLines = []

    for line in inputLines:
        if line == "[NightColor]\n":
            outputLines.append("[NightLight]\n")
        else:
            outputLines.append(line)

    kwinrc.truncate(0)
    kwinrc.seek(0)
    kwinrc.writelines(outputLines)
