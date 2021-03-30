add_executable(kwin_wayland_wrapper kwin_wrapper.cpp wl-socket.c)
target_link_libraries(kwin_wayland_wrapper Qt5::Core)
set_property(TARGET kwin_wayland_wrapper PROPERTY C_STANDARD 11)
install(TARGETS kwin_wayland_wrapper ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
