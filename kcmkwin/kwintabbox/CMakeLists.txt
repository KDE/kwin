include_directories( ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/tabbox )

########### next target ###############

set(kcm_kwintabbox_PART_SRCS
    main.cpp
    layoutconfig.cpp
    previewhandlerimpl.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/tabbox/clientitemdelegate.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/tabbox/clientmodel.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/tabbox/desktopitemdelegate.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/tabbox/desktopmodel.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/tabbox/itemlayoutconfig.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/tabbox/tabboxconfig.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/tabbox/tabboxhandler.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/tabbox/tabboxview.cpp )

kde4_add_ui_files( kcm_kwintabbox_PART_SRCS main.ui )
kde4_add_ui_files( kcm_kwintabbox_PART_SRCS layoutconfig.ui )

kde4_add_plugin(kcm_kwintabbox ${kcm_kwintabbox_PART_SRCS})

target_link_libraries(kcm_kwintabbox ${KDE4_KDEUI_LIBS} ${KDE4_KCMUTILS_LIBS} ${KDE4_PLASMA_LIBS} ${X11_LIBRARIES} ${QT_QTXML_LIBRARY} kephal )

install(TARGETS kcm_kwintabbox  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############
install( FILES kwintabbox.desktop DESTINATION  ${SERVICES_INSTALL_DIR} )

