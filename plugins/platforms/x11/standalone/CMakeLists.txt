set(X11PLATFORM_SOURCES
    edge.cpp
    logging.cpp
    x11cursor.cpp
    x11_platform.cpp
    screens_xrandr.cpp
    windowselector.cpp
    overlaywindow_x11.cpp
    screenedges_filter.cpp
    non_composited_outline.cpp
    x11_decoration_renderer.cpp
    xfixes_cursor_event_filter.cpp
    effects_x11.cpp
    effects_mouse_interception_x11_filter.cpp
    sync_filter.cpp
)

if(X11_Xinput_FOUND)
    set(X11PLATFORM_SOURCES ${X11PLATFORM_SOURCES} xinputintegration.cpp)
endif()

if(HAVE_EPOXY_GLX)
    set(X11PLATFORM_SOURCES ${X11PLATFORM_SOURCES} glxbackend.cpp glx_context_attribute_builder.cpp)
endif()

include_directories(${CMAKE_SOURCE_DIR}/platformsupport/scenes/opengl)

add_library(KWinX11Platform MODULE ${X11PLATFORM_SOURCES})
target_link_libraries(KWinX11Platform eglx11common kwin kwinxrenderutils SceneOpenGLBackend Qt5::X11Extras XCB::CURSOR KF5::Crash)
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
