target_sources(kwin PRIVATE
    x11_windowed_backend.cpp
    x11_windowed_egl_backend.cpp
    x11_windowed_logging.cpp
    x11_windowed_output.cpp
    x11_windowed_qpainter_backend.cpp
)

target_link_libraries(kwin
    X11::XCB
    X11::X11
    XCB::PRESENT
)
if (X11_Xi_FOUND)
    target_link_libraries(kwin X11::Xi)
endif()
