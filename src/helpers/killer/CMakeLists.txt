########### next target ###############

set(kwin_killer_helper_SRCS killer.cpp)

add_executable(kwin_killer_helper ${kwin_killer_helper_SRCS})

if (Qt6_VERSION VERSION_GREATER_EQUAL "6.8.0")
    set(private_code_option  "PRIVATE_CODE")
endif()
qt6_generate_wayland_protocol_client_sources(kwin_killer_helper
    ${private_code_option}
    FILES
        ${WaylandProtocols_DATADIR}/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml
)

target_link_libraries(kwin_killer_helper
    KF6::AuthCore
    KF6::GuiAddons
    KF6::I18n
    KF6::Service
    KF6::WidgetsAddons
    Qt::GuiPrivate
    Qt::WaylandClient
    Qt::Widgets
)

ecm_qt_declare_logging_category(kwin_killer_helper
    HEADER debug.h
    IDENTIFIER KWIN_KILLER
    CATEGORY_NAME org.kde.kwin.killer
)

install(TARGETS kwin_killer_helper DESTINATION ${KDE_INSTALL_LIBEXECDIR})

configure_file(org.kde.kwin.killer.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwin.killer.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwin.killer.desktop DESTINATION ${KDE_INSTALL_APPDIR})
