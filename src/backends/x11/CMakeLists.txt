target_sources(kwin PRIVATE
    x11_windowed_backend.cpp
    x11_windowed_egl_backend.cpp
    x11_windowed_logging.cpp
    x11_windowed_output.cpp
    x11_windowed_qpainter_backend.cpp
)

target_link_libraries(kwin
PRIVATE
    XCB::XCB
    XCB::DRI3
    XCB::PRESENT
)
if (TARGET XCB::XINPUT)
    target_link_libraries(kwin PRIVATE XCB::XINPUT)
endif()
