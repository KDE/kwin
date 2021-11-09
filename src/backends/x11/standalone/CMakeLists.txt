set(X11PLATFORM_SOURCES
    edge.cpp
    effects_mouse_interception_x11_filter.cpp
    effects_x11.cpp
    eglbackend.cpp
    logging.cpp
    non_composited_outline.cpp
    overlaywindow_x11.cpp
    screenedges_filter.cpp
    windowselector.cpp
    x11_output.cpp
    x11_platform.cpp
    x11cursor.cpp
    x11placeholderoutput.cpp
    xfixes_cursor_event_filter.cpp
)

add_library(KWinX11Platform MODULE ${X11PLATFORM_SOURCES})
set_target_properties(KWinX11Platform PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/org.kde.kwin.platforms/")
target_link_libraries(KWinX11Platform eglx11common kwin kwinxrenderutils Qt::X11Extras KF5::Crash X11::X11)
if (X11_Xi_FOUND)
    target_sources(KWinX11Platform PRIVATE xinputintegration.cpp)
    target_link_libraries(KWinX11Platform X11::Xi)
endif()

if (HAVE_EPOXY_GLX)
    target_sources(KWinX11Platform PRIVATE
        glx_context_attribute_builder.cpp
        glxbackend.cpp
        glxconvenience.cpp
        omlsynccontrolvsyncmonitor.cpp
        sgivideosyncvsyncmonitor.cpp
    )
endif()

if (HAVE_DL_LIBRARY)
    target_link_libraries(KWinX11Platform ${DL_LIBRARY})
endif()

install(
    TARGETS
        KWinX11Platform
    DESTINATION
        ${KDE_INSTALL_PLUGINDIR}/org.kde.kwin.platforms/
)
