########### add version number into compilation defines
add_definitions ( -DAPP_VERSION=\\\"${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}\\\")
add_subdirectory( config )

########### next target ###############
set(kwin_nitrogen_SRCS
     lib/helper.cpp
     lib/tileset.cpp
     nitrogen.cpp
     nitrogenbutton.cpp
     nitrogenclient.cpp
     nitrogenconfiguration.cpp
     nitrogenexception.cpp
     nitrogenexceptionlist.cpp
     nitrogensizegrip.cpp
     x11util.cpp
  )

kde4_add_plugin(kwin3_nitrogen ${kwin_nitrogen_SRCS})
target_link_libraries(
    kwin3_nitrogen
    ${KDE4_KDEUI_LIBS}
    ${X11_X11_LIB}
    ${X11_Xrender_LIB}
    kdecorations
  )

install(TARGETS kwin3_nitrogen  DESTINATION ${PLUGIN_INSTALL_DIR} )

########### install files ###############
install( FILES nitrogenclient.desktop  DESTINATION  ${DATA_INSTALL_DIR}/kwin/ )

