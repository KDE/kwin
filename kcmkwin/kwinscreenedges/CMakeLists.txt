set(
    kcm_kwinscreenedges_PART_SRCS
    main.cpp
    monitor.cpp
    screenpreviewwidget.cpp
    )
qt5_wrap_ui( kcm_kwinscreenedges_PART_SRCS main.ui )
add_library( kcm_kwinscreenedges MODULE ${kcm_kwinscreenedges_PART_SRCS} )
target_link_libraries( kcm_kwinscreenedges ${X11_LIBRARIES}
    Qt5::DBus
    KF5::Completion
    KF5::ConfigCore
    KF5::ConfigWidgets
    KF5::I18n
    KF5::Service
    KF5::Plasma
    )
install( TARGETS kcm_kwinscreenedges  DESTINATION ${PLUGIN_INSTALL_DIR} )

install( FILES kwinscreenedges.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
