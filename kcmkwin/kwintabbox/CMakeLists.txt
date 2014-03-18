include_directories( ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/tabbox ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/)

########### next target ###############

set(kcm_kwintabbox_PART_SRCS
    main.cpp
    layoutpreview.cpp
    thumbnailitem.cpp
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/tabbox/tabboxconfig.cpp
)

qt5_wrap_ui( kcm_kwintabbox_PART_SRCS main.ui )

add_library(kcm_kwintabbox MODULE ${kcm_kwintabbox_PART_SRCS})

target_link_libraries(kcm_kwintabbox
    Qt5::Quick
    KF5::KCMUtils
    KF5::Completion
    KF5::GlobalAccel
    KF5::I18n
    KF5::Service
    KF5::NewStuff
    XCB::XCB)

install(TARGETS kcm_kwintabbox  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############
install( FILES kwintabbox.desktop DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES thumbnails/konqueror.png thumbnails/kmail.png thumbnails/systemsettings.png thumbnails/dolphin.png DESTINATION ${DATA_INSTALL_DIR}/kwin/kcm_kwintabbox)
install( FILES kwinswitcher.knsrc  DESTINATION  ${CONFIG_INSTALL_DIR} )
