########### next target ###############

set(kwin_killer_helper_SRCS killer.cpp )


kde4_add_executable(kwin_killer_helper ${kwin_killer_helper_SRCS})
set_target_properties(kwin_killer_helper PROPERTIES COMPILE_FLAGS ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS})

target_link_libraries(kwin_killer_helper
    Qt5::Widgets
    Qt5::X11Extras
    ${KDE4Support_LIBRARIES} # KCmdLineArgs
    ${KInterProcessWindowing_LIBRARY}
    KF5::KAuth
    KF5::KI18n
    KF5::KWidgetsAddons
)

install(TARGETS kwin_killer_helper DESTINATION ${LIBEXEC_INSTALL_DIR} )
