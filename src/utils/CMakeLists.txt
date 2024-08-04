target_sources(kwin PRIVATE
    common.cpp
    cursortheme.cpp
    drm_format_helper.cpp
    edid.cpp
    filedescriptor.cpp
    orientationsensor.cpp
    ramfile.cpp
    realtime.cpp
    softwarevsyncmonitor.cpp
    subsurfacemonitor.cpp
    udev.cpp
    vsyncmonitor.cpp
)
if (KWIN_BUILD_X11)
    target_sources(kwin PRIVATE xcbutils.cpp)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_sources(kwin PRIVATE executable_path_proc.cpp)
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    target_sources(kwin PRIVATE executable_path_sysctl.cpp)
else()
    message(FATAL_ERROR "Unsupported platform ${CMAKE_SYSTEM_NAME}")
endif()
