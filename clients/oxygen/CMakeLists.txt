# $Id: CMakeLists.txt,v 1.39 2009/09/13 00:33:34 hpereira Exp $

find_package (KDE4 REQUIRED)
add_definitions (-DQT3_SUPPORT -DQT3_SUPPORT_WARNINGS)

include (KDE4Defaults)
include (MacroLibrary)
add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})

include_directories (${KDE4_INCLUDES} ${CMAKE_BINARY_DIR})

########### add version number into compilation defines
add_definitions ( -DAPP_VERSION=\\\"${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}\\\")
add_subdirectory( config )

########### next target ###############
set(kwin_oxygen_SRCS
     lib/helper.cpp
     lib/tileset.cpp
     oxygen.cpp
     oxygenbutton.cpp
     oxygenclient.cpp
     oxygenconfiguration.cpp
     oxygenexception.cpp
     oxygenexceptionlist.cpp
     oxygenshadowconfiguration.cpp
     oxygenshadowcache.cpp
     oxygensizegrip.cpp
     x11util.cpp
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
