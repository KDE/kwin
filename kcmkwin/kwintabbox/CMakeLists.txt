include_directories( ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/tabbox ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/)

########### next target ###############

add_definitions(-DTABBOX_KCM)

set(kcm_kwintabbox_PART_SRCS
    main.cpp
    layoutpreview.cpp
    thumbnailitem.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/tabbox/clientmodel.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/tabbox/declarative.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/tabbox/desktopmodel.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/tabbox/tabboxconfig.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/tabbox/tabboxhandler.cpp
)

kde4_add_ui_files( kcm_kwintabbox_PART_SRCS main.ui )

add_library(kcm_kwintabbox MODULE ${kcm_kwintabbox_PART_SRCS})

target_link_libraries(kcm_kwintabbox
    Qt5::Quick
    Qt5::X11Extras
    KF5::KCMUtils
    KF5::KCompletion
    KF5::KDeclarative
    KF5::KI18n
    KF5::KIconThemes
    KF5::KService
    KF5::KWindowSystem
    KF5::Plasma
    KF5::XmlGui
    KF5::KNewStuff
    ${XCB_XCB_LIBRARY})

install(TARGETS kcm_kwintabbox  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############
install( FILES kwintabbox.desktop DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES qml/main.qml DESTINATION ${DATA_INSTALL_DIR}/kwin/kcm_kwintabbox)
install( FILES thumbnails/konqueror.png thumbnails/kmail.png thumbnails/systemsettings.png thumbnails/dolphin.png DESTINATION ${DATA_INSTALL_DIR}/kwin/kcm_kwintabbox)
install( FILES kwinswitcher.knsrc  DESTINATION  ${CONFIG_INSTALL_DIR} )
