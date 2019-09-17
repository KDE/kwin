########### next target ###############

set(kwin_killer_helper_SRCS killer.cpp)

add_executable(kwin_killer_helper ${kwin_killer_helper_SRCS})

target_link_libraries(kwin_killer_helper
    KF5::AuthCore
    KF5::I18n
    KF5::WidgetsAddons
    Qt5::Widgets
    Qt5::X11Extras
)

install(TARGETS kwin_killer_helper DESTINATION ${LIBEXEC_INSTALL_DIR})
