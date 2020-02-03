# Quick building

KWin uses CMake. This means that KWin can be build in a normal cmake-style out of source tree.

    mkdir build
    cd build
    cmake ../
    make

# Dependencies

All of KWin's dependencies are found through CMake. CMake will report what is missing. The dependencies can be installed using system packages. For the master branch it is possible that system packages are not up to date. KWin master sometimes depends from the master branch of some KDE frameworks (mostly KWayland and KWindowSystems) and other components inside Plasma (KDecoration). KWin never depends on unreleased third party components. In such a case it is required to build these components also from master. Alternatively some distributions provide daily build packages which can also be used instead. Stable branches never depend on unstable components. On Debian based distributions the easiest way to install all build dependencies is

    sudo apt build-dep kwin-wayland

# Running the build KWin
KWin can be executed directly from the build directory. All binaries are put into a subdirectory bin. From there KWin and its tests can be started.

## Nested KWin/Wayland
The best way to test changes in KWin is through using the nested KWin Wayland in a running X11 or Wayland session.

To start a nested KWin Wayland use:

    cd build
    cd bin
    QT_PLUGIN_PATH=`pwd` dbus-run-session ./kwin_wayland --xwayland --socket=wayland-1

The socket option is not required if KWin is started from an X11 session. On Wayland of course a socket not matching the session's socket must be chosen. To show windows in the nested KWin adjust the environment variables DISPLAY (for X11 windows) and WAYLAND_DISPLAY (for Wayland windows). Alternatively it's possible to pass applications to launch as command line arguments to kwin_wayland command. E.g.

    QT_PLUGIN_PATH=`pwd` dbus-run-session ./kwin_wayland --xwayland --socket=wayland-1 konsole

Will start a konsole in the nested KWin.

### Why adjusting QT_PLUGIN_PATH?

Qt's plugin path needs to point to the build directory so that KWin can load the plugins from the build directory instead of using the system installed plugins which would normally be preferred.

### Why start a dedicated DBus session?

KWin interacts with kglobalaccel, so starting KWin without a dedicated DBus session would steal all global shortcuts from the running session. KGlobalaccel is just a very prominent example, there are further DBus interaction problems without a dedicated DBus session.

## DRM platform

The nested setup only works for the X11 and Wayland platform plugins. Changes in the DRM platform plugin or libinput cannot be tested in a nested setup. To test these, change to a tty, login and start kwin_wayland with the same command as for nested. KWin automatically picks the DRM platform as neither DISPLAY nor WAYLAND_DISPLAY environment variables should be defined.

## KWin/X11

KWin for the X11 windowing system cannot be tested with a nested Wayland setup. Instead the common way is to run KWin and replace the existing window manager of the X session:

    cd build
    cd bin
    QT_PLUGIN_PATH=`pwd` ./kwin_x11 --replace

In this case also the current DBus session should be used and dbus-run-session should not be used. Of course it's only possible to start kwin_x11 in an X session. On Wayland kwin_x11 will refuse to start.

### Xephyr

It is possible to run kwin_x11 in a Xephyr window, but this is rather limited and especially the OpenGL compositor cannot really be tested. For X11 it's better to replace the running session. On Wayland using Xephyr is better than nothing.

## Containers
While it is possible to run KWin through container technologies such as docker this is not recommended. KWin needs to interact with the actual hardware such as the OpenGL drivers, input devices, etc. Getting this setup is possible, but complicated. With containers one can only achieve a nested setup and this requires passing through the socket of the host's windowing system, device files for graphics stack, etc.

# Attaching a debugger

Debugging KWin is challenging as KWin is drawing the screen. If you gdb into a running kwin_x11 or kwin_wayland from your current session, it's probably the last thing you'll do in the session. The session hard locks the moment the debugger is attached to the process. Due to that never attach a debugger from your running session.

It is possible to attach gdb from another tty, but that is only a solution for X11. On Wayland one would not be able to switch back to the tty once a breakpoint is hit as KWin is responsible for tty switching.

Overall the only sensible solution for attaching gdb is from another system through ssh.

## Better ways of debugging
As attaching gdb to a running session is not a satisfying solution it is better to run nested KWin Wayland in gdb. E.g.

    cd build
    cd bin
    QT_PLUGIN_PATH=`pwd` dbus-run-session gdb --args ./kwin_wayland --xwayland --socket=wayland-1

Another solution is using KWin's extensive test suite and run the appropriate test binary through gdb.

# Automatic tests
KWin's test suite is explained in document [TESTING](TESTING.md).

# Contributing patches

KWin uses [KDE's phabricator instance](https://phabricator.kde.org) for code review. Patches can be uploaded automatically using the tool arcanist. A possible workflow could look like:

    git checkout -b my-feature-branch
    git add ...
    git commit
    arc diff

More complete documentation can be found in [KDE's wiki](https://community.kde.org/Infrastructure/Phabricator). Please add "#KWin" as reviewers. Please run KWin's automated test suite prior to uploading a patch to ensure that the change does not break existing code.

# Coding conventions
KWin's coding conventions are explained in document [coding-conventions.md](doc/coding-conventions.md).

# Coding style
KWin code follows the [Frameworks coding style](https://techbase.kde.org/Policies/Frameworks_Coding_Style).
