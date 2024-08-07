# Testing in KWin
KWin provides a unit and integration test suite for X11 and Wayland. The source code for the tests can be found in the subdirectory autotests. The test suite should be run prior to any merge to KWin.

# Dependencies
The following additional software needs to be installed for running the test suite:

* Xvfb
* Xephyr
* glxgears
* DMZ-white cursor theme
* breeze window decoration

# Preparing OpenGL
Some of the tests require OpenGL. The test suite is implemented against Mesa and uses the Mesa specific EGL extension
EGL_MESA_platform_surfaceless. This extension supports rendering without any real GPU using llvmpipe as software
emulation. This gives the tests a stable base removing variance introduced by different hardware and drivers.

Users of non-Mesa drivers (e.g. proprietary NVIDIA driver) need to ensure that Mesa is also installed. If your system
uses libglvnd this should work out of the box, if not you might need to tune LD_LIBRARY_PATH.

# Preventing side effects
To prevent side effects with the running session it is recommended to run tests
in a dedicated dbus session. This can be achieved by prefixing the test command 
with `dbus-run-session`, as shown in the examples below.

# Running tests
Tests are more likely to succeed when run from ssh, as the environment is
further isolated from the user's session. For example:

```bash
ssh localhost
```

Then, run the tests as described below.

## Running the test suite
The test suite can be run from the build directory. Best is to do:

    cd path/to/build/directory
    dbus-run-session xvfb-run ctest

## Running individual tests
All tests executables are created in the directory "bin" in the build directory. Each test can be executed by just starting it from within the test directory:

    cd path/to/build/directory/bin
    dbus-run-session ./testFoo

For tests relying on X11 one should also either start a dedicated Xvfb and export DISPLAY or use xvfb-run as described above.
