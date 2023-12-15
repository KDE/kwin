add_subdirectory(drm)
add_subdirectory(fakeinput)
add_subdirectory(libinput)
add_subdirectory(virtual)
add_subdirectory(wayland)

if (KWIN_BUILD_X11)
    add_subdirectory(x11)
endif()
