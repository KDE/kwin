
########### next target ###############

set(kcm_kwindesktop_PART_SRCS main.cpp desktopnameswidget.cpp)
kde4_add_ui_files(kcm_kwindesktop_PART_SRCS main.ui)

add_library(kcm_kwindesktop MODULE ${kcm_kwindesktop_PART_SRCS})

target_link_libraries(kcm_kwindesktop
    Qt5::X11Extras
    KF5::KCMUtils
    KF5::KCompletion
    KF5::KI18n
    KF5::KWindowSystem
    KF5::XmlGui
    ${X11_LIBRARIES}
)

install(TARGETS kcm_kwindesktop  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############
install( FILES desktop.desktop DESTINATION  ${SERVICES_INSTALL_DIR} )

