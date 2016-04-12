set(X11PLATFORM_SOURCES
    logging.cpp
    x11_platform.cpp
    screens_xrandr.cpp
)

add_library(KWinX11Platform MODULE ${X11PLATFORM_SOURCES})
target_link_libraries(KWinX11Platform kwin Qt5::X11Extras)

install(
    TARGETS
        KWinX11Platform
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/org.kde.kwin.platforms/
)
