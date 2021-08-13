#!/usr/bin/env python3

import fileinput
import configparser
import os.path
import subprocess

# Get the config standard locations
config_locations = subprocess.check_output(['qtpaths', '--paths', 'ConfigLocation']).decode('utf-8').strip().split(':')
config_paths = [os.path.join(folder, 'kwinrc') for folder in config_locations]

# Get the desktops information from `kwinrc` config file
kwinrc = configparser.ConfigParser(strict=False, allow_no_value=True)
kwinrc.read(config_paths)

num_desktops = int(kwinrc.get('Desktops', 'Number', fallback=''))

# Generete the map from x11ids (ennumeration) to UUIDs
desktopUUIDs = { -1: "" } # NET::OnAllDesktops -> Empty string
for i in range(1, num_desktops + 1):
    uuid = kwinrc.get('Desktops', f'Id_{i}', fallback='')
    desktopUUIDs[i] = str(uuid)

# Apply the conversion to `kwinrulesrc`
for line in fileinput.input():
    # Rename key `desktoprule` to `desktopsrule`
    if line.startswith("desktoprule="):
        value = line[len("desktoprule="):].strip()
        print("desktopsrule=" + value)
        print("# DELETE desktoprule")
        continue

    if not line.startswith("desktop="):
        print(line.strip())
        continue

    # Convert key `desktop` (x11id) to `desktops` (list of UUIDs)
    try:
        value = line[len("desktop="):].strip()
        x11id = int(value)
        uuid = desktopUUIDs[x11id]
    except:
        print(line.strip())  # If there is some error, print back the line
        continue

    print("desktops=" + uuid)
    print("# DELETE desktop")
