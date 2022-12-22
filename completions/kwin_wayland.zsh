#compdef kwin_wayland kwin_wayland_wrapper

# SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>
#
# SPDX-License-Identifier: GPL-2.0-or-later

# `--lock`, `--crashes` & `--no-kactivities` are common between x11 & wayland.
# `--replace` only differs in description.

function _wayland_scale() {
  # identical to kscreen-doctor completions from libkscreen, except ',' decimal separator is replaced '.'
  local -a scale_descr scale_comp
  scale_descr=('100%' '125%' '150%' '175%' '200%' '225%' '250%' '275%' '300%' )
  scale_comp=( '1'    '1.25' '1.5'  '1.75' '2'    '2.25' '2.5'  '2.75' '3'    )
  _describe -t scale "global scaling" scale_descr scale_comp -o nosort
}

_arguments -s \
  '(- *)'{-h,--help}'[Displays help on commandline options]' \
  '(- *)'{-v,--version}'[Displays version information]' \
  \
  '--lock[Disable configuration options]' \
  '--crashes=[Indicate that KWin has recently crashed n times]:n:_numbers' \
  '--no-kactivities[Disable KActivities integration]' \
  '--replace[Exits this instance so it can be restarted by kwin_wayland_wrapper.]' \
  \
  '(-s --socket)'{-s=,--socket=}'[Name of the Wayland socket to listen on. If not set "wayland-0" is used.]:socket:( wayland-{0..9} )' \
  \
  '--xwayland[Start a rootless Xwayland server.]' \
  '--wayland-fd=[Wayland socket to use for incoming connections. This can be combined with --socket to name the socket.]:wayland-fd:_numbers "file descriptor number"' \
  '--xwayland-fd=[XWayland socket to use for Xwayland''s incoming connections. This can be set multiple times]:xwayland-fds:_numbers "file descriptor number"' \
  '--xwayland-xauthority=[Name of the xauthority file]:xwayland-xauthority:_files' \
  \
  '--x11-display=[The X11 Display to use in windowed mode on platform X11.]:display:( \:{0..9} )' \
  '--wayland-display=[The Wayland Display to use in windowed mode on platform Wayland.]:display:' \
  '--xwayland-display=[Name of the xwayland display that has been pre-set up]:xwayland-display:' \
  \
  '--virtual[Render to a virtual framebuffer.]' \
  '--width=[The width for windowed mode. Default width is 1024.]:width:_numbers' \
  '--height=[The height for windowed mode. Default height is 768.]:height:_numbers' \
  '--scale=[The scale for windowed mode. Default value is 1.]:scale:_wayland_scale' \
  \
  '--output-count=[The number of windows to open as outputs in windowed mode. Default value is 1]:count:_numbers' \
  '--drm[Render through drm node.]' \
  "--locale1[Extract locale information from locale1 rather than the user's configuration]" \
  '--inputmethod=[Input method that KWin starts.]:path/to/imserver:_files' \
  \
  '(--lockscreen --no-lockscreen)--lockscreen[Starts the session in locked mode.]' \
  '(--lockscreen --no-lockscreen)--no-lockscreen[Starts the session without lock screen support.]' \
  '--no-global-shortcuts[Starts the session without global shortcuts support.]' \
  '--exit-with-session=[Exit after the session application, which is started by KWin, closed.]:/path/to/session:_files' \
  '*::applications: _command_names -e'
