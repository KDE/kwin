include_directories( ${CMAKE_SOURCE_DIR}/workspace/kwin/lib  )


########### next target ###############

set(kcm_kwindecoration_PART_SRCS kwindecoration.cpp buttons.cpp preview.cpp )

set(kwin_xml  ${CMAKE_SOURCE_DIR}/workspace/kwin/org.kde.KWin.xml)

QT4_ADD_DBUS_INTERFACE( kcm_kwindecoration_PART_SRCS ${kwin_xml} kwin_interface )


kde4_automoc(${kcm_kwindecoration_PART_SRCS})


kde4_add_plugin(kcm_kwindecoration ${kcm_kwindecoration_PART_SRCS})



target_link_libraries(kcm_kwindecoration ${KDE4_KDE3SUPPORT_LIBS} kdecorations ${X11_LIBRARIES})

install(TARGETS kcm_kwindecoration  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES kwindecoration.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )


