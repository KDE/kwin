set(xclipboard_SRCS main.cpp waylandclipboard.cpp)
add_executable(org_kde_kwin_xclipboard_syncer ${xclipboard_SRCS})
target_link_libraries(org_kde_kwin_xclipboard_syncer Qt5::Gui KF5::WaylandClient KF5::Crash)

install(TARGETS org_kde_kwin_xclipboard_syncer DESTINATION ${LIBEXEC_INSTALL_DIR} )
