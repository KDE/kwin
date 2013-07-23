########### next target ###############

set(kwin_killer_helper_SRCS killer.cpp )


kde4_add_executable(kwin_killer_helper ${kwin_killer_helper_SRCS})
set_target_properties(kwin_killer_helper PROPERTIES COMPILE_FLAGS ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS})

target_link_libraries(kwin_killer_helper
    ${Qt5Widgets_LIBRARIES}
    ${KDE4Support_LIBRARIES} # KCmdLineArgs
    ${KDE4_KDECORE_LIBS} # kdebug
    KF5::KAuth
    KF5::KWidgets
    KF5::KWidgetsAddons
)

install(TARGETS kwin_killer_helper DESTINATION ${LIBEXEC_INSTALL_DIR} )
