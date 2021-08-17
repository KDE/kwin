set(kwin_wayland_wrapper_SOURCES
    kwin_wrapper.cpp
    updatelaunchenvjob.cpp
    wl-socket.c
)

ecm_qt_declare_logging_category(kwin_wayland_wrapper_SOURCES
    HEADER
        wrapper_logging.h
    IDENTIFIER
        KWIN_WRAPPER
    CATEGORY_NAME
        kwin_wayland_wrapper
    DEFAULT_SEVERITY
        Warning
)
qt_add_dbus_interface(kwin_wayland_wrapper_SOURCES org.kde.Startup.xml startup_interface)

add_executable(kwin_wayland_wrapper ${kwin_wayland_wrapper_SOURCES})

target_link_libraries(kwin_wayland_wrapper Qt5::Core Qt5::DBus KF5::CoreAddons KWinXwaylandCommon)
set_property(TARGET kwin_wayland_wrapper PROPERTY C_STANDARD 11)
install(TARGETS kwin_wayland_wrapper ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
