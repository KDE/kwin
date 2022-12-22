#compdef kwin_x11

# SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>
#
# SPDX-License-Identifier: GPL-2.0-or-later

# `--lock`, `--crashes` & `--no-kactivities` are common between x11 & wayland.
# `--replace` only differs in description.

_arguments -s \
  '(- *)'{-h,--help}'[Displays help on commandline options]' \
  '(- *)'{-v,--version}'[Displays version information]' \
  \
  '--lock[Disable configuration options]' \
  '--crashes=[Indicate that KWin has recently crashed n times]:n:_numbers' \
  '--no-kactivities[Disable KActivities integration]' \
  '--replace[Replace already-running ICCCM2.0-compliant window manager]'
