add_executable(kwin_wayland_wrapper kwin_wrapper.c wl-socket.c)
set_property(TARGET kwin_wayland_wrapper PROPERTY C_STANDARD 11)
install(TARGETS kwin_wayland_wrapper ${INSTALL_TARGETS_DEFAULT_ARGS})
