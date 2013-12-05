include_directories( ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin )

set(
    kcm_kwinscreenedges_PART_SRCS
    main.cpp
    monitor.cpp
    screenpreviewwidget.cpp
    )
kde4_add_ui_files( kcm_kwinscreenedges_PART_SRCS main.ui )
add_library( kcm_kwinscreenedges MODULE ${kcm_kwinscreenedges_PART_SRCS} )
target_link_libraries( kcm_kwinscreenedges ${X11_LIBRARIES}
    Qt5::DBus
    KF5::KCompletion
    KF5::KConfigCore
    KF5::KConfigWidgets
    KF5::KI18n
    KF5::KService
    KF5::Plasma
    )
install( TARGETS kcm_kwinscreenedges  DESTINATION ${PLUGIN_INSTALL_DIR} )

install( FILES kwinscreenedges.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
