include_directories( ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin )

set(
    kcm_kwinscreenedges_PART_SRCS
    main.cpp
    monitor.cpp
    )
kde4_add_ui_files( kcm_kwinscreenedges_PART_SRCS main.ui )
kde4_add_plugin( kcm_kwinscreenedges ${kcm_kwinscreenedges_PART_SRCS} )
target_link_libraries( kcm_kwinscreenedges ${X11_LIBRARIES} kworkspace ${KDE4_PLASMA_LIBS})
install( TARGETS kcm_kwinscreenedges  DESTINATION ${PLUGIN_INSTALL_DIR} )

install( FILES kwinscreenedges.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
