########### next target ###############

set(kwin_killer_helper_SRCS killer.cpp)

add_executable(kwin_killer_helper ${kwin_killer_helper_SRCS})

target_link_libraries(kwin_killer_helper
    KF6::AuthCore
    KF6::I18n
    KF6::WidgetsAddons
    Qt::Widgets
)
if (QT_MAJOR_VERSION EQUAL "5")
    target_link_libraries(kwin_killer_helper Qt::X11Extras)
else()
    target_link_libraries(kwin_killer_helper Qt::GuiPrivate)
endif()

install(TARGETS kwin_killer_helper DESTINATION ${KDE_INSTALL_LIBEXECDIR})
