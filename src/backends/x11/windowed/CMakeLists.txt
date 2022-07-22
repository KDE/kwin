target_sources(kwin PRIVATE
    egl_x11_backend.cpp
    logging.cpp
    scene_qpainter_x11_backend.cpp
    x11windowed_backend.cpp
    x11windowed_output.cpp
)

target_link_libraries(kwin X11::XCB X11::X11)
if (X11_Xi_FOUND)
    target_link_libraries(kwin X11::Xi)
endif()
