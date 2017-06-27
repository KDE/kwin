set(X11PLATFORM_SOURCES
    edge.cpp
    logging.cpp
    x11cursor.cpp
    x11_platform.cpp
    screens_xrandr.cpp
    windowselector.cpp
    overlaywindow_x11.cpp
)

if(X11_Xinput_FOUND)
    set(X11PLATFORM_SOURCES ${X11PLATFORM_SOURCES} xinputintegration.cpp)
endif()

if(HAVE_EPOXY_GLX)
    set(X11PLATFORM_SOURCES ${X11PLATFORM_SOURCES} glxbackend.cpp glx_context_attribute_builder.cpp)
endif()

add_library(KWinX11Platform MODULE ${X11PLATFORM_SOURCES})
target_link_libraries(KWinX11Platform eglx11common kwin Qt5::X11Extras XCB::CURSOR)
if(X11_Xinput_FOUND)
    target_link_libraries(KWinX11Platform ${X11_Xinput_LIB})
endif()

if(HAVE_DL_LIBRARY)
    target_link_libraries(KWinX11Platform ${DL_LIBRARY})
endif()

install(
    TARGETS
        KWinX11Platform
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/org.kde.kwin.platforms/
)
