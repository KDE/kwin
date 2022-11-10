set(X11PLATFORM_SOURCES
    x11_standalone_backend.cpp
    x11_standalone_cursor.cpp
    x11_standalone_edge.cpp
    x11_standalone_effects.cpp
    x11_standalone_effects_keyboard_interception_filter.cpp
    x11_standalone_effects_mouse_interception_filter.cpp
    x11_standalone_egl_backend.cpp
    x11_standalone_keyboard.cpp
    x11_standalone_logging.cpp
    x11_standalone_non_composited_outline.cpp
    x11_standalone_output.cpp
    x11_standalone_overlaywindow.cpp
    x11_standalone_placeholderoutput.cpp
    x11_standalone_screenedges_filter.cpp
    x11_standalone_windowselector.cpp
    x11_standalone_xfixes_cursor_event_filter.cpp
)

add_library(KWinX11Platform OBJECT ${X11PLATFORM_SOURCES})
target_link_libraries(KWinX11Platform kwin KF5::Crash X11::X11 XCB::XKB PkgConfig::XKBX11)
if (QT_MAJOR_VERSION EQUAL "5")
    target_link_libraries(KWinX11Platform Qt::X11Extras)
endif()
if (X11_Xi_FOUND)
    target_sources(KWinX11Platform PRIVATE x11_standalone_xinputintegration.cpp)
    target_link_libraries(KWinX11Platform X11::Xi)
endif()

if (HAVE_EPOXY_GLX)
    target_sources(KWinX11Platform PRIVATE
        x11_standalone_glx_backend.cpp
        x11_standalone_glx_context_attribute_builder.cpp
        x11_standalone_glxconvenience.cpp
        x11_standalone_omlsynccontrolvsyncmonitor.cpp
        x11_standalone_sgivideosyncvsyncmonitor.cpp
    )
endif()

if (HAVE_DL_LIBRARY)
    target_link_libraries(KWinX11Platform ${DL_LIBRARY})
endif()
