########### next target ###############

set(kwin_killer_helper_SRCS killer.cpp )


add_executable(kwin_killer_helper ${kwin_killer_helper_SRCS})
set_target_properties(kwin_killer_helper PROPERTIES COMPILE_FLAGS ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS})

target_link_libraries(kwin_killer_helper
    Qt5::Widgets
    Qt5::X11Extras
    KF5::Auth
    KF5::I18n
    KF5::WidgetsAddons
)

install(TARGETS kwin_killer_helper DESTINATION ${LIBEXEC_INSTALL_DIR} )
