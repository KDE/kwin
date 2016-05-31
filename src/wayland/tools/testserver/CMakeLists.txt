add_executable(org-kde-kf5-kwayland-testserver main.cpp testserver.cpp)
target_link_libraries(org-kde-kf5-kwayland-testserver Qt5::Core KF5::WaylandServer)
install(TARGETS org-kde-kf5-kwayland-testserver DESTINATION ${LIBEXEC_INSTALL_DIR} )
