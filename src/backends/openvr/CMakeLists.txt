find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENVR REQUIRED IMPORTED_TARGET GLOBAL openvr)

target_link_libraries(kwin PRIVATE PkgConfig::OPENVR)

target_sources(kwin PRIVATE
    openvr_backend.cpp
    openvr_egl_backend.cpp
    openvr_logging.cpp
    openvr_output.cpp
)
