# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcmkwinscreenedges\")

include_directories(${KWIN_SOURCE_DIR}/effects)
set(
    kcm_kwinscreenedges_PART_SRCS
    main.cpp
    monitor.cpp
    screenpreviewwidget.cpp
    )
ki18n_wrap_ui( kcm_kwinscreenedges_PART_SRCS main.ui )
qt5_add_dbus_interface( kcm_kwinscreenedges_PART_SRCS
    ${KWIN_SOURCE_DIR}/org.kde.kwin.Effects.xml kwin_effects_interface)
add_library( kcm_kwinscreenedges MODULE ${kcm_kwinscreenedges_PART_SRCS} )
target_link_libraries( kcm_kwinscreenedges ${X11_LIBRARIES}
    Qt5::DBus
    KF5::Completion
    KF5::ConfigCore
    KF5::ConfigWidgets
    KF5::I18n
    KF5::Service
    KF5::Package
    KF5::Plasma
    kwin4_effect_builtins
    )
install( TARGETS kcm_kwinscreenedges  DESTINATION ${PLUGIN_INSTALL_DIR} )

install( FILES kwinscreenedges.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
