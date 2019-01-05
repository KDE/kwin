# Testing in KWin
KWin provides a unit and integration test suit for X11 and Wayland. The source code for the tests can be found in the subdirectory autotests. The test suite should be run prior to any merge to KWin.

# Dependencies
The following additional software needs to be installed for running the test suite:

* Xvfb
* Xephyr
* glxgears
* DMZ-white cursor theme
* breeze window decoration

# Preparing a run of the test suite
In case your system does not support the EGL extension EGL_MESA_platform_surfaceless,
please load the kernel module "vgem". This is required to provide a virtual OpenGL device.

    sudo modprobe vgem

Furthermore the user executing the test suite must be able to read and write to the dri device created by vgem.

# Running the test suite
The test suite can be run from the build directory. Best is to do:

    cd path/to/build/directory
    xvfb-run ctest

# Running individual tests
All tests executables are created in the directory "bin" in the build directory. Each test can be executed by just starting it from within the test directory. To prevent side effects with the running session it is recommended to start a dedicated dbus session:

    cd path/to/build/directory/bin
    dbus-run-session ./testFoo

For tests relying on X11 one should also either start a dedicated Xvfb and export DISPLAY or use xvfb-run as described above.
