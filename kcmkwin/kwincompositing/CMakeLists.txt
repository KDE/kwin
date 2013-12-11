########### next target ###############

include_directories( ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin )

set(kcm_kwincompositing_PART_SRCS
    main.cpp
    ktimerdialog.cpp
    )
kde4_add_ui_files(kcm_kwincompositing_PART_SRCS main.ui)
set(kwin_xml ${KDEBASE_WORKSPACE_SOURCE_DIR}/kwin/org.kde.KWin.xml)
set_source_files_properties(${kwin_xml} PROPERTIES INCLUDE "interface_util.h")
qt5_add_dbus_interface(kcm_kwincompositing_PART_SRCS ${kwin_xml} kwin_interface)
add_library(kcm_kwincompositing MODULE ${kcm_kwincompositing_PART_SRCS})
target_link_libraries(kcm_kwincompositing KF5::NewStuff ${KDE4_KCMUTILS_LIBS} ${KDE4_KDEUI_LIBS} ${X11_LIBRARIES})
install(TARGETS kcm_kwincompositing  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES kwincompositing.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES kwineffect.knsrc  DESTINATION  ${CONFIG_INSTALL_DIR} )


