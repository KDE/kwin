########### add version number into compilation defines
add_definitions ( -DAPP_VERSION=\\\"${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}\\\")
add_subdirectory( config )

########### next target ###############
set(kwin_oxygen_SRCS
     lib/helper.cpp
     lib/tileset.cpp
     lib/oxygenanimation.cpp
     oxygen.cpp
     oxygenbutton.cpp
     oxygenclient.cpp
     oxygenclientgroupitemdata.cpp
     oxygenconfiguration.cpp
     oxygenexception.cpp
     oxygenexceptionlist.cpp
     oxygenshadowconfiguration.cpp
     oxygenshadowcache.cpp
     oxygensizegrip.cpp
  )

kde4_add_plugin(kwin3_oxygen ${kwin_oxygen_SRCS})
target_link_libraries(
    kwin3_oxygen
    ${KDE4_KDEUI_LIBS}
    ${X11_X11_LIB}
    ${X11_Xrender_LIB}
    kdecorations
  )

install(TARGETS kwin3_oxygen  DESTINATION ${PLUGIN_INSTALL_DIR} )

########### install files ###############
install( FILES oxygenclient.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin/ )

